// src/admin/admin_auth.cpp
// SECURE TOKEN-BASED ADMIN AUTH (JWT-READY, NO HARDCODED SECRETS)

#include "admin_auth.h"
#include "core/logger.h"

#include <cstring>
#include <cstdlib>

std::string AdminAuth::token_;


/*
 * Configure admin token at startup.
 * Token should be injected via config or environment variable.
 */
void AdminAuth::setToken(const std::string& token) {
    const char* env = std::getenv("ADMIN_TOKEN");
    if (env && std::strlen(env) > 0) {
        token_ = env;
    } else {
        token_ = token;
    }
    Logger::instance().log(
        LogLevel::Info,
        "AdminAuth: admin token configured"
    );
}


/*
 * Constant-time string comparison to avoid timing attacks.
 */
static bool secureEqual(const std::string& a, const std::string& b) {
    if (a.size() != b.size())
        return false;

    volatile unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= a[i] ^ b[i];
    }
    return diff == 0;
}


/*
 * Authorize admin request using header-based token authentication.
 *
 * Supported headers:
 *  1) X-Admin-Token: <token>        (legacy)
 *  2) Authorization: Bearer <token>
 */
bool AdminAuth::authorize(const std::string& request) {

    if (token_.empty()) {
        Logger::instance().log(
            LogLevel::Warn,
            "AdminAuth: no admin token configured"
        );
        return false;
    }

    /* 1. Legacy header support */
    std::string legacyHeader = "X-Admin-Token: ";
    size_t legacyPos = request.find(legacyHeader);
    if (legacyPos != std::string::npos) {
        size_t valueStart = legacyPos + legacyHeader.size();
        size_t valueEnd = request.find("\r\n", valueStart);

        std::string clientToken =
            request.substr(valueStart, valueEnd - valueStart);

        if (secureEqual(clientToken, token_)) {
            return true;
        }
    }

    /* 2. Authorization: Bearer <token> */
    const char* bearer = "Authorization: Bearer ";
    size_t bearerPos = request.find(bearer);
    if (bearerPos != std::string::npos) {
        size_t valueStart = bearerPos + std::strlen(bearer);
        size_t valueEnd = request.find("\r\n", valueStart);

        std::string clientToken =
            request.substr(valueStart, valueEnd - valueStart);

        if (secureEqual(clientToken, token_)) {
            return true;
        }
    }

    Logger::instance().log(
        LogLevel::Warn,
        "AdminAuth: access denied"
    );
    return false;
}
