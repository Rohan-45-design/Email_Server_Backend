#pragma once
#include <string>
#include "retro_action.h"
struct RetroEvent {
    std::string messageId;
    std::string mailboxUser;
    std::string reason;
    RetroAction action;
};
