#include "sender_reputation.h"
#include "core/logger.h"

#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class SenderReputationStoreImpl {
private:
    std::string dbPath = "data/sender_reputation.json";
    mutable std::mutex mutex_;
    std::unordered_map<std::string, SenderReputation> senders_;

    void saveUnlocked() {
        json j;
        for (const auto& [domain, rep] : senders_) {
            j[domain] = {
                {"malicious", rep.maliciousCount},
                {"clean", rep.cleanCount}
            };
        }

        std::string tmp = dbPath + ".tmp";
        std::ofstream out(tmp);
        out << j.dump(2);
        out.close();
        std::filesystem::rename(tmp, dbPath);
    }

public:
    SenderReputationStoreImpl() {
        std::filesystem::create_directories("data");
        load();
    }

    void load() {
        std::lock_guard lock(mutex_);
        if (!std::filesystem::exists(dbPath)) return;

        try {
            std::ifstream in(dbPath);
            json j; in >> j;
            for (auto& [domain, data] : j.items()) {
                senders_[domain] = {
                    data["malicious"].get<int>(),
                    data["clean"].get<int>()
                };
            }
        } catch (...) {
            Logger::instance().log(LogLevel::Error, "SenderRep: Load failed");
        }
    }

    void recordMalicious(const std::string& domain) {
        {
            std::lock_guard lock(mutex_);
            senders_[domain].maliciousCount++;
        }
        std::lock_guard lock(mutex_);
        saveUnlocked();
    }

    void recordClean(const std::string& domain) {
        {
            std::lock_guard lock(mutex_);
            senders_[domain].cleanCount++;
        }
        std::lock_guard lock(mutex_);
        saveUnlocked();
    }

    int score(const std::string& domain) const {
        std::lock_guard lock(mutex_);
        auto it = senders_.find(domain);
        return it == senders_.end()
            ? 0
            : it->second.maliciousCount - it->second.cleanCount;
    }
};

static SenderReputationStoreImpl& impl() {
    static SenderReputationStoreImpl s;
    return s;
}

SenderReputationStore& SenderReputationStore::instance() {
    return reinterpret_cast<SenderReputationStore&>(impl());
}

void SenderReputationStore::recordMalicious(const std::string& domain) {
    impl().recordMalicious(domain);
}

void SenderReputationStore::recordClean(const std::string& domain) {
    impl().recordClean(domain);
}

int SenderReputationStore::score(const std::string& domain) const {
    return impl().score(domain);
}
