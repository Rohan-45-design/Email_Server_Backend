#include "ha/leader_election.h"
#include "core/logger.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#endif

LeaderElection::LeaderElection(const std::string& lockPath)
    : lockPath_(lockPath) {}

LeaderElection::~LeaderElection() {
    releaseLeadership();
}

bool LeaderElection::tryBecomeLeader() {
    if (leader_) return true;

#ifdef _WIN32
    HANDLE h = CreateFileA(
        lockPath_.c_str(),
        GENERIC_WRITE,
        0,              // NO sharing = exclusive
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }

    handle_ = h;
#else
    int fd = open(lockPath_.c_str(), O_CREAT | O_RDWR, 0644);
    if (fd < 0) return false;

    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        close(fd);
        return false;
    }

    fd_ = fd;
#endif

    leader_ = true;
    Logger::instance().log(LogLevel::Info, "HA: Leadership acquired");
    return true;
}

bool LeaderElection::isLeader() const {
    return leader_;
}

void LeaderElection::releaseLeadership() {
    if (!leader_) return;

#ifdef _WIN32
    CloseHandle(handle_);
    handle_ = nullptr;
#else
    flock(fd_, LOCK_UN);
    close(fd_);
    fd_ = -1;
#endif

    leader_ = false;
    Logger::instance().log(LogLevel::Info, "HA: Leadership released");
}
