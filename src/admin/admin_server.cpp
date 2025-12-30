#include "admin/admin_server.h"
#include "admin/admin_routes.h"
#include "core/logger.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")

void AdminServer::start(int port) {
    running_ = true;
    thread_ = std::thread(&AdminServer::run, this, port);
}

void AdminServer::stop() {
    running_ = false;
    // Close listener to interrupt accept()
    if (listenSock_ != INVALID_SOCKET) {
        shutdown(listenSock_, SD_BOTH);
        closesocket(listenSock_);
        listenSock_ = INVALID_SOCKET;
    }
    if (thread_.joinable())
        thread_.join();
}

void AdminServer::run(int port) {
    WSADATA wsa;
    int result = WSAStartup(MAKEWORD(2,2), &wsa);
    if (result != 0) {
        Logger::instance().log(LogLevel::Error,
            "AdminServer: WSAStartup failed with error " + std::to_string(result));
        return;
    }

    listenSock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSock_ == INVALID_SOCKET) {
        int error = WSAGetLastError();
        Logger::instance().log(LogLevel::Error,
            "AdminServer: socket() failed with error " + std::to_string(error));
        WSACleanup();
        return;
    }

    // Set SO_REUSEADDR to allow port reuse
    int opt = 1;
    if (setsockopt(listenSock_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        Logger::instance().log(LogLevel::Warn,
            "AdminServer: setsockopt(SO_REUSEADDR) failed");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenSock_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        Logger::instance().log(LogLevel::Error,
            "AdminServer: bind() failed on port " + std::to_string(port) + 
            " with error " + std::to_string(error));
        closesocket(listenSock_);
        listenSock_ = INVALID_SOCKET;
        WSACleanup();
        return;
    }

    if (listen(listenSock_, 5) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        Logger::instance().log(LogLevel::Error,
            "AdminServer: listen() failed with error " + std::to_string(error));
        closesocket(listenSock_);
        listenSock_ = INVALID_SOCKET;
        WSACleanup();
        return;
    }

    Logger::instance().log(LogLevel::Info,
        "Admin API listening on port " + std::to_string(port));

    while (running_) {
        SOCKET c = accept(listenSock_, nullptr, nullptr);
        if (c == INVALID_SOCKET) {
            if (running_) { // Only log if we're still supposed to be running
                int error = WSAGetLastError();
                if (error != WSAEINTR) { // Don't log interrupt errors
                    Logger::instance().log(LogLevel::Warn,
                        "AdminServer: accept() failed with error " + std::to_string(error));
                }
            }
            continue;
        }

        try {
            std::string response = AdminRoutes::handleRequest(c);
            int sent = send(c, response.c_str(), static_cast<int>(response.size()), 0);
            if (sent == SOCKET_ERROR) {
                int error = WSAGetLastError();
                Logger::instance().log(LogLevel::Warn,
                    "AdminServer: send() failed with error " + std::to_string(error));
            }
        } catch (const std::exception& ex) {
            Logger::instance().log(LogLevel::Error,
                "AdminServer: Exception handling request: " + std::string(ex.what()));
        }
        closesocket(c);
    }

    if (listenSock_ != INVALID_SOCKET) {
        closesocket(listenSock_);
        listenSock_ = INVALID_SOCKET;
    }
    WSACleanup();
}
