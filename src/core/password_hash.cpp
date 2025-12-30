#include "password_hash.h"
#include "logger.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <vector>
#include <stdexcept>

namespace {
    // Base64 encoding using OpenSSL BIO
    std::string base64_encode(const unsigned char* data, size_t len) {
        BIO* bio = BIO_new(BIO_s_mem());
        BIO* b64 = BIO_new(BIO_f_base64());
        bio = BIO_push(b64, bio);
        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
        BIO_write(bio, data, len);
        BIO_flush(bio);
        
        BUF_MEM* bufferPtr;
        BIO_get_mem_ptr(bio, &bufferPtr);
        std::string result(bufferPtr->data, bufferPtr->length);
        BIO_free_all(bio);
        return result;
    }
    
    // Base64 decoding using OpenSSL BIO
    std::vector<unsigned char> base64_decode(const std::string& encoded) {
        BIO* bio = BIO_new_mem_buf(encoded.c_str(), encoded.length());
        BIO* b64 = BIO_new(BIO_f_base64());
        bio = BIO_push(b64, bio);
        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
        
        std::vector<unsigned char> result(encoded.length());
        int decoded_len = BIO_read(bio, result.data(), encoded.length());
        BIO_free_all(bio);
        
        if (decoded_len > 0) {
            result.resize(decoded_len);
        } else {
            result.clear();
        }
        return result;
    }
}

std::string PasswordHash::hash(const std::string& plaintext) {
    if (plaintext.empty()) {
        Logger::instance().log(LogLevel::Warn, "PasswordHash: Attempted to hash empty password");
        return "";
    }
    
    // Use PBKDF2 with SHA-256 (production-grade, available in OpenSSL)
    const int iterations = 100000; // High iteration count for security
    const int saltlen = 16;
    const int keylen = 32; // 256 bits
    unsigned char salt[saltlen];
    unsigned char key[keylen];
    
    // Generate cryptographically secure random salt
    if (RAND_bytes(salt, saltlen) != 1) {
        Logger::instance().log(LogLevel::Error, "PasswordHash: Failed to generate salt");
        throw std::runtime_error("Failed to generate salt");
    }
    
    // Derive key using PBKDF2
    if (PKCS5_PBKDF2_HMAC(
            plaintext.c_str(), static_cast<int>(plaintext.length()),
            salt, saltlen,
            iterations,
            EVP_sha256(),
            keylen, key) != 1) {
        Logger::instance().log(LogLevel::Error, "PasswordHash: PBKDF2 failed");
        throw std::runtime_error("Password hashing failed");
    }
    
    // Format: $pbkdf2-sha256$iterations$salt$hash (all base64 encoded)
    std::ostringstream oss;
    oss << "$pbkdf2-sha256$" << iterations << "$";
    oss << base64_encode(salt, saltlen);
    oss << "$" << base64_encode(key, keylen);
    
    return oss.str();
}

bool PasswordHash::verify(const std::string& plaintext, const std::string& hash) {
    if (plaintext.empty() || hash.empty()) {
        return false;
    }
    
    // Check if it's our PBKDF2 format
    if (hash.substr(0, 15) == "$pbkdf2-sha256$") {
        // Parse: $pbkdf2-sha256$iterations$salt$hash
        size_t pos1 = hash.find('$', 15);
        if (pos1 == std::string::npos) return false;
        
        int iterations = std::stoi(hash.substr(15, pos1 - 15));
        size_t pos2 = hash.find('$', pos1 + 1);
        if (pos2 == std::string::npos) return false;
        
        std::string salt_b64 = hash.substr(pos1 + 1, pos2 - pos1 - 1);
        std::string hash_b64 = hash.substr(pos2 + 1);
        
        // Decode salt and hash
        std::vector<unsigned char> salt_bytes = base64_decode(salt_b64);
        std::vector<unsigned char> hash_bytes = base64_decode(hash_b64);
        
        if (salt_bytes.empty() || hash_bytes.empty()) {
            return false;
        }
        
        // Derive key and compare (constant-time)
        unsigned char computed_key[32];
        if (PKCS5_PBKDF2_HMAC(
                plaintext.c_str(), static_cast<int>(plaintext.length()),
                salt_bytes.data(), static_cast<int>(salt_bytes.size()),
                iterations,
                EVP_sha256(),
                32, computed_key) != 1) {
            return false;
        }
        
        // Constant-time comparison to prevent timing attacks
        if (hash_bytes.size() != 32) {
            return false;
        }
        
        volatile unsigned char diff = 0;
        for (size_t i = 0; i < 32; i++) {
            diff |= computed_key[i] ^ hash_bytes[i];
        }
        return diff == 0;
    }
    
    // Legacy plaintext support (for migration period only)
    // Log warning and allow comparison during migration
    if (!isHashed(hash)) {
        Logger::instance().log(LogLevel::Warn, 
            "PasswordHash: Plaintext password detected - should be migrated to hashed");
        // Constant-time comparison even for plaintext
        if (plaintext.length() != hash.length()) {
            return false;
        }
        volatile unsigned char diff = 0;
        for (size_t i = 0; i < plaintext.length(); i++) {
            diff |= plaintext[i] ^ hash[i];
        }
        return diff == 0;
    }
    
    return false;
}

bool PasswordHash::isHashed(const std::string& str) {
    // Check if it looks like a hash (starts with $ and has proper format)
    return str.length() > 20 && str[0] == '$' && 
           (str.substr(0, 15) == "$pbkdf2-sha256$" || str.substr(0, 8) == "$pbkdf2$");
}

