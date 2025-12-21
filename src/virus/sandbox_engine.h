#pragma once
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <memory>
#include <vector>
#include <condition_variable>

#include "sandbox_provider.h"

class SandboxEngine {
public:
    static SandboxEngine& instance();

    void addProvider(std::unique_ptr<SandboxProvider> provider);

    // Submit attachment/message for detonation (async)
    void submit(const std::string& sha256,
                const std::string& rawData);

    void start();
    void stop();

private:
    SandboxEngine();
    void workerLoop();

    struct Job {
        std::string sha256;
        std::string raw;
    };

    bool running_{false};
    std::thread worker_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<Job> queue_;
    std::vector<std::unique_ptr<SandboxProvider>> providers_;
};
