#include "threat_intel/hash_reputation.h"

HashReputationStore& HashReputationStore::instance() {
    static HashReputationStore s;
    return s;
}

void HashReputationStore::update(
    const std::string& sha256,
    ThreatVerdict verdict,
    const std::string& source
) {
    std::lock_guard<std::mutex> lock(mutex_);
    map_[sha256] = { verdict, source };
}

bool HashReputationStore::lookup(
    const std::string& sha256,
    HashReputation& out
) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(sha256);
    if (it == map_.end()) return false;
    out = it->second;
    return true;
}
