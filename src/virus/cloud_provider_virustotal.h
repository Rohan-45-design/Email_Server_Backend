#pragma once
#include "cloud_provider.h"

class VirusTotalProvider : public CloudProvider {
public:
    CloudScanResult scan(
        const std::string& sha256,
        const std::string& rawMessage
    ) override;

    std::string name() const override {
        return "VirusTotal";
    }
};
