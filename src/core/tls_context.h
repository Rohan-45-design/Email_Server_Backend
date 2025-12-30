#pragma once
#include <string>
#include <openssl/ssl.h>
#include <openssl/err.h>

class TlsContext {
public:
    static TlsContext& instance();
    bool init(const std::string& certFile, const std::string& keyFile);
    SSL* createSSL(int fd);
    SSL_CTX* raw() const { return ctx_; }

private:
    TlsContext() = default;
    ~TlsContext();
    SSL_CTX* ctx_ = nullptr;
};
