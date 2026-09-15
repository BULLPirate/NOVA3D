#include <Editor/ViewportManipulator.h>

#include <gtest/gtest.h>

#include <Nova/Math/Math.h>

using namespace Nova;
using namespace Nova::Editor;

namespace {

Camera MakeFrontCamera() {
    Camera cam;
    cam.Position = {0.0f, 0.0f, 5.0f};
    cam.Target = {0.0f, 0.0f, 0.0f};
    cam.Up = {0.0f, 1.0f, 0.0f};
    return cam;
}

Quat WorldDelta(const ViewportRotateSession& session, const Transform& after) {
    return (after.Rotation * session.StartRotation.Inverse()).Normalized();
}

} // namespace

TEST(ViewportManipulator, CenterDragRightMovesFacingPointRight) {
    const Camera cam = MakeFrontCamera();
    ViewportRotateSession session;
    BeginViewportRotateSession(session, Quat::Identity(), 200.0f, 200.0f, 200.0f, 200.0f, 160.0f,
                               cam);

    Transform transform;
    ApplyViewportRotateDrag(session, transform, 280.0f, 200.0f);
    const Vec3 moved = transform.Rotation.Rotate({0.0f, 0.0f, 1.0f});
    EXPECT_GT(moved.x, 0.05f);
    EXPECT_NEAR(moved.y, 0.0f, 0.08f);
}

TEST(ViewportManipulator, CenterDragDownMovesFacingPointDown) {
    const Camera cam = MakeFrontCamera();
    ViewportRotateSession session;
    BeginViewportRotateSession(session, Quat::Identity(), 200.0f, 200.0f, 200.0f, 200.0f, 160.0f,
                               cam);

    Transform transform;
    ApplyViewportRotateDrag(session, transform, 200.0f, 280.0f);
    const Vec3 moved = transform.Rotation.Rotate({0.0f, 0.0f, 1.0f});
    EXPECT_LT(moved.y, -0.05f);
    EXPECT_NEAR(moved.x, 0.0f, 0.08f);
}

TEST(ViewportManipulator, SameMousePathSameWorldDeltaAfterObjectTurned) {
    const Camera cam = MakeFrontCamera();
    const Quat turned =
        Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, Radians(90.0f)) *
        Quat::FromAxisAngle({1.0f, 0.0f, 0.0f}, Radians(35.0f));

    ViewportRotateSession a;
    ViewportRotateSession b;
    BeginViewportRotateSession(a, Quat::Identity(), 200.0f, 200.0f, 200.0f, 200.0f, 160.0f, cam);
    BeginViewportRotateSession(b, turned, 200.0f, 200.0f, 200.0f, 200.0f, 160.0f, cam);

    Transform ta;
    Transform tb;
    tb.Rotation = turned;
    ApplyViewportRotateDrag(a, ta, 260.0f, 250.0f);
    ApplyViewportRotateDrag(b, tb, 260.0f, 250.0f);

    const Quat deltaA = WorldDelta(a, ta);
    const Quat deltaB = WorldDelta(b, tb);
    EXPECT_GT(std::abs(deltaA.Dot(deltaB)), 0.999f);
}

TEST(ViewportManipulator, GrabVectorFollowsScreenAxes) {
    const Camera cam = MakeFrontCamera();
    const Vec3 right = ManipulatorCameraRight(cam);
    const Vec3 up = ManipulatorCameraUp(cam);
    const Vec3 toward = ManipulatorTowardCamera(cam);

    const Vec3 center = ArcballGrabVector(200.0f, 200.0f, 200.0f, 200.0f, 160.0f, right, up, toward);
    const Vec3 rightGrab =
        ArcballGrabVector(280.0f, 200.0f, 200.0f, 200.0f, 160.0f, right, up, toward);
    const Vec3 downGrab =
        ArcballGrabVector(200.0f, 280.0f, 200.0f, 200.0f, 160.0f, right, up, toward);

    EXPECT_GT(center.Dot(toward), 0.9f);
    EXPECT_GT(rightGrab.Dot(right), 0.2f);
    EXPECT_LT(downGrab.Dot(up), -0.2f);
}
