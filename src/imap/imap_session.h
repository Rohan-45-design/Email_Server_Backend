#pragma once

#include <string>
#include <sstream>
#include <memory>
#include "core/ssl_raii.h"
#include "core/server_context.h"

class ImapSession {
private:
    SslPtr ssl_ = nullptr;
    bool tlsActive_ = false;

public:
    ImapSession(ServerContext& ctx, int clientSock);
    ImapSession(ServerContext& ctx, int clientSock, SslPtr ssl);  // TLS overload
    ~ImapSession();
    void run();

private:
    ServerContext& context_;
    int sock_;
    
    bool authed_ = false;
    std::string username_;

    // TLS wrappers
    int secureSend(const std::string& data);
    int secureRecv(char* buf, int len);
    void sendLine(const std::string& line);
    bool readLine(std::string& out);
    
    void handleCommand(const std::string& line);
    void handleLogin(const std::string& tag, const std::string& args);
    void handleCapability(const std::string& tag);
    void handleLogout(const std::string& tag);
    void handleStarttls(const std::string& tag);
    
    static void splitImap(const std::string& in, std::string& tag,
                         std::string& cmd, std::string& args);
};
