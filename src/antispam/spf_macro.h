#pragma once
#include <string>

class SpfMacro {
public:
    static std::string expand(const std::string& input,
                              const std::string& ip,
                              const std::string& sender,
                              const std::string& helo,
                              const std::string& domain);
};
