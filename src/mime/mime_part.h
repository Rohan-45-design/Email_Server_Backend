#pragma once
#include <string>
#include <vector>
#include "mime_header.h"

struct MimePart {
    MimeHeaderMap headers;
    std::string body;                 // decoded body
    std::vector<MimePart> children;   // multipart children

    bool isMultipart() const;
    std::string contentType() const;
    std::string filename() const;
};
