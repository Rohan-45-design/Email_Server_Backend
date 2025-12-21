#include "sandbox_verdict_store.h"

SandboxVerdictStore& SandboxVerdictStore::instance() {
    static SandboxVerdictStore s;
    return s;
}

void SandboxVerdictStore::store(const SandboxResult& r) {
    std::lock_guard<std::mutex> lock(mtx_);
    verdicts_[r.sha256] = r;
}

SandboxResult SandboxVerdictStore::get(const std::string& sha256) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = verdicts_.find(sha256);
    if (it != verdicts_.end())
        return it->second;
    return {};
}
