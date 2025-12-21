#pragma once
#include <string>

enum class PolicyVerdict {
    Allow,
    Quarantine,
    Reject
};

struct PolicyResult {
    PolicyVerdict verdict = PolicyVerdict::Allow;
    std::string reason;
};
