#pragma once
#include "mime_part.h"

struct MimeMessage {
    MimeHeaderMap headers;
    MimePart root;
};
