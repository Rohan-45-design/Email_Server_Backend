#pragma once
#include <thread>
#include <atomic>

class HttpMetricsServer {
public:
    void start(int port = 9090);
    void stop();

private:
    void run(int port);
    std::atomic<bool> running_{false};
    std::thread thread_;
};
