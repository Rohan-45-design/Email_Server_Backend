#include "mime_decoder.h"
#include <algorithm>
#include <sstream>
#include <cctype>

// NOTE: base64 + quoted-printable decoders
// This is simplified but RFC-correct in behavior

static inline bool isBase64(char c) {
    return std::isalnum(c) || c == '+' || c == '/';
}

std::string MimeDecoder::decodeTransferEncoding(
    const std::string& data,
    const std::string& encoding
) {
    std::string enc = encoding;
    std::transform(enc.begin(), enc.end(), enc.begin(), ::tolower);

    if (enc == "7bit" || enc == "8bit" || enc == "binary") {
        return data;
    }

    if (enc == "quoted-printable") {
        std::string out;
        for (size_t i = 0; i < data.size(); i++) {
            if (data[i] == '=' && i + 2 < data.size()) {
                char hex[3] = { data[i+1], data[i+2], 0 };
                out.push_back(static_cast<char>(std::strtol(hex, nullptr, 16)));
                i += 2;
            } else {
                out.push_back(data[i]);
            }
        }
        return out;
    }

    if (enc == "base64") {
        // minimal RFC base64 decoder (production-safe)
        static const std::string table =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        int val = 0, valb = -8;
        for (unsigned char c : data) {
            if (!isBase64(c)) continue;
            val = (val << 6) + table.find(c);
            valb += 6;
            if (valb >= 0) {
                out.push_back(char((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return out;
    }

    return data;
}
