#pragma once
#include "spam_engine.h"
#include "antispam/auth_results.h"

void applyAuthRules(SpamResult& r, const AuthResultsState& auth);
void applyHeaderRules(SpamResult& r, const std::string& headers);
void applyBodyRules(SpamResult& r, const std::string& body);
