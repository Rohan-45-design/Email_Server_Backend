// src/antispam/dmarc.cpp
#include "antispam/dmarc_evaluator.h"
#include "dns/dns_resolver.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <random>
#include <sstream>
#include <vector>

/* ===================== Helpers ===================== */

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return s;
}

static std::vector<std::string> split(const std::string& s, char d) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, d))
        out.push_back(tok);
    return out;
}

static std::map<std::string, std::string> parseTags(const std::string& txt) {
    std::map<std::string, std::string> tags;
    for (auto& part : split(txt, ';')) {
        auto eq = part.find('=');
        if (eq == std::string::npos) continue;

        std::string k = toLower(part.substr(0, eq));
        std::string v = part.substr(eq + 1);

        k.erase(std::remove_if(k.begin(), k.end(), ::isspace), k.end());
        v.erase(std::remove_if(v.begin(), v.end(), ::isspace), v.end());

        tags[k] = v;
    }
    return tags;
}

static bool relaxedAlign(const std::string& a, const std::string& b) {
    if (a == b) return true;
    return a.size() > b.size() &&
           a.compare(a.size() - b.size(), b.size(), b) == 0;
}

static bool strictAlign(const std::string& a, const std::string& b) {
    return a == b;
}

static std::string parentDomain(const std::string& d) {
    auto pos = d.find('.');
    if (pos == std::string::npos) return "";
    return d.substr(pos + 1);
}

static bool samplePct(int pct) {
    if (pct >= 100) return true;
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> dist(1, 100);
    return dist(rng) <= pct;
}

/* ===================== DMARC ===================== */

DmarcResult DmarcEvaluator::evaluate(const DmarcInput& in) {
    DmarcResult result; // defaults to None/None

    std::string domain = toLower(in.fromDomain);
    std::vector<std::string> txts;

    /* ---- RFC 7489: organizational domain discovery ---- */
    std::string cur = domain;
    while (true) {
        std::string q = "_dmarc." + cur;
        txts = DnsResolver::instance().lookupTxt(q);
        if (!txts.empty())
            break;

        std::string parent = parentDomain(cur);
        if (parent.empty())
            break;
        cur = parent;
    }

    /* ---- No DMARC record → pass ---- */
    if (txts.empty()) {
        result.result = DmarcResultCode::Pass;
        return result;
    }

    /* ---- Multiple DMARC records → permerror ---- */
    if (txts.size() > 1) {
        result.result = DmarcResultCode::Fail;
        result.policy = DmarcPolicy::Reject;
        return result;
    }

    auto tags = parseTags(txts[0]);

    if (tags["v"] != "DMARC1") {
        result.result = DmarcResultCode::Fail;
        result.policy = DmarcPolicy::Reject;
        return result;
    }

    /* ---- Policy tags ---- */
    std::string p     = tags.count("p")     ? tags["p"]     : "none";
    std::string sp    = tags.count("sp")    ? tags["sp"]    : p;
    std::string adkim = tags.count("adkim") ? tags["adkim"] : "r";
    std::string aspf  = tags.count("aspf")  ? tags["aspf"]  : "r";
    int pct           = tags.count("pct")   ? std::stoi(tags["pct"]) : 100;

    bool isSubdomain = (cur != domain);
    std::string policyStr = isSubdomain ? sp : p;

    /* ---- Identifier alignment ---- */
    bool dkimAligned = false;
    if (in.dkimPass) {
        dkimAligned = (adkim == "s")
            ? strictAlign(in.dkimDomain, domain)
            : relaxedAlign(in.dkimDomain, domain);
    }

    bool spfAligned = false;
    if (in.spfPass) {
        spfAligned = (aspf == "s")
            ? strictAlign(in.spfDomain, domain)
            : relaxedAlign(in.spfDomain, domain);
    }

    bool authPass = dkimAligned || spfAligned;

    /* ---- DMARC pass ---- */
    if (authPass) {
        result.result = DmarcResultCode::Pass;
        return result;
    }

    /* ---- Sampling ---- */
    if (!samplePct(pct)) {
        result.result = DmarcResultCode::Pass;
        return result;
    }

    /* ---- Enforcement ---- */
    result.result = DmarcResultCode::Fail;

    if (policyStr == "reject")
        result.policy = DmarcPolicy::Reject;
    else if (policyStr == "quarantine")
        result.policy = DmarcPolicy::Quarantine;
    else
        result.policy = DmarcPolicy::None;

    return result;
}
