#include "imap/flags_index.h"
#include "core/logger.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

FlagsIndex::FlagsIndex(const std::string& rootDir)
    : rootDir_(rootDir) {}

std::string FlagsIndex::mailboxFlagsPath(const std::string& user) const {
    // Simple flat file per user: mail_root/<user>/flags.txt
    return (fs::path(rootDir_) / user / "flags.txt").string();
}

void FlagsIndex::loadMailbox(const std::string& user) {
    std::lock_guard<std::mutex> lock(mutex_);

    data_[user].clear();
    std::string path = mailboxFlagsPath(user);

    std::ifstream in(path);
    if (!in) {
        return; // no flags file yet
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string msgId;
        iss >> msgId;
        if (msgId.empty()) continue;

        std::unordered_set<std::string> flags;
        std::string flag;
        while (iss >> flag) {
            flags.insert(flag);
        }

        data_[user][msgId] = std::move(flags);
    }
}

std::unordered_set<std::string> FlagsIndex::getFlags(
    const std::string& user,
    const std::string& msgId) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto uIt = data_.find(user);
    if (uIt == data_.end()) return {};
    auto mIt = uIt->second.find(msgId);
    if (mIt == uIt->second.end()) return {};
    return mIt->second;
}

void FlagsIndex::addFlag(const std::string& user,
                         const std::string& msgId,
                         const std::string& flag) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_[user][msgId].insert(flag);
}

void FlagsIndex::save(const std::string& user) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string path = mailboxFlagsPath(user);
    fs::create_directories(fs::path(path).parent_path());

    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        Logger::instance().log(
            LogLevel::Error,
            "FlagsIndex: cannot open " + path);
        return;
    }

    auto uIt = data_.find(user);
    if (uIt == data_.end()) return;

    for (const auto& [msgId, flags] : uIt->second) {
        out << msgId;
        for (const auto& f : flags) {
            out << ' ' << f;
        }
        out << '\n';
    }
}
