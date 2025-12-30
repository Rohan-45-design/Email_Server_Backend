#pragma once
#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>

class RateLimiter {
public:
    static RateLimiter& instance();
    void releaseConnection(const std::string& ip);
    // Connection-level
    bool allowConnection(const std::string& ip);

    // Per-session command throttling
    bool allowCommand(void* sessionKey);

    // AUTH brute-force protection
    void recordAuthFailure(const std::string& ip);
    bool allowAuth(const std::string& ip);

private:
    RateLimiter() = default;

    using clock = std::chrono::steady_clock;

    struct Counter {
        int count = 0;
        clock::time_point window;
    };

    std::mutex mutex_;

    std::unordered_map<std::string, Counter> conn_;
    std::unordered_map<void*, Counter> cmd_;
    std::unordered_map<std::string, Counter> auth_;
};
