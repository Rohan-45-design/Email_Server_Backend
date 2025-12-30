#include "shutdown_coordinator.h"
#include "logger.h"
#include <algorithm>

ShutdownCoordinator& ShutdownCoordinator::instance() {
    static ShutdownCoordinator inst;
    return inst;
}

void ShutdownCoordinator::registerHook(const ShutdownHook& hook) {
    std::lock_guard<std::mutex> lock(mutex_);
    hooks_.push_back(hook);
    // Sort by priority (lower first)
    std::sort(hooks_.begin(), hooks_.end(),
        [](const ShutdownHook& a, const ShutdownHook& b) {
            return a.priority < b.priority;
        });
}

void ShutdownCoordinator::initiateShutdown() {
    if (shuttingDown_.exchange(true)) {
        return; // Already shutting down
    }

    Logger::instance().log(LogLevel::Info, 
        "ShutdownCoordinator: Initiating graceful shutdown");

    std::lock_guard<std::mutex> lock(mutex_);

    // Phase 1: Stop accepting new connections
    Logger::instance().log(LogLevel::Info, 
        "ShutdownCoordinator: Phase 1 - Stopping new connections");
    for (auto& hook : hooks_) {
        try {
            if (hook.stopAccepting) {
                Logger::instance().log(LogLevel::Info,
                    "ShutdownCoordinator: Stopping " + hook.name);
                hook.stopAccepting();
            }
        } catch (const std::exception& ex) {
            Logger::instance().log(LogLevel::Error,
                "ShutdownCoordinator: Error stopping " + hook.name + ": " + ex.what());
        }
    }

    // Phase 2: Drain active sessions (with timeout per component)
    Logger::instance().log(LogLevel::Info,
        "ShutdownCoordinator: Phase 2 - Draining active sessions");
    const auto drainTimeout = std::chrono::seconds(10);
    auto drainStart = std::chrono::steady_clock::now();

    for (auto& hook : hooks_) {
        try {
            if (hook.drain) {
                Logger::instance().log(LogLevel::Info,
                    "ShutdownCoordinator: Draining " + hook.name);
                auto remaining = drainTimeout - 
                    (std::chrono::steady_clock::now() - drainStart);
                if (remaining.count() > 0) {
                    hook.drain();
                } else {
                    Logger::instance().log(LogLevel::Warn,
                        "ShutdownCoordinator: Drain timeout for " + hook.name);
                }
            }
        } catch (const std::exception& ex) {
            Logger::instance().log(LogLevel::Error,
                "ShutdownCoordinator: Error draining " + hook.name + ": " + ex.what());
        }
    }

    // Phase 3: Final shutdown (close sockets, stop threads)
    Logger::instance().log(LogLevel::Info,
        "ShutdownCoordinator: Phase 3 - Final shutdown");
    for (auto it = hooks_.rbegin(); it != hooks_.rend(); ++it) {
        try {
            if (it->shutdown) {
                Logger::instance().log(LogLevel::Info,
                    "ShutdownCoordinator: Shutting down " + it->name);
                it->shutdown();
            }
        } catch (const std::exception& ex) {
            Logger::instance().log(LogLevel::Error,
                "ShutdownCoordinator: Error in final shutdown of " + it->name + ": " + ex.what());
        }
    }

    shutdownComplete_ = true;
    cv_.notify_all();
    Logger::instance().log(LogLevel::Info, "ShutdownCoordinator: Shutdown complete");
}

bool ShutdownCoordinator::waitForShutdown(std::chrono::seconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    return cv_.wait_for(lock, timeout, [this] { return shutdownComplete_; });
}

