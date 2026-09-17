#pragma once

#include <Nova/Math/Vec.h>
#include <Nova/Math/Mat4.h>

namespace Nova {

/// Single directional light (world space). Direction points from the surface toward the light.
struct DirectionalLight {
    Vec3 Direction = {0.35f, 0.85f, 0.4f};
    Vec3 Color     = {1.0f, 0.98f, 0.95f};
    float Ambient  = 0.14f;
};

/// Scene stores ray direction (sun → scene). Renderer needs vector toward the light.
Vec3 LightDirectionTowardSurface(const Vec3& rayDirectionWorld);

struct ShadowSettings {
    float OrthoHalfExtent = 2.5f;
    float NearPlane       = 0.5f;
    float FarPlane        = 25.0f;
    float Bias            = 0.0025f;
    float Strength        = 0.72f;
    bool  Enabled         = true;
};

struct PointLight {
    Vec3 Position{0.0f, 0.0f, 0.0f};
    Vec3 Color{1.0f, 0.85f, 0.55f};
    float Range = 0.0f; // 0 = disabled
};

inline DirectionalLight DefaultDirectionalLight() {
    DirectionalLight light;
    light.Direction = light.Direction.Normalized();
    return light;
}

inline PointLight DisabledPointLight() {
    return {};
}

/// View-projection from the light toward scene focus (no model matrix).
Mat4 ComputeDirectionalLightViewProjection(const Vec3& lightDirectionTowardLight,
                                           const Vec3& focus,
                                           float orthoHalfExtent,
                                           float nearPlane,
                                           float farPlane);

} // namespace Nova
