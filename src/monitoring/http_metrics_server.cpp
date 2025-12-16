#include "monitoring/http_metrics_server.h"
#include "monitoring/metrics.h"
#include "monitoring/health.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")

void HttpMetricsServer::start(int port) {
    running_ = true;
    thread_ = std::thread(&HttpMetricsServer::run, this, port);
}

void HttpMetricsServer::stop() {
    running_ = false;
    if (thread_.joinable())
        thread_.join();
}

void HttpMetricsServer::run(int port) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(s, (sockaddr*)&addr, sizeof(addr));
    listen(s, 5);

    while (running_) {
        SOCKET client = accept(s, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;

        std::string response;
        auto health = Health::check();

        response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n\r\n" +
            Metrics::instance().renderPrometheus();

        send(client, response.c_str(), (int)response.size(), 0);
        closesocket(client);
    }

    closesocket(s);
    WSACleanup();
}
