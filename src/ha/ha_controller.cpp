// ha/ha_controller.cpp
#include "ha/ha_controller.h"
#include "ha/leader_election.h"
#include "queue/retry_worker.h"
#include "core/logger.h"

#include <chrono>

using namespace std::chrono;

HaController::HaController(const std::string& dataDir)
    : dataDir_(dataDir) {

    election_ = std::make_unique<LeaderElection>(
        dataDir_ + "/queue-leader.lock");
}

HaController::~HaController() {
    stop();
}

void HaController::start() {
    if (running_) return;
    running_ = true;

    Logger::instance().log(LogLevel::Info,
        "HA Controller starting");

    thread_ = std::thread(&HaController::run, this);
}

void HaController::stop() {
    if (!running_) return;
    running_ = false;

    if (thread_.joinable())
        thread_.join();

    election_->releaseLeadership();

    Logger::instance().log(LogLevel::Info,
        "HA Controller stopped");
}

bool HaController::isLeader() const {
    return leader_;
}

void HaController::run() {
    RetryWorker worker;

    while (running_) {
        if (election_->tryBecomeLeader()) {
            if (!leader_) {
                Logger::instance().log(LogLevel::Info, "HA: Node is LEADER");
            }
            leader_ = true;
            worker.runOnce();
        } else {
            if (leader_) {
                Logger::instance().log(LogLevel::Info, "HA: Node is FOLLOWER");
            }
            leader_ = false;
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

