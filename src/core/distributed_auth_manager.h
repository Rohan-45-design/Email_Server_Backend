#pragma once

#include "i_auth_manager.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <unordered_set>

class DistributedAuthManager : public IAuthManager {
public:
    static DistributedAuthManager& instance();

    // Initialize with Redis connection details and cluster configuration
    bool initialize(const std::string& redisHost = "localhost",
                   int redisPort = 6379,
                   const std::string& password = "",
                   const std::string& clusterId = "email-server-cluster",
                   const std::string& nodeId = "");

    // Shutdown the distributed auth manager
    void shutdown();

    // Authentication operations (compatible with existing AuthManager interface)
    bool validate(const std::string& user, const std::string& pass) const;
    bool authenticateSmtp(const std::string& args, std::string& username);
    bool userExists(const std::string& user) const;
    size_t getUserCount() const;

    // Distributed operations
    bool addUser(const std::string& username, const std::string& hashedPassword);
    bool removeUser(const std::string& username);
    bool updateUserPassword(const std::string& username, const std::string& newHashedPassword);

    // Session management for HA
    std::string createSession(const std::string& username, const std::string& clientInfo = "");
    bool validateSession(const std::string& sessionId, std::string& username);
    void invalidateSession(const std::string& sessionId);
    std::vector<std::string> getActiveSessions(const std::string& username = "");

    // Replication and synchronization
    void syncFromCluster();
    void broadcastUserChange(const std::string& username, const std::string& operation);
    void broadcastSessionChange(const std::string& sessionId, const std::string& operation);

    // Health and cluster status
    bool isHealthy() const;
    std::string getClusterStatus() const;
    std::vector<std::string> getClusterNodes() const;
    std::string getNodeId() const { return nodeId_; }

    // Load users from file (for initial bootstrap)
    bool loadFromFile(const std::string& path);

private:
    DistributedAuthManager() = default;
    ~DistributedAuthManager();

    // Redis connection management
    bool connectToRedis();
    void disconnectFromRedis();
    bool pingRedis() const;

    // Data operations
    bool storeUserInRedis(const std::string& username, const std::string& hashedPassword);
    std::string getUserFromRedis(const std::string& username) const;
    bool deleteUserFromRedis(const std::string& username);

    bool storeSessionInRedis(const std::string& sessionId, const std::string& username,
                           const std::string& clientInfo, std::time_t expiry);
    std::pair<std::string, std::string> getSessionFromRedis(const std::string& sessionId) const;
    bool deleteSessionFromRedis(const std::string& sessionId);
    std::vector<std::string> getUserSessionsFromRedis(const std::string& username) const;

    // Replication
    void replicationWatcherLoop();
    void processReplicationMessage(const std::string& message);

    // Cache management for performance
    void updateLocalCache(const std::string& key, const std::string& value, std::chrono::seconds ttl = std::chrono::seconds(300));
    std::string getFromLocalCache(const std::string& key) const;
    void invalidateLocalCache(const std::string& key);
    void cleanupExpiredCache();

    // Utility functions
    std::string generateSessionId() const;
    std::string hashPassword(const std::string& password) const;
    bool verifyPassword(const std::string& password, const std::string& hash) const;
    std::string generateNodeId() const;

    // Redis key generation
    std::string userKey(const std::string& username) const;
    std::string sessionKey(const std::string& sessionId) const;
    std::string clusterKey(const std::string& suffix = "") const;
    std::string nodeKey() const;

    // Member variables
    std::string redisHost_;
    int redisPort_;
    std::string redisPassword_;
    std::string clusterId_;
    std::string nodeId_;

    // Redis connection (simplified - in real implementation would use hiredis or similar)
    void* redisContext_ = nullptr; // Placeholder for Redis context

    // Local caches for performance
    mutable std::mutex cacheMutex_;
    std::unordered_map<std::string, std::string> userCache_;
    std::unordered_map<std::string, std::pair<std::string, std::chrono::steady_clock::time_point>> sessionCache_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> cacheExpiry_;

    // Replication thread
    std::atomic<bool> replicationRunning_{false};
    std::thread replicationThread_;

    // Health status
    std::atomic<bool> healthy_{false};
    mutable std::mutex statusMutex_;
    std::string lastError_;

    // Session configuration
    static constexpr std::chrono::hours SESSION_TIMEOUT = std::chrono::hours(24);
    static constexpr std::chrono::seconds CACHE_CLEANUP_INTERVAL = std::chrono::seconds(60);
};