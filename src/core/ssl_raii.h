#pragma once

#include <openssl/ssl.h>
#include <memory>

struct SSLDeleter {
    void operator()(SSL* s) const noexcept {
        if (s) SSL_free(s);
    }
};

using SslPtr = std::unique_ptr<SSL, SSLDeleter>;

inline SslPtr make_ssl_ptr(SSL* raw) {
    return SslPtr(raw);
}
