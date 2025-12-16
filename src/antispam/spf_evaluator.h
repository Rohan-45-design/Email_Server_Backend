#pragma once
#include "spf_record.h"
#include <string>

class SpfEvaluator {
public:
    SpfEvaluator(const std::string& ip,
                 const std::string& sender,
                 const std::string& helo);

    SpfResultCode evaluate(const std::string& domain);

private:
    std::string ip_;
    std::string sender_;
    std::string helo_;
    int dnsCount_ = 0;
};
