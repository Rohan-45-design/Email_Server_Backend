#include "attachment_policy.h"
#include "policy_result.h"
#include <algorithm>
#include <string>

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static bool endsWith(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    return std::equal(
        suffix.rbegin(), suffix.rend(), s.rbegin()
    );
}

static bool startsWith(const std::string& s, const std::string& prefix) {
    if (s.size() < prefix.size()) return false;
    return std::equal(
        prefix.begin(), prefix.end(), s.begin()
    );
}

bool AttachmentPolicy::isExecutable(const std::string& filename) {
    std::string f = lower(filename);
    return endsWith(f, ".exe") ||
           endsWith(f, ".js")  ||
           endsWith(f, ".vbs") ||
           endsWith(f, ".bat") ||
           endsWith(f, ".cmd") ||
           endsWith(f, ".scr");
}

bool AttachmentPolicy::hasDoubleExtension(const std::string& filename) {
    std::string f = lower(filename);

    auto last = f.find_last_of('.');
    if (last == std::string::npos) return false;

    auto prev = f.find_last_of('.', last - 1);
    if (prev == std::string::npos) return false;

    std::string inner = f.substr(prev + 1, last - prev - 1);
    return inner == "pdf" || inner == "doc" || inner == "jpg";
}

bool AttachmentPolicy::allowedMime(const std::string& mimeType) {
    return startsWith(mimeType, "image/") ||
           mimeType == "application/pdf" ||
           mimeType == "text/plain";
}

PolicyResult AttachmentPolicy::evaluate(
    const std::vector<AttachmentMeta>& attachments
) {
    for (const auto& a : attachments) {

        if (isExecutable(a.filename)) {
            return {
                PolicyVerdict::Reject,
                "Executable attachment blocked"
            };
        }

        if (hasDoubleExtension(a.filename)) {
            return {
                PolicyVerdict::Reject,
                "Double-extension attachment blocked"
            };
        }

        if (a.isArchive && a.isEncrypted) {
            return {
                PolicyVerdict::Quarantine,
                "Password-protected archive"
            };
        }

        if (!allowedMime(a.mimeType)) {
            return {
                PolicyVerdict::Quarantine,
                "Disallowed MIME type"
            };
        }
    }

    return {};
}
