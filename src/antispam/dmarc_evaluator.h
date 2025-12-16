#pragma once
#include <string>
#include "antispam/auth_results.h"

struct DmarcInput {
    std::string fromDomain;
    bool spfPass;
    std::string spfDomain;
    bool dkimPass;
    std::string dkimDomain;
};

class DmarcEvaluator {
public:
    DmarcResult evaluate(const DmarcInput& in);
};
