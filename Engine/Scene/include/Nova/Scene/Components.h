#pragma once

#include <cstdint>

#include <Nova/Math/Transform.h>
#include <Nova/Math/Math.h>

#include <string>

namespace Nova {

enum class MeshPrimitive : uint8_t {
    UnitCube = 0,
};

struct MeshRendererComponent {
    MeshPrimitive Primitive = MeshPrimitive::UnitCube;
    bool ReceiveShadows = false;
};

/// Camera uses entity Transform for position/rotation; looks at LookAtTarget.
struct CameraComponent {
    bool IsPrimary = false;
    Vec3 LookAtTarget{0.0f, 0.0f, 0.0f};
    float FovYRadians = Radians(60.0f);
    float NearPlane   = 0.1f;
    float FarPlane    = 100.0f;
};

struct DirectionalLightComponent {
    /// World-space direction **light rays travel** (from sun toward the scene).
    Vec3 Direction{0.45f, -0.88f, 0.15f};
    Vec3 Color{1.0f, 0.98f, 0.95f};
    float Ambient = 0.12f;
};

} // namespace Nova
