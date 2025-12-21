#pragma once
#include <map>
#include <mutex>
#include "sandbox_provider.h"

class SandboxVerdictStore {
public:
    static SandboxVerdictStore& instance();

    void store(const SandboxResult& result);
    SandboxResult get(const std::string& sha256) const;

private:
    SandboxVerdictStore() = default;

    mutable std::mutex mtx_;
    std::map<std::string, SandboxResult> verdicts_;
};
