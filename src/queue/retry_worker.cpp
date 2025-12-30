#include "queue/retry_worker.h"
#include "queue/mail_queue.h"
#include "queue/retry_policy.h"
#include "virus/cloud_scanner.h"
#include "virus/sandbox_engine.h"
#include "core/logger.h" 
#include <chrono>

void RetryWorker::runOnce() {
    int backlog = MailQueue::instance().countReadyMessages();
    Logger::instance().set_queue_backlog(backlog); 
    
    auto msg = MailQueue::instance().fetchReady();
    if (!msg) return;

    // Raw message is already loaded from inflight
    const std::string& raw = msg->rawData;
    if (raw.empty()) {
        MailQueue::instance().markTempFail(*msg, "Empty message");
        return;
    }

    // Phase 2: Cloud scanning (ASYNC)
    CloudScanner::instance().scanAsync(*msg);

    // Phase 3: Sandbox detonation (ASYNC)  
    SandboxEngine::instance().submit(msg->id, raw);

    // Delivery happens elsewhere
    MailQueue::instance().markSuccess(msg->id);
}
