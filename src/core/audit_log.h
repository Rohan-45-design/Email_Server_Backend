#pragma once

#include <string>

/**
 * Security audit logging for production-grade security monitoring
 * Logs all security-relevant events for compliance and incident response
 */
class AuditLog {
public:
    static AuditLog& instance();

    // Authentication events
    void logAuthSuccess(const std::string& username);
    void logAuthFailure(const std::string& username, const std::string& reason);
    
    // Admin actions
    void logAdminAction(const std::string& admin, const std::string& action, 
                       const std::string& details = "");
    
    // Security events
    void logSecurityEvent(const std::string& event, const std::string& details);
    
    // Configuration changes
    void logConfigChange(const std::string& admin, const std::string& change);

private:
    AuditLog();
    AuditLog(const AuditLog&) = delete;
    AuditLog& operator=(const AuditLog&) = delete;
    
    void writeLog(const std::string& level, const std::string& event, 
                 const std::string& details);
    std::string getTimestamp() const;
};

