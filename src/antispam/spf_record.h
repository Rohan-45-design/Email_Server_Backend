#pragma once
#include <string>
#include <vector>
#include <optional>

enum class SpfResultCode {
    Pass, Fail, SoftFail, Neutral, None, TempError, PermError
};

enum class SpfQualifier {
    Plus, Minus, Tilde, Question
};

enum class SpfMechanismType {
    IP4, IP6, A, MX, PTR, INCLUDE, EXISTS, ALL
};

struct SpfMechanism {
    SpfQualifier qualifier;
    SpfMechanismType type;
    std::string domain;
    int cidr = -1;
};

struct SpfRecord {
    std::vector<SpfMechanism> mechanisms;
    std::optional<std::string> redirect;
    std::optional<std::string> exp;
};
