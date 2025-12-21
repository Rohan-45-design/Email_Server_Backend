#include "smtp/smtp_session.h"
#include "storage/mail_store.h"

#include "core/logger.h"
#include "core/auth_manager.h"
#include "monitoring/metrics.h"
#include "antivirus/virus_scanner.h"
#include "mime/mime_parser.h"
#include "policy/attachment_policy.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <algorithm>

SmtpSession::SmtpSession(ServerContext& ctx, int clientSock)
    : context_(ctx), sock_(clientSock) {}

void SmtpSession::run() {
    sendLine("220 " + context_.config.domain + " Simple SMTP ready");

    std::string line;
    while (readLine(line)) {
        if (line.empty())
            continue;
        handleCommand(line);
        if (sock_ == INVALID_SOCKET)
            break;
    }

    if (sock_ != INVALID_SOCKET)
        closesocket(sock_);
    sock_ = INVALID_SOCKET;
}

void SmtpSession::sendLine(const std::string& line) {
    std::string out = line + "\r\n";
    send(sock_, out.c_str(), static_cast<int>(out.size()), 0);
}

bool SmtpSession::readLine(std::string& out) {
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

static void splitCommand(const std::string& in, std::string& cmd, std::string& args) {
    std::istringstream iss(in);
    iss >> cmd;
    std::getline(iss, args);
    if (!args.empty() && args[0] == ' ')
        args.erase(0, 1);
}

void SmtpSession::handleCommand(const std::string& line) {
    std::string cmd, args;
    splitCommand(line, cmd, args);
    std::string ucmd = cmd;
    std::transform(ucmd.begin(), ucmd.end(), ucmd.begin(), ::toupper);

    if (ucmd == "EHLO") {
        handleEhlo(args);
    } else if (ucmd == "HELO") {
        handleHelo(args);
    } else if (ucmd == "AUTH") {
        handleAuth(args);
    } else if (ucmd == "MAIL") {
        handleMailFrom(args);
    } else if (ucmd == "RCPT") {
        handleRcptTo(args);
    } else if (ucmd == "DATA") {
        handleData();
    } else if (ucmd == "QUIT") {
        handleQuit();
    } else {
        sendLine("502 Command not implemented");
    }
}

void SmtpSession::handleEhlo(const std::string& arg) {
    heloDomain_ = arg;
    sendLine("250-" + context_.config.domain + " Hello " + heloDomain_);
    sendLine("250-AUTH LOGIN PLAIN");
    sendLine("250-PIPELINING");
    sendLine("250-SIZE 10485760");
    sendLine("250 HELP");
}

void SmtpSession::handleHelo(const std::string& arg) {
    heloDomain_ = arg;
    sendLine("250 " + context_.config.domain + " Hello " + heloDomain_);
}

void SmtpSession::handleAuth(const std::string& args) {
    if (authed_) {
        sendLine("503 Already authenticated");
        return;
    }

    std::istringstream iss(args);
    std::string mech, param;
    iss >> mech >> param;
    std::string umech = mech;
    std::transform(umech.begin(), umech.end(), umech.begin(), ::toupper);

    if (umech == "PLAIN") {
        std::string blob = param;
        if (blob.empty()) {
            sendLine("334 ");
            if (!readLine(blob)) return;
        }
        std::string decoded = base64Decode(blob);
        size_t p1 = decoded.find('\0');
        size_t p2 = decoded.find('\0', p1 + 1);
        if (p1 == std::string::npos || p2 == std::string::npos) {
            sendLine("501 Invalid PLAIN blob");
            return;
        }
        std::string authcid = decoded.substr(p1 + 1, p2 - p1 - 1);
        std::string password = decoded.substr(p2 + 1);

        if (context_.auth.validate(authcid, password)) {
            authed_ = true;
            username_ = authcid;
            sendLine("235 Authentication successful");
        } else {
            Metrics::instance().inc("smtp_auth_failures_total");
            sendLine("535 Authentication failed");
        }
        return;
    }

    if (umech == "LOGIN") {
        std::string user, pass;

        if (!param.empty()) {
            user = base64Decode(param);
        } else {
            sendLine("334 " + base64Encode("Username:"));
            std::string uline;
            if (!readLine(uline)) return;
            user = base64Decode(uline);
        }

        sendLine("334 " + base64Encode("Password:"));
        std::string pline;
        if (!readLine(pline)) return;
        pass = base64Decode(pline);

        if (context_.auth.validate(user, pass)) {
            authed_ = true;
            username_ = user;
            sendLine("235 Authentication successful");
        } else {
            Metrics::instance().inc("smtp_auth_failures_total");
            sendLine("535 Authentication failed");
        }
        return;
    }

    sendLine("504 Unrecognized authentication type");
}

void SmtpSession::handleMailFrom(const std::string& args) {
    mailFrom_ = args;
    sendLine("250 OK");
}

void SmtpSession::handleRcptTo(const std::string& args) {
    rcptTo_ = args;
    sendLine("250 OK");
}
static std::string extractAddress(const std::string& s) {
    auto start = s.find('<');
    auto end   = s.find('>');
    if (start != std::string::npos && end != std::string::npos && end > start)
        return s.substr(start + 1, end - start - 1);
    return s;
}

void SmtpSession::handleData() {
    if (mailFrom_.empty() || rcptTo_.empty()) {
        sendLine("503 Bad sequence of commands");
        return;
    }

    sendLine("354 End data with <CR><LF>.<CR><LF>");
    std::string line, data;
    while (readLine(line)) {
        if (line == ".")
            break;
        if (!line.empty() && line[0] == '.')
            line.erase(0, 1);
        data.append(line);
        data.append("\r\n");
    }

    // Split headers/body
    std::string::size_type sep = data.find("\r\n\r\n");
    std::string headers = (sep == std::string::npos) ? data : data.substr(0, sep);
    std::string body    = (sep == std::string::npos) ? ""   : data.substr(sep + 4);

    // From: domain (very simple parse)
    std::string fromDomain;
    auto fpos = headers.find("\nFrom:");
    if (fpos == std::string::npos)
        fpos = headers.find("\r\nFrom:");
    if (fpos != std::string::npos) {
        auto end = headers.find("\n", fpos + 1);
        std::string fromLine = headers.substr(fpos, end - fpos);
        auto at = fromLine.find('@');
        if (at != std::string::npos) {
            auto endd = fromLine.find_first_of(">\r\n", at);
            fromDomain = fromLine.substr(at + 1, endd - at - 1);
        }
    }

    // Run SPF, DKIM, DMARC
    SpfChecker spfChecker;
    DkimVerifier dkimVerifier;
    DmarcEvaluator dmarcEval;

    authResults_.spf  = spfChecker.check(peerIp_, mailFrom_, heloDomain_);
    // ----- DKIM verification (headers + body) -----
    authResults_.dkim = dkimVerifier.verify(headers, body);

    // ----- DMARC evaluation (single structured input) -----
    DmarcInput din;
    din.fromDomain = fromDomain;
    din.spfPass    = (authResults_.spf.result == SpfResult::Pass);
    din.spfDomain  = authResults_.spf.smtpMailFrom;
    din.dkimPass   = (authResults_.dkim.result == DkimResult::Pass);
    din.dkimDomain = authResults_.dkim.headerDomain;

    authResults_.dmarc = dmarcEval.evaluate(din);

    // Authentication-Results header
    std::string authHeader = "Authentication-Results: " +
        authResults_.toHeaderValue(context_.config.domain);

    // Prepend it
    std::string newHeaders = authHeader + "\r\n" + headers;
    std::string finalRaw = newHeaders + "\r\n\r\n" + body;
    // ---- VIRUS SCAN (Phase 1: ClamAV) ----
    auto scanResult = VirusScanner::scan(finalRaw);

    if (scanResult.unavailable) {
        Logger::instance().log(
            LogLevel::Error,
            "Virus scanner unavailable"
        );
        Metrics::instance().inc("virus_scanner_unavailable_total");
        sendLine("451 Temporary failure, virus scanner unavailable");
        return;
    }

    if (scanResult.infected) {
        Logger::instance().log(
            LogLevel::Warn,
            "Virus detected: " + scanResult.virusName
        );
        Metrics::instance().inc("messages_virus_rejected_total");
        sendLine("550 Message rejected due to virus detection");
        return;
    }
    // ---------------- MIME PARSING (RFC-compliant) ----------------
    MimeMessage mime = MimeParser::parse(finalRaw);

    std::vector<std::string> filenames;
    for (const auto& part : mime.root.children) {
        auto name = part.filename();
        if (!name.empty()) {
            filenames.push_back(name);
        }
    }
    // ---- PHASE 4: ATTACHMENT POLICY ----
    std::vector<AttachmentMeta> attachments;
    for (const auto& name : filenames) {
        attachments.push_back({
            name,
            "",
            false,
            false
        });
    }

    PolicyResult policy = AttachmentPolicy::evaluate(attachments);

    if (policy.verdict == PolicyVerdict::Reject) {
        Logger::instance().log(
            LogLevel::Warn,
            "Message rejected by attachment policy: " + policy.reason
        );
        sendLine("550 Message rejected: " + policy.reason);
        return;
    }

    if (policy.verdict == PolicyVerdict::Quarantine) {
        Logger::instance().log(
            LogLevel::Info,
            "Message quarantined by attachment policy: " + policy.reason
        );
        // mark as spam / quarantine
    }



    StoredMessage msg;
    msg.id = std::to_string(std::time(nullptr)) + "-" + std::to_string(rand());
    msg.from = extractAddress(mailFrom_);
    msg.recipients = { extractAddress(rcptTo_) };
    msg.rawData = finalRaw;
    msg.mailboxUser = username_.empty() ? rcptTo_ : username_;

    std::string storedId = context_.mailStore.store(msg);
    if (storedId.empty()) {
        sendLine("451 Requested action aborted: local error in processing");
    } else {
        Metrics::instance().inc("messages_received_total");
        sendLine("250 Message accepted for delivery");
    }
}

void SmtpSession::handleQuit() {
    sendLine("221 Bye");
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
}

static const char b64chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string SmtpSession::base64Encode(const std::string& in) {
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(b64chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
        out.push_back(b64chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4)
        out.push_back('=');
    return out;
}

std::string SmtpSession::base64Decode(const std::string& in) {
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++)
        T[static_cast<unsigned char>(b64chars[i])] = i;

    std::string out;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}
