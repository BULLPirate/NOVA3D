#include <Editor/ViewportManipulator.h>

#include <algorithm>
#include <cmath>

namespace Nova::Editor {

namespace {

constexpr float kCardinalRatio = 1.08f;
constexpr float kRadiansForFullHeight = 1.6f;

} // namespace

void ResetViewportRotateSession(ViewportRotateSession& session) {
    session.Active = false;
}

Vec3 ManipulatorCameraForward(const Camera& camera) {
    return (camera.Target - camera.Position).Normalized();
}

Vec3 ManipulatorCameraRight(const Camera& camera) {
    const Vec3 forward = ManipulatorCameraForward(camera);
    Vec3 right = forward.Cross(camera.Up);
    if (right.LengthSq() < 1e-10f) {
        right = forward.Cross({1.0f, 0.0f, 0.0f});
        if (right.LengthSq() < 1e-10f) {
            return {1.0f, 0.0f, 0.0f};
        }
    }
    return right.Normalized();
}

Vec3 ManipulatorCameraUp(const Camera& camera) {
    return ManipulatorCameraRight(camera).Cross(ManipulatorCameraForward(camera)).Normalized();
}

void FilterNearlyCardinalDelta(float& dx, float& dy) {
    const float ax = std::fabs(dx);
    const float ay = std::fabs(dy);
    if (ax > kCardinalRatio * ay) {
        dy = 0.0f;
    } else if (ay > kCardinalRatio * ax) {
        dx = 0.0f;
    }
}

Quat RotationFromMouseDelta(const Vec3& yawAxis,
                            const Vec3& pitchAxis,
                            float dragPixelsX,
                            float dragPixelsY,
                            float viewportHeightPx) {
    FilterNearlyCardinalDelta(dragPixelsX, dragPixelsY);

    const float radiansPerPixel = kRadiansForFullHeight / std::max(viewportHeightPx, 128.0f);
    const float yaw = dragPixelsX * radiansPerPixel;
    const float pitch = dragPixelsY * radiansPerPixel;
    const Vec3 omega = yawAxis * yaw + pitchAxis * pitch;
    const float angle = omega.Length();
    if (angle < 1e-8f) {
        return Quat::Identity();
    }
    return Quat::FromAxisAngle(omega / angle, angle);
}

void BeginViewportRotateSession(ViewportRotateSession& session, float viewportHeightPx) {
    session.Active = true;
    session.ViewportHeight = std::max(viewportHeightPx, 64.0f);
}

void ApplyViewportRotateDrag(ViewportRotateSession& session,
                             Transform& transform,
                             float mouseDeltaX,
                             float mouseDeltaY,
                             const Camera& camera) {
    if (!session.Active) {
        return;
    }
    const float clampPx = std::max(session.ViewportHeight * 0.2f, 24.0f);
    const float dx = std::clamp(mouseDeltaX, -clampPx, clampPx);
    const float dy = std::clamp(mouseDeltaY, -clampPx, clampPx);

    const Vec3 yawAxis = ManipulatorCameraUp(camera);
    const Vec3 pitchAxis = ManipulatorCameraRight(camera);
    const Quat delta =
        RotationFromMouseDelta(yawAxis, pitchAxis, dx, dy, session.ViewportHeight);
    // World multiply: the increment lives on the camera, never on the cube's
    // leftover local axes from the previous face.
    transform.Rotation = (delta * transform.Rotation).Normalized();
}

float SnapScalarToGrid(float value, float grid) {
    if (grid <= 1e-6f) {
        return value;
    }
    return std::round(value / grid) * grid;
}

Vec3 SnapPositionToGrid(const Vec3& position, float grid) {
    return {SnapScalarToGrid(position.x, grid), SnapScalarToGrid(position.y, grid),
            SnapScalarToGrid(position.z, grid)};
}

} // namespace Nova::Editor
