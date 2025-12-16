// src/antispam/dkim_signer.h
#pragma once
#include <string>

struct DkimSignConfig {
    std::string domain;          // d=
    std::string selector;        // s=
    std::string privateKeyPath;  // unused in dummy version
    std::string headersToSign;   // e.g. "from:to:subject:date:mime-version"
};

class DkimSigner {
public:
    explicit DkimSigner(const DkimSignConfig& cfg);
    std::string sign(const std::string& headers,
                     const std::string& body);

private:
    DkimSignConfig cfg_;
};
