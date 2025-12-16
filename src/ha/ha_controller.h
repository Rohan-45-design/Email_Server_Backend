#pragma once
#include "ha/leader_election.h"
#include <thread>
#include <atomic>

class HaController {
public:
    HaController();
    void start();
    void stop();

private:
    void run();

    LeaderElection election_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};
