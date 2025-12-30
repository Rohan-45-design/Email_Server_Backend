#pragma once

#include <functional>
#include <exception>
#include <string>

/**
 * Crash Containment
 * 
 * WHY REQUIRED:
 * - Prevent exceptions from escaping worker threads (crashes entire process)
 * - Add top-level exception boundaries (catch-all safety net)
 * - Log fatal errors before shutdown (diagnostics)
 * - Prevent cascading failures
 */
class CrashContainment {
public:
    static CrashContainment& instance();

    // Execute function with exception containment
    // Returns true if executed successfully, false if exception caught
    bool executeSafely(const std::string& context, std::function<void()> fn);

    // Set global exception handler
    void setGlobalHandler(std::function<void(const std::exception&)> handler);

    // Install top-level exception handlers
    void installTopLevelHandlers();

private:
    CrashContainment() = default;
    std::function<void(const std::exception&)> globalHandler_;
};

