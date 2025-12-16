#pragma once

#include <fstream>
#include <mutex>
#include <string>

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};

class Logger {
public:
    static Logger& instance();

    void setLevel(LogLevel level);
    void setFile(const std::string& path);

    void log(LogLevel level, const std::string& message);

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::ofstream out_;
    std::mutex mutex_;
    LogLevel level_ = LogLevel::Info;

    static const char* levelToString(LogLevel level);
};
