# Production Integration Guide

This document shows how to integrate the new production-grade components into your main.cpp.

## Updated main.cpp Integration

Replace your current `main.cpp` with this production-ready version that uses all the new components:

```cpp
#include "core/app_lifecycle.h"
#include "core/shutdown_coordinator.h"
#include "core/crash_containment.h"
#include "core/readiness_state.h"
#include "core/tls_enforcement.h"
#include "core/connection_manager.h"
#include "core/config_loader.h"
#include "core/server_context.h"
#include "core/logger.h"
#include "core/tls_context.h"
#include "smtp/smtp_server.h"
#include "imap/imap_server.h"
#include "monitoring/http_metrics_server.h"
#include "admin/admin_server.h"
#include "admin/admin_auth.h"
#include "virus/sandbox_engine.h"
#include "virus/sandbox_provider_anyrun.h"
#include "virus/cloud_scanner.h"
#include "virus/cloud_provider_virustotal.h"
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

// Global components (will be registered with lifecycle)
static std::unique_ptr<SmtpServer> g_smtpServer;
static std::unique_ptr<ImapServer> g_imapServer;
static std::unique_ptr<HttpMetricsServer> g_metricsServer;
static std::unique_ptr<AdminServer> g_adminServer;
static std::unique_ptr<ServerContext> g_serverContext;

void signalHandler(int signal) {
    Logger::instance().log(LogLevel::Info, 
        "Received signal " + std::to_string(signal) + ", initiating graceful shutdown...");
    ShutdownCoordinator::instance().initiateShutdown();
}

LogLevel logLevelFromString(const std::string& s) {
    if (s == "debug") return LogLevel::Debug;
    if (s == "info") return LogLevel::Info;
    if (s == "warn" || s == "warning") return LogLevel::Warn;
    if (s == "error") return LogLevel::Error;
    return LogLevel::Info;
}

int main() {
    // Install crash containment handlers
    CrashContainment::instance().installTopLevelHandlers();
    
    // Set global exception handler
    CrashContainment::instance().setGlobalHandler([](const std::exception& ex) {
        Logger::instance().log(LogLevel::Error, 
            "FATAL: Unhandled exception: " + std::string(ex.what()));
        ReadinessStateMachine::instance().setState(ReadinessState::STOPPING, "Fatal error");
    });

    // Register signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
#ifndef _WIN32
    std::signal(SIGHUP, signalHandler);
#endif

    ReadinessStateMachine::instance().setState(ReadinessState::STARTING);

    try {
        // ===== PHASE 1: CONFIG =====
        AppLifecycle::Subsystem configSubsystem;
        configSubsystem.name = "Configuration";
        configSubsystem.phase = AppLifecycle::Phase::Config;
        configSubsystem.init = []() {
            std::string configPath = "config/server.yml";
            const char* envConfig = std::getenv("CONFIG_PATH");
            if (envConfig) configPath = envConfig;
            
            ServerConfig cfg = ConfigLoader::loadFromFile(configPath);
            
            // Validate critical config
            if (cfg.domain.empty()) {
                Logger::instance().log(LogLevel::Error, "Configuration error: domain is required");
                return false;
            }
            if (cfg.smtpPort <= 0 || cfg.imapPort <= 0) {
                Logger::instance().log(LogLevel::Error, "Configuration error: invalid ports");
                return false;
            }
            
            // Store config globally (simplified - in real code, use proper storage)
            // For now, we'll recreate it in each phase
            return true;
        };
        configSubsystem.shutdownOrder = 100;
        AppLifecycle::instance().registerSubsystem(configSubsystem);

        // ===== PHASE 2: LOGGING =====
        AppLifecycle::Subsystem loggingSubsystem;
        loggingSubsystem.name = "Logging";
        loggingSubsystem.phase = AppLifecycle::Phase::Logging;
        loggingSubsystem.init = []() {
            std::string configPath = "config/server.yml";
            const char* envConfig = std::getenv("CONFIG_PATH");
            if (envConfig) configPath = envConfig;
            ServerConfig cfg = ConfigLoader::loadFromFile(configPath);
            
            Logger::instance().setFile(cfg.logFile);
            Logger::instance().setLevel(logLevelFromString(cfg.logLevel));
            return true;
        };
        loggingSubsystem.shutdownOrder = 1; // Shutdown last
        AppLifecycle::instance().registerSubsystem(loggingSubsystem);

        // ===== PHASE 3: TLS =====
        AppLifecycle::Subsystem tlsSubsystem;
        tlsSubsystem.name = "TLS";
        tlsSubsystem.phase = AppLifecycle::Phase::TLS;
        tlsSubsystem.init = []() {
            std::string configPath = "config/server.yml";
            const char* envConfig = std::getenv("CONFIG_PATH");
            if (envConfig) configPath = envConfig;
            ServerConfig cfg = ConfigLoader::loadFromFile(configPath);
            
            const char* envCert = std::getenv("TLS_CERT");
            const char* envKey = std::getenv("TLS_KEY");
            const char* envCertPath = std::getenv("TLS_CERT_PATH");
            const char* envKeyPath = std::getenv("TLS_KEY_PATH");
            
            std::string cert = envCert ? envCert : (envCertPath ? envCertPath : cfg.tlsCertFile);
            std::string key = envKey ? envKey : (envKeyPath ? envKeyPath : cfg.tlsKeyFile);

            if (!cert.empty() && !key.empty()) {
                if (!TlsContext::instance().init(cert, key)) {
                    Logger::instance().log(LogLevel::Error, "TLS initialization failed");
                    return false;
                }
                // Configure TLS enforcement
                TlsEnforcement::instance().setTlsRequired(cfg.tlsRequired);
                TlsEnforcement::instance().setRequireStartTls(cfg.requireStartTls);
                TlsEnforcement::instance().setMinTlsVersion(cfg.minTlsVersion);
            } else {
                Logger::instance().log(LogLevel::Warn, "TLS certificate/key not provided");
            }
            return true;
        };
        tlsSubsystem.shutdownOrder = 90;
        AppLifecycle::instance().registerSubsystem(tlsSubsystem);

        // ===== PHASE 4: STORAGE =====
        AppLifecycle::Subsystem storageSubsystem;
        storageSubsystem.name = "Storage";
        storageSubsystem.phase = AppLifecycle::Phase::Storage;
        storageSubsystem.init = []() {
            std::string configPath = "config/server.yml";
            const char* envConfig = std::getenv("CONFIG_PATH");
            if (envConfig) configPath = envConfig;
            ServerConfig cfg = ConfigLoader::loadFromFile(configPath);
            
            g_serverContext = std::make_unique<ServerContext>(cfg);
            if (!g_serverContext->auth.load(cfg.usersFile)) {
                Logger::instance().log(LogLevel::Warn,
                    "AuthManager: continuing without valid users file");
            }
            return true;
        };
        storageSubsystem.shutdownOrder = 80;
        AppLifecycle::instance().registerSubsystem(storageSubsystem);

        // ===== PHASE 5: SERVICES =====
        AppLifecycle::Subsystem servicesSubsystem;
        servicesSubsystem.name = "Background Services";
        servicesSubsystem.phase = AppLifecycle::Phase::Services;
        servicesSubsystem.init = []() {
            CloudScanner::instance().addProvider(std::make_unique<VirusTotalProvider>());
            SandboxEngine::instance().addProvider(std::make_unique<AnyRunProvider>());
            SandboxEngine::instance().start();
            
            // Configure connection manager
            std::string configPath = "config/server.yml";
            const char* envConfig = std::getenv("CONFIG_PATH");
            if (envConfig) configPath = envConfig;
            ServerConfig cfg = ConfigLoader::loadFromFile(configPath);
            
            ConnectionManager::instance().setGlobalMaxConnections(cfg.globalMaxConnections);
            ConnectionManager::instance().setMaxConnectionsPerIP(cfg.maxConnectionsPerIP);
            
            return true;
        };
        servicesSubsystem.shutdown = []() {
            SandboxEngine::instance().stop();
        };
        servicesSubsystem.shutdownOrder = 70;
        AppLifecycle::instance().registerSubsystem(servicesSubsystem);

        // ===== PHASE 6: SERVERS =====
        AppLifecycle::Subsystem serversSubsystem;
        serversSubsystem.name = "Network Servers";
        serversSubsystem.phase = AppLifecycle::Phase::Servers;
        serversSubsystem.init = []() {
            std::string configPath = "config/server.yml";
            const char* envConfig = std::getenv("CONFIG_PATH");
            if (envConfig) configPath = envConfig;
            ServerConfig cfg = ConfigLoader::loadFromFile(configPath);
            
            // Admin token
            const char* envAdminToken = std::getenv("ADMIN_TOKEN");
            std::string adminToken = envAdminToken ? envAdminToken : cfg.adminToken;
            AdminAuth::setToken(adminToken);
            
            // Start monitoring
            g_metricsServer = std::make_unique<HttpMetricsServer>();
            g_metricsServer->start(9090);
            
            // Start admin server
            g_adminServer = std::make_unique<AdminServer>();
            g_adminServer->start(8080);
            
            // Start SMTP/IMAP servers
            g_smtpServer = std::make_unique<SmtpServer>(*g_serverContext, cfg.smtpPort);
            g_imapServer = std::make_unique<ImapServer>(*g_serverContext, cfg.imapPort);
            
            // Register shutdown hooks
            ShutdownCoordinator::ShutdownHook smtpHook;
            smtpHook.name = "SMTP Server";
            smtpHook.stopAccepting = [&]() { /* SMTP server stops accepting in stop() */ };
            smtpHook.drain = [&]() {
                // Wait for active sessions (simplified)
                std::this_thread::sleep_for(std::chrono::seconds(5));
            };
            smtpHook.shutdown = [&]() { if (g_smtpServer) g_smtpServer->stop(); };
            smtpHook.priority = 10;
            ShutdownCoordinator::instance().registerHook(smtpHook);
            
            ShutdownCoordinator::ShutdownHook imapHook;
            imapHook.name = "IMAP Server";
            imapHook.stopAccepting = [&]() { /* IMAP server stops accepting in stop() */ };
            imapHook.drain = [&]() {
                std::this_thread::sleep_for(std::chrono::seconds(5));
            };
            imapHook.shutdown = [&]() { if (g_imapServer) g_imapServer->stop(); };
            imapHook.priority = 20;
            ShutdownCoordinator::instance().registerHook(imapHook);
            
            g_smtpServer->start();
            g_imapServer->start();
            
            Logger::instance().log(LogLevel::Info, "Mailserver started");
            return true;
        };
        serversSubsystem.shutdown = []() {
            if (g_adminServer) g_adminServer->stop();
            if (g_metricsServer) g_metricsServer->stop();
        };
        serversSubsystem.shutdownOrder = 50;
        AppLifecycle::instance().registerSubsystem(serversSubsystem);

        // Initialize all subsystems (fail-fast)
        if (!AppLifecycle::instance().initialize()) {
            Logger::instance().log(LogLevel::Error, "Initialization failed - aborting");
            ReadinessStateMachine::instance().setState(ReadinessState::STOPPING, "Init failed");
            return 1;
        }

        ReadinessStateMachine::instance().setState(ReadinessState::READY);
        Logger::instance().log(LogLevel::Info, "Mailserver ready - waiting for shutdown signal...");

        // Wait for shutdown signal
        while (!ShutdownCoordinator::instance().isShuttingDown()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // ShutdownCoordinator handles graceful shutdown
        ShutdownCoordinator::instance().waitForShutdown(std::chrono::seconds(30));
        
        ReadinessStateMachine::instance().setState(ReadinessState::STOPPING);
        AppLifecycle::instance().shutdown();
        
        Logger::instance().log(LogLevel::Info, "Shutdown complete");
        return 0;

    } catch (const std::exception& ex) {
        Logger::instance().log(LogLevel::Error, 
            "Fatal error: " + std::string(ex.what()));
        ReadinessStateMachine::instance().setState(ReadinessState::STOPPING, "Fatal error");
        AppLifecycle::instance().shutdown();
        return 1;
    } catch (...) {
        Logger::instance().log(LogLevel::Error, "Fatal error: Unknown exception");
        ReadinessStateMachine::instance().setState(ReadinessState::STOPPING, "Unknown error");
        AppLifecycle::instance().shutdown();
        return 1;
    }
}
```

## Key Integration Points

1. **AppLifecycle**: All subsystems register with deterministic startup order
2. **ShutdownCoordinator**: Servers register shutdown hooks for coordinated shutdown
3. **CrashContainment**: Top-level exception handlers installed
4. **ReadinessState**: State machine tracks STARTING/READY/DEGRADED/STOPPING
5. **TlsEnforcement**: Configured from config file
6. **ConnectionManager**: Global limits configured from config

## CMakeLists.txt Updates

Add these new source files:

```cmake
add_executable(mailserver
    # ... existing files ...
    src/core/app_lifecycle.cpp
    src/core/shutdown_coordinator.cpp
    src/core/crash_containment.cpp
    src/core/tls_enforcement.cpp
    src/core/connection_manager.cpp
    src/core/readiness_state.cpp
    # ... rest of files ...
)
```

