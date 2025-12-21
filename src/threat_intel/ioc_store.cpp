#include "threat_intel/ioc_store.h"

IOCStore& IOCStore::instance() {
    static IOCStore s;
    return s;
}

void IOCStore::addHash(const std::string& sha256) {
    std::lock_guard<std::mutex> lock(mutex_);
    hashes_.insert(sha256);
}

bool IOCStore::hasHash(const std::string& sha256) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hashes_.count(sha256) != 0;
}
