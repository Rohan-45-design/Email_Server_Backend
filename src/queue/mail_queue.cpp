// queue/mail_queue.cpp
#include "queue/mail_queue.h"
#include "core/logger.h"
#include "monitoring/metrics.h"

#include <filesystem>
#include <fstream>
#include <chrono>
#include <random>
#include <windows.h>
#include <io.h>
std::mutex MailQueue::queueMutex_;
std::vector<std::shared_ptr<QueueMessage>> MailQueue::retryQueue_;
std::vector<std::shared_ptr<QueueMessage>> MailQueue::inflightQueue_;
namespace fs = std::filesystem;
using SysClock = std::chrono::system_clock;

static constexpr int LEASE_TIMEOUT_SEC = 300; 

// CRITICAL FIX: Atomic write with fsync for crash safety
static bool atomicWriteFile(const std::string& path, const std::string& content) {
    std::string tempPath = path + ".tmp";

    // Write to temp file with fsync
    HANDLE hFile = CreateFileA(tempPath.c_str(), GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        Logger::instance().log(LogLevel::Error,
            "Queue: Failed to create temp file " + tempPath);
        return false;
    }

    DWORD bytesWritten;
    if (!WriteFile(hFile, content.data(), static_cast<DWORD>(content.size()), &bytesWritten, NULL) ||
        bytesWritten != content.size()) {
        Logger::instance().log(LogLevel::Error,
            "Queue: Failed to write to temp file " + tempPath);
        CloseHandle(hFile);
        DeleteFileA(tempPath.c_str());
        return false;
    }

    // Flush to disk (fsync equivalent)
    if (!FlushFileBuffers(hFile)) {
        Logger::instance().log(LogLevel::Error,
            "Queue: Failed to flush temp file " + tempPath);
        CloseHandle(hFile);
        DeleteFileA(tempPath.c_str());
        return false;
    }

    CloseHandle(hFile);

    // Atomic rename
    if (!MoveFileExA(tempPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        Logger::instance().log(LogLevel::Error,
            "Queue: Failed to rename temp file " + tempPath + " to " + path);
        DeleteFileA(tempPath.c_str());
        return false;
    }

    // Verify final file exists
    if (!fs::exists(path)) {
        Logger::instance().log(LogLevel::Error,
            "Queue: Atomic write verification failed for " + path);
        return false;
    }

    return true;
}

MailQueue& MailQueue::instance() {
    static MailQueue q;
    return q;
}

MailQueue::MailQueue() {
    fs::create_directories("queue/active");
    fs::create_directories("queue/inflight");
    fs::create_directories("queue/failure");
    fs::create_directories("queue/permanent_fail");

    // CRITICAL FIX: Recovery of orphaned temp files on startup
    recoverOrphanedTempFiles();
}

void MailQueue::recoverOrphanedTempFiles() {
    try {
        // Find and clean up any orphaned .tmp files from crashed writes
        for (const auto& entry : fs::recursive_directory_iterator("queue")) {
            if (entry.is_regular_file() && entry.path().extension() == ".tmp") {
                try {
                    fs::remove(entry.path());
                    Logger::instance().log(LogLevel::Warn,
                        "Queue: Recovered orphaned temp file: " + entry.path().string());
                } catch (const std::exception& ex) {
                    Logger::instance().log(LogLevel::Error,
                        "Queue: Failed to remove orphaned temp file " + entry.path().string() +
                        ": " + ex.what());
                }
            }
        }
    } catch (const std::exception& ex) {
        Logger::instance().log(LogLevel::Error,
            "Queue: Error during temp file recovery: " + std::string(ex.what()));
    }
}

std::string MailQueue::genId() {
    static std::mt19937_64 rng{std::random_device{}()};
    static std::uniform_int_distribution<uint64_t> dist;

    return std::to_string(
        SysClock::now().time_since_epoch().count()
    ) + "-" + std::to_string(dist(rng));
}


std::string MailQueue::enqueue(
    const std::string& from,
    const std::string& to,
    const std::string& raw
) {
    // CRITICAL FIX: Check queue depth to prevent disk exhaustion
    static constexpr int MAX_QUEUE_DEPTH = 100000; // Configurable limit
    int currentDepth = 0;
    try {
        for (auto& f : fs::directory_iterator("queue/active")) {
            if (f.path().extension() == ".msg") {
                currentDepth++;
            }
        }
        if (currentDepth >= MAX_QUEUE_DEPTH) {
            Logger::instance().log(LogLevel::Error,
                "Queue: Maximum queue depth reached (" + std::to_string(currentDepth) + ")");
            throw std::runtime_error("Queue depth limit exceeded");
        }
    } catch (const std::exception& ex) {
        Logger::instance().log(LogLevel::Error,
            "Queue: Error checking queue depth: " + std::string(ex.what()));
        throw;
    }

    std::string id = genId();
    fs::path p = "queue/active/" + id + ".msg";

    // CRITICAL FIX: Build message content and use atomic write with fsync
    std::string content;
    content.reserve(raw.size() + 256); // Pre-allocate
    content += "FROM: " + from + "\n";
    content += "TO: " + to + "\n";
    content += "---RAW---\n";
    content += raw;

    if (!atomicWriteFile(p.string(), content)) {
        Logger::instance().log(LogLevel::Error,
            "Queue: Atomic write failed for message " + id);
        throw std::runtime_error("Failed to durably enqueue message");
    }

    Logger::instance().log(
        LogLevel::Info,
        "Queue: Enqueued " + id + " (depth: " + std::to_string(currentDepth + 1) + ")"
    );
    
    // Update queue depth metric
    Metrics::instance().set("mail_queue_depth", currentDepth + 1);
    
    return id;
}

static bool isLeaseExpired(const fs::path& p) {
    auto last = fs::last_write_time(p);
    auto now = fs::file_time_type::clock::now();
    return (now - last) > std::chrono::seconds(LEASE_TIMEOUT_SEC);
}
std::vector<QueueMessage> MailQueue::list() {
    std::vector<QueueMessage> out;

    // Active messages
    for (auto& f : fs::directory_iterator("queue/active")) {
        if (f.path().extension() == ".msg") {
            QueueMessage m;
            m.id = f.path().stem().string();
            out.push_back(m);
        }
    }

    // In-flight messages
    for (auto& f : fs::directory_iterator("queue/inflight")) {
        if (f.path().extension() == ".msg") {
            QueueMessage m;
            m.id = f.path().stem().string();
            out.push_back(m);
        }
    }

    // Failed (retryable)
    for (auto& f : fs::directory_iterator("queue/failure")) {
        if (f.path().extension() == ".msg") {
            QueueMessage m;
            m.id = f.path().stem().string();
            out.push_back(m);
        }
    }

    return out;
}

std::optional<QueueMessage> MailQueue::fetchReady() {

    /* Recover expired leases */
    for (auto& f : fs::directory_iterator("queue/inflight")) {
        if (f.path().extension() == ".msg" &&
            isLeaseExpired(f.path())) {

            fs::path dst =
                "queue/active/" + f.path().filename().string();

            try {
                fs::rename(f.path(), dst);
                Logger::instance().log(
                    LogLevel::Warn,
                    "Queue: Reclaimed expired lease " +
                    dst.stem().string()
                );
            } catch (...) {}
        }
    }

    /* Lease a ready message */
    for (auto& f : fs::directory_iterator("queue/active")) {
        if (f.path().extension() != ".msg")
            continue;

        fs::path inflight =
            "queue/inflight/" + f.path().filename().string();

        // CRITICAL FIX: Better error handling for atomic lease operation
        try {
            // Check if destination already exists (race condition protection)
            if (fs::exists(inflight)) {
                continue; // Already leased by another process
            }
            
            // Atomic rename operation (filesystem-level atomicity)
            fs::rename(f.path(), inflight);
            
            // Verify rename succeeded (defensive check)
            if (!fs::exists(inflight)) {
                Logger::instance().log(LogLevel::Error,
                    "Queue: Atomic lease failed for " + f.path().string());
                continue;
            }
        } catch (const fs::filesystem_error& ex) {
            // Filesystem errors (permissions, disk full, etc.)
            Logger::instance().log(LogLevel::Error,
                "Queue: Filesystem error during lease: " + std::string(ex.what()));
            continue;
        } catch (const std::exception& ex) {
            Logger::instance().log(LogLevel::Error,
                "Queue: Error during lease: " + std::string(ex.what()));
            continue;
        } catch (...) {
            // Someone else got it or other error
            continue;
        }

        // CRITICAL FIX: Better error handling for file read
        std::ifstream in(inflight, std::ios::binary);
        if (!in.is_open()) {
            Logger::instance().log(LogLevel::Error,
                "Queue: Failed to open leased message: " + inflight.string());
            // Try to recover: move back to active
            try {
                fs::rename(inflight, f.path());
            } catch (...) {}
            continue;
        }
        
        std::string raw(
            (std::istreambuf_iterator<char>(in)),
            std::istreambuf_iterator<char>()
        );
        in.close();
        
        if (raw.empty()) {
            Logger::instance().log(LogLevel::Warn,
                "Queue: Leased message is empty: " + inflight.string());
            // Move to permanent failure
            try {
                fs::rename(inflight, "queue/permanent_fail/" + inflight.filename().string());
            } catch (...) {}
            continue;
        }

        // Parse FROM and TO from message headers
        QueueMessage m;
        m.id = inflight.stem().string();
        m.rawData = raw;
        
        // Extract FROM and TO from raw message (simplified parsing)
        size_t fromPos = raw.find("FROM: ");
        if (fromPos != std::string::npos) {
            size_t fromEnd = raw.find('\n', fromPos);
            if (fromEnd != std::string::npos) {
                m.from = raw.substr(fromPos + 6, fromEnd - fromPos - 6);
                // Trim whitespace
                while (!m.from.empty() && (m.from.back() == ' ' || m.from.back() == '\r')) {
                    m.from.pop_back();
                }
            }
        }
        
        size_t toPos = raw.find("TO: ");
        if (toPos != std::string::npos) {
            size_t toEnd = raw.find('\n', toPos);
            if (toEnd != std::string::npos) {
                m.to = raw.substr(toPos + 4, toEnd - toPos - 4);
                while (!m.to.empty() && (m.to.back() == ' ' || m.to.back() == '\r')) {
                    m.to.pop_back();
                }
            }
        }

        Logger::instance().log(
            LogLevel::Debug,
            "Queue: Leased " + m.id
        );
        return m;
    }

    return std::nullopt;
}

void MailQueue::markSuccess(const std::string& id) {
    fs::path p = "queue/inflight/" + id + ".msg";
    if (fs::exists(p)) {
        fs::remove(p);
        Logger::instance().log(
            LogLevel::Info,
            "Queue: Delivered " + id
        );
    }
}

void MailQueue::markTempFail(
    const QueueMessage& msg,
    const std::string& reason
) {
    fs::path src = "queue/inflight/" + msg.id + ".msg";
    fs::path dst = "queue/failure/" + msg.id + ".msg";
    try {
        fs::rename(src, dst);
        Logger::instance().log(
            LogLevel::Warn,
            "Queue: TempFail " + msg.id + " → " + reason
        );
    } catch (...) {}
}

void MailQueue::markPermFail(
    const QueueMessage& msg,
    const std::string& reason
) {
    fs::path src = "queue/inflight/" + msg.id + ".msg";
    fs::path dst = "queue/permanent_fail/" + msg.id + ".msg";
    try {
        fs::rename(src, dst);
        Logger::instance().log(
            LogLevel::Error,
            "Queue: PermFail " + msg.id + " → " + reason
        );
    } catch (...) {}
}
