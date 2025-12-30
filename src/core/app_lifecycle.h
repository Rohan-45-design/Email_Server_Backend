#pragma once

#include <functional>
#include <vector>
#include <memory>
#include <string>
#include <atomic>

/**
 * Application Lifecycle Manager
 * 
 * WHY REQUIRED:
 * - Enforces deterministic startup order (critical subsystems first)
 * - Fail-fast behavior: abort on init errors instead of partial startup
 * - Clear ownership: each subsystem registers its init/shutdown
 * - Prevents race conditions during startup/shutdown
 */
class AppLifecycle {
public:
    enum class Phase {
        Config,      // Load and validate configuration
        Logging,     // Initialize logging
        TLS,         // Initialize TLS context
        Storage,     // Initialize data stores
        Services,    // Start background services (virus scanning, etc.)
        Servers      // Start network servers (SMTP/IMAP/Admin)
    };

    struct Subsystem {
        std::string name;
        Phase phase;
        std::function<bool()> init;      // Returns false on failure
        std::function<void()> shutdown;  // Called in reverse order
        int shutdownOrder = 0;            // Lower = shutdown first
    };

    static AppLifecycle& instance();

    // Register a subsystem with its lifecycle hooks
    void registerSubsystem(const Subsystem& subsystem);

    // Initialize all subsystems in phase order (fail-fast)
    bool initialize();

    // Shutdown all subsystems in reverse order
    void shutdown();

    // Check if initialization completed successfully
    bool isInitialized() const { return initialized_; }

private:
    AppLifecycle() = default;
    std::vector<Subsystem> subsystems_;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shuttingDown_{false};
};

