#pragma once

#include <string>
#include <sstream>
#include "core/server_context.h"

class ImapSession {
public:
    ImapSession(ServerContext& ctx, int clientSock);

    void run();

private:
    ServerContext& context_;
    int sock_;

    bool authed_ = false;
    std::string username_;

    void sendLine(const std::string& line);
    bool readLine(std::string& out);

    void handleCommand(const std::string& line);
    void handleLogin(const std::string& tag, const std::string& args);
    void handleCapability(const std::string& tag);
    void handleLogout(const std::string& tag);
};
