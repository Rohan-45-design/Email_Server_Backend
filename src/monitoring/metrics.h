#pragma once
#include <atomic>
#include <string>
#include <map>

class Metrics {
public:
    static Metrics& instance();

    void inc(const std::string& name, int value = 1);
    void set(const std::string& name, int value);

    std::string renderPrometheus() const;

private:
    Metrics() = default;
    mutable std::map<std::string, std::atomic<int>> counters_;
};
