#include "core/auth_manager.h"
#include "core/logger.h"

#include <yaml-cpp/yaml.h>

bool AuthManager::load(const std::string& path) {
    try {
        YAML::Node root = YAML::LoadFile(path);
        if (!root["users"]) {
            Logger::instance().log(LogLevel::Warn,
                "AuthManager: users.yml has no 'users' section");
            return false;
        }

        auto u = root["users"];
        for (auto it = u.begin(); it != u.end(); ++it) {
            std::string name = it->first.as<std::string>();
            YAML::Node userNode = it->second;
            if (userNode["password"]) {
                users_[name] = userNode["password"].as<std::string>();
            }
        }

        Logger::instance().log(LogLevel::Info,
            "AuthManager: loaded " + std::to_string(users_.size()) + " users");
        return true;
    } catch (const std::exception& ex) {
        Logger::instance().log(LogLevel::Error,
            std::string("AuthManager: failed to load users: ") + ex.what());
        return false;
    }
}

bool AuthManager::validate(const std::string& user, const std::string& pass) const {
    auto it = users_.find(user);
    if (it == users_.end()) return false;
    return it->second == pass;
}
