#include "core/config_loader.h"
#include "core/server_context.h"
#include "core/logger.h"
#include "core/tls_context.h"
#include "core/tls_enforcement.h"
#include "smtp/smtp_server.h"
#include "imap/imap_server.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include "monitoring/http_metrics_server.h"
#include "admin/admin_server.h"
#include "admin/admin_auth.h"
#include "virus/sandbox_engine.h"
#include "virus/sandbox_provider_anyrun.h"
#include "virus/cloud_scanner.h"
#include "virus/cloud_provider_virustotal.h"

// Global flag for graceful shutdown
std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    Logger::instance().log(LogLevel::Info, "Received signal " + std::to_string(signal) + ", shutting down gracefully...");
    g_running = false;
}
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
        // Register signal handlers for graceful shutdown
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
#ifndef _WIN32
        std::signal(SIGHUP, signalHandler);
#endif

        // 1) Load configuration from YAML: config/server.yml or /app/config/server.yml (for containers)
        std::string configPath = "config/server.yml";
        const char* envConfig = std::getenv("CONFIG_PATH");
        if (envConfig) {
            configPath = envConfig;
        }
        ServerConfig cfg = ConfigLoader::loadFromFile(configPath);

        // Validate critical config
        if (cfg.domain.empty()) {
            Logger::instance().log(LogLevel::Error, "Configuration error: domain is required");
            return 1;
        }
        if (cfg.smtpPort <= 0 || cfg.imapPort <= 0) {
            Logger::instance().log(LogLevel::Error, "Configuration error: invalid ports");
            return 1;
        }

        // 2) Initialize logging from config
        Logger::instance().setFile(cfg.logFile);
        Logger::instance().setLevel(logLevelFromString(cfg.logLevel));

        // 3) Set admin token (allow env override)
        const char* envAdminToken = std::getenv("ADMIN_TOKEN");
        std::string adminToken = envAdminToken ? envAdminToken : cfg.adminToken;
        AdminAuth::setToken(adminToken);

        // 4) Initialize virus scanning providers
        CloudScanner::instance().addProvider(
            std::make_unique<VirusTotalProvider>()
        );
        SandboxEngine::instance().addProvider(
            std::make_unique<AnyRunProvider>()
        );
        SandboxEngine::instance().start();

        // 5) Build shared server context (includes MailStore)
        ServerContext ctx(cfg);

        // 6) Initialize TLS context (fail-fast)
        // Allow env override for cert/key
        const char* envCert = std::getenv("TLS_CERT");
        const char* envKey  = std::getenv("TLS_KEY");
        const char* envCertPath = std::getenv("TLS_CERT_PATH");
        const char* envKeyPath = std::getenv("TLS_KEY_PATH");
        
        std::string cert = envCert ? envCert : (envCertPath ? envCertPath : cfg.tlsCertFile);
        std::string key  = envKey  ? envKey  : (envKeyPath ? envKeyPath : cfg.tlsKeyFile);

        if (!cert.empty() && !key.empty()) {
            if (!TlsContext::instance().init(cert, key)) {
                Logger::instance().log(LogLevel::Error, "TLS initialization failed — aborting startup");
                return 2;
            }
            Logger::instance().log(LogLevel::Info, "TLS initialized successfully");
        } else {
            Logger::instance().log(LogLevel::Warn, "TLS certificate/key not provided — TLS features disabled");
        }

        // CRITICAL: Configure TLS enforcement based on config
        // This ensures AUTH requires TLS even if TLS certs are not available
        TlsEnforcement::instance().setTlsRequired(cfg.tlsRequired);
        TlsEnforcement::instance().setMinTlsVersion(cfg.minTlsVersion);
        TlsEnforcement::instance().setRequireStartTls(cfg.requireStartTls);

        // If TLS is required but not configured, fail startup
        if (cfg.tlsRequired && (cert.empty() || key.empty())) {
            Logger::instance().log(LogLevel::Error, "TLS required but certificate/key not configured — aborting startup");
            return 2;
        }

        // 7) Start monitoring and admin servers
        HttpMetricsServer metrics;
        metrics.start(9090);

        AdminServer admin;
        // Use port 8080 for admin API (matches docker-compose healthcheck)
        admin.start(8080);

        Logger::instance().log(LogLevel::Info, "Mailserver starting up");
        Logger::instance().log(
            LogLevel::Info,
            "Host=" + cfg.host +
            " SMTP=" + std::to_string(cfg.smtpPort) +
            " IMAP=" + std::to_string(cfg.imapPort)
        );

        // 8) Start SMTP and IMAP servers
        SmtpServer smtp(ctx, cfg.smtpPort);
        ImapServer imap(ctx, cfg.imapPort);
        smtp.start();
        imap.start();
        
        Logger::instance().log(LogLevel::Info, "Mailserver running. Waiting for shutdown signal...");

        // 9) Wait for shutdown signal (container-friendly)
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 10) Stop servers cleanly
        Logger::instance().log(LogLevel::Info, "Shutting down servers...");
        smtp.stop();
        imap.stop();
        admin.stop();
        metrics.stop();
        SandboxEngine::instance().stop();
        Logger::instance().log(LogLevel::Info, "Shutdown complete");
    }
    catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        Logger::instance().log(LogLevel::Error, "Fatal error: " + std::string(ex.what()));
        return 1;
    }

    return 0;
}
