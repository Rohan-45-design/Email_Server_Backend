#pragma once
#include <string>

class LeaderElection {
public:
    explicit LeaderElection(const std::string& lockFile);
    bool tryBecomeLeader();
    bool isLeader() const;

private:
    std::string lockFile_;
    bool leader_ = false;
};
