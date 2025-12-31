#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <filesystem>

struct StoredMessage {
    std::string id;                 // unique id (optional; generated if empty)
    std::string from;
    std::vector<std::string> recipients;
    std::string rawData;            // full body received after DATA
    std::string mailboxUser;        // which user's mailbox this goes to
};

class MailStore {
public:
    explicit MailStore(const std::string& rootDir);

    // Store message on disk; returns message id or empty string on failure
    std::string store(const StoredMessage& msg);
    bool moveToQuarantine(const std::string& user,
                      const std::string& id);

    bool deleteMessage(const std::string& user,
                   const std::string& id);

private:
    std::string rootDir_;           // base: e.g. "data/mail"
    mutable std::mutex mutex_;

    std::string generateId() const;

    // Per-user paths (data/mail/<user>/INBOX and message path)
    std::string makeUserInboxDir(const std::string& user) const;
    std::string makeMessagePath(const std::string& user,
                                const std::string& id) const;

    bool ensureDirExists(const std::string& dir) const;

    // CRITICAL FIX: Atomic write with fsync for crash safety
    bool atomicWriteFile(const std::string& path, const std::string& content);

    // CRITICAL FIX: Recovery of orphaned temp files
    void recoverOrphanedTempFiles();

};
