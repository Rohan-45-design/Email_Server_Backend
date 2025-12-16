#include "spam_engine.h"
#include "spam_rules.h"
#include "monitoring/metrics.h"

#include <sstream>

std::string SpamResult::buildStatusHeader() const {
    std::ostringstream oss;
    oss << (isSpam ? "Yes" : "No")
        << ", score=" << totalScore;
    return oss.str();
}

std::string SpamResult::buildResultHeader() const {
    std::ostringstream oss;
    for (size_t i = 0; i < tests.size(); ++i) {
        if (i) oss << ", ";
        oss << tests[i].name << "=" << tests[i].score;
    }
    return oss.str();
}

SpamResult SpamEngine::evaluate(const AuthResultsState& auth,
                                const std::string& headers,
                                const std::string& body)
{
    SpamResult r;

    applyAuthRules(r, auth);
    applyHeaderRules(r, headers);
    applyBodyRules(r, body);

    // ✅ Use engine-level threshold
    r.isSpam = (r.totalScore >= requiredScore_);

    if (r.isSpam) {
        Metrics::instance().inc("messages_spam_total");
    }

    return r;
}

void SpamEngine::setRequiredScore(double s) {
    requiredScore_ = s;
}

double SpamEngine::getRequiredScore() const {
    return requiredScore_;
}
