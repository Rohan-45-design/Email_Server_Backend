#include "monitoring/health.h"
#include "monitoring/metrics.h"
#include "core/server_context.h"
#include "core/logger.h"
#include "core/readiness_state.h"
#include <filesystem>
#include <sstream>
#include <fstream>
#include <iomanip>

namespace fs = std::filesystem;

HealthStatus Health::check() {
    HealthStatus s;
    
    // Check readiness state first
    auto readiness = ReadinessStateMachine::instance().getState();
    if (readiness == ReadinessState::STOPPING) {
        s.ok = false;
        s.message = "STOPPING";
        return s;
    }
    if (readiness == ReadinessState::STARTING) {
        s.ok = false;
        s.message = "STARTING";
        return s;
    }
    if (readiness == ReadinessState::DEGRADED) {
        s.ok = false;
        s.message = "DEGRADED: " + ReadinessStateMachine::instance().getReason();
        return s;
    }
    
    s.ok = true;
    std::ostringstream issues;
    
    // Check disk space (critical for mail server)
    try {
        auto space = fs::space("data");
        double free_gb = static_cast<double>(space.free) / (1024 * 1024 * 1024);
        if (free_gb < 1.0) { // Less than 1GB free
            s.ok = false;
            issues << "Low disk space: " << std::fixed << std::setprecision(2) 
                   << free_gb << "GB free; ";
        }
    } catch (...) {
        issues << "Cannot check disk space; ";
    }
    
    // Check if data directory is writable
    try {
        if (!fs::exists("data")) {
            fs::create_directories("data");
        }
        // Try to create a test file
        std::ofstream test("data/.healthcheck");
        if (!test.is_open()) {
            s.ok = false;
            issues << "Data directory not writable; ";
        } else {
            test.close();
            fs::remove("data/.healthcheck");
        }
    } catch (...) {
        s.ok = false;
        issues << "Data directory access error; ";
    }
    
    // Check if logger is working
    try {
        Logger::instance().log(LogLevel::Debug, "Health check ping");
    } catch (...) {
        s.ok = false;
        issues << "Logger not functioning; ";
    }
    
    if (s.ok) {
        s.message = "OK";
    } else {
        s.message = issues.str();
        // Remove trailing semicolon and space
        if (!s.message.empty() && s.message.back() == ' ') {
            s.message.pop_back();
            if (!s.message.empty() && s.message.back() == ';') {
                s.message.pop_back();
            }
        }
    }
    
    return s;
}
