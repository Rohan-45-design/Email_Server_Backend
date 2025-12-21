#include "virus/cloud_scanner.h"
#include "core/logger.h"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>

static std::string sha256(const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(
        reinterpret_cast<const unsigned char*>(data.data()),
        data.size(),
        hash
    );

    std::ostringstream oss;
    for (unsigned char c : hash)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;

    return oss.str();
}

CloudScanner& CloudScanner::instance() {
    static CloudScanner c;
    return c;
}

void CloudScanner::addProvider(std::unique_ptr<CloudProvider> p) {
    providers_.push_back(std::move(p));
}

void CloudScanner::setRetroManager(RetroManager* r) {
    retro_ = r;
}

// ✅ SIGNATURE MUST MATCH HEADER EXACTLY
void CloudScanner::scanAsync(const QueueMessage& msg) {
    std::string hash = sha256(msg.rawData);

    for (auto& p : providers_) {
        auto r = p->scan(hash, msg.rawData);

        if (r.verdict == CloudVerdict::Malicious && retro_) {
            retro_->execute({
                msg.id,
                msg.to,
                "Cloud scan: " + r.provider,
                RetroAction::Quarantine
            });

            Logger::instance().log(
                LogLevel::Warn,
                "Cloud scanner flagged message " + msg.id
            );
        }
    }
}
