#ifndef ROBI_MDNS_H_
#define ROBI_MDNS_H_

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace robi {

// Case-insensitive RFC 6762 mDNS / LLMNR query parser checking whether any question
// asks for "robi.local" or single-label "robi".
inline bool ParseMdnsRobiLocalQuery(const uint8_t* pkt, size_t len, uint16_t& qtype_out) {
    if (pkt == nullptr || len < 22) return false;
    uint16_t flags = (static_cast<uint16_t>(pkt[2]) << 8) | pkt[3];
    if (flags & 0x8000u) return false;  // Ignore DNS responses
    uint16_t qdcount = (static_cast<uint16_t>(pkt[4]) << 8) | pkt[5];
    if (qdcount == 0) return false;

    size_t pos = 12;
    qtype_out = 1;
    bool matched = false;
    for (uint16_t q = 0; q < qdcount && pos < len; ++q) {
        bool first_is_robi = false;
        bool second_is_local_or_end = false;
        int label_idx = 0;
        bool jumped = false;
        size_t cur = pos;
        int hops = 0;

        while (cur < len && hops < 16) {
            uint8_t llen = pkt[cur];
            if (llen == 0) {
                if (label_idx == 1 && first_is_robi) {
                    second_is_local_or_end = true;
                }
                if (!jumped) pos = cur + 1;
                break;
            }
            if ((llen & 0xC0u) == 0xC0u) {
                if (cur + 1 >= len) return matched;
                uint16_t ptr = (static_cast<uint16_t>(llen & 0x3Fu) << 8) | pkt[cur + 1];
                if (ptr >= len) return matched;
                if (!jumped) pos = cur + 2;
                jumped = true;
                cur = ptr;
                ++hops;
                continue;
            }
            if (cur + 1 + llen > len) return matched;
            if (label_idx == 0 && llen == 4) {
                first_is_robi = (std::tolower(pkt[cur + 1]) == 'r' &&
                                 std::tolower(pkt[cur + 2]) == 'o' &&
                                 std::tolower(pkt[cur + 3]) == 'b' &&
                                 std::tolower(pkt[cur + 4]) == 'i');
            } else if (label_idx == 1 && first_is_robi && llen == 5) {
                second_is_local_or_end = (std::tolower(pkt[cur + 1]) == 'l' &&
                                          std::tolower(pkt[cur + 2]) == 'o' &&
                                          std::tolower(pkt[cur + 3]) == 'c' &&
                                          std::tolower(pkt[cur + 4]) == 'a' &&
                                          std::tolower(pkt[cur + 5]) == 'l');
            } else if (label_idx >= 2) {
                second_is_local_or_end = false;
            }
            cur += 1 + llen;
            ++label_idx;
        }
        if (hops >= 16 || pos + 4 > len) break;
        uint16_t qtype = (static_cast<uint16_t>(pkt[pos]) << 8) | pkt[pos + 1];
        pos += 4;  // skip QTYPE + QCLASS

        if (first_is_robi && second_is_local_or_end) {
            matched = true;
            if (qtype == 1 || qtype == 255) {
                qtype_out = 1;
                return true;
            }
            qtype_out = qtype;
        }
    }
    return matched;
}

// Builds an RFC 6762 mDNS response (up to 62 bytes; `resp` must be >= 64 bytes)
// containing both the IPv4 Type-A record for robi.local and an RFC 6762 Section 6.1
// NSEC record proving robi.local has ONLY Type A (no IPv6 AAAA), eliminating mobile IPv6 timeouts.
inline size_t BuildMdnsRobiLocalPacket(uint8_t* resp, uint16_t txid,
                                       const uint8_t ipv4_bytes[4], uint16_t qtype) {
    std::memset(resp, 0, 64);
    resp[0] = static_cast<uint8_t>((txid >> 8) & 0xFFu);
    resp[1] = static_cast<uint8_t>(txid & 0xFFu);
    resp[2] = 0x84;  // QR=1, AA=1
    resp[3] = 0x00;
    if (qtype == 28) {
        // Explicit AAAA query: place A + NSEC in Answer section (ANCOUNT = 2)
        resp[6] = 0x00;
        resp[7] = 0x02;
    } else {
        // Standard A/ANY query: A in Answer (ANCOUNT = 1), NSEC in Additional (ARCOUNT = 1)
        resp[6] = 0x00;
        resp[7] = 0x01;
        resp[10] = 0x00;
        resp[11] = 0x01;
    }

    size_t off = 12;
    const uint8_t qname[] = {4, 'r', 'o', 'b', 'i', 5, 'l', 'o', 'c', 'a', 'l', 0};
    std::memcpy(resp + off, qname, sizeof(qname));
    off += sizeof(qname);
    resp[off++] = 0x00;
    resp[off++] = 0x01;  // TYPE A
    resp[off++] = 0x80;  // CLASS IN + cache-flush
    resp[off++] = 0x01;
    resp[off++] = 0x00;
    resp[off++] = 0x00;
    resp[off++] = 0x00;
    resp[off++] = 120;   // TTL 120s
    resp[off++] = 0x00;
    resp[off++] = 0x04;  // RDLENGTH 4
    std::memcpy(resp + off, ipv4_bytes, 4);
    off += 4;

    // NSEC record pointing to 0xC00C ("robi.local.")
    resp[off++] = 0xC0;
    resp[off++] = 0x0C;
    resp[off++] = 0x00;
    resp[off++] = 0x2F;  // TYPE NSEC (47)
    resp[off++] = 0x80;  // CLASS IN + cache-flush
    resp[off++] = 0x01;
    resp[off++] = 0x00;
    resp[off++] = 0x00;
    resp[off++] = 0x00;
    resp[off++] = 120;   // TTL 120s
    resp[off++] = 0x00;
    resp[off++] = 0x05;  // RDLENGTH 5
    resp[off++] = 0xC0;
    resp[off++] = 0x0C;  // Next Domain Name -> robi.local.
    resp[off++] = 0x00;  // Window Block 0
    resp[off++] = 0x01;  // Bitmap Length 1
    resp[off++] = 0x40;  // Bit 1 = TYPE A (1) only
    return off;
}

}  // namespace robi

#endif  // ROBI_MDNS_H_
