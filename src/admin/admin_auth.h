#pragma once
#include <string>

class AdminAuth {
public:
    // Load token from config
    static void setToken(const std::string& token);

    // Validate incoming HTTP request
    static bool authorize(const std::string& httpRequest);

private:
    static std::string token_;
};
