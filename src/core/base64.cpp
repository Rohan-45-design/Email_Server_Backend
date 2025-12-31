#include "base64.h"
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

std::string base64Decode(const std::string& input) {
    BIO* bio = BIO_new_mem_buf(input.data(), static_cast<int>(input.size()));
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);

    std::string output(input.size(), '\0');
    int len = BIO_read(bio, output.data(), static_cast<int>(output.size()));
    BIO_free_all(bio);

    if (len < 0) return {};
    output.resize(len);
    return output;
}
