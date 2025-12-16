#pragma once

#include "core/config_loader.h"
#include "storage/mail_store.h"
#include "core/auth_manager.h"
#include "imap/flags_index.h"

struct ServerContext {
    ServerConfig config;
    MailStore mailStore;
    AuthManager auth;
    FlagsIndex flags;   // NEW: for IMAP flags like \Seen

    explicit ServerContext(const ServerConfig& cfg)
        : config(cfg)
        , mailStore(cfg.mailRoot)
        , flags(cfg.mailRoot) {}
};
