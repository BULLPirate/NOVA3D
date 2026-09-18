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
        return Mat4::PerspectiveMetal(FovYRadians, Aspect, NearPlane, FarPlane);
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

/// Third-person orbit: yaw around world Y, pitch from the horizon (0 = level, + = above).
inline Vec3 OrbitEyePosition(const Vec3& target, float yawRadians, float pitchRadians,
                             float distance) {
    const float cp = std::cos(pitchRadians);
    const float sp = std::sin(pitchRadians);
    const float sy = std::sin(yawRadians);
    const float cy = std::cos(yawRadians);
    return {target.x + distance * cp * sy, target.y + distance * sp,
            target.z + distance * cp * cy};
}

/// Maps world position to viewport pixel coordinates (top-left origin). Returns false if behind camera.
bool ProjectWorldToViewport(const Mat4& viewProjection,
                            const Vec3& world,
                            float viewportWidth,
                            float viewportHeight,
                            float& outX,
                            float& outY);

struct Ray {
    Vec3 Origin{0.0f, 0.0f, 0.0f};
    Vec3 Direction{0.0f, 0.0f, -1.0f};
};

/// Pixel coords: origin top-left, Y down. Metal clip Z in [0, 1].
bool ViewportPointToRay(const Camera& camera,
                        float pixelX,
                        float pixelY,
                        float viewportWidth,
                        float viewportHeight,
                        Ray& outRay);

} // namespace Nova
