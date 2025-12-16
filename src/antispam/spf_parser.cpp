#include "spf_parser.h"
#include <sstream>
#include <stdexcept>

static SpfQualifier parseQualifier(char c) {
    switch (c) {
        case '+': return SpfQualifier::Plus;
        case '-': return SpfQualifier::Minus;
        case '~': return SpfQualifier::Tilde;
        case '?': return SpfQualifier::Question;
        default:  return SpfQualifier::Plus;
    }
}

SpfRecord SpfParser::parse(const std::string& txt) {
    if (txt.find("v=spf1") != 0)
        throw std::runtime_error("Invalid SPF");

    SpfRecord rec;
    std::istringstream iss(txt);
    std::string term;
    iss >> term; // skip v=spf1

    while (iss >> term) {
        if (term.rfind("redirect=", 0) == 0) {
            rec.redirect = term.substr(9);
            continue;
        }
        if (term.rfind("exp=", 0) == 0) {
            rec.exp = term.substr(4);
            continue;
        }

        SpfMechanism mech;
        mech.qualifier = parseQualifier(term[0]);
        if (term[0] == '+' || term[0] == '-' ||
            term[0] == '~' || term[0] == '?')
            term.erase(0, 1);

        auto colon = term.find(':');
        auto slash = term.find('/');

        std::string name = term.substr(0, colon);
        if (colon != std::string::npos)
            mech.domain = term.substr(colon + 1);

        if (slash != std::string::npos)
            mech.cidr = std::stoi(term.substr(slash + 1));

        if (name == "ip4") mech.type = SpfMechanismType::IP4;
        else if (name == "ip6") mech.type = SpfMechanismType::IP6;
        else if (name == "a") mech.type = SpfMechanismType::A;
        else if (name == "mx") mech.type = SpfMechanismType::MX;
        else if (name == "ptr") mech.type = SpfMechanismType::PTR;
        else if (name == "include") mech.type = SpfMechanismType::INCLUDE;
        else if (name == "exists") mech.type = SpfMechanismType::EXISTS;
        else if (name == "all") mech.type = SpfMechanismType::ALL;
        else throw std::runtime_error("Unknown mechanism");

        rec.mechanisms.push_back(mech);
    }

    return rec;
}
