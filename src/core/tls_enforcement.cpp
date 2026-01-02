#include "tls_enforcement.h"
#include "logger.h"
#include <openssl/ssl.h>

TlsEnforcement& TlsEnforcement::instance() {
    static TlsEnforcement inst;
    return inst;
}

void TlsEnforcement::setTlsRequired(bool required) {
    tlsRequired_ = required;
    Logger::instance().log(LogLevel::Info,
        "TlsEnforcement: TLS required = " + std::string(required ? "true" : "false"));
}

void TlsEnforcement::setMinTlsVersion(int version) {
    if (version == 0) {
        throw std::runtime_error("min_tls_version not configured or invalid");
    }
    switch (version) {
        case 1: minTlsVersion_ = TLS1_VERSION; break;
        case 2: minTlsVersion_ = TLS1_1_VERSION; break;
        case 3: minTlsVersion_ = TLS1_2_VERSION; break;
        default:
            Logger::instance().log(LogLevel::Error,
                "Invalid min_tls_version: " + std::to_string(version));
            throw std::runtime_error(
                "server.min_tls_version must be 1 (TLS 1.0), 2 (TLS 1.1), or 3 (TLS 1.2+)"
            );
    }
    Logger::instance().log(LogLevel::Info,
        "TlsEnforcement: Minimum TLS version set to " + std::to_string(minTlsVersion_));
}

void TlsEnforcement::setRequireStartTls(bool require) {
    requireStartTls_ = require;
    Logger::instance().log(LogLevel::Info,
        "TlsEnforcement: STARTTLS required = " + std::string(require ? "true" : "false"));
}

bool TlsEnforcement::validateTlsConnection(void* ssl) const {
    if (!ssl) return false;
    if (minTlsVersion_ == 0) {
        Logger::instance().log(LogLevel::Error, "TlsEnforcement: min_tls_version not configured");
        return false;
    }

    SSL* ssl_ptr = static_cast<SSL*>(ssl);
    int version = SSL_version(ssl_ptr);
    
    if (version < minTlsVersion_) {
        Logger::instance().log(LogLevel::Warn,
            "TlsEnforcement: TLS version " + std::to_string(version) + 
            " below minimum " + std::to_string(minTlsVersion_));
        return false;
    }

    // Check cipher strength
    const SSL_CIPHER* cipher = SSL_get_current_cipher(ssl_ptr);
    if (cipher) {
        int bits = SSL_CIPHER_get_bits(cipher, nullptr);
        if (bits < 128) {
            Logger::instance().log(LogLevel::Warn,
                "TlsEnforcement: Weak cipher detected (" + std::to_string(bits) + " bits)");
            return false;
        }
    }

    return true;
}

bool TlsEnforcement::allowPlaintext(int port) const {
    // Port 25 (SMTP) can be plaintext for server-to-server
    // Port 587 (submission) should require TLS
    // Port 143 (IMAP) can be plaintext if STARTTLS is available
    // Port 993 (IMAPS) is always TLS
    
    if (port == 993 || port == 465) {
        return false; // Always encrypted ports
    }

    if (tlsRequired_) {
        // If TLS is required globally, only allow on ports that support STARTTLS
        if (port == 25 || port == 143) {
            return true; // Allow if STARTTLS is available
        }
        // Port 587 (submission) should require TLS
        if (port == 587 && requireStartTls_) {
            return false;
        }
    }

    return true;
}

