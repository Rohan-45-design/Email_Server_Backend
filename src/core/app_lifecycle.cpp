#include "app_lifecycle.h"
#include "logger.h"

#include <algorithm>
#include <stdexcept>

AppLifecycle& AppLifecycle::instance() {
    static AppLifecycle inst;
    return inst;
}

void AppLifecycle::registerSubsystem(const Subsystem& subsystem) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != State::Stopped) {
        throw std::logic_error(
            "AppLifecycle: Cannot register subsystem after initialization started");
    }

    subsystems_.push_back(subsystem);

    // Ensure deterministic startup order by phase
    std::sort(subsystems_.begin(), subsystems_.end(),
        [](const Subsystem& a, const Subsystem& b) {
            return static_cast<int>(a.phase) <
                   static_cast<int>(b.phase);
        });
}

bool AppLifecycle::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != State::Stopped) {
        Logger::instance().log(LogLevel::Warn,
            "AppLifecycle: initialize() called more than once");
        return false;
    }

    state_ = State::Starting;
    Logger::instance().log(LogLevel::Info,
        "AppLifecycle: Starting initialization");

    for (auto& subsystem : subsystems_) {
        Logger::instance().log(LogLevel::Info,
            "Initializing subsystem: " + subsystem.name);

        try {
            if (!subsystem.init()) {
                Logger::instance().log(LogLevel::Error,
                    "Initialization failed: " + subsystem.name);
                shutdown();
                return false;
            }

            // Track only successfully initialized subsystems
            initialized_.push_back(&subsystem);

        } catch (const std::exception& e) {
            Logger::instance().log(LogLevel::Error,
                "Exception during init of " + subsystem.name + ": " + e.what());
            shutdown();
            return false;
        } catch (...) {
            Logger::instance().log(LogLevel::Error,
                "Unknown exception during init of " + subsystem.name);
            shutdown();
            return false;
        }
    }

    state_ = State::Ready;
    Logger::instance().log(LogLevel::Info,
        "AppLifecycle: Initialization complete");

    return true;
}

void AppLifecycle::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ == State::Stopping || state_ == State::Stopped) {
        return;
    }

    state_ = State::Stopping;
    Logger::instance().log(LogLevel::Info,
        "AppLifecycle: Starting shutdown");

    // Shutdown only initialized subsystems, in reverse dependency order
    std::sort(initialized_.begin(), initialized_.end(),
        [](const Subsystem* a, const Subsystem* b) {
            return a->shutdownOrder > b->shutdownOrder;
        });

    for (const Subsystem* subsystem : initialized_) {
        if (!subsystem->shutdown) {
            continue;
        }

        Logger::instance().log(LogLevel::Info,
            "Shutting down subsystem: " + subsystem->name);

        try {
            subsystem->shutdown();
        } catch (const std::exception& e) {
            Logger::instance().log(LogLevel::Error,
                "Shutdown error in " + subsystem->name + ": " + e.what());
        } catch (...) {
            Logger::instance().log(LogLevel::Error,
                "Unknown shutdown error in " + subsystem->name);
        }
    }

    initialized_.clear();
    subsystems_.clear();

    state_ = State::Stopped;
    Logger::instance().log(LogLevel::Info,
        "AppLifecycle: Shutdown complete");
}

AppLifecycle::State AppLifecycle::state() const {
    return state_.load();
}
