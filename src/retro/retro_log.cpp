#include "retro_log.h"
#include <fstream>
#include <ctime>

void RetroLog::record(
    const std::string& messageId,
    const std::string& user,
    const std::string& reason
) {
    std::ofstream out("logs/retro.log", std::ios::app);
    out << std::time(nullptr)
        << " user=" << user
        << " msg=" << messageId
        << " reason=" << reason
        << "\n";
}
