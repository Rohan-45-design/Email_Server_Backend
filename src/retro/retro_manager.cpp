#include "retro/retro_manager.h"
#include "core/logger.h"
#include "monitoring/metrics.h"

RetroManager::RetroManager(MailStore& store)
    : store_(store) {}

void RetroManager::execute(const RetroEvent& e) {
    if (e.action == RetroAction::Quarantine) {
        store_.moveToQuarantine(e.mailboxUser, e.messageId);
    }
    else if (e.action == RetroAction::Delete) {
        store_.deleteMessage(e.mailboxUser, e.messageId);
    }

    Metrics::instance().inc("messages_retroactive_total");

    Logger::instance().log(
        LogLevel::Warn,
        "Retroactive action applied to message " + e.messageId
    );
}
