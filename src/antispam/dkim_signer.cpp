// src/antispam/dkim_signer.cpp
#include "antispam/dkim_signer.h"
#include "core/logger.h"

// Dummy DKIM signer: creates a syntactically valid DKIM-Signature header
// with placeholder bh= and b= values so you can see DKIM on outbound mail
// without needing OpenSSL or DkimCanon.

DkimSigner::DkimSigner(const DkimSignConfig& cfg) : cfg_(cfg) {}

std::string DkimSigner::sign(const std::string& /*headers*/,
                             const std::string& /*body*/) {
    std::string d = cfg_.domain;
    std::string s = cfg_.selector;
    std::string h = cfg_.headersToSign;

    std::string dkimHeader =
        "DKIM-Signature: v=1; a=rsa-sha256; c=relaxed/relaxed; d=" + d +
        "; s=" + s + "; h=" + h +
        "; bh=dummybodyhash==; b=dummysignature==";

    Logger::instance().log(LogLevel::Info,
        "DKIM: added dummy DKIM-Signature header for domain " + d);

    return dkimHeader;
}
