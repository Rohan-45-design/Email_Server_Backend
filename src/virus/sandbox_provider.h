#pragma once
#include <string>

enum class SandboxVerdict {
    Unknown,
    Clean,
    Suspicious,
    Malicious
};

struct SandboxResult {
    std::string sha256;
    SandboxVerdict verdict = SandboxVerdict::Unknown;
    std::string reportUrl;
};

class SandboxProvider {
public:
    virtual ~SandboxProvider() = default;

    virtual SandboxResult analyze(const std::string& sha256,
                                  const std::string& rawData) = 0;
};
