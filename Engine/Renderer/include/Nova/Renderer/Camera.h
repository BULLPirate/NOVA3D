#pragma once

#include <Nova/Math/Math.h>

namespace Nova {

/// Minimal 3D camera: eye looks at a target, perspective projection.
struct Camera {
    Vec3 Position{0.0f, 0.0f, 3.0f};
    Vec3 Target {0.0f, 0.0f, 0.0f};
    Vec3 Up    {0.0f, 1.0f, 0.0f};

    float FovYRadians = Radians(60.0f);
    float Aspect      = 16.0f / 9.0f;
    float NearPlane   = 0.1f;
    float FarPlane    = 100.0f;

    Mat4 GetViewMatrix() const {
        return Mat4::LookAt(Position, Target, Up);
    }

    Mat4 GetProjectionMatrix() const {
        return Mat4::Perspective(FovYRadians, Aspect, NearPlane, FarPlane);
    }

    /// Column-major, for Metal: clip = viewProj * localPosition.
    Mat4 GetViewProjectionMatrix() const {
        return GetProjectionMatrix() * GetViewMatrix();
    }

    /// Orbit around Target on the XZ plane (radians).
    void SetOrbit(float yawRadians, float distance, float height = 0.0f) {
        Position.x = Target.x + std::sin(yawRadians) * distance;
        Position.y = Target.y + height;
        Position.z = Target.z + std::cos(yawRadians) * distance;
    }
};

} // namespace Nova
