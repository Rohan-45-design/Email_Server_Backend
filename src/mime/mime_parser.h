#pragma once
#include <string>
#include "mime_message.h"

class MimeParser {
public:
    static MimeMessage parse(const std::string& raw);

private:
    static MimeHeaderMap parseHeaders(std::string& data);
    static MimePart parsePart(
        const std::string& data,
        const std::string& boundary
    );
};
