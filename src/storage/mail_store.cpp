#include "storage/mail_store.h"
#include "core/logger.h"

#include <filesystem>
#include <fstream>
#include <chrono>
#include <windows.h>
#include <io.h>

namespace fs = std::filesystem;

MailStore::MailStore(const std::string& rootDir)
    : rootDir_(rootDir) {
    // CRITICAL FIX: Recovery of orphaned temp files on startup
    recoverOrphanedTempFiles();
}

void MailStore::recoverOrphanedTempFiles() {
    try {
        // Find and clean up any orphaned .tmp files from crashed writes
        for (const auto& entry : fs::recursive_directory_iterator(rootDir_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".tmp") {
                try {
                    fs::remove(entry.path());
                    Logger::instance().log(LogLevel::Warn,
                        "MailStore: Recovered orphaned temp file: " + entry.path().string());
                } catch (const std::exception& ex) {
                    Logger::instance().log(LogLevel::Error,
                        "MailStore: Failed to remove orphaned temp file " + entry.path().string() +
                        ": " + ex.what());
                }
            }
        }
    } catch (const std::exception& ex) {
        Logger::instance().log(LogLevel::Error,
            "MailStore: Error during temp file recovery: " + std::string(ex.what()));
    }
}

std::string MailStore::generateId() const {
    using namespace std::chrono;
    auto now = system_clock::now().time_since_epoch();
    auto ms  = duration_cast<milliseconds>(now).count();
    return std::to_string(ms);
}

std::string MailStore::makeUserInboxDir(const std::string& user) const {
    // data/mail/<user>/INBOX
    return (fs::path(rootDir_) / user / "INBOX").string();
}

bool MailStore::ensureDirExists(const std::string& dir) const {
    try {
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
        }
        return true;
    } catch (...) {
        return false;
    }
}

// CRITICAL FIX: Atomic write with fsync for crash safety
bool MailStore::atomicWriteFile(const std::string& path, const std::string& content) {
    std::string tempPath = path + ".tmp";

    // Write to temp file with fsync
    HANDLE hFile = CreateFileA(tempPath.c_str(), GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        Logger::instance().log(LogLevel::Error,
            "MailStore: Failed to create temp file " + tempPath);
        return false;
    }

    DWORD bytesWritten;
    if (!WriteFile(hFile, content.data(), static_cast<DWORD>(content.size()), &bytesWritten, NULL) ||
        bytesWritten != content.size()) {
        Logger::instance().log(LogLevel::Error,
            "MailStore: Failed to write to temp file " + tempPath);
        CloseHandle(hFile);
        DeleteFileA(tempPath.c_str());
        return false;
    }

    // Flush to disk (fsync equivalent)
    if (!FlushFileBuffers(hFile)) {
        Logger::instance().log(LogLevel::Error,
            "MailStore: Failed to flush temp file " + tempPath);
        CloseHandle(hFile);
        DeleteFileA(tempPath.c_str());
        return false;
    }

    CloseHandle(hFile);

    // Atomic rename
    if (!MoveFileExA(tempPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        Logger::instance().log(LogLevel::Error,
            "MailStore: Failed to rename temp file " + tempPath + " to " + path);
        DeleteFileA(tempPath.c_str());
        return false;
    }

    // Verify final file exists
    if (!fs::exists(path)) {
        Logger::instance().log(LogLevel::Error,
            "MailStore: Atomic write verification failed for " + path);
        return false;
    }

    return true;
}

std::string MailStore::makeMessagePath(const std::string& user,
                                       const std::string& id) const {
    return (fs::path(makeUserInboxDir(user)) / (id + ".eml")).string();
}
bool MailStore::moveToQuarantine(const std::string& user,
                                 const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);

    fs::path src = makeMessagePath(user, id);
    fs::path dst = fs::path(rootDir_) / user / "Quarantine" / (id + ".eml");

    try {
        fs::create_directories(dst.parent_path());
        fs::rename(src, dst);
        Logger::instance().log(
            LogLevel::Warn,
            "MailStore: quarantined message " + id);
        return true;
    } catch (...) {
        return false;
    }
}

bool MailStore::deleteMessage(const std::string& user,
                              const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);

    fs::path p = makeMessagePath(user, id);
    try {
        fs::remove(p);
        Logger::instance().log(
            LogLevel::Warn,
            "MailStore: deleted message " + id);
        return true;
    } catch (...) {
        return false;
    }
}

std::string MailStore::store(const StoredMessage& msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (msg.mailboxUser.empty()) {
        Logger::instance().log(
            LogLevel::Error,
            "MailStore: mailboxUser is empty, cannot store message");
        return {};
    }

    std::string inboxDir = makeUserInboxDir(msg.mailboxUser);
    if (!ensureDirExists(inboxDir)) {
        Logger::instance().log(
            LogLevel::Error,
            "MailStore: cannot create mailbox directory " + inboxDir);
        return {};
    }

    std::string id = msg.id.empty() ? generateId() : msg.id;
    std::string path = makeMessagePath(msg.mailboxUser, id);

    // CRITICAL FIX: Build message content
    std::string content;
    content.reserve(msg.rawData.size() + 1024); // Pre-allocate for efficiency

    content += "From: " + msg.from + "\r\n";
    for (const auto& rcpt : msg.recipients) {
        content += "To: " + rcpt + "\r\n";
    }
    content += "Message-ID: <" + id + "@local>\r\n";
    content += "\r\n";
    content += msg.rawData;

    // CRITICAL FIX: Atomic write with fsync for crash safety
    if (!atomicWriteFile(path, content)) {
        Logger::instance().log(
            LogLevel::Error,
            "MailStore: atomic write failed for message " + id);
        return {};
    }

    Logger::instance().log(
        LogLevel::Info,
        "MailStore: durably stored message " + id +
        " for user " + msg.mailboxUser +
        " at " + path);

    return id;
}
