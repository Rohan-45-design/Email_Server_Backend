#include "virus_verdict_store.h"
#include "core/logger.h"

#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class VirusVerdictStoreImpl {
private:
    std::string dbPath = "data/virus_verdicts.json";
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
    VirusVerdictStoreImpl() {
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
                "VirusVerdict: Loaded " + std::to_string(verdicts_.size()) + " entries"
            );
        } catch (...) {
            Logger::instance().log(LogLevel::Error, "VirusVerdict: Load failed");
        }
    }

    void store(const std::string& sha256,
               const CloudScanResult& r) {
        {
            std::lock_guard lock(mutex_);
            verdicts_[sha256] =
                (r.verdict == CloudVerdict::Malicious || r.enginesDetected > 0);
        }
        std::lock_guard lock(mutex_);
        saveUnlocked();
    }

    bool get(const std::string& sha256,
             CloudScanResult& out) const {
        std::lock_guard lock(mutex_);
        auto it = verdicts_.find(sha256);
        if (it == verdicts_.end())
            return false;

        out.verdict = it->second
            ? CloudVerdict::Malicious
            : CloudVerdict::Clean;
        out.enginesDetected = it->second ? 5 : 0;
        out.enginesTotal = 70;
        out.provider = "persistent-cache";
        out.reportUrl = "cached";
        return true;
    }
};

static VirusVerdictStoreImpl& impl() {
    static VirusVerdictStoreImpl s;
    return s;
}

VirusVerdictStore& VirusVerdictStore::instance() {
    return reinterpret_cast<VirusVerdictStore&>(impl());
}

void VirusVerdictStore::store(const std::string& sha256,
                              const CloudScanResult& r) {
    impl().store(sha256, r);
}

bool VirusVerdictStore::get(const std::string& sha256,
                            CloudScanResult& out) {
    return impl().get(sha256, out);
}
