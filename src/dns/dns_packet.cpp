#include "dns_packet.h"
#include <stdexcept>

static uint16_t read16(const uint8_t* buf, size_t& off) {
    uint16_t v = (buf[off] << 8) | buf[off + 1];
    off += 2;
    return v;
}

static uint32_t read32(const uint8_t* buf, size_t& off) {
    uint32_t v = (buf[off] << 24) | (buf[off + 1] << 16)
               | (buf[off + 2] << 8) | buf[off + 3];
    off += 4;
    return v;
}

static std::string readName(const uint8_t* buf, size_t& off, size_t max) {
    std::string name;
    while (off < max) {
        uint8_t len = buf[off++];
        if (len == 0) break;
        if ((len & 0xC0) == 0xC0) {
            size_t ptr = ((len & 0x3F) << 8) | buf[off++];
            size_t tmp = ptr;
            return name + readName(buf, tmp, max);
        }
        if (!name.empty()) name += '.';
        name.append(reinterpret_cast<const char*>(buf + off), len);
        off += len;
    }
    return name;
}

DnsPacket parseDnsResponse(const uint8_t* buf, size_t len) {
    DnsPacket pkt{};
    size_t off = 0;

    pkt.id = read16(buf, off);
    uint16_t flags = read16(buf, off);
    pkt.rcode = static_cast<DnsResponseCode>(flags & 0x000F);

    uint16_t qd = read16(buf, off);
    uint16_t an = read16(buf, off);
    read16(buf, off); // NS
    read16(buf, off); // AR

    for (int i = 0; i < qd; i++) {
        readName(buf, off, len);
        off += 4;
    }

    for (int i = 0; i < an; i++) {
        DnsAnswer a;
        a.name = readName(buf, off, len);
        a.type = static_cast<DnsRecordType>(read16(buf, off));
        off += 2; // class
        a.ttl = read32(buf, off);
        uint16_t rdlen = read16(buf, off);

        if (a.type == DnsRecordType::A && rdlen == 4) {
            char ip[16];
            snprintf(ip, sizeof(ip), "%u.%u.%u.%u",
                     buf[off], buf[off+1], buf[off+2], buf[off+3]);
            a.data = ip;
        } else if (a.type == DnsRecordType::TXT) {
            uint8_t sl = buf[off];
            a.data.assign(reinterpret_cast<const char*>(buf + off + 1), sl);
        }
        off += rdlen;
        pkt.answers.push_back(a);
    }

    return pkt;
}
