#include "storage/mail_store.h"
#include "core/logger.h"

#include <filesystem>
#include <fstream>
#include <chrono>

namespace fs = std::filesystem;

MailStore::MailStore(const std::string& rootDir)
    : rootDir_(rootDir) {}

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

    std::ofstream out(path, std::ios::out | std::ios::binary);
    if (!out) {
        Logger::instance().log(
            LogLevel::Error,
            "MailStore: cannot open file " + path);
        return {};
    }

    // Simple RFC5322-like message
    out << "From: " << msg.from << "\r\n";
    for (const auto& rcpt : msg.recipients) {
        out << "To: " << rcpt << "\r\n";
    }
    out << "Message-ID: <" << id << "@local>\r\n";
    out << "\r\n";
    out << msg.rawData;

    out.close();

    Logger::instance().log(
        LogLevel::Info,
        "MailStore: stored message " + id +
        " for user " + msg.mailboxUser +
        " at " + path);

    return id;
}
