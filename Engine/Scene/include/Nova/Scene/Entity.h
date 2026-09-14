#pragma once

#include <cstdint>

namespace Nova {

/// Opaque entity handle (index into Scene storage + generation).
struct Entity {
    uint32_t Id = kInvalidEntity;

    constexpr bool IsValid() const { return Id != kInvalidEntity; }

    static constexpr uint32_t kInvalidEntity = UINT32_MAX;
};

constexpr bool operator==(Entity a, Entity b) { return a.Id == b.Id; }
constexpr bool operator!=(Entity a, Entity b) { return a.Id != b.Id; }

} // namespace Nova
