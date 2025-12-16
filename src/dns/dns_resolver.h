#pragma once
#include <vector>
#include <string>

class DnsResolver {
public:
    static DnsResolver& instance();

    std::vector<std::string> lookupA(const std::string& name);
    std::vector<std::string> lookupAAAA(const std::string& name);
    std::vector<std::string> lookupTxt(const std::string& name);
    std::vector<std::string> lookupMx(const std::string& name);

private:
    DnsResolver();
    std::vector<std::string> query(const std::string& name, uint16_t type);
};
