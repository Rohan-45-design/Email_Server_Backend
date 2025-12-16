#include "ha/ha_controller.h"
#include "queue/retry_worker.h"
#include "core/logger.h"

#include <chrono>

HaController::HaController()
    : election_("queue/leader.lock") {}

void HaController::start() {
    running_ = true;
    thread_ = std::thread(&HaController::run, this);
}

void HaController::stop() {
    running_ = false;
    if (thread_.joinable())
        thread_.join();
}

void HaController::run() {
    RetryWorker worker;

    while (running_) {
        if (election_.tryBecomeLeader()) {
            Logger::instance().log(LogLevel::Info, "Node is LEADER");
            worker.runOnce();   // process retries
        }
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}
