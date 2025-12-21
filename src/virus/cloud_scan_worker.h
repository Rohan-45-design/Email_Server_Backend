#pragma once
#include "queue/mail_queue.h"

class CloudScanWorker {
public:
    static CloudScanWorker& instance();

    // Phase 2: async cloud scan
    void submit(const QueueMessage& msg);

private:
    CloudScanWorker() = default;
};
