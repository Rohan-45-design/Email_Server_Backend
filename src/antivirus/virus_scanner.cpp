#include "virus_scanner.h"
#include "core/logger.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

VirusScanResult VirusScanner::scan(const std::string& raw) {
    VirusScanResult r;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        r.unavailable = true;
        return r;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        r.unavailable = true;
        WSACleanup();
        return r;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(3310);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        r.unavailable = true;
        closesocket(sock);
        WSACleanup();
        return r;
    }

    std::string cmd = "zINSTREAM\0";
    send(sock, cmd.c_str(), cmd.size(), 0);

    uint32_t len = htonl((uint32_t)raw.size());
    send(sock, (char*)&len, 4, 0);
    send(sock, raw.c_str(), raw.size(), 0);

    uint32_t zero = 0;
    send(sock, (char*)&zero, 4, 0);

    char buf[1024]{};
    recv(sock, buf, sizeof(buf), 0);

    std::string reply(buf);

    if (reply.find("FOUND") != std::string::npos) {
        r.infected = true;
        r.virusName = reply;
    } else if (reply.find("OK") != std::string::npos) {
        r.clean = true;
    } else {
        r.unavailable = true;
    }

    closesocket(sock);
    WSACleanup();
    return r;
}
