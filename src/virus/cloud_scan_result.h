#pragma once
#include <string>

enum class CloudVerdict {
    Clean,
    Suspicious,
    Malicious,
    Unknown,
    Error
};

struct CloudScanResult {
    CloudVerdict verdict = CloudVerdict::Unknown;
    int enginesDetected = 0;
    int enginesTotal = 0;
    std::string provider;
    std::string reportUrl;
};
