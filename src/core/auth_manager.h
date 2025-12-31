#pragma once

#include "i_auth_manager.h"
#include <string>
#include <unordered_map>

class AuthManager : public IAuthManager {
public:
    AuthManager() = default;

    // Load users from YAML file: users: { name: { password: "..." }, ... }
    // Auto-migrates any plaintext passwords to secure hashes on load
    // CRITICAL: Only hashed passwords are allowed in production
    bool loadFromFile(const std::string& path) override;

    // Check username/password (secure verification with hashing only)
    // Returns true if password matches hashed password, false otherwise
    // CRITICAL: Plaintext passwords are never allowed to authenticate
    bool validate(const std::string& user, const std::string& pass) const override;

    // SMTP AUTH parsing and validation
    bool authenticateSmtp(const std::string& args, std::string& username) override;

    // Check if user exists
    bool userExists(const std::string& user) const override;

    // Get number of loaded users
    size_t getUserCount() const override { return users_.size(); }

    // Legacy method for backward compatibility
    bool load(const std::string& path) { return loadFromFile(path); }

private:
    // Map: username -> password_hash (always hashed, never plaintext)
    std::unordered_map<std::string, std::string> users_;
};
