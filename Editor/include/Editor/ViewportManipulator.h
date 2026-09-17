#pragma once

#include <Nova/Math/Transform.h>
#include <Nova/Math/Quat.h>
#include <Nova/Math/Vec.h>
#include <Nova/Renderer/Camera.h>

namespace Nova::Editor {

/// Screen-space inspect: each mouse step rotates around the camera's current
/// screen axes, not the object's local axes and not world-Y from the first face.
/// After any other side is toward the camera, the next drag still follows the mouse.
struct ViewportRotateSession {
    bool Active = false;
    float ViewportHeight = 400.0f;
};

void ResetViewportRotateSession(ViewportRotateSession& session);

Vec3 ManipulatorCameraForward(const Camera& camera);
Vec3 ManipulatorCameraRight(const Camera& camera);
Vec3 ManipulatorCameraUp(const Camera& camera);

void FilterNearlyCardinalDelta(float& dx, float& dy);

Quat RotationFromMouseDelta(const Vec3& yawAxis,
                            const Vec3& pitchAxis,
                            float dragPixelsX,
                            float dragPixelsY,
                            float viewportHeightPx);

void BeginViewportRotateSession(ViewportRotateSession& session, float viewportHeightPx);

void ApplyViewportRotateDrag(ViewportRotateSession& session,
                             Transform& transform,
                             float mouseDeltaX,
                             float mouseDeltaY,
                             const Camera& camera);

constexpr float kDefaultMoveGrid = 0.25f;

float SnapScalarToGrid(float value, float grid);
Vec3 SnapPositionToGrid(const Vec3& position, float grid = kDefaultMoveGrid);

} // namespace Nova::Editor
