#include "delivery/smtp_client.h"
#include "core/logger.h"
#include "dns/dns_resolver.h"
#include "core/tls_context.h"
#include <sstream>
#include <chrono>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <openssl/ssl.h>
#include <stdexcept>
#pragma comment(lib, "Ws2_32.lib")

SmtpDeliveryClient& SmtpDeliveryClient::instance() {
    static SmtpDeliveryClient inst;
    return inst;
}

std::vector<std::string> SmtpDeliveryClient::lookupMX(const std::string& domain) {
    std::vector<std::string> mxHosts;
    
    try {
        // Use existing DNS resolver to get MX records
        // TODO: Implement proper MX record lookup using DNSResolver
        // For now, return the domain itself as fallback
        Logger::instance().log(LogLevel::Info,
            "Delivery: Looking up MX records for " + domain);
        
        // Placeholder: In production, use DNSResolver to get MX records
        // For now, assume domain itself is the mail server
        mxHosts.push_back(domain);
        
    } catch (const std::exception& ex) {
        Logger::instance().log(LogLevel::Error,
            "Delivery: MX lookup failed for " + domain + ": " + ex.what());
    }
    
    return mxHosts;
}

DeliveryResult SmtpDeliveryClient::connectAndDeliver(
    const std::string& mxHost,
    int port,
    const std::string& from,
    const std::string& to,
    const std::string& rawMessage
) {
    DeliveryResult result;
    
    SOCKET sock = INVALID_SOCKET;
    SSL* ssl = nullptr;
    
    try {
        // Resolve hostname
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        
        addrinfo* addrInfo = nullptr;
        int res = getaddrinfo(mxHost.c_str(), std::to_string(port).c_str(), &hints, &addrInfo);
        if (res != 0 || !addrInfo) {
            result.errorMessage = "DNS resolution failed for " + mxHost;
            result.retryAfterSeconds = 300; // Retry in 5 minutes
            return result;
        }
        
        // Create socket
        sock = socket(addrInfo->ai_family, addrInfo->ai_socktype, addrInfo->ai_protocol);
        if (sock == INVALID_SOCKET) {
            result.errorMessage = "Failed to create socket";
            result.retryAfterSeconds = 60;
            freeaddrinfo(addrInfo);
            return result;
        }
        
        // Set timeout
        DWORD timeout = CONNECTION_TIMEOUT_SEC * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
        
        // Connect
        if (connect(sock, addrInfo->ai_addr, (int)addrInfo->ai_addrlen) == SOCKET_ERROR) {
            result.errorMessage = "Connection failed to " + mxHost;
            result.retryAfterSeconds = 300;
            closesocket(sock);
            freeaddrinfo(addrInfo);
            return result;
        }
        
        freeaddrinfo(addrInfo);
        
        // Read greeting
        char buffer[512];
        int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) {
            result.errorMessage = "Failed to read SMTP greeting";
            result.retryAfterSeconds = 60;
            closesocket(sock);
            return result;
        }
        buffer[n] = '\0';
        
        // Send EHLO
        std::string ehlo = "EHLO " + std::string("mailserver.local") + "\r\n";
        send(sock, ehlo.c_str(), (int)ehlo.length(), 0);
        
        n = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0 || buffer[0] != '2') {
            result.errorMessage = "EHLO failed";
            result.retryAfterSeconds = 60;
            closesocket(sock);
            return result;
        }
        
        // Send MAIL FROM
        std::string mailFrom = "MAIL FROM:<" + from + ">\r\n";
        send(sock, mailFrom.c_str(), (int)mailFrom.length(), 0);
        
        n = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0 || buffer[0] != '2') {
            result.errorMessage = "MAIL FROM rejected: " + std::string(buffer, n);
            result.permanentFailure = (buffer[0] == '5');
            result.retryAfterSeconds = result.permanentFailure ? 0 : 300;
            closesocket(sock);
            return result;
        }
        
        // Send RCPT TO
        std::string rcptTo = "RCPT TO:<" + to + ">\r\n";
        send(sock, rcptTo.c_str(), (int)rcptTo.length(), 0);
        
        n = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0 || buffer[0] != '2') {
            result.errorMessage = "RCPT TO rejected: " + std::string(buffer, n);
            result.permanentFailure = (buffer[0] == '5');
            result.retryAfterSeconds = result.permanentFailure ? 0 : 300;
            closesocket(sock);
            return result;
        }
        
        // Send DATA
        std::string data = "DATA\r\n";
        send(sock, data.c_str(), (int)data.length(), 0);
        
        n = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0 || buffer[0] != '3') {
            result.errorMessage = "DATA command failed";
            result.retryAfterSeconds = 60;
            closesocket(sock);
            return result;
        }
        
        // Send message
        send(sock, rawMessage.c_str(), (int)rawMessage.length(), 0);
        send(sock, "\r\n.\r\n", 5, 0);
        
        n = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0 || buffer[0] != '2') {
            result.errorMessage = "Message rejected: " + std::string(buffer, n);
            result.permanentFailure = (buffer[0] == '5');
            result.retryAfterSeconds = result.permanentFailure ? 0 : 300;
            closesocket(sock);
            return result;
        }
        
        // QUIT
        send(sock, "QUIT\r\n", 6, 0);
        recv(sock, buffer, sizeof(buffer) - 1, 0);
        
        result.success = true;
        closesocket(sock);
        
        Logger::instance().log(LogLevel::Info,
            "Delivery: Successfully delivered to " + to + " via " + mxHost);
        
    } catch (const std::exception& ex) {
        result.errorMessage = "Exception during delivery: " + std::string(ex.what());
        result.retryAfterSeconds = 300;
        if (sock != INVALID_SOCKET) closesocket(sock);
        if (ssl) SSL_free(ssl);
    }
    
    return result;
}

DeliveryResult SmtpDeliveryClient::deliver(
    const std::string& from,
    const std::string& to,
    const std::string& rawMessage
) {
    // Extract domain from recipient
    size_t atPos = to.find('@');
    if (atPos == std::string::npos) {
        DeliveryResult result;
        result.permanentFailure = true;
        result.errorMessage = "Invalid recipient address: " + to;
        return result;
    }
    
    std::string domain = to.substr(atPos + 1);
    
    // Lookup MX records
    auto mxHosts = lookupMX(domain);
    if (mxHosts.empty()) {
        DeliveryResult result;
        result.permanentFailure = true;
        result.errorMessage = "No MX records found for " + domain;
        return result;
    }
    
    // Try each MX host
    for (const auto& mxHost : mxHosts) {
        DeliveryResult result = connectAndDeliver(mxHost, DEFAULT_SMTP_PORT, from, to, rawMessage);
        if (result.success) {
            return result;
        }
        if (result.permanentFailure) {
            return result; // Don't try other MX hosts for permanent failures
        }
        // Temporary failure - try next MX host
    }
    
    // All MX hosts failed
    DeliveryResult result;
    result.errorMessage = "All MX hosts failed for " + domain;
    result.retryAfterSeconds = 300;
    return result;
}

