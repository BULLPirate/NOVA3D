#pragma once

#include <Nova/Math/Transform.h>
#include <Nova/Math/Quat.h>
#include <Nova/Math/Vec.h>
#include <Nova/Math/Mat4.h>
#include <Nova/Renderer/Camera.h>

namespace Nova::Editor {

enum class GizmoAxis { None = 0, X, Y, Z, Screen };

struct ViewportGizmoSession {
    bool Active = false;
    GizmoAxis Axis = GizmoAxis::None;
    float ViewportHeight = 400.0f;
};

void ResetViewportGizmoSession(ViewportGizmoSession& session);

Vec3 ManipulatorCameraForward(const Camera& camera);
Vec3 ManipulatorCameraRight(const Camera& camera);
Vec3 ManipulatorCameraUp(const Camera& camera);

Vec3 GizmoAxisVector(GizmoAxis axis);

/// Pixel distance from mouse to the projected world-space circle around `origin`.
float RotationRingScreenDistance(const Mat4& viewProj,
                                 const Vec3& origin,
                                 GizmoAxis axis,
                                 float radius,
                                 float mouseX,
                                 float mouseY,
                                 float viewportW,
                                 float viewportH);

GizmoAxis PickRotationAxis(const Mat4& viewProj,
                           const Vec3& origin,
                           float radius,
                           float mouseX,
                           float mouseY,
                           float viewportW,
                           float viewportH,
                           float maxPixels = 14.0f);

GizmoAxis PickMoveAxis(const Mat4& viewProj,
                       const Vec3& origin,
                       float axisLength,
                       float mouseX,
                       float mouseY,
                       float viewportW,
                       float viewportH,
                       float maxPixels = 12.0f);

void BeginViewportGizmoSession(ViewportGizmoSession& session,
                               GizmoAxis axis,
                               float viewportHeightPx);

/// Rotate around one world axis (Unity/Godot gizmo). Never inspect-tumble.
void ApplyViewportRotateAxis(ViewportGizmoSession& session,
                             Transform& transform,
                             float mouseDeltaX,
                             float mouseDeltaY,
                             const Camera& camera);

void ApplyViewportMoveAxis(ViewportGizmoSession& session,
                           Transform& transform,
                           float mouseDeltaX,
                           float mouseDeltaY,
                           const Camera& camera);

void ApplyViewportScaleAxis(ViewportGizmoSession& session,
                            Transform& transform,
                            float mouseDeltaX,
                            float mouseDeltaY,
                            const Camera& camera);

constexpr float kDefaultMoveGrid = 0.25f;

float SnapScalarToGrid(float value, float grid);
Vec3 SnapPositionToGrid(const Vec3& position, float grid = kDefaultMoveGrid);

void ApplyObjectTurn(Transform& transform, float mouseDeltaX, float mouseDeltaY,
                     float viewportHeightPx);

void ApplyOrbitLook(float& yawRadians, float& pitchRadians, float mouseDeltaX, float mouseDeltaY,
                    float radiansPerPixel);
void ApplyOrbitDolly(float& distance, float amount, float minDistance, float maxDistance);
void FrameOrbitOnBounds(float& yawRadians, float& pitchRadians, float& distance, Vec3& target,
                        const Vec3& center, float radius);

/// RMB look around a pivot. W/S approach or leave the subject, A/D and Q/E pan the pivot.
void FlyEditCamera(float& yawRadians, float& pitchRadians, float& distance, Vec3& target,
                   float mouseDeltaX, float mouseDeltaY, float radiansPerPixel, float wishRight,
                   float wishUp, float wishForward, float moveSpeed, float deltaSeconds);

} // namespace Nova::Editor
