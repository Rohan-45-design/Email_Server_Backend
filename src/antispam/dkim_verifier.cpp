#include "antispam/dkim_verifier.h"
#include "antispam/dkim_canon.h"
#include "dns/dns_resolver.h"
#include "core/logger.h"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include <sstream>
#include <vector>
#include <string>

/* ===================== Helpers ===================== */

static std::string getTag(const std::string& h, const std::string& tag) {
    auto pos = h.find(tag + "=");
    if (pos == std::string::npos)
        return "";
    pos += tag.size() + 1;
    auto end = h.find(';', pos);
    return h.substr(pos, end - pos);
}

static EVP_PKEY* loadPublicKey(const std::string& txt) {
    auto p = txt.find("p=");
    if (p == std::string::npos)
        return nullptr;

    std::string key = txt.substr(p + 2);

    BIO* bio = BIO_new_mem_buf(key.data(), (int)key.size());
    EVP_PKEY* pub = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    return pub;
}

/* ===================== DKIM VERIFY ===================== */

DkimAuthResult DkimVerifier::verify(const std::string& headers,
                                   const std::string& body)
{
    DkimAuthResult out;
    out.result = DkimResult::None;
    out.headerDomain.clear();

    /* ---- Locate DKIM-Signature ---- */
    auto pos = headers.find("DKIM-Signature:");
    if (pos == std::string::npos)
        return out;

    auto end = headers.find("\r\n", pos);
    std::string dkimHeader = headers.substr(pos, end - pos);

    std::string domain   = getTag(dkimHeader, "d");
    std::string selector = getTag(dkimHeader, "s");
    std::string bh       = getTag(dkimHeader, "bh");
    std::string b        = getTag(dkimHeader, "b");
    std::string h        = getTag(dkimHeader, "h");

    if (domain.empty() || selector.empty()) {
        out.result = DkimResult::PermError;
        return out;
    }

    out.headerDomain = domain;

    /* ---- DNS TXT lookup ---- */
    std::string name = selector + "._domainkey." + domain;
    auto txts = DnsResolver::instance().lookupTxt(name);
    if (txts.empty()) {
        out.result = DkimResult::TempError;
        return out;
    }

    EVP_PKEY* pub = loadPublicKey(txts[0]);
    if (!pub) {
        out.result = DkimResult::PermError;
        return out;
    }

    /* ---- Canonicalize body ---- */
    std::string canonBody =
        DkimCanon::canonicalizeBodyRelaxed(body);

    unsigned char bhRaw[EVP_MAX_MD_SIZE];
    unsigned int bhLen = 0;

    EVP_MD_CTX* bhCtx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(bhCtx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(bhCtx, canonBody.data(), canonBody.size());
    EVP_DigestFinal_ex(bhCtx, bhRaw, &bhLen);
    EVP_MD_CTX_free(bhCtx);

    /* ---- Canonicalize headers ---- */
    std::vector<std::string> headerList;
    std::istringstream iss(h);
    std::string tok;
    while (std::getline(iss, tok, ':'))
        headerList.push_back(tok);

    auto canonHeaders =
        DkimCanon::canonicalizeHeadersRelaxed(headers, headerList);

    /* ---- Verify RSA signature ---- */
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, pub);

    for (const auto& ch : canonHeaders) {
        EVP_DigestVerifyUpdate(ctx, ch.data(), ch.size());
        EVP_DigestVerifyUpdate(ctx, "\r\n", 2);
    }

    int ok = EVP_DigestVerifyFinal(
        ctx,
        reinterpret_cast<const unsigned char*>(b.data()),
        b.size()
    );

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pub);

    out.result = (ok == 1)
        ? DkimResult::Pass
        : DkimResult::Fail;

    return out;
}
