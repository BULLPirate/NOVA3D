#include <Editor/ViewportManipulator.h>

#include <algorithm>
#include <cmath>

namespace Nova::Editor {

void ResetViewportRotateSession(ViewportRotateSession& session) {
    session.Active = false;
}

Vec3 ManipulatorCameraForward(const Camera& camera) {
    return (camera.Target - camera.Position).Normalized();
}

Vec3 ManipulatorCameraRight(const Camera& camera) {
    return ManipulatorCameraForward(camera).Cross(camera.Up).Normalized();
}

Vec3 ManipulatorCameraUp(const Camera& camera) {
    return ManipulatorCameraRight(camera).Cross(ManipulatorCameraForward(camera)).Normalized();
}

Vec3 ManipulatorTowardCamera(const Camera& camera) {
    const Vec3 forward = ManipulatorCameraForward(camera);
    return {-forward.x, -forward.y, -forward.z};
}

Vec3 ArcballGrabVector(float mouseX,
                       float mouseY,
                       float pivotX,
                       float pivotY,
                       float radius,
                       const Vec3& viewRight,
                       const Vec3& viewUp,
                       const Vec3& towardCamera) {
    const float safeRadius = std::max(radius, 32.0f);
    float x = (mouseX - pivotX) / safeRadius;
    float y = (pivotY - mouseY) / safeRadius;
    const float len2 = x * x + y * y;
    float z = 0.0f;
    if (len2 > 1.0f) {
        const float inv = 1.0f / std::sqrt(len2);
        x *= inv;
        y *= inv;
    } else {
        z = std::sqrt(1.0f - len2);
    }
    return (viewRight * x + viewUp * y + towardCamera * z).Normalized();
}

void BeginViewportRotateSession(ViewportRotateSession& session,
                                const Quat& startRotation,
                                float mouseX,
                                float mouseY,
                                float pivotX,
                                float pivotY,
                                float radius,
                                const Camera& camera) {
    session.Active = true;
    session.StartRotation = startRotation;
    session.FrozenCameraRight = ManipulatorCameraRight(camera);
    session.FrozenCameraUp = ManipulatorCameraUp(camera);
    session.FrozenTowardCamera = ManipulatorTowardCamera(camera);
    session.PivotX = pivotX;
    session.PivotY = pivotY;
    session.Radius = std::max(radius, 32.0f);
    session.GrabStart = ArcballGrabVector(mouseX, mouseY, pivotX, pivotY, session.Radius,
                                          session.FrozenCameraRight, session.FrozenCameraUp,
                                          session.FrozenTowardCamera);
}

void ApplyViewportRotateDrag(ViewportRotateSession& session,
                             Transform& transform,
                             float currentMouseX,
                             float currentMouseY) {
    if (!session.Active) {
        return;
    }
    const Vec3 grabNow =
        ArcballGrabVector(currentMouseX, currentMouseY, session.PivotX, session.PivotY,
                          session.Radius, session.FrozenCameraRight, session.FrozenCameraUp,
                          session.FrozenTowardCamera);
    const Quat delta = Quat::ShortestRotation(session.GrabStart, grabNow);
    transform.Rotation = (delta * session.StartRotation).Normalized();
}

} // namespace Nova::Editor
