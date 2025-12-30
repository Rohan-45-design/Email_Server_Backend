#include "core/auth_manager.h"
#include "core/logger.h"
#include "core/password_hash.h"
#include "core/audit_log.h"

#include <yaml-cpp/yaml.h>
#include <algorithm>

bool AuthManager::load(const std::string& path) {
    try {
        YAML::Node root = YAML::LoadFile(path);
        if (!root["users"]) {
            Logger::instance().log(LogLevel::Warn,
                "AuthManager: users.yml has no 'users' section");
            return false;
        }

        int plaintext_count = 0;
        int hashed_count = 0;

        auto u = root["users"];
        for (auto it = u.begin(); it != u.end(); ++it) {
            std::string name = it->first.as<std::string>();
            YAML::Node userNode = it->second;
            if (userNode["password"]) {
                std::string password = userNode["password"].as<std::string>();
                users_[name] = password;
                
                // Track migration status
                if (PasswordHash::isHashed(password)) {
                    hashed_count++;
                } else {
                    plaintext_count++;
                }
            }
        }

        Logger::instance().log(LogLevel::Info,
            "AuthManager: loaded " + std::to_string(users_.size()) + " users " +
            "(" + std::to_string(hashed_count) + " hashed, " + 
            std::to_string(plaintext_count) + " plaintext)");
        
        if (plaintext_count > 0) {
            Logger::instance().log(LogLevel::Warn,
                "AuthManager: " + std::to_string(plaintext_count) + 
                " users have plaintext passwords - migration recommended");
        }
        
        return true;
    } catch (const std::exception& ex) {
        Logger::instance().log(LogLevel::Error,
            std::string("AuthManager: failed to load users: ") + ex.what());
        return false;
    }
}

bool AuthManager::validate(const std::string& user, const std::string& pass) const {
    auto it = users_.find(user);
    if (it == users_.end()) {
        // Log failed authentication attempt (user doesn't exist)
        AuditLog::instance().logAuthFailure(user, "USER_NOT_FOUND");
        return false;
    }
    
    // Use secure password verification
    bool valid = PasswordHash::verify(pass, it->second);
    
    if (valid) {
        AuditLog::instance().logAuthSuccess(user);
    } else {
        AuditLog::instance().logAuthFailure(user, "INVALID_PASSWORD");
    }
    
    return valid;
}

bool AuthManager::userExists(const std::string& user) const {
    return users_.find(user) != users_.end();
}
