// ioc_store.cpp [file:140] - ✅ JSON FILE PERSISTENCE (NO config deps)
#include "ioc_store.h"
#include "core/logger.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <filesystem>

using json = nlohmann::json;

class IOCStoreImpl {
private:
    std::string dbPath = "data/ioc_store.json";
    mutable std::mutex mutex_;
    std::unordered_set<std::string> hashes_;

public:
    IOCStoreImpl() {
        // Create data folder if missing
        std::filesystem::create_directories("data");
        load();
    }

    void load() {
        std::lock_guard lock(mutex_);
        if (!std::filesystem::exists(dbPath)) return;
        
        try {
            std::ifstream file(dbPath);
            json j;
            file >> j;
            for (auto& [sha256, _] : j.items()) {
                hashes_.insert(sha256);
            }
            Logger::instance().log(LogLevel::Info, 
                "IOCStore: Loaded " + std::to_string(hashes_.size()) + " IOC hashes");
        } catch (const std::exception& e) {
            Logger::instance().log(LogLevel::Error, 
                "IOCStore: Load failed: " + std::string(e.what()));
        }
    }

    void save() {
        std::lock_guard lock(mutex_);
        try {
            json j;
            for (const auto& sha256 : hashes_) {
                j[sha256] = true;  // Simple boolean marker
            }
            std::ofstream file(dbPath);
            file << j.dump(2);
            Logger::instance().log(LogLevel::Debug, 
                "IOCStore: Saved " + std::to_string(hashes_.size()) + " IOC hashes");
        } catch (const std::exception& e) {
            Logger::instance().log(LogLevel::Error, 
                "IOCStore: Save failed: " + std::string(e.what()));
        }
    }

    void addHash(const std::string& sha256) {
        std::lock_guard lock(mutex_);
        hashes_.insert(sha256);
        save();
    }

    bool hasHash(const std::string& sha256) const {
        std::lock_guard lock(mutex_);
        return hashes_.count(sha256) != 0;
    }
};

static IOCStoreImpl& impl() {
    static IOCStoreImpl s;
    return s;
}

IOCStore& IOCStore::instance() {
    return reinterpret_cast<IOCStore&>(impl());
}

void IOCStore::addHash(const std::string& sha256) {
    impl().addHash(sha256);
}

bool IOCStore::hasHash(const std::string& sha256) const {
    return impl().hasHash(sha256);
}
