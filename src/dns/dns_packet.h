#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "dns_types.h"

struct DnsAnswer {
    std::string name;
    DnsRecordType type;
    uint32_t ttl;
    std::string data;
};

struct DnsPacket {
    uint16_t id;
    DnsResponseCode rcode;
    std::vector<DnsAnswer> answers;
};

DnsPacket parseDnsResponse(const uint8_t* buf, size_t len);
