#include "smtp/smtp_server.h"
#include "smtp/smtp_session.h"
#include "core/server_context.h"
#include "core/logger.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include "monitoring/metrics.h"
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

    // For a simple implementation we rely on process exit to close the listen socket.
    if (thread_.joinable()) {
        thread_.join();
    }
}

void SmtpServer::run() {
    Logger::instance().log(LogLevel::Info, "SMTP server starting");

    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
        Logger::instance().log(LogLevel::Error, "WSAStartup failed");
        return;
    }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        Logger::instance().log(LogLevel::Error, "socket() failed");
        WSACleanup();
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<u_short>(port_));

    if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        Logger::instance().log(LogLevel::Error, "bind() failed");
        closesocket(listenSock);
        WSACleanup();
        return;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        Logger::instance().log(LogLevel::Error, "listen() failed");
        closesocket(listenSock);
        WSACleanup();
        return;
    }

    Logger::instance().log(LogLevel::Info,
        "SMTP listening on port " + std::to_string(port_));

    while (running_) {
        SOCKET client = accept(listenSock, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            if (!running_) break;
            Logger::instance().log(LogLevel::Warn, "accept() failed");
            continue;
        }
        Metrics::instance().inc("smtp_connections_total");
        // One connection per thread (simple, fine for your project)
        std::thread t([this, client]() {
            SmtpSession session(this->ctx_, static_cast<int>(client));
            session.run();
            closesocket(client);
        });
        t.detach();
    }

    closesocket(listenSock);
    WSACleanup();
    Logger::instance().log(LogLevel::Info, "SMTP server stopped");
}
