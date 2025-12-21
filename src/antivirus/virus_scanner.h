#pragma once
#include <string>

struct VirusScanResult {
    bool clean = false;
    bool infected = false;
    bool unavailable = false;
    std::string virusName;
};

class VirusScanner {
public:
    static VirusScanResult scan(const std::string& rawMessage);
};
