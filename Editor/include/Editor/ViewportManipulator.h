#pragma once

#include <Nova/Math/Transform.h>
#include <Nova/Math/Quat.h>
#include <Nova/Math/Vec.h>
#include <Nova/Renderer/Camera.h>

namespace Nova::Editor {

struct ViewportRotateSession {
    bool Active = false;
    Quat StartRotation = Quat::Identity();
    Vec3 GrabStart{0.0f, 0.0f, 1.0f};
    Vec3 FrozenCameraUp{0.0f, 1.0f, 0.0f};
    Vec3 FrozenCameraRight{1.0f, 0.0f, 0.0f};
    Vec3 FrozenTowardCamera{0.0f, 0.0f, 1.0f};
    float PivotX = 0.0f;
    float PivotY = 0.0f;
    float Radius = 160.0f;
};

void ResetViewportRotateSession(ViewportRotateSession& session);

Vec3 ManipulatorCameraForward(const Camera& camera);
Vec3 ManipulatorCameraRight(const Camera& camera);
Vec3 ManipulatorCameraUp(const Camera& camera);
Vec3 ManipulatorTowardCamera(const Camera& camera);

/// Shoemake hemisphere around the object. Screen Y is down.
Vec3 ArcballGrabVector(float mouseX,
                       float mouseY,
                       float pivotX,
                       float pivotY,
                       float radius,
                       const Vec3& viewRight,
                       const Vec3& viewUp,
                       const Vec3& towardCamera);

void BeginViewportRotateSession(ViewportRotateSession& session,
                                const Quat& startRotation,
                                float mouseX,
                                float mouseY,
                                float pivotX,
                                float pivotY,
                                float radius,
                                const Camera& camera);

/// World-space delta so the point under the cursor follows the mouse,
/// independent of which object face currently faces the camera.
void ApplyViewportRotateDrag(ViewportRotateSession& session,
                             Transform& transform,
                             float currentMouseX,
                             float currentMouseY);

} // namespace Nova::Editor
