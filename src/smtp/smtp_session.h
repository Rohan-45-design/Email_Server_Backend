#pragma once

#include <string>
#include <sstream>
#include "core/server_context.h"
#include "antispam/auth_results.h"
#include "antispam/spf_checker.h"
#include "antispam/dkim_verifier.h"
#include "antispam/dmarc_evaluator.h"

class SmtpSession {
public:
    SmtpSession(ServerContext& ctx, int clientSock);
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

    // NEW: per‑message auth results (SPF/DKIM/DMARC)
    AuthResultsState authResults_;

    void sendLine(const std::string& line);
    bool readLine(std::string& out);

    void handleCommand(const std::string& line);

    void handleEhlo(const std::string& arg);
    void handleHelo(const std::string& arg);
    void handleAuth(const std::string& args);
    void handleMailFrom(const std::string& args);
    void handleRcptTo(const std::string& args);
    void handleData();
    void handleQuit();

    std::string base64Encode(const std::string& in);
    std::string base64Decode(const std::string& in);
};
