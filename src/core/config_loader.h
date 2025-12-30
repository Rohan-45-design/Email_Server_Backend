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
};

class ConfigLoader {
public:
    static ServerConfig loadFromFile(const std::string& path);
};
