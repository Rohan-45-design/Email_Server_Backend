#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

class FlagsIndex {
public:
    explicit FlagsIndex(const std::string& rootDir);

    // Load flags for a user's mailbox (INBOX) into memory
    void loadMailbox(const std::string& user);

    // Get flags for a given message id
    std::unordered_set<std::string> getFlags(const std::string& user,
                                             const std::string& msgId) const;

    // Add a flag (e.g. \Seen)
    void addFlag(const std::string& user,
                 const std::string& msgId,
                 const std::string& flag);

    // Persist current flags to disk
    void save(const std::string& user);

private:
    std::string rootDir_;
    mutable std::mutex mutex_;

    // user -> msgId -> set(flags)
    std::unordered_map<std::string,
        std::unordered_map<std::string, std::unordered_set<std::string>>> data_;

    std::string mailboxFlagsPath(const std::string& user) const;
};
