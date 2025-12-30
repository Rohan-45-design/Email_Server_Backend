#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <chrono>

/**
 * Connection Limits and Backpressure
 * 
 * WHY REQUIRED:
 * - Global max sessions (prevent resource exhaustion)
 * - Per-IP limits (prevent single IP DoS)
 * - Backpressure when limits exceeded (graceful degradation)
 * - Coordinated limits across SMTP/IMAP
 */
class ConnectionManager {
public:
    static ConnectionManager& instance();

    // Configuration
    void setGlobalMaxConnections(int max);
    void setMaxConnectionsPerIP(int max);
    void setBackpressureDelay(std::chrono::milliseconds delay);

    // Connection tracking
    bool tryAcquireConnection(const std::string& ip);
    void releaseConnection(const std::string& ip);

    // Statistics
    int getActiveConnections() const { return activeConnections_.load(); }
    int getConnectionsForIP(const std::string& ip) const;

    // Backpressure: wait if at limit
    bool waitForCapacity(const std::string& ip, std::chrono::milliseconds timeout);

private:
    ConnectionManager() = default;
    std::atomic<int> activeConnections_{0};
    std::atomic<int> globalMax_{1000};
    std::atomic<int> perIPMax_{10};
    std::atomic<int> backpressureDelayMs_{100};

    struct IPConnection {
        int count = 0;
        std::chrono::steady_clock::time_point lastAccess;
    };
    mutable std::mutex mutex_;
    std::unordered_map<std::string, IPConnection> ipConnections_;
};

