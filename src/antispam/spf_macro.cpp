#include "spf_macro.h"
#include <ctime>

static std::string replaceAll(std::string s,
                              const std::string& f,
                              const std::string& r) {
    size_t pos = 0;
    while ((pos = s.find(f, pos)) != std::string::npos) {
        s.replace(pos, f.length(), r);
        pos += r.length();
    }
    return s;
}

std::string SpfMacro::expand(const std::string& input,
                             const std::string& ip,
                             const std::string& sender,
                             const std::string& helo,
                             const std::string& domain) {
    std::string out = input;

    out = replaceAll(out, "%{i}", ip);
    out = replaceAll(out, "%{s}", sender);
    out = replaceAll(out, "%{h}", helo);
    out = replaceAll(out, "%{d}", domain);

    auto at = sender.find('@');
    if (at != std::string::npos) {
        out = replaceAll(out, "%{l}", sender.substr(0, at));
        out = replaceAll(out, "%{o}", sender.substr(at + 1));
    }

    out = replaceAll(out, "%%", "%");
    out = replaceAll(out, "%_", " ");
    out = replaceAll(out, "%-", "%20");

    return out;
}
