#pragma once
#include <string>
#include <map>

struct MimeHeader {
    std::string name;
    std::string value;
};

using MimeHeaderMap = std::map<std::string, std::string>;
