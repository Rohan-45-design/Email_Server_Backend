#pragma once
#include <string>
#include <vector>
#include "antispam/auth_results.h"

struct SpamTest {
    std::string name;     // e.g. SPF_FAIL, DKIM_NONE
    double score;         // positive = spammy, negative = good
};

struct SpamResult {
    double totalScore = 0.0;
    bool isSpam = false;
    std::vector<SpamTest> tests;

    std::string buildStatusHeader() const;
    std::string buildResultHeader() const;
};

class SpamEngine {
public:
    SpamResult evaluate(const AuthResultsState& auth,
                        const std::string& headers,
                        const std::string& body);

    void setRequiredScore(double score);
    double getRequiredScore() const;

private:
    double requiredScore_ = 5.0;  
};
