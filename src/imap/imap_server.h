#pragma once

#include <thread>
#include <atomic>

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
};
