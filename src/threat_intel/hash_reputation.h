#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include "threat_intel/threat_types.h"

struct HashReputation {
    ThreatVerdict verdict;
    std::string source;
};

class HashReputationStore {
public:
    static HashReputationStore& instance();

    void update(const std::string& sha256,
                ThreatVerdict verdict,
                const std::string& source);

    bool lookup(const std::string& sha256,
                HashReputation& out) const;

private:
    HashReputationStore() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, HashReputation> map_;
};
