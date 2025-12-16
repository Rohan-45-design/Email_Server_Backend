#pragma once
#include <string>

/* ===================== SPF ===================== */

enum class SpfResult {
    None, Pass, Fail, SoftFail, Neutral, TempError, PermError
};

struct SpfCheckResult {
    SpfResult result = SpfResult::None;
    std::string smtpMailFrom;
};

/* ===================== DKIM ===================== */

enum class DkimResult {
    None, Pass, Fail, TempError, PermError
};

struct DkimAuthResult {
    DkimResult result = DkimResult::None;
    std::string headerDomain;   // d=
};

/* ===================== DMARC ===================== */

// DMARC authentication result (RFC 7489 / 7601)
enum class DmarcResultCode {
    None, Pass, Fail
};

// DMARC enforcement policy
enum class DmarcPolicy {
    None, Quarantine, Reject
};

struct DmarcResult {
    DmarcResultCode result = DmarcResultCode::None;
    DmarcPolicy policy = DmarcPolicy::None;
};

/* ===================== AUTH RESULTS ===================== */

struct AuthResultsState {
    SpfCheckResult   spf;
    DkimAuthResult   dkim;
    DmarcResult      dmarc;

    std::string toHeaderValue(const std::string& authServId) const {
        std::string h = authServId + "; ";

        /* SPF */
        h += "spf=";
        switch (spf.result) {
        case SpfResult::Pass:      h += "pass"; break;
        case SpfResult::Fail:      h += "fail"; break;
        case SpfResult::SoftFail:  h += "softfail"; break;
        case SpfResult::Neutral:   h += "neutral"; break;
        case SpfResult::TempError: h += "temperror"; break;
        case SpfResult::PermError: h += "permerror"; break;
        case SpfResult::None:      h += "none"; break;
        }
        if (!spf.smtpMailFrom.empty())
            h += " smtp.mailfrom=" + spf.smtpMailFrom;
        h += "; ";

        /* DKIM */
        h += "dkim=";
        switch (dkim.result) {
        case DkimResult::Pass:      h += "pass"; break;
        case DkimResult::Fail:      h += "fail"; break;
        case DkimResult::TempError: h += "temperror"; break;
        case DkimResult::PermError: h += "permerror"; break;
        case DkimResult::None:      h += "none"; break;
        }
        if (!dkim.headerDomain.empty())
            h += " header.d=" + dkim.headerDomain;
        h += "; ";

        /* DMARC */
        h += "dmarc=";
        switch (dmarc.result) {
        case DmarcResultCode::Pass: h += "pass"; break;
        case DmarcResultCode::Fail: h += "fail"; break;
        case DmarcResultCode::None: h += "none"; break;
        }

        if (dmarc.policy != DmarcPolicy::None) {
            h += " policy=";
            h += (dmarc.policy == DmarcPolicy::Reject)
                   ? "reject"
                   : "quarantine";
        }

        return h;
    }
};
