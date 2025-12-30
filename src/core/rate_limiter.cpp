#include "core/rate_limiter.h"

static constexpr int CONN_LIMIT = 30;     // per minute per IP
static constexpr int CMD_LIMIT  = 120;    // per minute per session
static constexpr int AUTH_LIMIT = 5;      // failures per 10 minutes

RateLimiter& RateLimiter::instance() {
    static RateLimiter r;
    return r;
}

bool RateLimiter::allowConnection(const std::string& ip) {
    auto now = clock::now();
    std::lock_guard lock(mutex_);

    auto& c = conn_[ip];
    if (now - c.window > std::chrono::minutes(1)) {
        c.window = now;
        c.count = 0;
    }
    return ++c.count <= CONN_LIMIT;
}

bool RateLimiter::allowCommand(void* key) {
    auto now = clock::now();
    std::lock_guard lock(mutex_);

    auto& c = cmd_[key];
    if (now - c.window > std::chrono::minutes(1)) {
        c.window = now;
        c.count = 0;
    }
    return ++c.count <= CMD_LIMIT;
}

void RateLimiter::recordAuthFailure(const std::string& ip) {
    auto now = clock::now();
    std::lock_guard lock(mutex_);

    auto& c = auth_[ip];
    if (now - c.window > std::chrono::minutes(10)) {
        c.window = now;
        c.count = 0;
    }
    ++c.count;
}

bool RateLimiter::allowAuth(const std::string& ip) {
    auto now = clock::now();
    std::lock_guard lock(mutex_);

    auto it = auth_.find(ip);
    if (it == auth_.end()) return true;

    if (now - it->second.window > std::chrono::minutes(10))
        return true;

    return it->second.count < AUTH_LIMIT;
}
void RateLimiter::releaseConnection(const std::string& ip) {
    std::lock_guard lock(mutex_);
    
    
    auto it = conn_.find(ip);
    if (it != conn_.end() && it->second.count > 0) {
        it->second.count--;
    }
}
