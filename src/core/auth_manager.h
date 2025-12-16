#pragma once

#include <string>
#include <unordered_map>

class AuthManager {
public:
    AuthManager() = default;

    // Load users from YAML file: users: { name: { password: "..." }, ... }
    bool load(const std::string& path);

    // Check username/password
    bool validate(const std::string& user, const std::string& pass) const;

private:
    std::unordered_map<std::string, std::string> users_;
};
