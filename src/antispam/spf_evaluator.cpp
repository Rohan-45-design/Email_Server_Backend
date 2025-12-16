#include "spf_evaluator.h"
#include "spf_parser.h"
#include "spf_macro.h"
#include "dns/dns_resolver.h"
#include <stdexcept>

static SpfResultCode mapQ(SpfQualifier q) {
    switch (q) {
        case SpfQualifier::Plus: return SpfResultCode::Pass;
        case SpfQualifier::Minus: return SpfResultCode::Fail;
        case SpfQualifier::Tilde: return SpfResultCode::SoftFail;
        case SpfQualifier::Question: return SpfResultCode::Neutral;
    }
    return SpfResultCode::Neutral;
}

SpfEvaluator::SpfEvaluator(const std::string& ip,
                           const std::string& sender,
                           const std::string& helo)
    : ip_(ip), sender_(sender), helo_(helo) {}

SpfResultCode SpfEvaluator::evaluate(const std::string& domain) {
    if (++dnsCount_ > 10)
        return SpfResultCode::PermError;

    auto txts = DnsResolver::instance().lookupTxt(domain);
    std::vector<std::string> spfs;
    for (auto& t : txts)
        if (t.find("v=spf1") == 0) spfs.push_back(t);

    if (spfs.empty()) return SpfResultCode::None;
    if (spfs.size() > 1) return SpfResultCode::PermError;

    auto record = SpfParser::parse(spfs[0]);

    for (auto& m : record.mechanisms) {
        if (m.type == SpfMechanismType::ALL)
            return mapQ(m.qualifier);
    }

    if (record.redirect)
        return evaluate(*record.redirect);

    return SpfResultCode::Neutral;
}
