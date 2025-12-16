#include "imap/imap_session.h"

#include "core/logger.h"
#include "core/auth_manager.h"

#include <winsock2.h>
#include <algorithm>

ImapSession::ImapSession(ServerContext& ctx, int clientSock)
    : context_(ctx), sock_(clientSock) {}

void ImapSession::run() {
    // Use domain for greeting
    sendLine("* OK " + context_.config.domain + " IMAP4rev1 Service Ready");

    std::string line;
    while (readLine(line)) {
        if (line.empty()) continue;
        handleCommand(line);
        if (sock_ == INVALID_SOCKET)
            break;
    }

    if (sock_ != INVALID_SOCKET)
        closesocket(sock_);
    sock_ = INVALID_SOCKET;
}

void ImapSession::sendLine(const std::string& line) {
    std::string out = line + "\r\n";
    send(sock_, out.c_str(), static_cast<int>(out.size()), 0);
}

bool ImapSession::readLine(std::string& out) {
    out.clear();
    char c;
    while (true) {
        int n = recv(sock_, &c, 1, 0);
        if (n <= 0)
            return false;
        if (c == '\r')
            continue;
        if (c == '\n')
            break;
        out.push_back(c);
    }
    return true;
}

static void splitImap(const std::string& in, std::string& tag,
                      std::string& cmd, std::string& args) {
    std::istringstream iss(in);
    iss >> tag >> cmd;
    std::getline(iss, args);
    if (!args.empty() && args[0] == ' ')
        args.erase(0, 1);
}

void ImapSession::handleCommand(const std::string& line) {
    std::string tag, cmd, args;
    splitImap(line, tag, cmd, args);
    std::string ucmd = cmd;
    std::transform(ucmd.begin(), ucmd.end(), ucmd.begin(), ::toupper);

    if (ucmd == "CAPABILITY") {
        handleCapability(tag);
    } else if (ucmd == "LOGIN") {
        handleLogin(tag, args);
    } else if (ucmd == "LOGOUT") {
        handleLogout(tag);
    } else {
        sendLine(tag + " BAD Unknown or unsupported command");
    }
}

void ImapSession::handleCapability(const std::string& tag) {
    sendLine("* CAPABILITY IMAP4rev1 AUTH=LOGIN");
    sendLine(tag + " OK CAPABILITY completed");
}

void ImapSession::handleLogin(const std::string& tag,
                              const std::string& args) {
    if (authed_) {
        sendLine(tag + " BAD Already authenticated");
        return;
    }

    std::istringstream iss(args);
    std::string user, pass;
    iss >> user >> pass;

    if (user.empty() || pass.empty()) {
        sendLine(tag + " BAD LOGIN requires username and password");
        return;
    }

    if (user.size() >= 2 && user.front() == '"' && user.back() == '"')
        user = user.substr(1, user.size() - 2);
    if (pass.size() >= 2 && pass.front() == '"' && pass.back() == '"')
        pass = pass.substr(1, pass.size() - 2);

    // replace authenticate() with your actual method name
    if (context_.auth.validate(user, pass)) {
        authed_ = true;
        username_ = user;
        sendLine(tag + " OK LOGIN completed");
    } else {
        sendLine(tag + " NO LOGIN failed");
    }
}

void ImapSession::handleLogout(const std::string& tag) {
    sendLine("* BYE Logging out");
    sendLine(tag + " OK LOGOUT completed");
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
}
