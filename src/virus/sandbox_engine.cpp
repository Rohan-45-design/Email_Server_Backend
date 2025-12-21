#include "sandbox_engine.h"
#include "sandbox_verdict_store.h"
#include "core/logger.h"

#include "threat_intel/hash_reputation.h"
#include "threat_intel/ioc_store.h"
#include "threat_intel/intel_feedback.h"

SandboxEngine& SandboxEngine::instance() {
    static SandboxEngine e;
    return e;
}

SandboxEngine::SandboxEngine() {}

void SandboxEngine::addProvider(std::unique_ptr<SandboxProvider> p) {
    providers_.push_back(std::move(p));
}

void SandboxEngine::start() {
    if (running_) return;
    running_ = true;
    worker_ = std::thread(&SandboxEngine::workerLoop, this);
}

void SandboxEngine::stop() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        running_ = false;
    }
    cv_.notify_all();
    if (worker_.joinable())
        worker_.join();
}

void SandboxEngine::submit(
    const std::string& sha256,
    const std::string& rawData
) {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        queue_.push({ sha256, rawData });
    }
    cv_.notify_one();
}

void SandboxEngine::workerLoop() {
    Logger::instance().log(LogLevel::Info, "SandboxEngine worker started");

    while (true) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [&]() {
                return !queue_.empty() || !running_;
            });

            if (!running_ && queue_.empty())
                break;

            job = queue_.front();
            queue_.pop();
        }

        for (auto& provider : providers_) {
            auto verdict = provider->analyze(job.sha256, job.raw);

            if (verdict.verdict == SandboxVerdict::Malicious) {

                // 🧠 Phase 6 – Threat intelligence
                HashReputationStore::instance().update(
                    job.sha256,
                    ThreatVerdict::Malicious,
                    "sandbox"
                );

                IOCStore::instance().addHash(job.sha256);
                SandboxVerdictStore::instance().store(verdict);

                Logger::instance().log(
                    LogLevel::Warn,
                    "Sandbox flagged malware: " + job.sha256
                );

                break;
            }

            if (verdict.verdict != SandboxVerdict::Unknown) {
                SandboxVerdictStore::instance().store(verdict);
                break;
            }
        }
    }
}
