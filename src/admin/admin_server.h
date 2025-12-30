#pragma once
#include <thread>
#include <atomic>

// Socket portability
#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
using SOCKET = int;
constexpr SOCKET INVALID_SOCKET = -1;
#endif

class AdminServer {
public:
    void start(int port = 8081);
    void stop();

private:
    void run(int port);
    std::atomic<bool> running_{false};
    std::thread thread_;
    SOCKET listenSock_{INVALID_SOCKET};
};
