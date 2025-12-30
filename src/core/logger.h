#pragma once
#include <fstream>
#include <mutex>
#include <string>
#include <atomic>
#include <chrono>

enum class LogLevel { Debug, Info, Warn, Error };

class Logger {
public:
    static Logger& instance();
    void setLevel(LogLevel level);
    void setFile(const std::string& path);
    void log(LogLevel level, const std::string& message);

    // Metrics (Prometheus-ready)
    static void observe_smtp_session(double duration_ms);
    static void observe_virus_scan(double duration_ms);
    static void set_queue_backlog(int count);
    static void inc_connections_total();
    static void observe_imap_session(double duration_ms);
private:
    Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::ofstream out_;
    std::mutex mutex_;
    LogLevel level_ = LogLevel::Info;
    static const char* levelToString(LogLevel level);

    // Rotation state
    std::atomic<size_t> fileSize_{0};
    std::chrono::steady_clock::time_point lastRotation_;
    std::string logPath_;

    void rotateIfNeeded();
    void writeToStream(std::ostream& os, LogLevel level, const std::string& message, const std::tm& tm);
};
