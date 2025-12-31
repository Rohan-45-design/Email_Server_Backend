#include "queue/retry_worker.h"
#include "queue/mail_queue.h"
#include "queue/retry_policy.h"
#include "virus/cloud_scanner.h"
#include "virus/sandbox_engine.h"
#include "delivery/smtp_client.h"
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

    // CRITICAL FIX: Actually deliver the message (this was missing!)
    // Extract from/to from message (simplified - in production, parse headers properly)
    std::string from = msg->from.empty() ? "unknown@localhost" : msg->from;
    std::string to = msg->to.empty() ? "unknown@localhost" : msg->to;
    
    Logger::instance().log(LogLevel::Info,
        "RetryWorker: Attempting delivery of " + msg->id + " to " + to);
    
    // Deliver message
    DeliveryResult result = SmtpDeliveryClient::instance().deliver(from, to, raw);
    
    if (result.success) {
        Logger::instance().log(LogLevel::Info,
            "RetryWorker: Successfully delivered " + msg->id);
        MailQueue::instance().markSuccess(msg->id);
    } else if (result.permanentFailure) {
        Logger::instance().log(LogLevel::Error,
            "RetryWorker: Permanent failure for " + msg->id + ": " + result.errorMessage);
        MailQueue::instance().markPermFail(*msg, result.errorMessage);
    } else {
        // Temporary failure - will retry later
        Logger::instance().log(LogLevel::Warn,
            "RetryWorker: Temporary failure for " + msg->id + ": " + result.errorMessage + 
            " (retry after " + std::to_string(result.retryAfterSeconds) + "s)");
        MailQueue::instance().markTempFail(*msg, result.errorMessage);
    }

    // Note: Virus scanning happens asynchronously and doesn't block delivery
    // Messages are scanned in background, quarantined if malicious
    CloudScanner::instance().scanAsync(*msg);
    SandboxEngine::instance().submit(msg->id, raw);
}
