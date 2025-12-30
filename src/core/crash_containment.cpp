#include "crash_containment.h"
#include "logger.h"
#include <csignal>
#include <cstdlib>

CrashContainment& CrashContainment::instance() {
    static CrashContainment inst;
    return inst;
}

bool CrashContainment::executeSafely(const std::string& context, std::function<void()> fn) {
    try {
        fn();
        return true;
    } catch (const std::exception& ex) {
        Logger::instance().log(LogLevel::Error,
            "CrashContainment: Exception in " + context + ": " + ex.what());
        if (globalHandler_) {
            globalHandler_(ex);
        }
        return false;
    } catch (...) {
        Logger::instance().log(LogLevel::Error,
            "CrashContainment: Unknown exception in " + context);
        return false;
    }
}

void CrashContainment::setGlobalHandler(std::function<void(const std::exception&)> handler) {
    globalHandler_ = handler;
}

void CrashContainment::installTopLevelHandlers() {
    // Install signal handlers for fatal errors
    std::signal(SIGSEGV, [](int sig) {
        Logger::instance().log(LogLevel::Error,
            "CrashContainment: FATAL: Segmentation fault (SIGSEGV)");
        std::abort();
    });

    std::signal(SIGABRT, [](int sig) {
        Logger::instance().log(LogLevel::Error,
            "CrashContainment: FATAL: Abort signal (SIGABRT)");
        std::abort();
    });

    std::signal(SIGFPE, [](int sig) {
        Logger::instance().log(LogLevel::Error,
            "CrashContainment: FATAL: Floating point exception (SIGFPE)");
        std::abort();
    });

#ifndef _WIN32
    std::signal(SIGBUS, [](int sig) {
        Logger::instance().log(LogLevel::Error,
            "CrashContainment: FATAL: Bus error (SIGBUS)");
        std::abort();
    });
#endif
}

