#pragma once
#include "storage/mail_store.h"
#include "retro/retro_event.h"

class RetroManager {
public:
    explicit RetroManager(MailStore& store);
    void execute(const RetroEvent& event);

private:
    MailStore& store_;
};
