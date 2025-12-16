#include "imap/imap_server.h"
#include "imap/imap_session.h"
#include "core/server_context.h"
#include "core/logger.h"
#include "monitoring/metrics.h"
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

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
        thread_.join();
    }
}

void ImapServer::run() {
    Logger::instance().log(LogLevel::Info, "IMAP server starting");

    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
        Logger::instance().log(LogLevel::Error, "IMAP WSAStartup failed");
        return;
    }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        Logger::instance().log(LogLevel::Error, "IMAP socket() failed");
        WSACleanup();
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<u_short>(port_));

    if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        Logger::instance().log(LogLevel::Error, "IMAP bind() failed");
        closesocket(listenSock);
        WSACleanup();
        return;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        Logger::instance().log(LogLevel::Error, "IMAP listen() failed");
        closesocket(listenSock);
        WSACleanup();
        return;
    }

    Logger::instance().log(
        LogLevel::Info,
        "IMAP listening on port " + std::to_string(port_));

    while (running_) {
        SOCKET client = accept(listenSock, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            if (!running_) break;
            Logger::instance().log(LogLevel::Warn, "IMAP accept() failed");
            continue;
        }
        Metrics::instance().inc("imap_connections_total");
        std::thread t([this, client]() {
            ImapSession session(this->ctx_, static_cast<int>(client));
            session.run();
            closesocket(client);
        });
        t.detach();
    }

    closesocket(listenSock);
    WSACleanup();
    Logger::instance().log(LogLevel::Info, "IMAP server stopped");
}
