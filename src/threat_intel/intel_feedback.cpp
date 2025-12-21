#include "threat_intel/intel_feedback.h"

void IntelFeedback::applyHashIntel(
    SpamResult& r,
    const std::string& sha256
) {
    HashReputation rep;
    if (!HashReputationStore::instance().lookup(sha256, rep))
        return;

    if (rep.verdict == ThreatVerdict::Malicious) {
        r.tests.push_back({ "THREAT_INTEL_HASH", 10.0 });
        r.totalScore += 10.0;
    }
    else if (rep.verdict == ThreatVerdict::Suspicious) {
        r.tests.push_back({ "THREAT_INTEL_SUSPECT", 4.0 });
        r.totalScore += 4.0;
    }
}

void IntelFeedback::applySenderIntel(
    SpamResult& r,
    const std::string& senderDomain
) {
    int score = SenderReputationStore::instance().score(senderDomain);
    if (score > 0) {
        r.tests.push_back({ "SENDER_REPUTATION", score * 2.0 });
        r.totalScore += score * 2.0;
    }
}
