#pragma once
#include <atomic>
#include <string>
#include <unordered_map>
#include <mutex>

class Metrics {
public:
    static Metrics& instance();

    void inc(const std::string& name, int value = 1);
    void set(const std::string& name, int value);

    std::string renderPrometheus() const;

private:
    Metrics() = default;
    mutable std::mutex mutex_;
    // use a simple int64 map guarded by mutex for thread-safety
    mutable std::unordered_map<std::string, int64_t> counters;
};
