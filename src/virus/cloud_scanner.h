#pragma once

#include <vector>
#include <memory>

#include "virus/cloud_provider.h"
#include "retro/retro_manager.h"
#include "queue/mail_queue.h"   // ✅ REQUIRED (defines QueueMessage)

class CloudScanner {
public:
    static CloudScanner& instance();

    void addProvider(std::unique_ptr<CloudProvider> p);
    void setRetroManager(RetroManager* r);

    // ✅ Correct API
    void scanAsync(const QueueMessage& msg);

private:
    RetroManager* retro_ = nullptr;
    std::vector<std::unique_ptr<CloudProvider>> providers_;
};
