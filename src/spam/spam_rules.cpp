#include "spam_rules.h"
#include <algorithm>

static void add(SpamResult& r, const std::string& name, double score) {
    r.tests.push_back({name, score});
    r.totalScore += score;
}

/* ---------- AUTHENTICATION SIGNALS ---------- */

void applyAuthRules(SpamResult& r, const AuthResultsState& auth) {
    if (auth.spf.result == SpfResult::Fail)
        add(r, "SPF_FAIL", 2.5);
    else if (auth.spf.result == SpfResult::Pass)
        add(r, "SPF_PASS", -0.5);

    if (auth.dkim.result == DkimResult::None)
        add(r, "DKIM_NONE", 1.0);
    else if (auth.dkim.result == DkimResult::Fail)
        add(r, "DKIM_FAIL", 2.0);
    else if (auth.dkim.result == DkimResult::Pass)
        add(r, "DKIM_PASS", -0.7);

    if (auth.dmarc.result == DmarcResultCode::Fail &&
        auth.dmarc.policy == DmarcPolicy::Reject)
        add(r, "DMARC_FAIL", 3.0);
}

/* ---------- HEADER HEURISTICS ---------- */

void applyHeaderRules(SpamResult& r, const std::string& headers) {
    if (headers.find("Reply-To:") != std::string::npos &&
        headers.find("From:") != std::string::npos)
        add(r, "REPLY_TO_MISMATCH", 1.0);
}

/* ---------- BODY HEURISTICS ---------- */

void applyBodyRules(SpamResult& r, const std::string& body) {
    std::string b = body;
    std::transform(b.begin(), b.end(), b.begin(), ::tolower);

    if (b.find("free money") != std::string::npos)
        add(r, "FREE_MONEY", 2.0);

    if (b.find("click here") != std::string::npos)
        add(r, "CLICK_HERE", 1.5);
}
