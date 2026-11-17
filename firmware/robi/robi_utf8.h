#ifndef ROBI_UTF8_H_
#define ROBI_UTF8_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace robi {

// Returns the valid RFC 3629 UTF-8 sequence length (1..4) starting at s[pos],
// or 0 if s[pos..] does not begin with a complete, valid UTF-8 codepoint.
inline size_t Utf8ValidCodepointLen(std::string_view s, size_t pos) {
    if (pos >= s.size()) return 0;
    const auto b0 = static_cast<uint8_t>(s[pos]);
    if (b0 == 0) return 0;
    if (b0 < 0x80) return 1;

    auto is_cont = [](uint8_t b) { return (b & 0xC0u) == 0x80u; };

    if (b0 >= 0xC2 && b0 <= 0xDF) {
        if (pos + 1 >= s.size()) return 0;
        return is_cont(static_cast<uint8_t>(s[pos + 1])) ? 2 : 0;
    }
    if (b0 >= 0xE0 && b0 <= 0xEF) {
        if (pos + 2 >= s.size()) return 0;
        const auto b1 = static_cast<uint8_t>(s[pos + 1]);
        const auto b2 = static_cast<uint8_t>(s[pos + 2]);
        if (!is_cont(b1) || !is_cont(b2)) return 0;
        if (b0 == 0xE0 && b1 < 0xA0) return 0;  // Overlong 3-byte
        if (b0 == 0xED && b1 >= 0xA0) return 0; // UTF-16 surrogates U+D800..U+DFFF
        return 3;
    }
    if (b0 >= 0xF0 && b0 <= 0xF4) {
        if (pos + 3 >= s.size()) return 0;
        const auto b1 = static_cast<uint8_t>(s[pos + 1]);
        const auto b2 = static_cast<uint8_t>(s[pos + 2]);
        const auto b3 = static_cast<uint8_t>(s[pos + 3]);
        if (!is_cont(b1) || !is_cont(b2) || !is_cont(b3)) return 0;
        if (b0 == 0xF0 && b1 < 0x90) return 0;  // Overlong 4-byte
        if (b0 == 0xF4 && b1 >= 0x90) return 0; // > U+10FFFF
        return 4;
    }
    return 0;
}

// Sanitizes arbitrary input into strictly valid RFC 3629 / RFC 6455 UTF-8
// (dropping stray continuation/truncated/overlong bytes) and truncates cleanly
// on a codepoint boundary at <= max_bytes.
inline std::string Utf8SafeTruncate(std::string_view s, size_t max_bytes) {
    if (s.empty() || max_bytes == 0) return {};
    std::string out;
    out.reserve(s.size() < max_bytes ? s.size() : max_bytes);

    size_t pos = 0;
    while (pos < s.size()) {
        size_t cp_len = Utf8ValidCodepointLen(s, pos);
        if (cp_len == 0) {
            // Skip invalid/truncated byte so WebSocket text frames never fail RFC 6455 UTF-8 checks
            ++pos;
            continue;
        }
        if (out.size() + cp_len > max_bytes) {
            break;
        }
        out.append(s.data() + pos, cp_len);
        pos += cp_len;
    }
    return out;
}

inline std::string Utf8Sanitize(std::string_view s) {
    return Utf8SafeTruncate(s, s.size());
}

}  // namespace robi

#endif  // ROBI_UTF8_H_
