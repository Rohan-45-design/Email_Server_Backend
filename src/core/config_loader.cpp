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
            if (s["mail_root"])  cfg.mailRoot = s["mail_root"].as<std::string>();
            if (s["tls_cert"])   cfg.tlsCertFile = s["tls_cert"].as<std::string>();
            if (s["tls_key"])    cfg.tlsKeyFile  = s["tls_key"].as<std::string>();
            if (s["tls_required"]) cfg.tlsRequired = s["tls_required"].as<bool>();
            if (s["require_starttls"]) cfg.requireStartTls = s["require_starttls"].as<bool>();
            if (s["min_tls_version"]) cfg.minTlsVersion = s["min_tls_version"].as<int>();
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

        if (root["ha"]) {
            auto ha = root["ha"];
            if (ha["enabled"]) cfg.enableHA = ha["enabled"].as<bool>();
            if (ha["redis_host"]) cfg.redisHost = ha["redis_host"].as<std::string>();
            if (ha["redis_port"]) cfg.redisPort = ha["redis_port"].as<int>();
            if (ha["redis_password"]) cfg.redisPassword = ha["redis_password"].as<std::string>();
            if (ha["cluster_id"]) cfg.clusterId = ha["cluster_id"].as<std::string>();
            if (ha["node_id"]) cfg.nodeId = ha["node_id"].as<std::string>();
        }

        if (root["admin"]) {
            auto a = root["admin"];
            if (a["token"])
                cfg.adminToken = a["token"].as<std::string>();
        }
        
        if (root["smtp"]) {
            auto s = root["smtp"];
            if (s["max_message_size"]) cfg.maxMessageSize = s["max_message_size"].as<size_t>();
            if (s["timeout"]) cfg.smtpTimeout = s["timeout"].as<int>();
            if (s["data_timeout"]) cfg.dataTimeout = s["data_timeout"].as<int>();
        }
    } catch (const std::exception& ex) {
        Logger::instance().log(
            LogLevel::Error,
            std::string("Failed to load config: ") + ex.what());
        throw;
    }

    // Validate configuration
    validateConfig(cfg);

    return cfg;
}

void ConfigLoader::validateConfig(const ServerConfig& cfg) {
    std::vector<std::string> errors;

    // Required fields
    if (cfg.domain.empty()) {
        errors.push_back("server.domain is required");
    }

    // Port validation
    if (cfg.smtpPort <= 0 || cfg.smtpPort > 65535) {
        errors.push_back("server.smtp_port must be between 1-65535");
    }
    if (cfg.imapPort <= 0 || cfg.imapPort > 65535) {
        errors.push_back("server.imap_port must be between 1-65535");
    }
    if (cfg.smtpPort == cfg.imapPort) {
        errors.push_back("server.smtp_port and server.imap_port must be different");
    }

    // TLS validation
    if (cfg.tlsRequired) {
        if (cfg.tlsCertFile.empty()) {
            errors.push_back("server.tls_cert is required when tls_required=true");
        }
        if (cfg.tlsKeyFile.empty()) {
            errors.push_back("server.tls_key is required when tls_required=true");
        }
    }

    // TLS version validation
    if (cfg.minTlsVersion < 1 || cfg.minTlsVersion > 3) {
        errors.push_back("server.min_tls_version must be 1 (TLS 1.0), 2 (TLS 1.1), or 3 (TLS 1.2+)");
    }

    // SMTP limits validation
    if (cfg.maxMessageSize < 1024) {
        errors.push_back("smtp.max_message_size must be at least 1024 bytes");
    }
    if (cfg.maxMessageSize > 100LL * 1024 * 1024) { // 100MB
        errors.push_back("smtp.max_message_size cannot exceed 100MB");
    }
    if (cfg.smtpTimeout < 30) {
        errors.push_back("smtp.timeout must be at least 30 seconds");
    }
    if (cfg.dataTimeout < 60) {
        errors.push_back("smtp.data_timeout must be at least 60 seconds");
    }

    // Log level validation
    std::vector<std::string> validLevels = {"debug", "info", "warn", "warning", "error"};
    if (std::find(validLevels.begin(), validLevels.end(), cfg.logLevel) == validLevels.end()) {
        errors.push_back("logging.level must be one of: debug, info, warn, error");
    }

    // HA configuration validation
    if (cfg.enableHA) {
        if (cfg.redisHost.empty()) {
            errors.push_back("ha.redis_host is required when ha.enabled=true");
        }
        if (cfg.redisPort <= 0 || cfg.redisPort > 65535) {
            errors.push_back("ha.redis_port must be between 1-65535");
        }
        if (cfg.clusterId.empty()) {
            errors.push_back("ha.cluster_id is required when ha.enabled=true");
        }
        // node_id is optional and will be auto-generated if empty
    }

    if (!errors.empty()) {
        std::string errorMsg = "Configuration validation failed:\n";
        for (const auto& error : errors) {
            errorMsg += "  - " + error + "\n";
        }
        Logger::instance().log(LogLevel::Error, errorMsg);
        throw std::runtime_error("Invalid configuration: " + errorMsg);
    }

    Logger::instance().log(LogLevel::Info, "Configuration validation passed");
}
