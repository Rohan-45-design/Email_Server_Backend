#pragma once
#include <string>
#include <optional>
#include <vector>
#include <memory>
#include <mutex>
#include <algorithm>
#include <chrono>
struct QueueMessage {
    std::string id;
    std::string from;
    std::string to;
    std::string rawData;
    int retryCount = 0;
    std::chrono::system_clock::time_point enqueuedAt;
    std::chrono::system_clock::time_point nextRetryAt;
};

class MailQueue {
public:
    static MailQueue& instance();
    std::vector<QueueMessage> list();
    std::string enqueue(
        const std::string& from,
        const std::string& to,
        const std::string& raw
    );

    std::optional<QueueMessage> fetchReady();

    void markSuccess(const std::string& id);
    void markTempFail(const QueueMessage& msg, const std::string& reason);
    void markPermFail(const QueueMessage& msg, const std::string& reason);
    static int countReadyMessages() {
        std::lock_guard<std::mutex> lock(queueMutex_);
        return std::count_if(retryQueue_.begin(), retryQueue_.end(),
            [](const std::shared_ptr<QueueMessage>& msg) { 
                return msg->retryCount < 5; 
            });
    }

private:
    MailQueue();
    std::string genId();
    static std::mutex queueMutex_;
    static std::vector<std::shared_ptr<QueueMessage>> retryQueue_;
    static std::vector<std::shared_ptr<QueueMessage>> inflightQueue_;

    // CRITICAL FIX: Recovery of orphaned temp files
    void recoverOrphanedTempFiles();
};
