#include "core/logger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <filesystem>

using namespace std::chrono;
std::atomic<uint64_t> imap_total_us{0};
std::atomic<uint64_t> imap_count{0};
// =======================
// Singleton
// =======================
Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::Logger()
    : lastRotation_(steady_clock::now()) {}

// =======================
// Configuration
// =======================
void Logger::setLevel(LogLevel level) {
    level_ = level;
}
void Logger::observe_imap_session(double duration_ms) {
    imap_total_us.fetch_add(
        static_cast<uint64_t>(duration_ms * 1000),
        std::memory_order_relaxed);
    imap_count.fetch_add(1, std::memory_order_relaxed);
}
void Logger::setFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    logPath_ = path;

    if (out_.is_open())
        out_.close();

    out_.open(path, std::ios::app);
    fileSize_ = std::filesystem::exists(path)
        ? std::filesystem::file_size(path)
        : 0;
}

// =======================
// LogLevel helpers
// =======================
const char* Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        default:              return "UNKNOWN";
    }
}

// =======================
// Rotation
// =======================
void Logger::rotateIfNeeded() {
    constexpr size_t MAX_SIZE = 100 * 1024 * 1024; // 100 MB

    if (!out_.is_open() || fileSize_.load() < MAX_SIZE)
        return;

    out_.close();

    std::filesystem::path base(logPath_);

    // Rotate: log.5 -> log.6, log.4 -> log.5, ..., log -> log.1
    for (int i = 5; i >= 1; --i) {
        std::filesystem::path old = base.string() + ".log." + std::to_string(i);
        std::filesystem::path next = base.string() + ".log." + std::to_string(i + 1);

        if (std::filesystem::exists(old)) {
            std::filesystem::rename(old, next);
        }
    }

    if (std::filesystem::exists(base)) {
        std::filesystem::rename(base, base.string() + ".log.1");
    }

    out_.open(logPath_, std::ios::app);
    fileSize_.store(0);
    lastRotation_ = steady_clock::now();
}

// =======================
// Logging
// =======================
void Logger::log(LogLevel level, const std::string& message) {
    if (level < level_)
        return;
    std::lock_guard<std::mutex> log_lock(mutex_);
    rotateIfNeeded();

    auto now = system_clock::now();
    std::time_t tt = system_clock::to_time_t(now);

    std::tm tm{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif

    std::ostream& os = out_.is_open()
        ? static_cast<std::ostream&>(out_)
        : std::cout;

    writeToStream(os, level, message, tm);

    fileSize_.fetch_add(message.size() + 64, std::memory_order_relaxed);
}

void Logger::writeToStream(
    std::ostream& os,
    LogLevel level,
    const std::string& message,
    const std::tm& tm
) {
    os << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
       << " [" << levelToString(level) << "] "
       << message << std::endl;
}

// =======================
// Metrics (lock-free)
// =======================
namespace {
    std::atomic<uint64_t> smtp_total_us{0};
    std::atomic<uint64_t> smtp_count{0};
    std::atomic<uint64_t> virus_total_us{0};
    std::atomic<uint64_t> virus_count{0};
    std::atomic<int> queue_backlog{0};
    std::atomic<uint64_t> connections_total{0};
}

void Logger::observe_smtp_session(double duration_ms) {
    smtp_total_us.fetch_add(
        static_cast<uint64_t>(duration_ms * 1000),
        std::memory_order_relaxed);
    smtp_count.fetch_add(1, std::memory_order_relaxed);
}

void Logger::observe_virus_scan(double duration_ms) {
    virus_total_us.fetch_add(
        static_cast<uint64_t>(duration_ms * 1000),
        std::memory_order_relaxed);
    virus_count.fetch_add(1, std::memory_order_relaxed);
}

void Logger::set_queue_backlog(int count) {
    queue_backlog.store(count, std::memory_order_relaxed);
}

void Logger::inc_connections_total() {
    connections_total.fetch_add(1, std::memory_order_relaxed);
}
