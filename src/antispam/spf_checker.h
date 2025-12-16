#pragma once
#include <string>
#include "antispam/auth_results.h"

class SpfChecker {
public:
    SpfCheckResult check(const std::string& ip,
                         const std::string& mailFrom,
                         const std::string& heloDomain);
};
