#include "virus_scanner.h"
#include "core/logger.h"
#include <chrono>  // ← ADDED
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

VirusScanResult VirusScanner::scan(const std::string& raw) {
    auto scan_start = std::chrono::steady_clock::now();  // ← NEW TIMING
    
    VirusScanResult r;
    
    // CRITICAL FIX: Timeout protection for antivirus scanning
    const int SCAN_TIMEOUT_MS = 30000; // 30 seconds max
    auto timeout_point = scan_start + std::chrono::milliseconds(SCAN_TIMEOUT_MS);
    
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        r.unavailable = true;
        Logger::instance().log(LogLevel::Warn, "VirusScanner: WSAStartup failed");
        return r;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        r.unavailable = true;
        WSACleanup();
        Logger::instance().log(LogLevel::Warn, "VirusScanner: Socket creation failed");
        return r;
    }

    // Set socket timeout
    DWORD timeout = 10000; // 10 seconds
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(3310);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        r.unavailable = true;
        closesocket(sock);
        WSACleanup();
        Logger::instance().log(LogLevel::Warn, "VirusScanner: Connection to antivirus daemon failed");
        return r;
    }

    try {
        std::string cmd = "zINSTREAM\0";
        if (send(sock, cmd.c_str(), cmd.size(), 0) == SOCKET_ERROR) {
            throw std::runtime_error("Send command failed");
        }
        
        uint32_t len = htonl(static_cast<uint32_t>(raw.size()));
        if (send(sock, (char*)&len, 4, 0) == SOCKET_ERROR) {
            throw std::runtime_error("Send length failed");
        }
        if (send(sock, raw.c_str(), raw.size(), 0) == SOCKET_ERROR) {
            throw std::runtime_error("Send data failed");
        }
        
        uint32_t zero = 0;
        if (send(sock, (char*)&zero, 4, 0) == SOCKET_ERROR) {
            throw std::runtime_error("Send terminator failed");
        }

        char buf[1024];
        int received = recv(sock, buf, sizeof(buf), 0);
        if (received == SOCKET_ERROR) {
            throw std::runtime_error("Receive response failed");
        }
        
        std::string reply(buf, received);

        if (reply.find("FOUND") != std::string::npos) {
            r.infected = true;
            r.virusName = reply;
        } else if (reply.find("OK") != std::string::npos) {
            r.clean = true;
        } else {
            r.unavailable = true;
        }
    } catch (const std::exception& ex) {
        r.unavailable = true;
        Logger::instance().log(LogLevel::Warn, 
            "VirusScanner: Scan failed: " + std::string(ex.what()));
    }

    closesocket(sock);
    WSACleanup();
    
    auto scan_end = std::chrono::steady_clock::now();      // ← NEW TIMING
    auto duration_ms = std::chrono::duration<double, std::milli>(scan_end - scan_start).count();
    
    // Check for timeout
    if (std::chrono::steady_clock::now() > timeout_point) {
        Logger::instance().log(LogLevel::Warn, "VirusScanner: Scan timed out");
        r.unavailable = true;
    }
    
    Logger::instance().observe_virus_scan(duration_ms);    // ← NEW METRIC
    
    return r;
}
