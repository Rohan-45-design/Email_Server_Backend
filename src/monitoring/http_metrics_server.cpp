#include "monitoring/http_metrics_server.h"
#include "monitoring/metrics.h"
#include "monitoring/health.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>
#include <string>
#include <algorithm>

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

        // Read HTTP request
        char buffer[4096];
        int bytesRead = recv(client, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            closesocket(client);
            continue;
        }
        buffer[bytesRead] = '\0';
        std::string request(buffer);

        std::string response;
        int statusCode = 200;
        std::string statusText = "OK";
        std::string contentType = "text/plain";

        // Parse request path
        std::string path;
        size_t pathStart = request.find(' ');
        if (pathStart != std::string::npos) {
            size_t pathEnd = request.find(' ', pathStart + 1);
            if (pathEnd != std::string::npos) {
                path = request.substr(pathStart + 1, pathEnd - pathStart - 1);
            }
        }

        if (path == "/health") {
            // /health - always 200 if process is running
            response = "OK";
        } else if (path == "/ready") {
            // /ready - check if server can accept mail
            auto health = Health::check();
            if (health.ok) {
                response = "READY";
            } else {
                statusCode = 503;
                statusText = "Service Unavailable";
                response = health.message;
            }
        } else if (path == "/metrics") {
            // /metrics - Prometheus format metrics
            contentType = "text/plain; version=0.0.4; charset=utf-8";
            response = Metrics::instance().renderPrometheus();
        } else {
            // Unknown endpoint
            statusCode = 404;
            statusText = "Not Found";
            response = "Endpoint not found";
        }

        // Build HTTP response
        std::ostringstream httpResponse;
        httpResponse << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
        httpResponse << "Content-Type: " << contentType << "\r\n";
        httpResponse << "Content-Length: " << response.size() << "\r\n";
        httpResponse << "\r\n";
        httpResponse << response;

        send(client, httpResponse.str().c_str(), (int)httpResponse.str().size(), 0);
        closesocket(client);
    }

    closesocket(s);
    WSACleanup();
}
