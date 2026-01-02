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
    Logger::instance().log(
        LogLevel::Info,
        "Received signal " + std::to_string(signal) + ", shutting down gracefully..."
    );
    g_running = false;
}

LogLevel logLevelFromString(const std::string& s) {
    if (s == "debug")   return LogLevel::Debug;
    if (s == "info")    return LogLevel::Info;
    if (s == "warn" || s == "warning") return LogLevel::Warn;
    if (s == "error")   return LogLevel::Error;
    return LogLevel::Info;
}

int main() {
    try {
        // Register signal handlers
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
#ifndef _WIN32
        std::signal(SIGHUP, signalHandler);
#endif

        // Parse command-line args
        std::string configPath = "config/server.yml";
        for (int i = 1; i < __argc; ++i) {
            std::string arg = __argv[i];
            if (arg == "--config" && i + 1 < __argc) {
                configPath = __argv[i + 1];
                ++i;
            }
        }

        // Env override
        const char* envConfig = std::getenv("CONFIG_PATH");
        if (envConfig) {
            configPath = envConfig;
        }

        // 1️⃣ Load + validate config (TLS POLICY IS APPLIED HERE)
        ServerConfig cfg = ConfigLoader::loadFromFile(configPath);

        // Extra safety checks
        if (cfg.domain.empty()) {
            Logger::instance().log(LogLevel::Error, "Configuration error: domain is required");
            return 1;
        }
        if (cfg.smtpPort <= 0 || cfg.imapPort <= 0) {
            Logger::instance().log(LogLevel::Error, "Configuration error: invalid ports");
            return 1;
        }

        // 2️⃣ Init logging
        Logger::instance().setFile(cfg.logFile);
        Logger::instance().setLevel(logLevelFromString(cfg.logLevel));

        // 3️⃣ Admin auth
        const char* envAdminToken = std::getenv("ADMIN_TOKEN");
        std::string adminToken = envAdminToken ? envAdminToken : cfg.adminToken;
        AdminAuth::setToken(adminToken);

        // 4️⃣ Virus scanning
        CloudScanner::instance().addProvider(
            std::make_unique<VirusTotalProvider>()
        );
        SandboxEngine::instance().addProvider(
            std::make_unique<AnyRunProvider>()
        );
        SandboxEngine::instance().start();

        // 5️⃣ Server context
        ServerContext ctx(cfg);

        // 6️⃣ TLS TRANSPORT INIT (cert/key only)
        const char* envCert = std::getenv("TLS_CERT");
        const char* envKey  = std::getenv("TLS_KEY");
        const char* envCertPath = std::getenv("TLS_CERT_PATH");
        const char* envKeyPath = std::getenv("TLS_KEY_PATH");

        std::string cert = envCert ? envCert :
                           (envCertPath ? envCertPath : cfg.tlsCertFile);
        std::string key  = envKey  ? envKey  :
                           (envKeyPath  ? envKeyPath  : cfg.tlsKeyFile);

        if (!cert.empty() && !key.empty()) {
            if (!TlsContext::instance().init(cert, key)) {
                Logger::instance().log(
                    LogLevel::Error,
                    "TLS initialization failed — aborting startup"
                );
                return 2;
            }
            Logger::instance().log(LogLevel::Info, "TLS initialized successfully");
        } else {
            Logger::instance().log(
                LogLevel::Warn,
                "TLS certificate/key not provided — TLS features disabled"
            );
        }

        // ❗ IMPORTANT
        // ❗ DO NOT reapply TLS enforcement here
        // ❗ It is already validated + applied in ConfigLoader

        if (cfg.tlsRequired && (cert.empty() || key.empty())) {
            Logger::instance().log(
                LogLevel::Error,
                "TLS required but certificate/key not configured — aborting startup"
            );
            return 2;
        }

        // 7️⃣ Metrics + admin servers
        HttpMetricsServer metrics;
        metrics.start(9090);

        AdminServer admin;
        admin.start(8080);

        Logger::instance().log(LogLevel::Info, "Mailserver starting up");
        Logger::instance().log(
            LogLevel::Info,
            "Host=" + cfg.host +
            " SMTP=" + std::to_string(cfg.smtpPort) +
            " IMAP=" + std::to_string(cfg.imapPort)
        );

        // 8️⃣ SMTP / IMAP
        SmtpServer smtp(ctx, cfg.smtpPort);
        ImapServer imap(ctx, cfg.imapPort);
        smtp.start();
        imap.start();

        Logger::instance().log(
            LogLevel::Info,
            "Mailserver running. Waiting for shutdown signal..."
        );

        // 9️⃣ Wait loop
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 🔟 Shutdown
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
        Logger::instance().log(LogLevel::Error, ex.what());
        return 1;
    }

    return 0;
}
