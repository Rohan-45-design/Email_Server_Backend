#include "dns_resolver.h"
#include "dns_packet.h"
#include "dns_types.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

static const char* DNS_SERVER = "8.8.8.8";

DnsResolver& DnsResolver::instance() {
    static DnsResolver inst;
    return inst;
}

DnsResolver::DnsResolver() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
}

static std::vector<uint8_t> buildQuery(const std::string& name, uint16_t type) {
    std::vector<uint8_t> q(12);
    q[0] = rand() & 0xff;
    q[1] = rand() & 0xff;
    q[2] = 0x01; q[3] = 0x00;

    q[5] = 0x01;

    for (auto& label : name) {
        if (label == '.') q.push_back(0);
        else q.push_back(label);
    }
    q.push_back(0);

    q.push_back(type >> 8);
    q.push_back(type & 0xff);
    q.push_back(0); q.push_back(1);

    return q;
}

std::vector<std::string> DnsResolver::query(const std::string& name,
                                            uint16_t type) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    inet_pton(AF_INET, DNS_SERVER, &addr.sin_addr);

    auto q = buildQuery(name, type);
    sendto(s, (char*)q.data(), q.size(), 0,
           (sockaddr*)&addr, sizeof(addr));

    uint8_t buf[512];
    int len = recv(s, (char*)buf, sizeof(buf), 0);
    closesocket(s);

    auto pkt = parseDnsResponse(buf, len);
    std::vector<std::string> out;

    for (auto& a : pkt.answers)
        if ((uint16_t)a.type == type)
            out.push_back(a.data);

    return out;
}

std::vector<std::string> DnsResolver::lookupA(const std::string& n) {
    return query(n, (uint16_t)DnsRecordType::A);
}
std::vector<std::string> DnsResolver::lookupAAAA(const std::string& n) {
    return query(n, (uint16_t)DnsRecordType::AAAA);
}
std::vector<std::string> DnsResolver::lookupTxt(const std::string& n) {
    return query(n, (uint16_t)DnsRecordType::TXT);
}
std::vector<std::string> DnsResolver::lookupMx(const std::string& n) {
    return query(n, (uint16_t)DnsRecordType::MX);
}
