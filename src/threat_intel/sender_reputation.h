#pragma once
#include <string>
#include <unordered_map>
#include <mutex>

struct SenderReputation {
    int maliciousCount = 0;
    int cleanCount = 0;
};

class SenderReputationStore {
public:
    static SenderReputationStore& instance();

    void recordMalicious(const std::string& domain);
    void recordClean(const std::string& domain);

    int score(const std::string& domain) const;

private:
    SenderReputationStore() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, SenderReputation> senders_;
};
