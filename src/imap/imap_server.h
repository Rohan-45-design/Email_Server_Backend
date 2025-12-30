#pragma once

#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

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

class ServerContext;

class ImapServer {
public:
    ImapServer(ServerContext& ctx, int port);
    ~ImapServer();

    void start();
    void stop();

private:
    void run();

    ServerContext& ctx_;
    int port_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    SOCKET listenSock_{INVALID_SOCKET};
    std::vector<std::thread> sessions_;
    std::mutex sessionsMutex_;
};
