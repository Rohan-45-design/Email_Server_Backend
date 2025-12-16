#include "monitoring/metrics.h"
#include <sstream>

Metrics& Metrics::instance() {
    static Metrics m;
    return m;
}

void Metrics::inc(const std::string& name, int value) {
    counters_[name] += value;
}

void Metrics::set(const std::string& name, int value) {
    counters_[name] = value;
}

std::string Metrics::renderPrometheus() const {
    std::ostringstream out;
    for (const auto& [name, val] : counters_) {
        out << name << " " << val.load() << "\n";
    }
    return out.str();
}
