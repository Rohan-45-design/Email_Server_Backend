#include "sandbox_provider_anyrun.h"
#include "core/logger.h"

// Stubbed provider – real API integration later
SandboxResult AnyRunProvider::analyze(const std::string& sha256,
                                      const std::string&) {
    Logger::instance().log(
        LogLevel::Info,
        "Sandbox AnyRun analyzing " + sha256
    );

    // Simulated async verdict
    SandboxResult r;
    r.sha256 = sha256;
    r.verdict = SandboxVerdict::Unknown;
    r.reportUrl = "";

    return r;
}
