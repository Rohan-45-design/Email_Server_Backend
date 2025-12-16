// src/antispam/dkim_canon.cpp
#include "dkim_canon.h"
#include <algorithm>
#include <cctype>
#include <sstream>

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return s;
}

static std::string compressWsp(const std::string& s) {
    std::string out;
    bool inWsp = false;
    for (char c : s) {
        if (c == ' ' || c == '\t') {
            if (!inWsp) out.push_back(' ');
            inWsp = true;
        } else {
            out.push_back(c);
            inWsp = false;
        }
    }
    return out;
}

std::string DkimCanon::canonicalizeBodyRelaxed(const std::string& body) {
    std::string b;
    for (size_t i = 0; i < body.size(); ++i) {
        if (body[i] == '\r') continue;
        if (body[i] == '\n') b += "\r\n";
        else b += body[i];
    }

    while (b.size() >= 2 && b.substr(b.size() - 2) == "\r\n") {
        size_t pos = b.size() - 2;
        if (pos == 0 || b.substr(pos - 2, 2) != "\r\n")
            break;
        b.erase(pos);
    }

    if (b.size() < 2 || b.substr(b.size() - 2) != "\r\n")
        b += "\r\n";

    return b;
}

std::vector<std::string> DkimCanon::canonicalizeHeadersRelaxed(
    const std::string& headers,
    const std::vector<std::string>& headerNames) {

    std::vector<std::string> out;
    std::istringstream iss(headers);
    std::string line, current;

    std::vector<std::string> lines;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (!line.empty() && (line[0] == ' ' || line[0] == '\t')) {
            current += " " + line.substr(1);
        } else {
            if (!current.empty()) lines.push_back(current);
            current = line;
        }
    }
    if (!current.empty()) lines.push_back(current);

    for (auto it = headerNames.rbegin(); it != headerNames.rend(); ++it) {
        std::string want = toLower(*it);
        for (auto rit = lines.rbegin(); rit != lines.rend(); ++rit) {
            auto pos = rit->find(':');
            if (pos == std::string::npos) continue;
            std::string name = toLower(rit->substr(0, pos));
            if (name == want) {
                std::string value = rit->substr(pos + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                value = compressWsp(value);
                out.push_back(name + ":" + value);
                break;
            }
        }
    }
    return out;
}
