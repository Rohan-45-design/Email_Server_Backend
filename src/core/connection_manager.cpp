#include "connection_manager.h"
#include "logger.h"
#include <algorithm>
#include <thread>

ConnectionManager& ConnectionManager::instance() {
    static ConnectionManager inst;
    return inst;
}

void ConnectionManager::setGlobalMaxConnections(int max) {
    globalMax_.store(max);
    Logger::instance().log(LogLevel::Info,
        "ConnectionManager: Global max connections = " + std::to_string(max));
}

void ConnectionManager::setMaxConnectionsPerIP(int max) {
    perIPMax_.store(max);
    Logger::instance().log(LogLevel::Info,
        "ConnectionManager: Per-IP max connections = " + std::to_string(max));
}

void ConnectionManager::setBackpressureDelay(std::chrono::milliseconds delay) {
    backpressureDelayMs_.store(static_cast<int>(delay.count()));
}

bool ConnectionManager::checkResourceLimits() const {
    // Check thread count (approximate)
    // Note: This is a simple check - in production you'd use OS-specific APIs
    static int lastThreadCheck = 0;
    static int threadCount = 0;
    
    // Simple thread count estimation (not perfect but better than nothing)
    if (std::thread::hardware_concurrency() > 0) {
        int maxThreads = maxThreads_.load();
        if (maxThreads > 0 && activeConnections_.load() >= maxThreads / 2) {
            Logger::instance().log(LogLevel::Warn,
                "ConnectionManager: Approaching thread limit: " + 
                std::to_string(activeConnections_.load()) + "/" + std::to_string(maxThreads));
        }
    }
    
    // Check memory usage (simplified - in production use OS APIs)
    // For now, just check if we're getting close to connection limits as proxy
    int maxMemMB = maxMemoryMB_.load();
    if (maxMemMB > 0) {
        // Estimate: ~1MB per active connection (rough approximation)
        int estimatedMemMB = activeConnections_.load();
        if (estimatedMemMB >= maxMemMB * 0.8) { // 80% of limit
            Logger::instance().log(LogLevel::Warn,
                "ConnectionManager: Approaching memory limit: ~" + 
                std::to_string(estimatedMemMB) + "MB/" + std::to_string(maxMemMB) + "MB");
        }
    }
    
    return true; // For now, don't block - just warn
}

bool ConnectionManager::tryAcquireConnection(const std::string& ip) {
    // Check global limit
    int current = activeConnections_.load();
    if (current >= globalMax_.load()) {
        Logger::instance().log(LogLevel::Warn,
            "ConnectionManager: Global connection limit reached: " + 
            std::to_string(current));
        return false;
    }

    // Check per-IP limit
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& ipConn = ipConnections_[ip];
        
        // Cleanup old entries (older than 5 minutes)
        auto now = std::chrono::steady_clock::now();
        if (now - ipConn.lastAccess > std::chrono::minutes(5)) {
            ipConn.count = 0;
        }
        ipConn.lastAccess = now;

        if (ipConn.count >= perIPMax_.load()) {
            Logger::instance().log(LogLevel::Warn,
                "ConnectionManager: Per-IP limit reached for " + ip + 
                ": " + std::to_string(ipConn.count));
            return false;
        }

        ipConn.count++;
    }

    activeConnections_++;
    return true;
}

void ConnectionManager::releaseConnection(const std::string& ip) {
    activeConnections_--;

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = ipConnections_.find(ip);
    if (it != ipConnections_.end() && it->second.count > 0) {
        it->second.count--;
        if (it->second.count == 0) {
            // Optionally remove entry to save memory
        }
    }
}

int ConnectionManager::getConnectionsForIP(const std::string& ip) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = ipConnections_.find(ip);
    return (it != ipConnections_.end()) ? it->second.count : 0;
}

bool ConnectionManager::waitForCapacity(const std::string& ip, 
                                       std::chrono::milliseconds timeout) {
    auto start = std::chrono::steady_clock::now();
    int delayMs = backpressureDelayMs_.load();

    while (std::chrono::steady_clock::now() - start < timeout) {
        if (tryAcquireConnection(ip)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }

    return false;
}

