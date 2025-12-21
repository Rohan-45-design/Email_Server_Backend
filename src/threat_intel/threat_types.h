#pragma once
#include <string>

enum class ThreatVerdict {
    Clean,
    Suspicious,
    Malicious
};

inline std::string verdictToString(ThreatVerdict v) {
    switch (v) {
    case ThreatVerdict::Clean: return "clean";
    case ThreatVerdict::Suspicious: return "suspicious";
    case ThreatVerdict::Malicious: return "malicious";
    }
    return "unknown";
}
