#include "threat_intel/sender_reputation.h"

SenderReputationStore& SenderReputationStore::instance() {
    static SenderReputationStore s;
    return s;
}

void SenderReputationStore::recordMalicious(const std::string& domain) {
    std::lock_guard<std::mutex> lock(mutex_);
    senders_[domain].maliciousCount++;
}

void SenderReputationStore::recordClean(const std::string& domain) {
    std::lock_guard<std::mutex> lock(mutex_);
    senders_[domain].cleanCount++;
}

int SenderReputationStore::score(const std::string& domain) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = senders_.find(domain);
    if (it == senders_.end()) return 0;
    return it->second.maliciousCount - it->second.cleanCount;
}
