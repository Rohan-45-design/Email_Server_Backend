#pragma once
#include <string>
#include <unordered_set>
#include <mutex>

class IOCStore {
public:
    static IOCStore& instance();

    void addHash(const std::string& sha256);
    bool hasHash(const std::string& sha256) const;

private:
    IOCStore() = default;

    mutable std::mutex mutex_;
    std::unordered_set<std::string> hashes_;
};
