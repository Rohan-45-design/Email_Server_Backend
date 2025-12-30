#include "audit_log.h"
#include "logger.h"
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace {
    std::mutex audit_mutex;
    const std::string AUDIT_LOG_FILE = "logs/audit.log";
}

AuditLog& AuditLog::instance() {
    static AuditLog inst;
    return inst;
}

AuditLog::AuditLog() {
    // Ensure audit log directory exists
    std::filesystem::create_directories("logs");
}

std::string AuditLog::getTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void AuditLog::writeLog(const std::string& level, const std::string& event, 
                       const std::string& details) {
    std::lock_guard<std::mutex> lock(audit_mutex);
    
    std::ofstream out(AUDIT_LOG_FILE, std::ios::app);
    if (!out.is_open()) {
        Logger::instance().log(LogLevel::Error, 
            "AuditLog: Failed to open audit log file");
        return;
    }
    
    out << getTimestamp() << " [" << level << "] " << event;
    if (!details.empty()) {
        out << " | " << details;
    }
    out << std::endl;
    
    // Also log to main logger at appropriate level
    LogLevel logLevel = (level == "ERROR" || level == "SECURITY") 
        ? LogLevel::Warn : LogLevel::Info;
    Logger::instance().log(logLevel, "[AUDIT] " + event + 
        (details.empty() ? "" : " | " + details));
}

void AuditLog::logAuthSuccess(const std::string& username) {
    writeLog("INFO", "AUTH_SUCCESS", "user=" + username);
}

void AuditLog::logAuthFailure(const std::string& username, const std::string& reason) {
    writeLog("WARN", "AUTH_FAILURE", 
        "user=" + username + " reason=" + reason);
}

void AuditLog::logAdminAction(const std::string& admin, const std::string& action,
                              const std::string& details) {
    std::string msg = "admin=" + admin + " action=" + action;
    if (!details.empty()) {
        msg += " details=" + details;
    }
    writeLog("INFO", "ADMIN_ACTION", msg);
}

void AuditLog::logSecurityEvent(const std::string& event, const std::string& details) {
    writeLog("SECURITY", "SECURITY_EVENT", "event=" + event + 
        (details.empty() ? "" : " " + details));
}

void AuditLog::logConfigChange(const std::string& admin, const std::string& change) {
    writeLog("INFO", "CONFIG_CHANGE", "admin=" + admin + " change=" + change);
}

