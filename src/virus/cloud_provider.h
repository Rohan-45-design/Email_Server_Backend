#pragma once
#include <string>
#include "cloud_scan_result.h"

class CloudProvider {
public:
    virtual ~CloudProvider() = default;

    virtual CloudScanResult scan(
        const std::string& sha256,
        const std::string& rawMessage
    ) = 0;

    virtual std::string name() const = 0;
};
