#pragma once

#include <string>

/**
 * Secure password hashing using bcrypt (via OpenSSL)
 * Production-grade password storage and verification
 */
class PasswordHash {
public:
    /**
     * Hash a plaintext password using bcrypt
     * @param plaintext The plaintext password
     * @return bcrypt hash string (format: $2a$rounds$salt+hash)
     */
    static std::string hash(const std::string& plaintext);

    /**
     * Verify a plaintext password against a bcrypt hash
     * Uses constant-time comparison to prevent timing attacks
     * @param plaintext The plaintext password to verify
     * @param hash The bcrypt hash to verify against
     * @return true if password matches, false otherwise
     */
    static bool verify(const std::string& plaintext, const std::string& hash);

    /**
     * Check if a string looks like a bcrypt hash
     * Used for migration from plaintext passwords
     * @param str The string to check
     * @return true if it looks like a bcrypt hash
     */
    static bool isHashed(const std::string& str);

private:
    // Bcrypt cost factor (higher = more secure but slower)
    static constexpr int BCRYPT_COST = 12;
};

