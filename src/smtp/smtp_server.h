#pragma once

#include <string>
#include <thread>
#include <vector>
#include <atomic>

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
};
