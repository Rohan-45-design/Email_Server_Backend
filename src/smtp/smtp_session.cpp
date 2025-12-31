#include "smtp/smtp_session.h"
#include "core/tls_context.h"
#include "core/tls_enforcement.h"
#include "storage/mail_store.h"
#include "core/logger.h"
#include "core/auth_manager.h"
#include "monitoring/metrics.h"
#include "antivirus/virus_scanner.h"
#include "mime/mime_parser.h"
#include "policy/attachment_policy.h"
#include "core/rate_limiter.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <sstream>
#include <algorithm>
#include <vector>
#include <ctime>


SmtpSession::SmtpSession(ServerContext& ctx, int clientSock)
    : context_(ctx), sock_(clientSock), ssl_(nullptr), tlsActive_(false), 
      state_(SmtpState::CONNECTED), lastActivity_(std::chrono::steady_clock::now()) {

    // Increment active sessions metric
    Metrics::instance().inc("smtp_active_sessions");

    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (getpeername(sock_, (sockaddr*)&addr, &len) == 0) {
        char ipbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ipbuf, sizeof(ipbuf));
        peerIp_ = ipbuf;
    } else {
        peerIp_ = "unknown";
    }
}

SmtpSession::SmtpSession(ServerContext& ctx, int clientSock, SslPtr ssl)
    : context_(ctx), sock_(clientSock), ssl_(std::move(ssl)), tlsActive_(true),
      state_(SmtpState::CONNECTED), lastActivity_(std::chrono::steady_clock::now()) {

    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (getpeername(sock_, (sockaddr*)&addr, &len) == 0) {
        char ipbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ipbuf, sizeof(ipbuf));
        peerIp_ = ipbuf;
    } else {
        peerIp_ = "unknown";
    }
}

SmtpSession::~SmtpSession() {
    // Decrement active sessions metric
    Metrics::instance().inc("smtp_active_sessions", -1);
}

/* =========================
   Helpers
   ========================= */

bool SmtpSession::tlsRequiredAndMissing() const {
    return TlsEnforcement::instance().isTlsRequired() && !tlsActive_;
}

bool SmtpSession::startTlsRequiredAndMissing() const {
    return TlsEnforcement::instance().isStartTlsRequired() && !tlsActive_;
}

int SmtpSession::secureSend(const std::string& data) {
    if (tlsActive_ && ssl_) {
        return SSL_write(ssl_.get(), data.data(),
                         static_cast<int>(data.size()));
    }
    return send(sock_, data.data(),
                static_cast<int>(data.size()), 0);
}

int SmtpSession::secureRecv(char* buf, int len) {
    if (tlsActive_ && ssl_) {
        return SSL_read(ssl_.get(), buf, len);
    }
    return recv(sock_, buf, len, 0);
}

void SmtpSession::sendLine(const std::string& line) {
    try {
        secureSend(line + "\r\n");
    } catch (const std::exception& ex) {
        Logger::instance().log(LogLevel::Error,
            "SMTP sendLine failed (" + peerIp_ + "): " + ex.what());
        // Don't re-throw - session will handle disconnection
    } catch (...) {
        Logger::instance().log(LogLevel::Error,
            "SMTP sendLine failed (" + peerIp_ + "): unknown error");
    }
}

void SmtpSession::sendMultilineResponse(const std::vector<std::string>& lines) {
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i < lines.size() - 1) {
            sendLine(lines[i] + "-");  // Continuation line
        } else {
            sendLine(lines[i]);        // Final line
        }
    }
}

bool SmtpSession::readLine(std::string& out) {
    out.clear();
    char c;
    int consecutiveErrors = 0;
    
    while (true) {
        try {
            int n = secureRecv(&c, 1);
            if (n <= 0) {
                if (n < 0) {
                    consecutiveErrors++;
                    if (consecutiveErrors > 3) {
                        Logger::instance().log(LogLevel::Warn,
                            "SMTP readLine too many consecutive errors (" + peerIp_ + ")");
                        return false;
                    }
                    continue;  // Try again on transient errors
                }
                return false;  // Connection closed
            }
            consecutiveErrors = 0;  // Reset on success
        } catch (const std::exception& ex) {
            Logger::instance().log(LogLevel::Error,
                "SMTP secureRecv failed (" + peerIp_ + "): " + ex.what());
            return false;
        } catch (...) {
            Logger::instance().log(LogLevel::Error,
                "SMTP secureRecv failed (" + peerIp_ + "): unknown error");
            return false;
        }
        
        if (c == '\r') continue;
        if (c == '\n') break;
        out.push_back(c);
        
        // Prevent buffer overflow from malicious clients
        if (out.length() > 1024) {
            sendLine("500 Line too long");
            return false;
        }
    }
    return true;
}

bool SmtpSession::isTimeoutExceeded() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastActivity_);
    
    // Different timeouts for different states
    int timeoutSeconds = context_.config.smtpTimeout;  // Configurable default
    switch (state_) {
        case SmtpState::CONNECTED:
            timeoutSeconds = 60;   // 1 minute for initial connection
            break;
        case SmtpState::DATA:
            timeoutSeconds = context_.config.dataTimeout;  // Configurable DATA timeout
            break;
        default:
            timeoutSeconds = context_.config.smtpTimeout;  // Configurable default
            break;
    }
    
    return elapsed.count() > timeoutSeconds;
}

void SmtpSession::updateActivity() {
    lastActivity_ = std::chrono::steady_clock::now();
}

void SmtpSession::splitCommand(const std::string& in,
                              std::string& cmd,
                              std::string& args) {
    std::istringstream iss(in);
    iss >> cmd;
    std::getline(iss, args);
    if (!args.empty() && args[0] == ' ')
        args.erase(0, 1);
}

/* =========================
   Session Entry Point
   ========================= */

void SmtpSession::run() {
    try {
        sendLine("220 " + context_.config.domain + " ESMTP ready");

        if (!RateLimiter::instance().allowConnection(peerIp_)) {
            sendLine("421 Too many connections");
            closesocket(sock_);
            return;
        }

        std::string line;
        while (readLine(line)) {
            if (line.empty()) continue;

            if (isTimeoutExceeded()) {
                sendLine("421 Timeout - closing connection");
                break;
            }

            updateActivity();
            
            // ISOLATE each command in its own exception handler
            try {
                handleCommand(line);
            } catch (const std::exception& ex) {
                Logger::instance().log(LogLevel::Error,
                    "SMTP command crashed (" + peerIp_ + "): " + ex.what() + 
                    " [cmd: " + line.substr(0, 50) + "]");
                sendLine("451 Internal error - command failed");
                // Continue processing - don't crash the session
            } catch (...) {
                Logger::instance().log(LogLevel::Error,
                    "SMTP command crashed (" + peerIp_ + "): unknown exception [cmd: " + 
                    line.substr(0, 50) + "]");
                sendLine("451 Internal error - command failed");
                // Continue processing - don't crash the session
            }

            if (sock_ == INVALID_SOCKET)
                break;
        }
    }
    catch (const std::exception& ex) {
        Logger::instance().log(
            LogLevel::Error,
            "SMTP session crashed (" + peerIp_ + "): " + ex.what());
    }
    catch (...) {
        Logger::instance().log(
            LogLevel::Error,
            "SMTP session crashed (" + peerIp_ + "): unknown exception");
    }

    // GUARANTEED cleanup - executes even if exceptions occur above
    try {
        if (sock_ != INVALID_SOCKET)
            closesocket(sock_);
        
        RateLimiter::instance().releaseConnection(peerIp_);
    } catch (...) {
        // Last resort - log but don't crash
        Logger::instance().log(LogLevel::Error,
            "SMTP session cleanup failed (" + peerIp_ + ")");
    }
}


/* =========================
   Command Dispatch
   ========================= */

void SmtpSession::handleCommand(const std::string& line) {
    std::string cmd, args;
    splitCommand(line, cmd, args);

    std::string ucmd = cmd;
    std::transform(ucmd.begin(), ucmd.end(), ucmd.begin(), ::toupper);

    // RFC 5321 state machine enforcement
    if (ucmd == "EHLO" || ucmd == "HELO") {
        // Can be issued at any time
        if (ucmd == "EHLO") {
            handleEhlo(args);
            state_ = SmtpState::HELO_EHLO;
        } else {
            handleHelo(args);
            state_ = SmtpState::HELO_EHLO;
        }
    } else if (ucmd == "STARTTLS") {
        // Can be issued after HELO/EHLO
        if (state_ == SmtpState::CONNECTED) {
            sendLine("503 Bad sequence of commands");
            return;
        }
        handleStarttls();
    } else if (ucmd == "AUTH") {
        // Can be issued after HELO/EHLO
        if (state_ == SmtpState::CONNECTED) {
            sendLine("503 Bad sequence of commands");
            return;
        }
        if (startTlsRequiredAndMissing()) {
            sendLine("530 Must issue STARTTLS first");
            return;
        }
        handleAuth(args);
        if (authed_) {
            state_ = SmtpState::AUTH;
        }
    } else if (ucmd == "MAIL") {
        // Must be after HELO/EHLO and optionally AUTH
        if (state_ == SmtpState::CONNECTED) {
            sendLine("503 Bad sequence of commands");
            return;
        }
        if (tlsRequiredAndMissing()) {
            sendLine("530 Must issue STARTTLS first");
            return;
        }
        if (!authed_) {
            sendLine("530 Authentication required");
            return;
        }
        handleMailFrom(args);
        state_ = SmtpState::MAIL_FROM;
    } else if (ucmd == "RCPT") {
        // Must be after MAIL FROM
        if (state_ != SmtpState::MAIL_FROM && state_ != SmtpState::RCPT_TO) {
            sendLine("503 Bad sequence of commands");
            return;
        }
        if (tlsRequiredAndMissing()) {
            sendLine("530 Must issue STARTTLS first");
            return;
        }
        handleRcptTo(args);
        state_ = SmtpState::RCPT_TO;
    } else if (ucmd == "DATA") {
        // Must be after RCPT TO
        if (state_ != SmtpState::RCPT_TO) {
            sendLine("503 Bad sequence of commands");
            return;
        }
        if (tlsRequiredAndMissing()) {
            sendLine("530 Must issue STARTTLS first");
            return;
        }
        handleData();
        // Reset to MAIL_FROM state after DATA completion
        state_ = SmtpState::MAIL_FROM;
        mailFrom_.clear();
        rcptTo_.clear();
    } else if (ucmd == "QUIT") {
        handleQuit();
    } else if (ucmd == "RSET") {
        // Reset session state
        authed_ = false;
        username_.clear();
        mailFrom_.clear();
        rcptTo_.clear();
        state_ = SmtpState::HELO_EHLO;
        sendLine("250 OK");
    } else if (ucmd == "NOOP") {
        sendLine("250 OK");
    } else {
        sendLine("502 Command not implemented");
    }
}

/* =========================
   STARTTLS (HARD-ENFORCED)
   ========================= */

void SmtpSession::handleStarttls() {
    if (tlsActive_) {
        sendLine("454 TLS already active");
        return;
    }

    sendLine("220 Ready to start TLS");

    try {
        SSL* raw = TlsContext::instance().createSSL(sock_);
        if (!raw) {
            Logger::instance().log(LogLevel::Error,
                "SMTP STARTTLS: SSL creation failed (" + peerIp_ + ")");
            sendLine("454 TLS negotiation failed");
            closesocket(sock_);
            sock_ = INVALID_SOCKET;
            return;
        }

        int acceptResult = SSL_accept(raw);
        if (acceptResult <= 0) {
            int sslError = SSL_get_error(raw, acceptResult);
            Logger::instance().log(LogLevel::Error,
                "SMTP STARTTLS: SSL_accept failed (" + peerIp_ + "), error: " + std::to_string(sslError));
            sendLine("454 TLS negotiation failed");
            SSL_free(raw);
            closesocket(sock_);
            sock_ = INVALID_SOCKET;
            return;
        }

        if (!TlsEnforcement::instance().validateTlsConnection(raw)) {
            Logger::instance().log(LogLevel::Warn,
                "SMTP STARTTLS: TLS validation failed (" + peerIp_ + ")");
            Metrics::instance().inc("smtp_tls_validation_failures_total");
            sendLine("454 TLS security requirements not met");
            SSL_free(raw);
            closesocket(sock_);
            sock_ = INVALID_SOCKET;
            return;
        }

        ssl_ = make_ssl_ptr(raw);
        tlsActive_ = true;

        /* RFC 3207: Reset SMTP state */
        authed_ = false;
        username_.clear();
        heloDomain_.clear();
        mailFrom_.clear();
        rcptTo_.clear();
        
        Logger::instance().log(LogLevel::Info,
            "SMTP STARTTLS successful (" + peerIp_ + ")");
        Metrics::instance().inc("smtp_tls_handshakes_total");
            
    } catch (const std::exception& ex) {
        Logger::instance().log(LogLevel::Error,
            "SMTP STARTTLS crashed (" + peerIp_ + "): " + ex.what());
        Metrics::instance().inc("smtp_tls_handshake_errors_total");
        sendLine("454 TLS negotiation failed");
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    } catch (...) {
        Logger::instance().log(LogLevel::Error,
            "SMTP STARTTLS crashed (" + peerIp_ + "): unknown exception");
        Metrics::instance().inc("smtp_tls_handshake_errors_total");
        sendLine("454 TLS negotiation failed");
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
}

/* =========================
   EHLO / HELO
   ========================= */

void SmtpSession::handleEhlo(const std::string& arg) {
    heloDomain_ = arg;
    
    std::vector<std::string> capabilities = {
        context_.config.domain,
        "PIPELINING",
        "SIZE " + std::to_string(context_.config.maxMessageSize),
        "8BITMIME",
        "SMTPUTF8"
    };

    if (!tlsActive_)
        capabilities.push_back("STARTTLS");

    if (tlsActive_)
        capabilities.push_back("AUTH LOGIN PLAIN");

    capabilities.push_back("HELP");
    
    sendMultilineResponse(capabilities);
}

void SmtpSession::handleHelo(const std::string& arg) {
    heloDomain_ = arg;
    sendLine("250 " + context_.config.domain);
}

void SmtpSession::handleAuth(const std::string& args) {
    if (authed_) {
        sendLine("503 Already authenticated");
        return;
    }

    try {
        if (!context_.getAuthManager().authenticateSmtp(args, username_)) {
            Metrics::instance().inc("smtp_auth_failures_total");
            sendLine("535 Authentication failed");
            return;
        }

        authed_ = true;
        Metrics::instance().inc("smtp_auth_successes_total");
        sendLine("235 Authentication successful");
    } catch (const std::exception& ex) {
        Logger::instance().log(LogLevel::Error,
            "SMTP AUTH failed (" + peerIp_ + "): " + ex.what());
        Metrics::instance().inc("smtp_auth_errors_total");
        sendLine("451 Internal error - authentication failed");
    } catch (...) {
        Logger::instance().log(LogLevel::Error,
            "SMTP AUTH failed (" + peerIp_ + "): unknown error");
        Metrics::instance().inc("smtp_auth_errors_total");
        sendLine("451 Internal error - authentication failed");
    }
}

void SmtpSession::handleMailFrom(const std::string& args) {
    mailFrom_ = args;
    rcptTo_.clear();
    sendLine("250 OK");
}

void SmtpSession::handleRcptTo(const std::string& args) {
    rcptTo_.push_back(args);
    sendLine("250 OK");
}

void SmtpSession::handleData() {
    if (rcptTo_.empty()) {
        sendLine("503 No recipients specified");
        return;
    }

    sendLine("354 End data with <CR><LF>.<CR><LF>");

    std::string line;
    std::ostringstream message;
    size_t messageSize = 0;
    const size_t maxSize = context_.config.maxMessageSize;
    int consecutiveErrors = 0;

    while (readLine(line)) {
        if (line == ".")
            break;
        
        // Handle SMTP dot-stuffing (RFC 5321)
        if (!line.empty() && line[0] == '.') {
            line.erase(0, 1);  // Remove the extra dot
        }
        
        // Check size limit
        messageSize += line.length() + 2;  // +2 for \r\n
        if (messageSize > maxSize) {
            sendLine("552 Message size exceeds maximum permitted");
            return;
        }
        
        // Prevent runaway memory growth from malicious input
        if (message.str().length() > maxSize * 2) {  // Extra safety margin
            Logger::instance().log(LogLevel::Warn,
                "SMTP DATA command memory safety triggered (" + peerIp_ + ")");
            sendLine("451 Internal error - message too complex");
            return;
        }
        
        try {
            message << line << "\n";
        } catch (const std::bad_alloc&) {
            Logger::instance().log(LogLevel::Error,
                "SMTP DATA command out of memory (" + peerIp_ + ")");
            sendLine("451 Internal error - out of memory");
            return;
        } catch (const std::exception& ex) {
            Logger::instance().log(LogLevel::Error,
                "SMTP DATA command failed (" + peerIp_ + "): " + ex.what());
            sendLine("451 Internal error - data processing failed");
            consecutiveErrors++;
            if (consecutiveErrors > 3) {
                Logger::instance().log(LogLevel::Warn,
                    "SMTP DATA command too many errors, terminating (" + peerIp_ + ")");
                return;
            }
            continue;
        }
    }

    // CRITICAL FIX: Durable message storage with at-least-once delivery guarantee
    // Only acknowledge acceptance AFTER message is durably stored
    try {
        StoredMessage msg;
        msg.id = "";
        msg.from = mailFrom_;
        msg.recipients = rcptTo_;
        msg.rawData = message.str();
        msg.mailboxUser = rcptTo_[0];

        // Store message durably (atomic write + fsync + rename)
        std::string storedId = context_.mailStore.store(msg);

        if (storedId.empty()) {
            Logger::instance().log(LogLevel::Error,
                "SMTP message storage failed (" + peerIp_ + "): store returned empty ID");
            sendLine("451 Internal error - storage failed");
            return;
        }

        // CRITICAL FIX: Only acknowledge AFTER durable storage succeeds
        sendLine("250 Message accepted for delivery");

        Logger::instance().log(LogLevel::Info,
            "SMTP message durably stored (" + peerIp_ + "): ID=" + storedId +
            " from=" + mailFrom_ + " to=" + rcptTo_[0]);

        Metrics::instance().inc("smtp_messages_accepted_total");

    } catch (const std::exception& ex) {
        Logger::instance().log(LogLevel::Error,
            "SMTP message storage failed (" + peerIp_ + "): " + ex.what());
        Metrics::instance().inc("smtp_messages_rejected_total");
        sendLine("451 Internal error - storage failed");
    } catch (...) {
        Logger::instance().log(LogLevel::Error,
            "SMTP message storage failed (" + peerIp_ + "): unknown error");
        Metrics::instance().inc("smtp_messages_rejected_total");
        sendLine("451 Internal error - storage failed");
    }
}

void SmtpSession::handleQuit() {
    sendLine("221 Bye");
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
}
