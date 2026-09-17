#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace Nova {

/// RFC 4122 UUID v4. Stable identity for assets, plugins, and saved references.
struct Guid {
    std::array<uint8_t, 16> Bytes{};

    static Guid Nil();
    static Guid Generate();
    static bool TryParse(std::string_view text, Guid& out);

    bool IsNil() const;
    std::string ToString() const;

    friend bool operator==(const Guid& a, const Guid& b) { return a.Bytes == b.Bytes; }
    friend bool operator!=(const Guid& a, const Guid& b) { return !(a == b); }
};

struct GuidHash {
    std::size_t operator()(const Guid& guid) const {
        std::size_t hash = 1469598103934665603ull;
        for (uint8_t byte : guid.Bytes) {
            hash ^= byte;
            hash *= 1099511628211ull;
        }
        return hash;
    }
};

} // namespace Nova
