#include "core/auth_manager.h"
#include "core/logger.h"
#include "core/password_hash.h"
#include "core/audit_log.h"
#include "core/base64.h"

#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <fstream>
#include <filesystem>

bool AuthManager::loadFromFile(const std::string& path) {
    try {
        YAML::Node root = YAML::LoadFile(path);
        if (!root["users"]) {
            Logger::instance().log(LogLevel::Warn,
                "AuthManager: users.yml has no 'users' section");
            return false;
        }

        int plaintext_count = 0;
        int hashed_count = 0;
        int migrated_count = 0;

        auto u = root["users"];
        for (auto it = u.begin(); it != u.end(); ++it) {
            std::string name = it->first.as<std::string>();
            YAML::Node userNode = it->second;
            if (userNode["password"]) {
                std::string password = userNode["password"].as<std::string>();
                
                if (PasswordHash::isHashed(password)) {
                    users_[name] = password;
                    hashed_count++;
                } else {
                    // Auto-migrate plaintext password
                    std::string hashed = PasswordHash::hash(password);
                    userNode["password"] = hashed;  // Update YAML node
                    plaintext_count++;
                    migrated_count++;
                    
                    Logger::instance().log(LogLevel::Info,
                        "AuthManager: migrated plaintext password for user '" + name + "'");
                }
            }
        }

        // CRITICAL: Persist migration atomically before using migrated passwords
        if (migrated_count > 0) {
            // Use atomic file replacement to ensure persistence
            std::string tempPath = path + ".tmp";
            
            // Write to temporary file
            std::ofstream fout(tempPath);
            if (!fout) {
                Logger::instance().log(LogLevel::Error,
                    "AuthManager: failed to create temporary file for migration: " + tempPath);
                return false;  // Fail startup - don't use migrated passwords
            }
            
            fout << root;
            fout.close();
            
            if (!fout.good()) {
                Logger::instance().log(LogLevel::Error,
                    "AuthManager: failed to write migrated passwords to temporary file");
                std::filesystem::remove(tempPath);  // Clean up
                return false;  // Fail startup
            }
            
            // Atomic rename: replace original with migrated version
            try {
                std::filesystem::rename(tempPath, path);
                Logger::instance().log(LogLevel::Info,
                    "AuthManager: successfully persisted " + std::to_string(migrated_count) + 
                    " migrated passwords to " + path);
            } catch (const std::filesystem::filesystem_error& e) {
                Logger::instance().log(LogLevel::Error,
                    "AuthManager: failed to persist migrated passwords: " + std::string(e.what()));
                std::filesystem::remove(tempPath);  // Clean up
                return false;  // Fail startup - don't use migrated passwords
            }
            
            // Now it's safe to store migrated passwords in memory
            for (auto it = u.begin(); it != u.end(); ++it) {
                std::string name = it->first.as<std::string>();
                YAML::Node userNode = it->second;
                if (userNode["password"]) {
                    std::string password = userNode["password"].as<std::string>();
                    if (!PasswordHash::isHashed(password)) {
                        // This should not happen after migration
                        Logger::instance().log(LogLevel::Error,
                            "AuthManager: CRITICAL - password not hashed after migration for user '" + name + "'");
                        return false;
                    }
                    users_[name] = password;
                }
            }
        } else {
            // No migration needed, load normally
            for (auto it = u.begin(); it != u.end(); ++it) {
                std::string name = it->first.as<std::string>();
                YAML::Node userNode = it->second;
                if (userNode["password"]) {
                    users_[name] = userNode["password"].as<std::string>();
                }
            }
        }

        Logger::instance().log(LogLevel::Info,
            "AuthManager: loaded " + std::to_string(users_.size()) + " users " +
            "(" + std::to_string(hashed_count) + " already hashed, " + 
            std::to_string(migrated_count) + " migrated from plaintext)");
        
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
    
    // CRITICAL: Ensure password is hashed (plaintext should never be allowed)
    if (!PasswordHash::isHashed(it->second)) {
        Logger::instance().log(LogLevel::Error,
            "AuthManager: CRITICAL - plaintext password found for user '" + user + 
            "' - this should never happen after migration");
        AuditLog::instance().logAuthFailure(user, "PLAINTEXT_PASSWORD_BLOCKED");
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

bool AuthManager::authenticateSmtp(const std::string& args, std::string& username) {
    // Simple implementation for PLAIN auth
    // Expect "PLAIN <base64>"
    if (args.substr(0, 6) != "PLAIN ") {
        return false;
    }
    std::string b64 = args.substr(6);
    std::string decoded = base64Decode(b64);
    
    // PLAIN format: \0user\0pass
    size_t pos1 = decoded.find('\0');
    if (pos1 == std::string::npos) return false;
    size_t pos2 = decoded.find('\0', pos1 + 1);
    if (pos2 == std::string::npos) return false;
    
    std::string user = decoded.substr(pos1 + 1, pos2 - pos1 - 1);
    std::string pass = decoded.substr(pos2 + 1);
    
    username = user;
    return validate(user, pass);
}

