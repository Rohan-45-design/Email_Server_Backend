#include "input_validator.h"
#include "logger.h"
#include <algorithm>
#include <regex>
#include <filesystem>

namespace fs = std::filesystem;

bool InputValidator::isValidUsername(const std::string& username) {
    if (username.empty() || username.length() > 64) {
        return false;
    }
    
    // Username should only contain alphanumeric, underscore, hyphen, dot
    std::regex pattern("^[a-zA-Z0-9._-]+$");
    return std::regex_match(username, pattern);
}

std::string InputValidator::sanitizeUsername(const std::string& username) {
    std::string sanitized;
    sanitized.reserve(username.length());
    
    for (char c : username) {
        if (std::isalnum(c) || c == '_' || c == '-' || c == '.') {
            sanitized += c;
        }
    }
    
    // Limit length
    if (sanitized.length() > 64) {
        sanitized = sanitized.substr(0, 64);
    }
    
    return sanitized;
}

bool InputValidator::isValidEmail(const std::string& email) {
    if (email.empty() || email.length() > 254) { // RFC 5321 limit
        return false;
    }
    
    // Basic email validation (RFC 5322 simplified)
    std::regex pattern(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
    return std::regex_match(email, pattern);
}

bool InputValidator::containsPathTraversal(const std::string& path) {
    // Check for path traversal patterns
    return path.find("..") != std::string::npos ||
           path.find("//") != std::string::npos ||
           path.find("\\") != std::string::npos ||
           (path.length() > 0 && path[0] == '/');
}

bool InputValidator::isValidPath(const std::string& path, const std::string& baseDir) {
    if (path.empty()) {
        return false;
    }
    
    // Check for path traversal
    if (containsPathTraversal(path)) {
        return false;
    }
    
    try {
        // Resolve the full path
        fs::path fullPath = fs::canonical(baseDir) / path;
        fs::path basePath = fs::canonical(baseDir);
        
        // Check if resolved path is within base directory
        std::string fullStr = fullPath.string();
        std::string baseStr = basePath.string();
        
        // Normalize paths for comparison
        std::replace(fullStr.begin(), fullStr.end(), '\\', '/');
        std::replace(baseStr.begin(), baseStr.end(), '\\', '/');
        
        return fullStr.find(baseStr) == 0;
    } catch (...) {
        return false;
    }
}

std::string InputValidator::sanitizePath(const std::string& path, const std::string& baseDir) {
    std::string sanitized = path;
    
    // Remove path traversal sequences
    size_t pos;
    while ((pos = sanitized.find("..")) != std::string::npos) {
        sanitized.erase(pos, 2);
    }
    
    // Remove backslashes
    sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), '\\'), sanitized.end());
    
    // Remove leading slashes
    while (!sanitized.empty() && sanitized[0] == '/') {
        sanitized.erase(0, 1);
    }
    
    // Validate against base directory
    if (!isValidPath(sanitized, baseDir)) {
        Logger::instance().log(LogLevel::Warn,
            "InputValidator: Invalid path sanitized: " + path);
        return "";
    }
    
    return sanitized;
}

bool InputValidator::isValidDomain(const std::string& domain) {
    if (domain.empty() || domain.length() > 253) { // RFC 1035 limit
        return false;
    }
    
    // Basic domain validation
    std::regex pattern(R"(^([a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?\.)+[a-zA-Z]{2,}$)");
    return std::regex_match(domain, pattern);
}

std::string InputValidator::sanitizeString(const std::string& input, bool allowSpaces) {
    std::string sanitized;
    sanitized.reserve(input.length());
    
    for (char c : input) {
        if (std::isprint(c)) {
            if (allowSpaces || c != ' ') {
                // Remove potentially dangerous characters
                if (c != '<' && c != '>' && c != '"' && c != '\'' && 
                    c != '&' && c != ';' && c != '|' && c != '`' && c != '$') {
                    sanitized += c;
                }
            }
        }
    }
    
    return sanitized;
}

