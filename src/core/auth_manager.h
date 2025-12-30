#pragma once

#include <string>
#include <unordered_map>

class AuthManager {
public:
    AuthManager() = default;

    // Load users from YAML file: users: { name: { password: "..." }, ... }
    // Supports both plaintext (legacy) and hashed passwords
    bool load(const std::string& path);

    // Check username/password (secure verification with hashing)
    // Returns true if password matches, false otherwise
    bool validate(const std::string& user, const std::string& pass) const;

    // Check if user exists
    bool userExists(const std::string& user) const;

    // Get number of loaded users
    size_t getUserCount() const { return users_.size(); }

private:
    // Map: username -> password_hash (or plaintext during migration)
    std::unordered_map<std::string, std::string> users_;
};
