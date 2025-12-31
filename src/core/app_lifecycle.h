#pragma once

#include <functional>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>

class AppLifecycle {
public:
    enum class Phase {
        Config,
        Logging,
        TLS,
        Storage,
        Services,
        Servers
    };

    enum class State {
        Starting,
        Ready,
        Stopping,
        Stopped
    };

    struct Subsystem {
        std::string name;
        Phase phase;
        int shutdownOrder = 0;   // Higher = shutdown earlier

        std::function<bool()> init;     // return false on failure
        std::function<void()> shutdown; // must not throw
    };

    static AppLifecycle& instance();

    // Must be called before initialize()
    void registerSubsystem(const Subsystem& subsystem);

    // Fail-fast initialization
    bool initialize();

    // Safe to call from any thread
    void shutdown();

    State state() const;

private:
    AppLifecycle() = default;
    AppLifecycle(const AppLifecycle&) = delete;
    AppLifecycle& operator=(const AppLifecycle&) = delete;

private:
    mutable std::mutex mutex_;

    std::vector<Subsystem> subsystems_;
    std::vector<const Subsystem*> initialized_; // only those successfully started

    std::atomic<State> state_{State::Stopped};
};
