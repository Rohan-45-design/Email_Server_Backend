// ha/ha_controller.h
#pragma once
#include <thread>
#include <atomic>
#include <memory>
#include <string>
class LeaderElection;

class HaController {
public:
    explicit HaController(const std::string& dataDir);
    ~HaController();

    void start();
    void stop();

    bool isLeader() const;

private:
    void run();

    std::string dataDir_;
    std::atomic<bool> running_{false};
    std::atomic<bool> leader_{false};

    std::unique_ptr<LeaderElection> election_;
    std::thread thread_;
};
