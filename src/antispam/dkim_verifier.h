#pragma once
#include <string>
#include "antispam/auth_results.h"

class DkimVerifier {
public:
    DkimAuthResult verify(const std::string& headers,
                          const std::string& body);
};
