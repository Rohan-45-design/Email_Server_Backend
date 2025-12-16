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
    if (thread_.joinable())
        thread_.join();
}

void AdminServer::run(int port) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(s, (sockaddr*)&addr, sizeof(addr));
    listen(s, 5);

    Logger::instance().log(LogLevel::Info,
        "Admin API listening on port " + std::to_string(port));

    while (running_) {
        SOCKET c = accept(s, nullptr, nullptr);
        if (c == INVALID_SOCKET) continue;

        std::string response = AdminRoutes::handleRequest(c);
        send(c, response.c_str(), (int)response.size(), 0);
        closesocket(c);
    }

    closesocket(s);
    WSACleanup();
}
