#pragma once

#include <atomic>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <chrono>

/**
 * Safe, Coordinated Graceful Shutdown
 * 
 * WHY REQUIRED:
 * - Stop accepting new connections before draining active ones
 * - Drain active sessions gracefully (no data loss)
 * - Close sockets safely (prevent TIME_WAIT issues)
 * - Stop threads in correct order (dependencies)
 * - Flush logs/metrics before exit
 * - Prevent race conditions during shutdown
 */
class ShutdownCoordinator {
public:
    static ShutdownCoordinator& instance();

    // Register a component that needs graceful shutdown
    struct ShutdownHook {
        std::string name;
        std::function<void()> stopAccepting;  // Stop accepting new connections
        std::function<void()> drain;          // Drain active sessions (with timeout)
        std::function<void()> shutdown;        // Final shutdown
        int priority = 0;                      // Lower = shutdown first
    };

    void registerHook(const ShutdownHook& hook);

    // Initiate graceful shutdown
    void initiateShutdown();

    // Check if shutdown is in progress
    bool isShuttingDown() const { return shuttingDown_; }

    // Wait for shutdown to complete (with timeout)
    bool waitForShutdown(std::chrono::seconds timeout = std::chrono::seconds(30));

private:
    ShutdownCoordinator() = default;
    std::vector<ShutdownHook> hooks_;
    std::atomic<bool> shuttingDown_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
    bool shutdownComplete_ = false;
};

