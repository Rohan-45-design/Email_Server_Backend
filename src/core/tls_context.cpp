#include "tls_context.h"
#include "core/logger.h"
#include <openssl/ssl.h>
#include <openssl/err.h>

TlsContext& TlsContext::instance() {
    static TlsContext t;
    return t;
}

bool TlsContext::init(const std::string& certFile, const std::string& keyFile) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    ctx_ = SSL_CTX_new(TLS_server_method());
    if (!ctx_) return false;
    
    SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION);
    SSL_CTX_set_options(ctx_, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
    
    if (SSL_CTX_use_certificate_file(ctx_, certFile.c_str(), SSL_FILETYPE_PEM) <= 0)
        return false;
    if (SSL_CTX_use_PrivateKey_file(ctx_, keyFile.c_str(), SSL_FILETYPE_PEM) <= 0)
        return false;
    if (!SSL_CTX_check_private_key(ctx_))
        return false;
    
    Logger::instance().log(LogLevel::Info, "TLS initialized");
    return true;
}

SSL* TlsContext::createSSL(int fd) {
    SSL* ssl = SSL_new(ctx_);
    SSL_set_fd(ssl, fd);
    return ssl;
}

TlsContext::~TlsContext() {
    if (ctx_) SSL_CTX_free(ctx_);
}
