#pragma once
#include <cstdint>

enum class DnsRecordType : uint16_t {
    A     = 1,
    PTR   = 12,
    MX    = 15,
    TXT   = 16,
    AAAA  = 28
};

enum class DnsResponseCode {
    NoError,
    NxDomain,
    ServFail,
    Refused,
    Other
};
