#pragma once
#include "sandbox_provider.h"

class AnyRunProvider : public SandboxProvider {
public:
    SandboxResult analyze(const std::string& sha256,
                          const std::string& rawData) override;
};
