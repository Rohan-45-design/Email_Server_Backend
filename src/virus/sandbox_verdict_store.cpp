#include "sandbox_verdict_store.h"
#include "core/logger.h"

#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class SandboxVerdictStoreImpl {
private:
    std::string dbPath = "data/sandbox_verdicts.json";
    mutable std::mutex mutex_;
    std::unordered_map<std::string, bool> verdicts_; // true = malicious

    void saveUnlocked() {
        json j;
        for (const auto& [sha256, malicious] : verdicts_) {
            j[sha256] = malicious;
        }

        std::string tmp = dbPath + ".tmp";
        std::ofstream out(tmp);
        out << j.dump(2);
        out.close();
        std::filesystem::rename(tmp, dbPath);
    }

public:
    SandboxVerdictStoreImpl() {
        std::filesystem::create_directories("data");
        load();
    }

    void load() {
        std::lock_guard lock(mutex_);
        if (!std::filesystem::exists(dbPath)) return;

        try {
            std::ifstream in(dbPath);
            json j; in >> j;
            for (auto& [sha256, val] : j.items()) {
                verdicts_[sha256] = val.get<bool>();
            }
            Logger::instance().log(
                LogLevel::Info,
                "SandboxVerdict: Loaded " + std::to_string(verdicts_.size()) + " entries"
            );
        } catch (...) {
            Logger::instance().log(LogLevel::Error, "SandboxVerdict: Load failed");
        }
    }

    void store(const SandboxResult& r) {
        {
            std::lock_guard lock(mutex_);
            verdicts_[r.sha256] = (r.verdict == SandboxVerdict::Malicious);
        }
        std::lock_guard lock(mutex_);
        saveUnlocked();
    }

    SandboxResult get(const std::string& sha256) const {
        std::lock_guard lock(mutex_);
        auto it = verdicts_.find(sha256);

        if (it == verdicts_.end()) {
            return { sha256, SandboxVerdict::Unknown };
        }

        return {
            sha256,
            it->second ? SandboxVerdict::Malicious : SandboxVerdict::Clean
        };
    }
};

static SandboxVerdictStoreImpl& impl() {
    static SandboxVerdictStoreImpl s;
    return s;
}

SandboxVerdictStore& SandboxVerdictStore::instance() {
    return reinterpret_cast<SandboxVerdictStore&>(impl());
}

void SandboxVerdictStore::store(const SandboxResult& r) {
    impl().store(r);
}

SandboxResult SandboxVerdictStore::get(const std::string& sha256) const {
    return impl().get(sha256);
}
