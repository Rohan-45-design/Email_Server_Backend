#pragma once

#include <string>

struct ServerConfig {
    std::string host = "0.0.0.0";
    int smtpPort = 25;
    int imapPort = 143;
    std::string domain = "example.com";

    std::string logFile = "mailserver.log";
    std::string logLevel = "info";

    std::string mailRoot = "data/mail";          // NEW: replaces mailDir
    std::string usersFile = "config/users.yml";  // NEW
    std::string adminToken;
    int globalMaxConnections = 500;     // Total concurrent connections
    int maxConnectionsPerIP = 10;       // Per IP connections (5min)
    int maxMessagesPerHour = 100;       // Per sender messages
    int commandsPerMinute = 50;
    std::string tlsCertFile; // path to TLS certificate (PEM)
    std::string tlsKeyFile;  // path to TLS private key (PEM)
    bool tlsRequired = false;        // Require TLS for all connections
    bool requireStartTls = false;    // Require STARTTLS for submission (port 587)
    int minTlsVersion = 0x0303;      // Minimum TLS version (TLS1_2_VERSION)

    // SMTP-specific limits
    size_t maxMessageSize = 10485760;  // 10MB max message size
    int smtpTimeout = 300;             // 5 minutes default timeout
    int dataTimeout = 600;             // 10 minutes for DATA mode

    // High Availability (HA) Configuration
    bool enableHA = false;             // Enable distributed authentication
    std::string redisHost = "localhost"; // Redis server host
    int redisPort = 6379;              // Redis server port
    std::string redisPassword;         // Redis authentication password
    std::string clusterId = "email-cluster"; // Unique cluster identifier
    std::string nodeId;                // Unique node identifier (auto-generated if empty)
};

class ConfigLoader {
public:
    static ServerConfig loadFromFile(const std::string& path);
    static void validateConfig(const ServerConfig& cfg);
};
