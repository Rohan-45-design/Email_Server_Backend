#pragma once
#include <vector>
#include <string>
#include "policy_result.h"

struct AttachmentMeta {
    std::string filename;
    std::string mimeType;
    bool isArchive = false;
    bool isEncrypted = false;
};

class AttachmentPolicy {
public:
    static PolicyResult evaluate(
        const std::vector<AttachmentMeta>& attachments
    );

private:
    static bool isExecutable(const std::string& filename);
    static bool hasDoubleExtension(const std::string& filename);
    static bool allowedMime(const std::string& mimeType);
};
