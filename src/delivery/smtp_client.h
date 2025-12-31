#pragma once

#include <string>
#include <vector>
#include <optional>

/**
 * SMTP Delivery Client
 * 
 * CRITICAL: This was missing - email server needs to actually deliver mail!
 * 
 * WHY REQUIRED:
 * - Enqueued messages must be delivered to recipient servers
 * - MX record lookup and connection logic
 * - Retry with exponential backoff
 * - Bounce handling for permanent failures
 */
struct DeliveryResult {
    bool success = false;
    bool permanentFailure = false;
    std::string errorMessage;
    int retryAfterSeconds = 0; // For temporary failures
};

class SmtpDeliveryClient {
public:
    static SmtpDeliveryClient& instance();

    // Deliver a message to a recipient
    // Returns delivery result with retry information
    DeliveryResult deliver(
        const std::string& from,
        const std::string& to,
        const std::string& rawMessage
    );

    // Lookup MX records for a domain
    std::vector<std::string> lookupMX(const std::string& domain);

    // Connect to SMTP server and deliver
    DeliveryResult connectAndDeliver(
        const std::string& mxHost,
        int port,
        const std::string& from,
        const std::string& to,
        const std::string& rawMessage
    );

private:
    SmtpDeliveryClient() = default;
    static constexpr int DEFAULT_SMTP_PORT = 25;
    static constexpr int CONNECTION_TIMEOUT_SEC = 30;
};

