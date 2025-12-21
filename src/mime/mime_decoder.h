#pragma once
#include <string>

class MimeDecoder {
public:
    static std::string decodeTransferEncoding(
        const std::string& data,
        const std::string& encoding
    );
};
