#include "imap/imap_server.h"
#include "imap/imap_session.h"
#include "core/tls_context.h"
#include "core/rate_limiter.h"
#include "core/server_context.h"
#include "core/logger.h"
#include <chrono>
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>

ImapServer::ImapServer(ServerContext& ctx, int port)
    : ctx_(ctx), port_(port) {}

ImapServer::~ImapServer() {
    stop();
}

void ImapServer::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&ImapServer::run, this);
}

void ImapServer::stop() {
    if (!running_) return;
    running_ = false;
    if (thread_.joinable()) {
        // closing the listening socket will interrupt accept() in run()
        if (listenSock_ != INVALID_SOCKET) {
            shutdown(listenSock_, SD_BOTH);
            closesocket(listenSock_);
            listenSock_ = INVALID_SOCKET;
        }
        thread_.join();
    }

    // Join any active session threads
    {
        std::lock_guard<std::mutex> lk(sessionsMutex_);
        for (auto &t : sessions_) {
            if (t.joinable()) t.join();
        }
        sessions_.clear();
    }
}

void ImapServer::run() {
    Logger::instance().log(LogLevel::Info, "IMAP listening on port " + std::to_string(port_));

    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
        Logger::instance().log(LogLevel::Error, "IMAP WSAStartup failed");
        return;
    }

    listenSock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock_ == INVALID_SOCKET) {
        Logger::instance().log(LogLevel::Error, "IMAP socket() failed");
        WSACleanup();
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port_));

    if (bind(listenSock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        Logger::instance().log(LogLevel::Error, "IMAP bind() failed");
        closesocket(listenSock_);
        WSACleanup();
        return;
    }

    if (listen(listenSock_, SOMAXCONN) == SOCKET_ERROR) {
        Logger::instance().log(LogLevel::Error, "IMAP listen() failed");
        closesocket(listenSock_);
        listenSock_ = INVALID_SOCKET;
        WSACleanup();
        return;
    }

    static std::atomic<int> activeConnections{0};

    while (running_) {
        if (activeConnections.load() >= ctx_.config.globalMaxConnections) {
            Logger::instance().log(LogLevel::Warn, 
                "IMAP Max connections reached: " + std::to_string(activeConnections.load()));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        SOCKET client = accept(listenSock_, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            if (!running_) break;
            Logger::instance().log(LogLevel::Warn, "IMAP accept() failed");
            continue;
        }
        
        std::string ip = "unknown";
        sockaddr_in peer{};
        int peerLen = sizeof(peer);
        if (getpeername(client, (sockaddr*)&peer, &peerLen) == 0) {
            char ipbuf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &peer.sin_addr, ipbuf, sizeof(ipbuf));
            ip = ipbuf;

            if (!RateLimiter::instance().allowConnection(ip)) {
                Logger::instance().log(
                    LogLevel::Warn,
                    "IMAP rate limit exceeded for " + ip
                );
                closesocket(client);
                continue;
            }
        }
        
        Logger::instance().inc_connections_total();

        std::thread t([this, client, ip]() {
            activeConnections++;
            
            SslPtr ssl = nullptr;
            
            if (port_ == 993) {
                SSL* raw = TlsContext::instance().createSSL(client);
                if (!raw || SSL_accept(raw) <= 0) {
                    unsigned long err = ERR_get_error();
                    char errbuf[256];
                    ERR_error_string(err, errbuf);
                    Logger::instance().log(LogLevel::Error, std::string("IMAPS handshake failed: ") + errbuf);
                    // session wasn't started; free ssl and cleanup
                    if (raw) SSL_free(raw);
                    closesocket(client);
                    activeConnections--;
                    RateLimiter::instance().releaseConnection(ip);
                    return;
                }
                ssl = make_ssl_ptr(raw);
                Logger::instance().log(LogLevel::Info, "IMAPS connection established");
            }
            
            auto session_start = std::chrono::steady_clock::now();
            ImapSession session(this->ctx_, static_cast<int>(client), std::move(ssl));
            session.run();
            auto session_end = std::chrono::steady_clock::now();
            auto duration_ms = std::chrono::duration<double, std::milli>(session_end - session_start).count();
            Logger::instance().observe_imap_session(duration_ms);
            // session owns ssl_ and will free it in its destructor
            closesocket(client);
            activeConnections--;
            RateLimiter::instance().releaseConnection(ip);
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
    Logger::instance().log(LogLevel::Info, "IMAP server stopped");
}
