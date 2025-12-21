#pragma once
#include <string>

class RetroLog {
public:
    static void record(
        const std::string& messageId,
        const std::string& user,
        const std::string& reason
    );
};
