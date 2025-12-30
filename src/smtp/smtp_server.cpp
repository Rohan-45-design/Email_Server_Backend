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
    : ctx_(ctx), port_(port) {}

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

        if (!RateLimiter::instance().allowConnection(ip)) {
            Logger::instance().log(LogLevel::Warn,
                "SMTP rate limit exceeded for " + ip);
            closesocket(client);
            continue;
        }
        
        Logger::instance().inc_connections_total();

        {
            std::lock_guard<std::mutex> lk(clientsMutex_);
            clientSockets_.push_back(client);
        }

        std::thread t([this, client, ip]() {
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
            }
            catch (...) {
                Logger::instance().log(LogLevel::Error, "Unhandled unknown exception in SMTP session");
            }

            // session (and its destructor) is responsible for closing socket and freeing ssl
            ConnectionManager::instance().releaseConnection(ip);
            std::lock_guard<std::mutex> lk(clientsMutex_);
            clientSockets_.erase(std::remove(clientSockets_.begin(), clientSockets_.end(), client), clientSockets_.end());
        });

        {
            std::lock_guard<std::mutex> lk(sessionsMutex_);
            sessions_.push_back(std::move(t));
        }
    }

    if (listenSock_ != INVALID_SOCKET) {
        closesocket(listenSock_);
        listenSock_ = INVALID_SOCKET;
    }
    WSACleanup();
}
