#pragma once

#include <string>
#include <sstream>
#include <memory>
#include "core/ssl_raii.h"
#include "core/server_context.h"
#include "antispam/auth_results.h"
#include "antispam/spf_checker.h"
#include "antispam/dkim_verifier.h"
#include "antispam/dmarc_evaluator.h"

class SmtpSession {
private:
    SslPtr ssl_ = nullptr;
    bool tlsActive_ = false;

public:
    SmtpSession(ServerContext& ctx, int clientSock);
    SmtpSession(ServerContext& ctx, int clientSock, SslPtr ssl);
    ~SmtpSession();
    void run();

private:
    ServerContext& context_;
    int sock_;

    bool authed_ = false;
    std::string username_;
    std::string heloDomain_;
    std::string peerIp_;
    std::string mailFrom_;
    std::string rcptTo_;

    // NEW: per-message auth results (SPF/DKIM/DMARC)
    AuthResultsState authResults_;

    void sendLine(const std::string& line);
    bool readLine(std::string& out);

    int secureSend(const std::string& data);
    int secureRecv(char* buf, int len);

    void handleCommand(const std::string& line);

    void handleEhlo(const std::string& arg);
    void handleHelo(const std::string& arg);
    void handleStarttls();
    void handleAuth(const std::string& args);
    void handleMailFrom(const std::string& args);
    void handleRcptTo(const std::string& args);
    void handleData();
    void handleQuit();

    std::string base64Encode(const std::string& in);
    std::string base64Decode(const std::string& in);

    void splitCommand(const std::string& in, std::string& cmd, std::string& args);
};
