#pragma once

#include <string>

/**
 * Input validation and sanitization utilities
 * Prevents injection attacks, path traversal, and other security issues
 */
class InputValidator {
public:
    // Validate and sanitize username
    static bool isValidUsername(const std::string& username);
    static std::string sanitizeUsername(const std::string& username);
    
    // Validate email address format
    static bool isValidEmail(const std::string& email);
    
    // Validate file path (prevent path traversal)
    static bool isValidPath(const std::string& path, const std::string& baseDir);
    static std::string sanitizePath(const std::string& path, const std::string& baseDir);
    
    // Validate domain name
    static bool isValidDomain(const std::string& domain);
    
    // Remove dangerous characters
    static std::string sanitizeString(const std::string& input, bool allowSpaces = false);
    
private:
    static bool containsPathTraversal(const std::string& path);
};

