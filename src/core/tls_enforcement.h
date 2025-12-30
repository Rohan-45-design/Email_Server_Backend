#pragma once

#include <string>

/**
 * Hard TLS Enforcement
 * 
 * WHY REQUIRED:
 * - Once TLS is configured, plaintext must be blocked (security requirement)
 * - Prevent STARTTLS downgrade attacks
 * - Enforce minimum TLS version and cipher policies
 * - Compliance: Many regulations require encrypted email
 */
class TlsEnforcement {
public:
    static TlsEnforcement& instance();

    // Configure TLS requirements
    void setTlsRequired(bool required);
    void setMinTlsVersion(int version); // TLS1_2_VERSION or TLS1_3_VERSION
    void setRequireStartTls(bool require); // For SMTP submission

    // Check if TLS is required
    bool isTlsRequired() const { return tlsRequired_; }
    bool isStartTlsRequired() const { return requireStartTls_; }

    // Validate TLS connection meets requirements
    bool validateTlsConnection(void* ssl) const;

    // Check if plaintext should be allowed
    bool allowPlaintext(int port) const;

private:
    TlsEnforcement() = default;
    bool tlsRequired_ = false;
    bool requireStartTls_ = false;
    int minTlsVersion_ = 0x0303; // TLS 1.2
};

