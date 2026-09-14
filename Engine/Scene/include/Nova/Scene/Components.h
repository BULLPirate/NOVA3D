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
    Vec3 Direction{0.35f, 0.85f, 0.4f};
    Vec3 Color{1.0f, 0.98f, 0.95f};
    float Ambient = 0.18f;
};

} // namespace Nova
