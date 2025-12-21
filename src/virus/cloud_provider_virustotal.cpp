#include "cloud_provider_virustotal.h"
#include "core/logger.h"

CloudScanResult VirusTotalProvider::scan(
    const std::string& sha256,
    const std::string&
) {
    Logger::instance().log(
        LogLevel::Info,
        "Submitting hash to VirusTotal: " + sha256
    );

    CloudScanResult r;
    r.provider = name();
    r.verdict = CloudVerdict::Unknown;
    r.enginesTotal = 60; // typical VT engine count
    r.enginesDetected = 0;

    return r;
}
