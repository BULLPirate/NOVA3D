#include <Nova/Core/Guid.h>

#include <cstdio>
#include <random>

namespace Nova {

Guid Guid::Nil() {
    return {};
}

Guid Guid::Generate() {
    Guid guid;
    std::random_device device;
    std::mt19937_64 rng(device());
    for (int i = 0; i < 16; i += 8) {
        const uint64_t chunk = rng();
        for (int b = 0; b < 8; ++b) {
            guid.Bytes[static_cast<size_t>(i + b)] =
                static_cast<uint8_t>((chunk >> (8 * b)) & 0xffu);
        }
    }
    guid.Bytes[6] = static_cast<uint8_t>((guid.Bytes[6] & 0x0fu) | 0x40u);
    guid.Bytes[8] = static_cast<uint8_t>((guid.Bytes[8] & 0x3fu) | 0x80u);
    return guid;
}

bool Guid::IsNil() const {
    return *this == Nil();
}

namespace {

int HexNibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

} // namespace

bool Guid::TryParse(std::string_view text, Guid& out) {
    std::string hex;
    hex.reserve(32);
    for (char c : text) {
        if (c == '-') {
            continue;
        }
        if (HexNibble(c) < 0) {
            return false;
        }
        hex.push_back(c);
    }
    if (hex.size() != 32) {
        return false;
    }
    Guid parsed;
    for (size_t i = 0; i < 16; ++i) {
        const int hi = HexNibble(hex[i * 2]);
        const int lo = HexNibble(hex[i * 2 + 1]);
        parsed.Bytes[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    out = parsed;
    return true;
}

std::string Guid::ToString() const {
    char buffer[37];
    std::snprintf(buffer, sizeof(buffer),
                  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  Bytes[0], Bytes[1], Bytes[2], Bytes[3], Bytes[4], Bytes[5], Bytes[6], Bytes[7],
                  Bytes[8], Bytes[9], Bytes[10], Bytes[11], Bytes[12], Bytes[13], Bytes[14],
                  Bytes[15]);
    return buffer;
}

} // namespace Nova
