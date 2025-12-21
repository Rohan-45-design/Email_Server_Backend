#include "core/config_loader.h"
#include "core/server_context.h"
#include "core/logger.h"
#include "smtp/smtp_server.h"
#include "imap/imap_server.h"
#include <iostream>
#include <string>
#include "monitoring/http_metrics_server.h"
#include "admin/admin_server.h"
#include "admin/admin_auth.h"
#include "virus/sandbox_engine.h"
#include "virus/sandbox_provider_anyrun.h"
#include "virus/cloud_scanner.h"
#include "virus/cloud_provider_virustotal.h"
// If you have a helper to convert "info"/"debug" strings to LogLevel
LogLevel logLevelFromString(const std::string& s) {
    if (s == "debug")   return LogLevel::Debug;
    if (s == "info")    return LogLevel::Info;
    if (s == "warn" || s == "warning") return LogLevel::Warn;
    if (s == "error")   return LogLevel::Error;
    return LogLevel::Info;
}

int main() {
    try {
        // 1) Load configuration from YAML: config/server.yml
        ServerConfig cfg = ConfigLoader::loadFromFile("config/server.yml");
        AdminAuth::setToken(cfg.adminToken);

        // 🔐 ADMIN AUTH INITIALIZATION (HERE)
        AdminAuth::setToken(cfg.adminToken);

        HttpMetricsServer metrics;
        metrics.start(9090);

        AdminServer admin;
        admin.start(8081);
        // 2) Build shared server context (includes MailStore for Phase 3)
        ServerContext ctx(cfg);
        CloudScanner::instance().addProvider(
            std::make_unique<VirusTotalProvider>()
        );
        SandboxEngine::instance().start();
        if (!ctx.auth.load(cfg.usersFile)) {
            Logger::instance().log(
                LogLevel::Warn,
                "AuthManager: continuing without valid users file");
        }

        // 3) Initialize logging from config
        Logger::instance().setFile(cfg.logFile);
        Logger::instance().setLevel(logLevelFromString(cfg.logLevel));

        Logger::instance().log(LogLevel::Info, "Mailserver starting up");
        Logger::instance().log(
            LogLevel::Info,
            "Host=" + cfg.host +
            " SMTP=" + std::to_string(cfg.smtpPort) +
            " IMAP=" + std::to_string(cfg.imapPort)
        );

        // 4) Start SMTP server on configured port
        SmtpServer smtp(ctx, cfg.smtpPort);
        ImapServer imap(ctx, cfg.imapPort);
        smtp.start();
        imap.start();
        Logger::instance().log(LogLevel::Info, "Press Enter to stop server");
        std::string dummy;
        std::getline(std::cin, dummy);
        SandboxEngine::instance().addProvider(
            std::make_unique<AnyRunProvider>()
        );
        SandboxEngine::instance().start();

        // 5) Stop SMTP server cleanly
        smtp.stop();
        imap.stop();
    }
    catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
