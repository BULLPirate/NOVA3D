#include <Editor/ViewportManipulator.h>

#include <Nova/Renderer/Camera.h>

#include <algorithm>
#include <cmath>

namespace Nova::Editor {

namespace {

constexpr float kRadiansForFullHeight = 1.6f;
constexpr int kRingSegments = 48;

void CircleBasis(GizmoAxis axis, Vec3& u, Vec3& v) {
    switch (axis) {
    case GizmoAxis::X:
        u = {0.0f, 1.0f, 0.0f};
        v = {0.0f, 0.0f, 1.0f};
        break;
    case GizmoAxis::Y:
        u = {1.0f, 0.0f, 0.0f};
        v = {0.0f, 0.0f, 1.0f};
        break;
    case GizmoAxis::Z:
        u = {1.0f, 0.0f, 0.0f};
        v = {0.0f, 1.0f, 0.0f};
        break;
    default:
        u = {1.0f, 0.0f, 0.0f};
        v = {0.0f, 1.0f, 0.0f};
        break;
    }
}

bool Project(const Mat4& viewProj, const Vec3& world, float vw, float vh, float& sx, float& sy) {
    return Nova::ProjectWorldToViewport(viewProj, world, vw, vh, sx, sy);
}

float PointSegmentDistance(float px, float py, float ax, float ay, float bx, float by) {
    const float abx = bx - ax;
    const float aby = by - ay;
    const float apx = px - ax;
    const float apy = py - ay;
    const float abLenSq = abx * abx + aby * aby;
    float t = 0.0f;
    if (abLenSq > 1e-8f) {
        t = std::clamp((apx * abx + apy * aby) / abLenSq, 0.0f, 1.0f);
    }
    const float dx = px - (ax + abx * t);
    const float dy = py - (ay + aby * t);
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace

void ResetViewportGizmoSession(ViewportGizmoSession& session) {
    session.Active = false;
    session.Axis = GizmoAxis::None;
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

Vec3 GizmoAxisVector(GizmoAxis axis) {
    switch (axis) {
    case GizmoAxis::X:
        return {1.0f, 0.0f, 0.0f};
    case GizmoAxis::Y:
        return {0.0f, 1.0f, 0.0f};
    case GizmoAxis::Z:
        return {0.0f, 0.0f, 1.0f};
    default:
        return {0.0f, 0.0f, 0.0f};
    }
}

float RotationRingScreenDistance(const Mat4& viewProj,
                                 const Vec3& origin,
                                 GizmoAxis axis,
                                 float radius,
                                 float mouseX,
                                 float mouseY,
                                 float viewportW,
                                 float viewportH) {
    Vec3 u;
    Vec3 v;
    CircleBasis(axis, u, v);
    float minDist = 1.0e9f;
    float prevX = 0.0f;
    float prevY = 0.0f;
    bool havePrev = false;
    for (int i = 0; i <= kRingSegments; ++i) {
        const float t = (2.0f * 3.14159265f * static_cast<float>(i)) / kRingSegments;
        const Vec3 p = origin + (u * std::cos(t) + v * std::sin(t)) * radius;
        float sx = 0.0f;
        float sy = 0.0f;
        if (!Project(viewProj, p, viewportW, viewportH, sx, sy)) {
            havePrev = false;
            continue;
        }
        if (havePrev) {
            minDist = std::min(minDist, PointSegmentDistance(mouseX, mouseY, prevX, prevY, sx, sy));
        }
        prevX = sx;
        prevY = sy;
        havePrev = true;
    }
    return minDist;
}

GizmoAxis PickRotationAxis(const Mat4& viewProj,
                           const Vec3& origin,
                           float radius,
                           float mouseX,
                           float mouseY,
                           float viewportW,
                           float viewportH,
                           float maxPixels) {
    const GizmoAxis axes[3] = {GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z};
    GizmoAxis best = GizmoAxis::None;
    float bestDist = maxPixels;
    for (GizmoAxis axis : axes) {
        const float dist = RotationRingScreenDistance(viewProj, origin, axis, radius, mouseX,
                                                      mouseY, viewportW, viewportH);
        if (dist < bestDist) {
            bestDist = dist;
            best = axis;
        }
    }
    return best;
}

GizmoAxis PickMoveAxis(const Mat4& viewProj,
                       const Vec3& origin,
                       float axisLength,
                       float mouseX,
                       float mouseY,
                       float viewportW,
                       float viewportH,
                       float maxPixels) {
    float ox = 0.0f;
    float oy = 0.0f;
    if (!Project(viewProj, origin, viewportW, viewportH, ox, oy)) {
        return GizmoAxis::None;
    }
    const GizmoAxis axes[3] = {GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z};
    GizmoAxis best = GizmoAxis::None;
    float bestDist = maxPixels;
    for (GizmoAxis axis : axes) {
        float tx = 0.0f;
        float ty = 0.0f;
        if (!Project(viewProj, origin + GizmoAxisVector(axis) * axisLength, viewportW, viewportH, tx,
                     ty)) {
            continue;
        }
        const float dist = PointSegmentDistance(mouseX, mouseY, ox, oy, tx, ty);
        if (dist < bestDist) {
            bestDist = dist;
            best = axis;
        }
    }
    return best;
}

void BeginViewportGizmoSession(ViewportGizmoSession& session,
                               GizmoAxis axis,
                               float viewportHeightPx) {
    session.Active = axis != GizmoAxis::None;
    session.Axis = axis;
    session.ViewportHeight = std::max(viewportHeightPx, 64.0f);
}

void ApplyViewportRotateAxis(ViewportGizmoSession& session,
                             Transform& transform,
                             float mouseDeltaX,
                             float mouseDeltaY,
                             const Camera& camera) {
    if (!session.Active || session.Axis == GizmoAxis::None) {
        return;
    }
    const Vec3 worldAxis = GizmoAxisVector(session.Axis);
    const Vec3 right = ManipulatorCameraRight(camera);
    const Vec3 up = ManipulatorCameraUp(camera);
    const Vec3 look = ManipulatorCameraForward(camera);
    Vec3 tangent = worldAxis.Cross(look);
    if (tangent.LengthSq() < 1e-8f) {
        tangent = right;
    }
    tangent = tangent.Normalized();
    float sx = tangent.Dot(right);
    float sy = -tangent.Dot(up);
    const float tlen = std::sqrt(sx * sx + sy * sy);
    if (tlen < 1e-4f) {
        return;
    }
    sx /= tlen;
    sy /= tlen;
    const float pixels = -(mouseDeltaX * sx + mouseDeltaY * sy);
    const float radiansPerPixel = kRadiansForFullHeight / session.ViewportHeight;
    const float angle = pixels * radiansPerPixel;
    transform.Rotation =
        (Quat::FromAxisAngle(worldAxis, angle) * transform.Rotation).Normalized();
}

void ApplyViewportMoveAxis(ViewportGizmoSession& session,
                           Transform& transform,
                           float mouseDeltaX,
                           float mouseDeltaY,
                           const Camera& camera) {
    if (!session.Active || session.Axis == GizmoAxis::None) {
        return;
    }
    const Vec3 worldAxis = GizmoAxisVector(session.Axis);
    const Vec3 right = ManipulatorCameraRight(camera);
    const Vec3 up = ManipulatorCameraUp(camera);
    const float sx = worldAxis.Dot(right);
    const float sy = -worldAxis.Dot(up);
    const float tlen = std::sqrt(sx * sx + sy * sy);
    if (tlen < 1e-4f) {
        return;
    }
    const float pixels = (mouseDeltaX * sx + mouseDeltaY * sy) / tlen;
    const float speed = 0.012f;
    transform.Position = transform.Position + worldAxis * (pixels * speed);
}

void ApplyViewportScaleAxis(ViewportGizmoSession& session,
                            Transform& transform,
                            float mouseDeltaX,
                            float mouseDeltaY,
                            const Camera& camera) {
    if (!session.Active) {
        return;
    }
    const float pixels = -mouseDeltaY + mouseDeltaX * 0.15f;
    const float factor = 1.0f + pixels * 0.01f;
    if (session.Axis == GizmoAxis::Screen || session.Axis == GizmoAxis::None) {
        transform.Scale = transform.Scale * std::max(0.05f, factor);
        return;
    }
    const Vec3 axis = GizmoAxisVector(session.Axis);
    Vec3 scale = transform.Scale;
    if (axis.x > 0.5f) {
        scale.x = std::max(0.05f, scale.x * std::max(0.05f, factor));
    }
    if (axis.y > 0.5f) {
        scale.y = std::max(0.05f, scale.y * std::max(0.05f, factor));
    }
    if (axis.z > 0.5f) {
        scale.z = std::max(0.05f, scale.z * std::max(0.05f, factor));
    }
    transform.Scale = scale;
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

void ApplyObjectTurn(Transform& transform, float mouseDeltaX, float mouseDeltaY,
                     float viewportHeightPx) {
    const float s = 2.8f / std::max(viewportHeightPx, 160.0f);
    const Quat qy = Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, mouseDeltaX * s);
    const Quat qx = Quat::FromAxisAngle({1.0f, 0.0f, 0.0f}, mouseDeltaY * s);
    transform.Rotation = (qy * qx * transform.Rotation).Normalized();
}

void ApplyOrbitLook(float& yawRadians, float& pitchRadians, float mouseDeltaX, float mouseDeltaY,
                    float radiansPerPixel) {
    yawRadians += mouseDeltaX * radiansPerPixel;
    pitchRadians = std::clamp(pitchRadians - mouseDeltaY * radiansPerPixel, 0.08f, 1.35f);
}

void ApplyOrbitDolly(float& distance, float amount, float minDistance, float maxDistance) {
    distance = std::clamp(distance - amount, minDistance, maxDistance);
}

void FrameOrbitOnBounds(float& yawRadians, float& pitchRadians, float& distance, Vec3& target,
                        const Vec3& center, float radius) {
    target = {center.x, center.y, center.z};
    pitchRadians = 0.52f;
    yawRadians = 0.35f;
    distance = std::clamp(std::max(6.0f, radius * 2.4f), 6.0f, 80.0f);
}

void FlyEditCamera(float& yawRadians, float& pitchRadians, float& distance, Vec3& target,
                   float mouseDeltaX, float mouseDeltaY, float radiansPerPixel, float wishRight,
                   float wishUp, float wishForward, float moveSpeed, float deltaSeconds) {
    yawRadians += mouseDeltaX * radiansPerPixel;
    pitchRadians += mouseDeltaY * radiansPerPixel;
    pitchRadians = std::clamp(pitchRadians, -1.55f, 1.55f);

    const float step = moveSpeed * std::max(deltaSeconds, 1.0f / 120.0f);
    distance = std::clamp(distance - wishForward * step, 0.8f, 80.0f);

    const float cp = std::cos(pitchRadians);
    const float sp = std::sin(pitchRadians);
    const float cy = std::cos(yawRadians);
    const float sy = std::sin(yawRadians);
    const Vec3 offset{distance * cp * sy, distance * sp, distance * cp * cy};
    const Vec3 forward = Vec3{-offset.x, -offset.y, -offset.z}.Normalized();
    Vec3 right = forward.Cross({0.0f, 1.0f, 0.0f});
    if (right.LengthSq() < 1e-8f) {
        right = {1.0f, 0.0f, 0.0f};
    } else {
        right = right.Normalized();
    }
    const Vec3 up = right.Cross(forward).Normalized();
    target = target + right * (wishRight * step) + up * (wishUp * step);
}

} // namespace Nova::Editor
