// src/antispam/dkim_signer.cpp
#include "dkim_signer.h"
#include "dkim_canon.h"
#include "core/logger.h"

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include <fstream>
#include <sstream>
#include <vector>
#include <string>

DkimSigner::DkimSigner(const DkimSignConfig& cfg) : cfg_(cfg) {}


// -------------------- Utilities --------------------

static std::string base64Encode(const unsigned char* data, size_t len) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_push(b64, mem);

    BIO_write(b64, data, static_cast<int>(len));
    BIO_flush(b64);

    BUF_MEM* buf = nullptr;
    BIO_get_mem_ptr(b64, &buf);

    std::string out(buf->data, buf->length);
    BIO_free_all(b64);
    return out;
}

static EVP_PKEY* loadPrivateKey(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        return nullptr;

    std::string pem((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());

    BIO* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
    EVP_PKEY* key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    return key;
}

static std::vector<std::string> splitHeaderNames(const std::string& h) {
    std::vector<std::string> out;
    std::string token;
    std::istringstream iss(h);
    while (std::getline(iss, token, ':')) {
        if (!token.empty())
            out.push_back(token);
    }
    return out;
}


// -------------------- DKIM SIGN --------------------

std::string DkimSigner::sign(const std::string& headers,
                             const std::string& body) {

    /* STEP 1 — Canonicalize body (relaxed) */
    std::string canonBody =
        DkimCanon::canonicalizeBodyRelaxed(body);

    /* STEP 2 — Body hash (bh=) */
    unsigned char bodyHash[EVP_MAX_MD_SIZE];
    unsigned int bodyHashLen = 0;

    EVP_MD_CTX* bodyCtx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(bodyCtx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(bodyCtx,
                     canonBody.data(),
                     canonBody.size());
    EVP_DigestFinal_ex(bodyCtx,
                       bodyHash,
                       &bodyHashLen);
    EVP_MD_CTX_free(bodyCtx);

    std::string bh = base64Encode(bodyHash, bodyHashLen);

    /* STEP 3 — DKIM-Signature header (b= EMPTY) */
    std::ostringstream dkim;
    dkim << "DKIM-Signature: v=1; a=rsa-sha256; c=relaxed/relaxed; "
         << "d=" << cfg_.domain << "; "
         << "s=" << cfg_.selector << "; "
         << "h=" << cfg_.headersToSign << "; "
         << "bh=" << bh << "; "
         << "b=";

    std::string dkimHeaderBare = dkim.str();

    /* STEP 4 — Header canonicalization (CORRECT API USAGE) */
    std::string headersForSigning =
        headers + "\r\n" + dkimHeaderBare;

    std::vector<std::string> headerNames =
        splitHeaderNames(cfg_.headersToSign);

    auto canonHeaders =
        DkimCanon::canonicalizeHeadersRelaxed(
            headersForSigning,
            headerNames
        );

    std::string signingData;
    for (const auto& h : canonHeaders)
        signingData += h;

    /* STEP 5 — RSA-SHA256 signing */
    EVP_PKEY* key = loadPrivateKey(cfg_.privateKeyPath);
    if (!key) {
        Logger::instance().log(
            LogLevel::Error,
            "DKIM: failed to load private key");
        return "";
    }

    EVP_MD_CTX* signCtx = EVP_MD_CTX_new();
    EVP_DigestSignInit(signCtx,
                       nullptr,
                       EVP_sha256(),
                       nullptr,
                       key);

    EVP_DigestSignUpdate(signCtx,
                         signingData.data(),
                         signingData.size());

    size_t sigLen = 0;
    EVP_DigestSignFinal(signCtx, nullptr, &sigLen);

    std::vector<unsigned char> sig(sigLen);
    EVP_DigestSignFinal(signCtx,
                        sig.data(),
                        &sigLen);

    EVP_MD_CTX_free(signCtx);
    EVP_PKEY_free(key);

    std::string signature = base64Encode(sig.data(), sigLen);

    /* STEP 6 — Final DKIM header */
    std::string finalHeader =
        dkimHeaderBare + signature;

    Logger::instance().log(
        LogLevel::Info,
        "DKIM: signed message for domain " + cfg_.domain);

    return finalHeader;
}
