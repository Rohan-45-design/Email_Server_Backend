#pragma once

#include <string>
#include <vector>

class IAuthManager {
public:
    virtual ~IAuthManager() = default;

    // Authentication operations
    virtual bool validate(const std::string& user, const std::string& pass) const = 0;
    virtual bool authenticateSmtp(const std::string& args, std::string& username) = 0;
    virtual bool userExists(const std::string& user) const = 0;
    virtual size_t getUserCount() const = 0;

    // User management (for distributed auth)
    virtual bool addUser(const std::string& username, const std::string& hashedPassword) { return false; }
    virtual bool removeUser(const std::string& username) { return false; }
    virtual bool updateUserPassword(const std::string& username, const std::string& newHashedPassword) { return false; }

    // Session management (for distributed auth)
    virtual std::string createSession(const std::string& username, const std::string& clientInfo = "") { return ""; }
    virtual bool validateSession(const std::string& sessionId, std::string& username) { return false; }
    virtual void invalidateSession(const std::string& sessionId) {}
    virtual std::vector<std::string> getActiveSessions(const std::string& username = "") { return {}; }

    // Load users from file
    virtual bool loadFromFile(const std::string& path) = 0;

    // Health and status
    virtual bool isHealthy() const { return true; }
    virtual std::string getStatus() const { return "OK"; }
};