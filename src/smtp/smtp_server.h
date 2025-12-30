#pragma once

#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>

// Socket type portability: use Winsock on Windows, POSIX sockets otherwise
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

class ServerContext; // forward declaration

class SmtpServer {
public:
    SmtpServer(ServerContext& ctx, int port);
    ~SmtpServer();

    void start();   // start listener thread
    void stop();    // stop listener and join

private:
    void run();     // listener loop

    ServerContext& ctx_;
    int port_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    // Listener socket so we can close it to interrupt accept()
    SOCKET listenSock_{INVALID_SOCKET};

    // Track active session threads and client sockets so we can shutdown cleanly
    std::vector<std::thread> sessions_;
    std::mutex sessionsMutex_;
    std::vector<SOCKET> clientSockets_;
    std::mutex clientsMutex_;
};
