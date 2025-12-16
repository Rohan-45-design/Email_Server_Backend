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
};

class ConfigLoader {
public:
    static ServerConfig loadFromFile(const std::string& path);
};
