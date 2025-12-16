#include "spf_checker.h"
#include "spf_evaluator.h"

static std::string strip(const std::string& s) {
    if (s.size() > 2 && s.front() == '<' && s.back() == '>')
        return s.substr(1, s.size() - 2);
    return s;
}

SpfCheckResult SpfChecker::check(const std::string& ip,
                                 const std::string& mailFrom,
                                 const std::string& heloDomain) {
    SpfCheckResult res;
    res.smtpMailFrom = strip(mailFrom);

    std::string domain;
    auto at = res.smtpMailFrom.find('@');
    domain = (at != std::string::npos)
           ? res.smtpMailFrom.substr(at + 1)
           : heloDomain;

    SpfEvaluator eval(ip, res.smtpMailFrom, heloDomain);
    auto r = eval.evaluate(domain);

    res.result = static_cast<SpfResult>(r);
    return res;
}
