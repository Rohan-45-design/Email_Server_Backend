#include "core/config_loader.h"
#include "core/logger.h"

#include <yaml-cpp/yaml.h>
#include <stdexcept>

ServerConfig ConfigLoader::loadFromFile(const std::string& path) {
    ServerConfig cfg;

    try {
        YAML::Node root = YAML::LoadFile(path);

        if (root["server"]) {
            auto s = root["server"];
            if (s["host"])       cfg.host     = s["host"].as<std::string>();
            if (s["smtp_port"])  cfg.smtpPort = s["smtp_port"].as<int>();
            if (s["imap_port"])  cfg.imapPort = s["imap_port"].as<int>();
            if (s["domain"])     cfg.domain   = s["domain"].as<std::string>();
            if (s["mail_root"])  cfg.mailRoot = s["mail_root"].as<std::string>(); // NEW
        }

        if (root["logging"]) {
            auto l = root["logging"];
            if (l["file"])  cfg.logFile  = l["file"].as<std::string>();
            if (l["level"]) cfg.logLevel = l["level"].as<std::string>();
        }

        if (root["auth"]) {
            auto a = root["auth"];
            if (a["users_file"]) cfg.usersFile = a["users_file"].as<std::string>(); // NEW
        }
        if (root["admin"]) {
            auto a = root["admin"];
            if (a["token"])
                cfg.adminToken = a["token"].as<std::string>();
        }
    } catch (const std::exception& ex) {
        Logger::instance().log(
            LogLevel::Error,
            std::string("Failed to load config: ") + ex.what());
        throw;
    }

    return cfg;
}
