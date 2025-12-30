#pragma once

#include <atomic>
#include <string>
#include <mutex>

/**
 * Readiness Signaling
 * 
 * WHY REQUIRED:
 * - Distinguish STARTING/READY/DEGRADED/STOPPING states
 * - Kubernetes/container orchestration needs readiness probes
 * - Health checks should reflect actual service state
 * - Prevents routing traffic to non-ready instances
 */
enum class ReadinessState {
    STARTING,   // Initialization in progress
    READY,      // Fully operational
    DEGRADED,   // Operational but with issues
    STOPPING    // Shutdown in progress
};

class ReadinessStateMachine {
public:
    static ReadinessStateMachine& instance();

    // State transitions
    void setState(ReadinessState state);
    void setState(ReadinessState state, const std::string& reason);

    // State queries
    ReadinessState getState() const { return state_.load(); }
    std::string getStateString() const;
    std::string getStateString(ReadinessState state) const;  // Overloaded version
    std::string getReason() const;

    // Convenience checks
    bool isReady() const { return state_.load() == ReadinessState::READY; }
    bool isAcceptingTraffic() const {
        auto s = state_.load();
        return s == ReadinessState::READY || s == ReadinessState::DEGRADED;
    }

private:
    ReadinessStateMachine() = default;
    std::atomic<ReadinessState> state_{ReadinessState::STARTING};
    mutable std::mutex mutex_;
    std::string reason_;
};

