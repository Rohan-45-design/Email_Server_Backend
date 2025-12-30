#include "readiness_state.h"
#include "logger.h"

ReadinessStateMachine& ReadinessStateMachine::instance() {
    static ReadinessStateMachine inst;
    return inst;
}

void ReadinessStateMachine::setState(ReadinessState state) {
    setState(state, "");
}

void ReadinessStateMachine::setState(ReadinessState state, const std::string& reason) {
    ReadinessState oldState = state_.exchange(state);
    
    if (oldState != state) {
        std::lock_guard<std::mutex> lock(mutex_);
        reason_ = reason;
        
        Logger::instance().log(LogLevel::Info,
            "ReadinessState: " + getStateString(oldState) + " -> " + 
            getStateString(state) + 
            (reason.empty() ? "" : " (" + reason + ")"));
    }
}

std::string ReadinessStateMachine::getStateString() const {
    return getStateString(state_.load());
}

std::string ReadinessStateMachine::getStateString(ReadinessState state) const {
    switch (state) {
        case ReadinessState::STARTING: return "STARTING";
        case ReadinessState::READY: return "READY";
        case ReadinessState::DEGRADED: return "DEGRADED";
        case ReadinessState::STOPPING: return "STOPPING";
        default: return "UNKNOWN";
    }
}

std::string ReadinessStateMachine::getReason() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return reason_;
}

