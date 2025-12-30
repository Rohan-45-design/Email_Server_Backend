#include "imap/imap_session.h"
#include "core/tls_context.h"
#include "core/logger.h"
#include "core/auth_manager.h"
#include "core/input_validator.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sstream>
#include <algorithm>

ImapSession::ImapSession(ServerContext& ctx, int clientSock)
    : context_(ctx), sock_(clientSock), ssl_(nullptr) {}

ImapSession::ImapSession(ServerContext& ctx, int clientSock, SslPtr ssl)
    : context_(ctx), sock_(clientSock), ssl_(std::move(ssl)) {
    if (ssl_) tlsActive_ = true;
}

ImapSession::~ImapSession() = default;

void ImapSession::run() {
    if (tlsActive_) {
        sendLine("* OK [CAPABILITY IMAP4rev1 AUTH=LOGIN] " + context_.config.domain + " IMAPS ready (TLS)");
    } else {
        sendLine("* OK [CAPABILITY IMAP4rev1 STARTTLS] " + context_.config.domain + " IMAP4rev1 Service Ready");
    }
    
    std::string line;
    while (readLine(line)) {
        if (line.empty()) continue;
        handleCommand(line);
        if (sock_ == INVALID_SOCKET) break;
    }
    
    if (sock_ != INVALID_SOCKET) closesocket(sock_);
    sock_ = INVALID_SOCKET;
}

int ImapSession::secureSend(const std::string& data) {
    if (tlsActive_ && ssl_) {
        return SSL_write(ssl_.get(), data.data(), static_cast<int>(data.size()));
    }
    return send(sock_, data.data(), static_cast<int>(data.size()), 0);
}

int ImapSession::secureRecv(char* buf, int len) {
    if (tlsActive_ && ssl_) {
        return SSL_read(ssl_.get(), buf, len);
    }
    return recv(sock_, buf, len, 0);
}

void ImapSession::sendLine(const std::string& line) {
    std::string out = line + "\r\n";
    secureSend(out);
}

bool ImapSession::readLine(std::string& out) {
    out.clear();
    char c;
    while (true) {
        int n = secureRecv(&c, 1);
        if (n <= 0) return false;
        if (c == '\r') continue;
        if (c == '\n') break;
        out.push_back(c);
    }
    return true;
}

void ImapSession::splitImap(const std::string& in, std::string& tag, std::string& cmd, std::string& args) {
    std::istringstream iss(in);
    iss >> tag >> cmd;
    std::getline(iss, args);
    if (!args.empty() && args[0] == ' ') args.erase(0, 1);
}

void ImapSession::handleCommand(const std::string& line) {
    std::string tag, cmd, args;
    splitImap(line, tag, cmd, args);
    std::string ucmd = cmd;
    std::transform(ucmd.begin(), ucmd.end(), ucmd.begin(), ::toupper);

    if (ucmd == "CAPABILITY") {
        handleCapability(tag);
    } else if (ucmd == "STARTTLS") {
        handleStarttls(tag);
    } else if (ucmd == "LOGIN") {
        if (tlsActive_) {
            handleLogin(tag, args);
        } else {
            sendLine(tag + " BAD STARTTLS required first");
        }
    } else if (ucmd == "LOGOUT") {
        handleLogout(tag);
    } else {
        sendLine(tag + " BAD Unknown or unsupported command");
    }
}

void ImapSession::handleCapability(const std::string& tag) {
    std::string caps = "* CAPABILITY IMAP4rev1";

    if (!tlsActive_) {
        caps += " STARTTLS";
    } else {
        caps += " AUTH=LOGIN";
    }
    
    sendLine(caps);
    sendLine(tag + " OK CAPABILITY completed");
}


void ImapSession::handleStarttls(const std::string& tag) {
    sendLine(tag + " OK Begin TLS negotiation");
    SSL* raw = TlsContext::instance().createSSL(sock_);
    if (!raw || SSL_accept(raw) <= 0) {
        unsigned long err = ERR_get_error();
        char errbuf[256];
        ERR_error_string(err, errbuf);
        Logger::instance().log(LogLevel::Error, std::string("IMAP STARTTLS failed: ") + errbuf);
        sendLine(tag + " NO TLS negotiation failed");
        if (raw) SSL_free(raw);
        return;
    }
    ssl_ = make_ssl_ptr(raw);
    
    tlsActive_ = true;
    Logger::instance().log(LogLevel::Info, "IMAP STARTTLS handshake completed");
    sendLine("* OK TLS active - resend CAPABILITY");
    
    // RFC 2595: Reset state after STARTTLS
    authed_ = false;
    username_.clear();
}

void ImapSession::handleLogin(const std::string& tag, const std::string& args) {
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
    
    // Validate username input
    if (!InputValidator::isValidUsername(user)) {
        sendLine(tag + " NO LOGIN failed - invalid username");
        return;
    }
        
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
