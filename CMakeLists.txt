cmake_minimum_required(VERSION 3.16)
project(mailserver CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(mailserver
    src/main.cpp
    src/core/server_context.cpp
    src/core/config_loader.cpp
    src/core/logger.cpp
    src/smtp/smtp_server.cpp
    src/smtp/smtp_session.cpp
    src/storage/mail_store.cpp
    src/imap/imap_server.cpp
    src/imap/imap_session.cpp
    src/core/auth_manager.cpp
    src/imap/flags_index.cpp
    src/antispam/dkim_canon.cpp
    src/antispam/dkim_signer.cpp
    src/antispam/dkim_verifier.cpp
    src/antispam/dmarc_evaluator.cpp
    src/antispam/spf_evaluator.cpp
    src/antispam/spf_checker.cpp
    src/antispam/spf_parser.cpp
    src/queue/mail_queue.cpp
    src/queue/retry_worker.cpp
    src/dns/dns_resolver.cpp
    src/dns/dns_packet.cpp
    src/spam/spam_engine.cpp
    src/spam/spam_rules.cpp
    src/monitoring/metrics.cpp
    src/monitoring/health.cpp
    src/monitoring/http_metrics_server.cpp
    src/admin/admin_server.cpp
    src/admin/admin_routes.cpp
    src/admin/admin_auth.cpp
    src/ha/leader_election.cpp
    src/ha/ha_controller.cpp
    src/antivirus/virus_scanner.cpp    
    src/virus/cloud_scanner.cpp
    src/virus/cloud_provider_virustotal.cpp
    src/virus/sandbox_engine.cpp
    src/virus/sandbox_provider_anyrun.cpp
    src/virus/sandbox_verdict_store.cpp
    src/mime/mime_parser.cpp
    src/mime/mime_decoder.cpp
    src/policy/attachment_policy.cpp
    src/virus/cloud_scan_worker.cpp
    src/virus/virus_verdict_store.cpp
    src/retro/retro_manager.cpp
    src/threat_intel/hash_reputation.cpp
    src/threat_intel/intel_feedback.cpp
    src/threat_intel/ioc_store.cpp
    src/threat_intel/sender_reputation.cpp
)

# Project headers
target_include_directories(mailserver PRIVATE
    src
)

# -------------------- OpenSSL (DKIM REQUIRED) --------------------
target_include_directories(mailserver PRIVATE
    "C:/Program Files/OpenSSL-Win64/include"
)

target_link_libraries(mailserver PRIVATE
    "C:/Program Files/OpenSSL-Win64/lib/VC/x64/MD/libcrypto.lib"
    ws2_32
)


# -------------------- yaml-cpp --------------------
find_package(yaml-cpp REQUIRED)
target_link_libraries(mailserver PRIVATE yaml-cpp)
