#include "hash_reputation.h"
#include "core/logger.h"

#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class HashReputationStoreImpl {
private:
    std::string dbPath = "data/hash_reputation.json";
    mutable std::mutex mutex_;
    std::unordered_map<std::string, HashReputation> map_;

    void saveUnlocked() {
        json j;
        for (const auto& [sha256, rep] : map_) {
            j[sha256] = {
                {"verdict", static_cast<int>(rep.verdict)},
                {"source", rep.source}
            };
        }

        std::string tmp = dbPath + ".tmp";
        std::ofstream out(tmp);
        out << j.dump(2);
        out.close();
        std::filesystem::rename(tmp, dbPath);
    }

public:
    HashReputationStoreImpl() {
        std::filesystem::create_directories("data");
        load();
    }

    void load() {
        std::lock_guard lock(mutex_);
        if (!std::filesystem::exists(dbPath)) return;

        try {
            std::ifstream in(dbPath);
            json j; in >> j;
            for (auto& [sha, data] : j.items()) {
                map_[sha] = {
                    static_cast<ThreatVerdict>(data["verdict"].get<int>()),
                    data["source"].get<std::string>()
                };
            }
        } catch (...) {
            Logger::instance().log(LogLevel::Error, "HashRep: Load failed");
        }
    }

    void update(const std::string& sha256,
                ThreatVerdict verdict,
                const std::string& source) {
        {
            std::lock_guard lock(mutex_);
            map_[sha256] = {verdict, source};
        }
        std::lock_guard lock(mutex_);
        saveUnlocked();
    }

    bool lookup(const std::string& sha256,
                HashReputation& out) const {
        std::lock_guard lock(mutex_);
        auto it = map_.find(sha256);
        if (it == map_.end()) return false;
        out = it->second;
        return true;
    }
};

static HashReputationStoreImpl& impl() {
    static HashReputationStoreImpl s;
    return s;
}

HashReputationStore& HashReputationStore::instance() {
    // Safe: HashReputationStore has no data members
    return reinterpret_cast<HashReputationStore&>(impl());
}

void HashReputationStore::update(const std::string& sha256,
                                 ThreatVerdict verdict,
                                 const std::string& source) {
    impl().update(sha256, verdict, source);
}

bool HashReputationStore::lookup(const std::string& sha256,
                                 HashReputation& out) const {
    return impl().lookup(sha256, out);
}
