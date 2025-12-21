#pragma once
#include "spam/spam_engine.h"
#include "threat_intel/hash_reputation.h"
#include "threat_intel/sender_reputation.h"

class IntelFeedback {
public:
    static void applyHashIntel(SpamResult& r,
                               const std::string& sha256);

    static void applySenderIntel(SpamResult& r,
                                 const std::string& senderDomain);
};
