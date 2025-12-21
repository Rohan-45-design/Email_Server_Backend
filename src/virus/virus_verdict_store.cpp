#include "virus_verdict_store.h"

VirusVerdictStore& VirusVerdictStore::instance() {
    static VirusVerdictStore v;
    return v;
}

void VirusVerdictStore::store(
    const std::string& sha256,
    const CloudScanResult& r
) {
    std::lock_guard<std::mutex> lock(mtx_);
    store_[sha256] = r;
}

bool VirusVerdictStore::get(
    const std::string& sha256,
    CloudScanResult& out
) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = store_.find(sha256);
    if (it == store_.end()) return false;
    out = it->second;
    return true;
}
