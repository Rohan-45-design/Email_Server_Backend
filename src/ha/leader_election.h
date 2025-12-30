#pragma once
#include <string>

class LeaderElection {
public:
    explicit LeaderElection(const std::string& lockPath);
    ~LeaderElection();

    bool tryBecomeLeader();
    bool isLeader() const;
    void releaseLeadership();

private:
    std::string lockPath_;
    bool leader_{false};

#ifdef _WIN32
    void* handle_{nullptr};
#else
    int fd_{-1};
#endif
};
