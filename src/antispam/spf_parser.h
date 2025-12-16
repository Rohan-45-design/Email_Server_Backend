#pragma once
#include "spf_record.h"
#include <string>

class SpfParser {
public:
    static SpfRecord parse(const std::string& txt);
};
