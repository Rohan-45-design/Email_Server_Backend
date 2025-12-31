#pragma once

#include <iostream>
#include "core/config_loader.h"
#include "storage/mail_store.h"
#include "core/i_auth_manager.h"
#include "core/auth_manager.h"
#include "core/distributed_auth_manager.h"
#include "imap/flags_index.h"
#include "retro/retro_manager.h"

struct ServerContext {
    ServerConfig config;
    MailStore mailStore;
    AuthManager localAuth;  // Fallback local auth manager
    DistributedAuthManager& distributedAuth;  // Reference to singleton
    IAuthManager* activeAuth;  // Pointer to active auth manager
    FlagsIndex flags;   // NEW: for IMAP flags like \Seen
    RetroManager retroManager;
    bool useDistributedAuth = false;

    explicit ServerContext(const ServerConfig& cfg)
        : config(cfg)
        , mailStore(cfg.mailRoot)
        , distributedAuth(DistributedAuthManager::instance())
        , activeAuth(&localAuth)
        , flags(cfg.mailRoot)
        , retroManager(mailStore)
    {
        // Initialize distributed auth if HA is enabled
        if (cfg.enableHA && !cfg.redisHost.empty()) {
            if (distributedAuth.initialize(cfg.redisHost, cfg.redisPort,
                                        cfg.redisPassword, cfg.clusterId, cfg.nodeId)) {
                useDistributedAuth = true;
                activeAuth = &distributedAuth;
                std::cout << "Using distributed authentication for HA" << std::endl;

                // Load users from config into distributed store
                if (!cfg.usersFile.empty()) {
                    distributedAuth.loadFromFile(cfg.usersFile);
                }
            } else {
                std::cerr << "Failed to initialize distributed auth, falling back to local auth" << std::endl;
                useDistributedAuth = false;
                activeAuth = &localAuth;
            }
        }

        // Initialize local auth as fallback
        if (!useDistributedAuth) {
            localAuth.loadFromFile(cfg.usersFile);
        }
    }

    // Get the active auth manager
    IAuthManager& getAuthManager() {
        return *activeAuth;
    }

    const IAuthManager& getAuthManager() const {
        return *activeAuth;
    }

    // Distributed auth specific methods
    bool isDistributedAuthEnabled() const { return useDistributedAuth; }
    std::string getClusterStatus() const {
        return useDistributedAuth ? distributedAuth.getClusterStatus() : "LOCAL";
    }
    std::vector<std::string> getClusterNodes() const {
        return useDistributedAuth ? distributedAuth.getClusterNodes() : std::vector<std::string>();
    }
};
