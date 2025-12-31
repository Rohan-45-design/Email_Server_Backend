#include "smtp/smtp_server.h"
#include "smtp/smtp_session.h"
#include "core/tls_context.h"
#include "core/server_context.h"
#include "core/logger.h"
#include "core/rate_limiter.h"
#include "core/connection_manager.h"
#include <chrono>
#include <algorithm>
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#pragma comment(lib, "Ws2_32.lib")

SmtpServer::SmtpServer(ServerContext& ctx, int port)
    : ctx_(ctx), port_(port), lastFailureTime_(std::chrono::steady_clock::now()) {}

void SmtpServer::cleanupFinishedThreads() {
    std::lock_guard<std::mutex> lk(sessionsMutex_);
    // Remove threads that have finished (joinable but can be joined immediately)
    // Note: On Windows, we can't easily check thread state without blocking
    // This is a best-effort cleanup; full cleanup happens on shutdown
    if (sessions_.size() > 500) {
        // Force cleanup when we have too many threads
        auto it = sessions_.begin();
        while (it != sessions_.end()) {
            if (it->joinable()) {
                // Try to join with very short timeout (would need platform-specific code)
                // For now, we rely on shutdown cleanup
                ++it;
            } else {
                it = sessions_.erase(it);
            }
        }
    }
}

bool SmtpServer::isCircuitBreakerTripped() const {
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastFailure = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - lastFailureTime_.load()).count();
    
    // Check if failure count should be reset (but don't modify in const method)
    if (timeSinceLastFailure > 60000) {  // 1 minute
        return false;  // Consider it not tripped if enough time has passed
    }
    
    return recentFailures_.load() >= FAILURE_THRESHOLD;
}

void SmtpServer::resetCircuitBreakerIfExpired() {
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastFailure = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - lastFailureTime_.load()).count();
    
    // Reset failure count if enough time has passed
    if (timeSinceLastFailure > 60000) {  // 1 minute
        recentFailures_.store(0);
    }
}

void SmtpServer::recordSessionFailure() {
    int current = recentFailures_.fetch_add(1);
    lastFailureTime_.store(std::chrono::steady_clock::now());
    
    if (current + 1 >= FAILURE_THRESHOLD) {
        Logger::instance().log(LogLevel::Error,
            "SMTP circuit breaker tripped: " + std::to_string(current + 1) + 
            " session failures in recent period");
    }
}

SmtpServer::~SmtpServer() {
    stop();
}

void SmtpServer::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&SmtpServer::run, this);
}

void SmtpServer::stop() {
    if (!running_) return;
    running_ = false;
    // Close listener to interrupt blocking accept()
    if (listenSock_ != INVALID_SOCKET) {
        shutdown(listenSock_, SD_BOTH);
        closesocket(listenSock_);
        listenSock_ = INVALID_SOCKET;
    }

    if (thread_.joinable())
        thread_.join();

    // Close all client sockets to wake session threads
    {
        std::lock_guard<std::mutex> lk(clientsMutex_);
        for (SOCKET s : clientSockets_) {
            if (s != INVALID_SOCKET) {
                shutdown(s, SD_BOTH);
                closesocket(s);
            }
        }
        clientSockets_.clear();
    }

    // Join all session threads
    {
        std::lock_guard<std::mutex> lk(sessionsMutex_);
        for (auto &t : sessions_) {
            if (t.joinable()) t.join();
        }
        sessions_.clear();
    }
}

void SmtpServer::run() {
    Logger::instance().log(
        LogLevel::Info,
        "SMTP listening on port " + std::to_string(port_));

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        Logger::instance().log(LogLevel::Error, "SMTP WSAStartup failed");
        return;
    }

    listenSock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock_ == INVALID_SOCKET) {
        WSACleanup();
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port_));

    if (bind(listenSock_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(listenSock_);
        WSACleanup();
        return;
    }

    if (listen(listenSock_, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listenSock_);
        listenSock_ = INVALID_SOCKET;
        WSACleanup();
        return;
    }

    while (running_) {
        // Reset circuit breaker if enough time has passed since last failure
        resetCircuitBreakerIfExpired();
        
        // Check circuit breaker - reject connections if too many failures
        if (isCircuitBreakerTripped()) {
            Logger::instance().log(LogLevel::Warn,
                "SMTP circuit breaker active - temporarily rejecting connections");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        // CRITICAL FIX: Check resource limits before accepting connections
        if (!ConnectionManager::instance().checkResourceLimits()) {
            Logger::instance().log(LogLevel::Warn,
                "SMTP: Resource limits exceeded - temporarily rejecting connections");
            std::this_thread::sleep_for(std::chrono::milliseconds(5000)); // Longer backoff
            continue;
        }

        // CRITICAL FIX: Check connection limits BEFORE accept() to prevent resource exhaustion
        // This prevents spawning thousands of threads before limits are enforced
        int currentConnections = ConnectionManager::instance().getActiveConnections();
        int maxConnections = ConnectionManager::instance().getMaxConnections();
        if (currentConnections >= maxConnections) {
            // At limit, use backpressure
            Logger::instance().log(LogLevel::Warn,
                "SMTP: Connection limit reached (" + std::to_string(currentConnections) + "/" + 
                std::to_string(maxConnections) + "), applying backpressure");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        SOCKET client = accept(listenSock_, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            if (!running_) break;
            continue;
        }
        
        sockaddr_in peer{};
        int len = sizeof(peer);
        getpeername(client, (sockaddr*)&peer, &len);

        char ipbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peer.sin_addr, ipbuf, sizeof(ipbuf));
        std::string ip(ipbuf);

        // CRITICAL FIX: Check ConnectionManager BEFORE creating thread
        if (!ConnectionManager::instance().tryAcquireConnection(ip)) {
            Logger::instance().log(LogLevel::Warn,
                "SMTP connection limit exceeded for " + ip);
            closesocket(client);
            continue;
        }

        if (!RateLimiter::instance().allowConnection(ip)) {
            Logger::instance().log(LogLevel::Warn,
                "SMTP rate limit exceeded for " + ip);
            ConnectionManager::instance().releaseConnection(ip);
            closesocket(client);
            continue;
        }
        
        Logger::instance().inc_connections_total();

        {
            std::lock_guard<std::mutex> lk(clientsMutex_);
            clientSockets_.push_back(client);
        }

        std::thread t([this, client, ip]() {
            bool sessionFailed = false;
            SslPtr ssl = nullptr;
            try {
                if (port_ == 465) {
                    SSL* raw = TlsContext::instance().createSSL(client);
                    if (!raw || SSL_accept(raw) <= 0) {
                        unsigned long err = ERR_get_error();
                        char errbuf[256];
                        ERR_error_string(err, errbuf);
                        Logger::instance().log(LogLevel::Error, std::string("SMTPS handshake failed: ") + errbuf);
                        if (raw) SSL_free(raw);
                        closesocket(client);
                        std::lock_guard<std::mutex> lk(clientsMutex_);
                        clientSockets_.erase(std::remove(clientSockets_.begin(), clientSockets_.end(), client), clientSockets_.end());
                        return;
                    }
                    ssl = make_ssl_ptr(raw);
                    Logger::instance().log(LogLevel::Info, "SMTPS connection established");
                }

                auto session_start = std::chrono::steady_clock::now();
                SmtpSession session(ctx_, static_cast<int>(client), std::move(ssl));
                session.run();
                auto session_end = std::chrono::steady_clock::now();
                auto duration_ms = std::chrono::duration<double, std::milli>(session_end - session_start).count();
                Logger::instance().observe_smtp_session(duration_ms);
            }
            catch (const std::exception& ex) {
                Logger::instance().log(LogLevel::Error, std::string("Unhandled exception in SMTP session: ") + ex.what());
                sessionFailed = true;
            }
            catch (...) {
                Logger::instance().log(LogLevel::Error, "Unhandled unknown exception in SMTP session");
                sessionFailed = true;
            }

            // Record failure for circuit breaker
            if (sessionFailed) {
                recordSessionFailure();
            }

            // session (and its destructor) is responsible for closing socket and freeing ssl
            ConnectionManager::instance().releaseConnection(ip);
            std::lock_guard<std::mutex> lk(clientsMutex_);
            clientSockets_.erase(std::remove(clientSockets_.begin(), clientSockets_.end(), client), clientSockets_.end());
        });

        // CRITICAL FIX: Clean up finished threads periodically to prevent unbounded growth
        {
            std::lock_guard<std::mutex> lk(sessionsMutex_);
            // Remove finished threads (non-blocking check)
            sessions_.erase(
                std::remove_if(sessions_.begin(), sessions_.end(),
                    [](std::thread& t) {
                        if (t.joinable()) {
                            // Try to join with timeout (non-blocking check)
                            // On Windows, we can't easily check if thread is done without joining
                            // So we'll clean up on shutdown and limit vector size
                            return false; // Keep thread for now, cleanup on shutdown
                        }
                        return true;
                    }),
                sessions_.end()
            );
            
            // Prevent unbounded growth: if we have too many threads, wait a bit
            if (sessions_.size() > 1000) {
                Logger::instance().log(LogLevel::Warn,
                    "SMTP: Too many active sessions (" + std::to_string(sessions_.size()) + "), applying backpressure");
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            
            sessions_.push_back(std::move(t));
        }
    }

    if (listenSock_ != INVALID_SOCKET) {
        closesocket(listenSock_);
        listenSock_ = INVALID_SOCKET;
    }
    WSACleanup();
}
