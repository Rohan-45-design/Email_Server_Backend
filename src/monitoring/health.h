#pragma once
#include <string>

struct HealthStatus {
    bool ok;
    std::string message;
};

class Health {
public:
    static HealthStatus check();
};
