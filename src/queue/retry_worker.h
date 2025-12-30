#pragma once
#include <atomic>

class RetryWorker {
public:
    RetryWorker() = default;

    // Run a single retry pass (HA-safe)
    void runOnce();
};
