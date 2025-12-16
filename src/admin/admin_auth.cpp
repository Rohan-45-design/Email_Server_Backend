#include "admin/admin_auth.h"

std::string AdminAuth::token_;

void AdminAuth::setToken(const std::string& token) {
    token_ = token;
}

bool AdminAuth::authorize(const std::string& req) {
    if (token_.empty())
        return false;

    std::string needle = "X-Admin-Token: " + token_;
    return req.find(needle) != std::string::npos;
}
