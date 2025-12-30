#include "app_lifecycle.h"
#include "logger.h"
#include <algorithm>
#include <stdexcept>

AppLifecycle& AppLifecycle::instance() {
    static AppLifecycle inst;
    return inst;
}

void AppLifecycle::registerSubsystem(const Subsystem& subsystem) {
    subsystems_.push_back(subsystem);
    // Sort by phase order
    std::sort(subsystems_.begin(), subsystems_.end(),
        [](const Subsystem& a, const Subsystem& b) {
            return static_cast<int>(a.phase) < static_cast<int>(b.phase);
        });
}

bool AppLifecycle::initialize() {
    if (initialized_) {
        Logger::instance().log(LogLevel::Warn, "AppLifecycle: Already initialized");
        return true;
    }

    Logger::instance().log(LogLevel::Info, "AppLifecycle: Starting initialization");

    Phase currentPhase = Phase::Config;
    for (auto& subsystem : subsystems_) {
        // Log phase transitions
        if (subsystem.phase != currentPhase) {
            Logger::instance().log(LogLevel::Info, 
                "AppLifecycle: Entering phase " + std::to_string(static_cast<int>(subsystem.phase)));
            currentPhase = subsystem.phase;
        }

        Logger::instance().log(LogLevel::Info, 
            "AppLifecycle: Initializing " + subsystem.name);

        try {
            if (!subsystem.init()) {
                Logger::instance().log(LogLevel::Error,
                    "AppLifecycle: FAIL-FAST: " + subsystem.name + " initialization failed");
                // Shutdown already initialized subsystems in reverse order
                shutdown();
                return false;
            }
        } catch (const std::exception& ex) {
            Logger::instance().log(LogLevel::Error,
                "AppLifecycle: FAIL-FAST: Exception in " + subsystem.name + ": " + ex.what());
            shutdown();
            return false;
        } catch (...) {
            Logger::instance().log(LogLevel::Error,
                "AppLifecycle: FAIL-FAST: Unknown exception in " + subsystem.name);
            shutdown();
            return false;
        }
    }

    initialized_ = true;
    Logger::instance().log(LogLevel::Info, "AppLifecycle: Initialization complete");
    return true;
}

void AppLifecycle::shutdown() {
    if (shuttingDown_) {
        return;
    }
    shuttingDown_ = true;

    Logger::instance().log(LogLevel::Info, "AppLifecycle: Starting shutdown");

    // Shutdown in reverse order (highest shutdownOrder first)
    std::vector<Subsystem> sorted = subsystems_;
    std::sort(sorted.begin(), sorted.end(),
        [](const Subsystem& a, const Subsystem& b) {
            return a.shutdownOrder > b.shutdownOrder;
        });

    for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
        Logger::instance().log(LogLevel::Info, 
            "AppLifecycle: Shutting down " + it->name);
        try {
            if (it->shutdown) {
                it->shutdown();
            }
        } catch (const std::exception& ex) {
            Logger::instance().log(LogLevel::Error,
                "AppLifecycle: Error shutting down " + it->name + ": " + ex.what());
        } catch (...) {
            Logger::instance().log(LogLevel::Error,
                "AppLifecycle: Unknown error shutting down " + it->name);
        }
    }

    subsystems_.clear();
    initialized_ = false;
    Logger::instance().log(LogLevel::Info, "AppLifecycle: Shutdown complete");
}

