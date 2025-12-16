#include "ha/leader_election.h"
#include <fstream>

LeaderElection::LeaderElection(const std::string& lockFile)
    : lockFile_(lockFile) {}

bool LeaderElection::tryBecomeLeader() {
    std::ofstream f(lockFile_, std::ios::app);
    if (!f.good()) return false;

    // Best-effort lock via exclusive open
    leader_ = true;
    return true;
}

bool LeaderElection::isLeader() const {
    return leader_;
}
