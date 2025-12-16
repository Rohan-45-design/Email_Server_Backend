// src/antispam/dkim_canon.h
#pragma once
#include <string>
#include <vector>

namespace DkimCanon {

std::string canonicalizeBodyRelaxed(const std::string& body);

std::vector<std::string> canonicalizeHeadersRelaxed(
    const std::string& headers,
    const std::vector<std::string>& headerNames);

}
