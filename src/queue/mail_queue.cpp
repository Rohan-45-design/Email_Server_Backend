// queue/mail_queue.cpp
#include "queue/mail_queue.h"
#include "core/logger.h"

#include <filesystem>
#include <fstream>
#include <chrono>
#include <random>
std::mutex MailQueue::queueMutex_;
std::vector<std::shared_ptr<QueueMessage>> MailQueue::retryQueue_;
std::vector<std::shared_ptr<QueueMessage>> MailQueue::inflightQueue_;
namespace fs = std::filesystem;
using SysClock = std::chrono::system_clock;

static constexpr int LEASE_TIMEOUT_SEC = 300; 
MailQueue& MailQueue::instance() {
    static MailQueue q;
    return q;
}

MailQueue::MailQueue() {
    fs::create_directories("queue/active");
    fs::create_directories("queue/inflight");
    fs::create_directories("queue/failure");
    fs::create_directories("queue/permanent_fail");
}

std::string MailQueue::genId() {
    static std::mt19937_64 rng{std::random_device{}()};
    static std::uniform_int_distribution<uint64_t> dist;

    return std::to_string(
        SysClock::now().time_since_epoch().count()
    ) + "-" + std::to_string(dist(rng));
}


std::string MailQueue::enqueue(
    const std::string& from,
    const std::string& to,
    const std::string& raw
) {
    std::string id = genId();
    fs::path p = "queue/active/" + id + ".msg";

    std::ofstream out(p, std::ios::binary);
    out << "FROM: " << from << "\n";
    out << "TO: " << to << "\n";
    out << "---RAW---\n" << raw;
    out.flush();

    Logger::instance().log(
        LogLevel::Info,
        "Queue: Enqueued " + id
    );
    return id;
}

static bool isLeaseExpired(const fs::path& p) {
    auto last = fs::last_write_time(p);
    auto now = fs::file_time_type::clock::now();
    return (now - last) > std::chrono::seconds(LEASE_TIMEOUT_SEC);
}
std::vector<QueueMessage> MailQueue::list() {
    std::vector<QueueMessage> out;

    // Active messages
    for (auto& f : fs::directory_iterator("queue/active")) {
        if (f.path().extension() == ".msg") {
            QueueMessage m;
            m.id = f.path().stem().string();
            out.push_back(m);
        }
    }

    // In-flight messages
    for (auto& f : fs::directory_iterator("queue/inflight")) {
        if (f.path().extension() == ".msg") {
            QueueMessage m;
            m.id = f.path().stem().string();
            out.push_back(m);
        }
    }

    // Failed (retryable)
    for (auto& f : fs::directory_iterator("queue/failure")) {
        if (f.path().extension() == ".msg") {
            QueueMessage m;
            m.id = f.path().stem().string();
            out.push_back(m);
        }
    }

    return out;
}

std::optional<QueueMessage> MailQueue::fetchReady() {

    /* Recover expired leases */
    for (auto& f : fs::directory_iterator("queue/inflight")) {
        if (f.path().extension() == ".msg" &&
            isLeaseExpired(f.path())) {

            fs::path dst =
                "queue/active/" + f.path().filename().string();

            try {
                fs::rename(f.path(), dst);
                Logger::instance().log(
                    LogLevel::Warn,
                    "Queue: Reclaimed expired lease " +
                    dst.stem().string()
                );
            } catch (...) {}
        }
    }

    /* Lease a ready message */
    for (auto& f : fs::directory_iterator("queue/active")) {
        if (f.path().extension() != ".msg")
            continue;

        fs::path inflight =
            "queue/inflight/" + f.path().filename().string();

        try {
            fs::rename(f.path(), inflight); // ATOMIC LEASE
        } catch (...) {
            continue; // someone else got it
        }

        std::ifstream in(inflight, std::ios::binary);
        std::string raw(
            (std::istreambuf_iterator<char>(in)),
            std::istreambuf_iterator<char>()
        );

        QueueMessage m;
        m.id = inflight.stem().string();
        m.rawData = raw;

        Logger::instance().log(
            LogLevel::Debug,
            "Queue: Leased " + m.id
        );
        return m;
    }

    return std::nullopt;
}

void MailQueue::markSuccess(const std::string& id) {
    fs::path p = "queue/inflight/" + id + ".msg";
    if (fs::exists(p)) {
        fs::remove(p);
        Logger::instance().log(
            LogLevel::Info,
            "Queue: Delivered " + id
        );
    }
}

void MailQueue::markTempFail(
    const QueueMessage& msg,
    const std::string& reason
) {
    fs::path src = "queue/inflight/" + msg.id + ".msg";
    fs::path dst = "queue/failure/" + msg.id + ".msg";
    try {
        fs::rename(src, dst);
        Logger::instance().log(
            LogLevel::Warn,
            "Queue: TempFail " + msg.id + " → " + reason
        );
    } catch (...) {}
}

void MailQueue::markPermFail(
    const QueueMessage& msg,
    const std::string& reason
) {
    fs::path src = "queue/inflight/" + msg.id + ".msg";
    fs::path dst = "queue/permanent_fail/" + msg.id + ".msg";
    try {
        fs::rename(src, dst);
        Logger::instance().log(
            LogLevel::Error,
            "Queue: PermFail " + msg.id + " → " + reason
        );
    } catch (...) {}
}
