#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include "cloud_scan_result.h"

class VirusVerdictStore {
public:
    static VirusVerdictStore& instance();

    void store(const std::string& sha256, const CloudScanResult& r);
    bool get(const std::string& sha256, CloudScanResult& out);

private:
    std::mutex mtx_;
    std::unordered_map<std::string, CloudScanResult> store_;
};
