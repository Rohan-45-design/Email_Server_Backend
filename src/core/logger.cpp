#include "core/logger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::setLevel(LogLevel level) {
    level_ = level;
}

void Logger::setFile(const std::string& path) {
    if (out_.is_open()) {
        out_.close();
    }
    out_.open(path, std::ios::app);
}

const char* Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < level_) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);

    std::lock_guard<std::mutex> lock(mutex_);

    std::ostream& os = out_.is_open() ? static_cast<std::ostream&>(out_) : std::cout;

    os << "[" << std::put_time(std::localtime(&tt), "%F %T")
       << "] [" << levelToString(level) << "] "
       << message << std::endl;
}
