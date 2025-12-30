#include "monitoring/metrics.h"
#include <sstream>

Metrics& Metrics::instance() {
    static Metrics m;
    return m;
}

void Metrics::inc(const std::string& name, int value) {
    std::lock_guard<std::mutex> lk(mutex_);
    counters[name] += value;
}

void Metrics::set(const std::string& name, int value) {
    std::lock_guard<std::mutex> lk(mutex_);
    counters[name] = value;
}

std::string Metrics::renderPrometheus() const {
    std::ostringstream out;
    std::lock_guard<std::mutex> lk(mutex_);
    for (const auto& p : counters) {
        out << p.first << " " << p.second << "\n";
    }
    return out.str();
}
