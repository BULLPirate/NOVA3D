#include <Editor/ViewportManipulator.h>

#include <gtest/gtest.h>

#include <Nova/Math/Math.h>
#include <Nova/Scene/Scene.h>
#include <Nova/Scene/SceneRuntime.h>

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

Quat WorldDelta(const Quat& start, const Quat& after) {
    return (after * start.Inverse()).Normalized();
}

Camera MakeEditorOrbitCamera() {
    Camera cam;
    const float pitch = 0.25f;
    const float distance = 3.3f;
    cam.Target = {0.0f, 0.0f, 0.0f};
    cam.Position = {0.0f, distance * std::sin(pitch), distance * std::cos(pitch)};
    cam.Up = {0.0f, 1.0f, 0.0f};
    return cam;
}

void ExpectFacingFollowsScreen(const char* label,
                               const Camera& cam,
                               const Quat& startRotation,
                               float dx,
                               float dy) {
    ViewportRotateSession session;
    BeginViewportRotateSession(session, 400.0f);
    Transform transform;
    transform.Rotation = startRotation;
    ApplyViewportRotateDrag(session, transform, dx, dy, cam);

    const Quat delta = WorldDelta(startRotation, transform.Rotation);
    const Vec3 toward = (cam.Position - cam.Target).Normalized();
    const Vec3 motion = delta.Rotate(toward) - toward;
    const Vec3 right = ManipulatorCameraRight(cam);
    const Vec3 up = ManipulatorCameraUp(cam);

    if (dx > 0.0f && dy == 0.0f) {
        EXPECT_GT(motion.Dot(right), 0.02f) << label;
        EXPECT_NEAR(motion.Dot(up), 0.0f, 0.04f) << label;
    }
    if (dy > 0.0f && dx == 0.0f) {
        EXPECT_LT(motion.Dot(up), -0.02f) << label;
        EXPECT_NEAR(motion.Dot(right), 0.0f, 0.04f) << label;
    }
}

} // namespace

TEST(ViewportManipulator, DragRightMovesFacingPointRight) {
    const Camera cam = MakeFrontCamera();
    ViewportRotateSession session;
    BeginViewportRotateSession(session, 400.0f);

    Transform transform;
    ApplyViewportRotateDrag(session, transform, 80.0f, 0.0f, cam);
    const Vec3 moved = transform.Rotation.Rotate({0.0f, 0.0f, 1.0f});
    EXPECT_GT(moved.x, 0.05f);
    EXPECT_NEAR(moved.y, 0.0f, 0.08f);
}

TEST(ViewportManipulator, DragDownMovesFacingPointDown) {
    const Camera cam = MakeFrontCamera();
    ViewportRotateSession session;
    BeginViewportRotateSession(session, 400.0f);

    Transform transform;
    ApplyViewportRotateDrag(session, transform, 0.0f, 80.0f, cam);
    const Vec3 moved = transform.Rotation.Rotate({0.0f, 0.0f, 1.0f});
    EXPECT_LT(moved.y, -0.05f);
    EXPECT_NEAR(moved.x, 0.0f, 0.08f);
}

TEST(ViewportManipulator, DragDownWithSidewaysNoiseStaysVertical) {
    const Camera cam = MakeFrontCamera();
    ViewportRotateSession session;
    BeginViewportRotateSession(session, 400.0f);

    Transform transform;
    ApplyViewportRotateDrag(session, transform, 35.0f, 80.0f, cam);
    const Vec3 moved = transform.Rotation.Rotate({0.0f, 0.0f, 1.0f});
    EXPECT_LT(moved.y, -0.05f);
    EXPECT_NEAR(moved.x, 0.0f, 0.08f);
}

TEST(ViewportManipulator, DiagonalDragMovesFacingPointWithTheCursor) {
    const Camera cam = MakeFrontCamera();
    ViewportRotateSession session;
    BeginViewportRotateSession(session, 400.0f);

    Transform transform;
    ApplyViewportRotateDrag(session, transform, 90.0f, 90.0f, cam);
    const Vec3 moved = transform.Rotation.Rotate({0.0f, 0.0f, 1.0f});
    EXPECT_GT(moved.x, 0.05f);
    EXPECT_LT(moved.y, -0.05f);
}

TEST(ViewportManipulator, NearlyHorizontalDragIgnoresTinyVerticalNoise) {
    float dx = 80.0f;
    float dy = 8.0f;
    FilterNearlyCardinalDelta(dx, dy);
    EXPECT_FLOAT_EQ(dx, 80.0f);
    EXPECT_FLOAT_EQ(dy, 0.0f);
}

TEST(ViewportManipulator, DownishDragFiltersHorizontal) {
    float dx = 40.0f;
    float dy = 80.0f;
    FilterNearlyCardinalDelta(dx, dy);
    EXPECT_FLOAT_EQ(dx, 0.0f);
    EXPECT_FLOAT_EQ(dy, 80.0f);
}

TEST(ViewportManipulator, SameMouseDeltaSameRotationWhereverYouClick) {
    const Camera cam = MakeFrontCamera();
    ViewportRotateSession a;
    ViewportRotateSession b;
    BeginViewportRotateSession(a, 400.0f);
    BeginViewportRotateSession(b, 400.0f);

    Transform ta;
    Transform tb;
    ApplyViewportRotateDrag(a, ta, 60.0f, 55.0f, cam);
    ApplyViewportRotateDrag(b, tb, 60.0f, 55.0f, cam);

    EXPECT_GT(std::abs(ta.Rotation.Dot(tb.Rotation)), 0.999f);
}

TEST(ViewportManipulator, MouseStepUsesCameraAxesNotObjectAxes) {
    const Camera cam = MakeFrontCamera();
    const Quat poses[] = {
        Quat::Identity(),
        Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, Radians(90.0f)),
        Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, Radians(-90.0f)),
        Quat::FromAxisAngle({1.0f, 0.0f, 0.0f}, Radians(90.0f)),
        Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, Radians(90.0f)) *
            Quat::FromAxisAngle({1.0f, 0.0f, 0.0f}, Radians(35.0f)),
    };

    Transform identity;
    ViewportRotateSession identitySession;
    BeginViewportRotateSession(identitySession, 400.0f);
    ApplyViewportRotateDrag(identitySession, identity, 70.0f, 55.0f, cam);
    const Quat expectedDelta = WorldDelta(Quat::Identity(), identity.Rotation);

    for (const Quat& pose : poses) {
        ViewportRotateSession session;
        BeginViewportRotateSession(session, 400.0f);
        Transform transform;
        transform.Rotation = pose;
        ApplyViewportRotateDrag(session, transform, 70.0f, 55.0f, cam);
        const Quat delta = WorldDelta(pose, transform.Rotation);
        EXPECT_GT(std::abs(delta.Dot(expectedDelta)), 0.999f);
    }
}

TEST(ViewportManipulator, SameMousePathSameFacingMotionAfterObjectTurned) {
    const Camera cam = MakeFrontCamera();
    const Quat turned =
        Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, Radians(90.0f)) *
        Quat::FromAxisAngle({1.0f, 0.0f, 0.0f}, Radians(35.0f));
    ExpectFacingFollowsScreen("identity-right", cam, Quat::Identity(), 80.0f, 0.0f);
    ExpectFacingFollowsScreen("turned-right", cam, turned, 80.0f, 0.0f);
    ExpectFacingFollowsScreen("identity-down", cam, Quat::Identity(), 0.0f, 80.0f);
    ExpectFacingFollowsScreen("turned-down", cam, turned, 0.0f, 80.0f);
}

TEST(ViewportManipulator, EditorCameraFacingFollowsMouseForEveryCubePose) {
    const Camera cam = MakeEditorOrbitCamera();
    const Quat poses[] = {
        Quat::Identity(),
        Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, Radians(90.0f)),
        Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, Radians(-90.0f)),
        Quat::FromAxisAngle({1.0f, 0.0f, 0.0f}, Radians(90.0f)),
        Quat::FromAxisAngle({1.0f, 0.0f, 0.0f}, Radians(-90.0f)),
        Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, Radians(90.0f)) *
            Quat::FromAxisAngle({1.0f, 0.0f, 0.0f}, Radians(35.0f)),
    };
    for (const Quat& pose : poses) {
        ExpectFacingFollowsScreen("right", cam, pose, 90.0f, 0.0f);
        ExpectFacingFollowsScreen("down", cam, pose, 0.0f, 90.0f);
    }
}

TEST(ViewportManipulator, SideFaceThenNewStrokeDownIsStillScreenVertical) {
    const Camera cam = MakeEditorOrbitCamera();
    Transform transform;
    ViewportRotateSession turn;
    BeginViewportRotateSession(turn, 400.0f);
    ApplyViewportRotateDrag(turn, transform, 200.0f, 0.0f, cam);

    ViewportRotateSession nod;
    BeginViewportRotateSession(nod, 400.0f);
    const Quat afterTurn = transform.Rotation;
    ApplyViewportRotateDrag(nod, transform, 0.0f, 90.0f, cam);

    const Vec3 toward = (cam.Position - cam.Target).Normalized();
    const Vec3 motion = WorldDelta(afterTurn, transform.Rotation).Rotate(toward) - toward;
    EXPECT_LT(motion.Dot(ManipulatorCameraUp(cam)), -0.02f);
    EXPECT_NEAR(motion.Dot(ManipulatorCameraRight(cam)), 0.0f, 0.04f);
}

TEST(ViewportManipulator, AfterOtherFaceTowardCameraVerticalDragPitchesWorldFacing) {
    const Camera cam = MakeFrontCamera();
    const Quat start = Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, Radians(90.0f));
    Transform transform;
    transform.Rotation = start;

    ViewportRotateSession session;
    BeginViewportRotateSession(session, 400.0f);
    ApplyViewportRotateDrag(session, transform, 0.0f, 100.0f, cam);

    const Vec3 facingMoved = WorldDelta(start, transform.Rotation).Rotate({0.0f, 0.0f, 1.0f});
    EXPECT_LT(facingMoved.y, -0.05f);
    EXPECT_NEAR(facingMoved.x, 0.0f, 0.08f);
}

TEST(ViewportManipulator, TurnSidewaysThenDownInOneStrokeStaysControlled) {
    const Camera cam = MakeFrontCamera();
    ViewportRotateSession session;
    BeginViewportRotateSession(session, 400.0f);

    Transform transform;
    ApplyViewportRotateDrag(session, transform, 180.0f, 0.0f, cam);
    const Vec3 afterYaw = transform.Rotation.Rotate({0.0f, 0.0f, 1.0f});
    EXPECT_GT(afterYaw.x, 0.2f);

    ApplyViewportRotateDrag(session, transform, 0.0f, 110.0f, cam);
    const Vec3 facing = transform.Rotation.Rotate({0.0f, 0.0f, 1.0f});
    EXPECT_LT(facing.y, afterYaw.y - 0.05f);
}

TEST(ViewportManipulator, EditModeTickDoesNotSpinCubeWithoutPlay) {
    Scene scene;
    Entity cube = scene.CreateEntity("Cube");
    scene.AddMeshRenderer(cube);
    RotatorComponent rot;
    rot.AngularVelocity = {0.0f, 4.0f, 0.0f};
    scene.AddRotator(cube, rot);
    const Quat before = scene.GetTransform(cube).Rotation;
    (void)before;
    EXPECT_FLOAT_EQ(scene.GetTransform(cube).Rotation.w, 1.0f);
}

TEST(ViewportManipulator, RotatorOnlyChangesWhenTickSceneRuns) {
    Scene scene;
    Entity cube = scene.CreateEntity("Cube");
    RotatorComponent rot;
    rot.AngularVelocity = {0.0f, 2.0f, 0.0f};
    scene.AddRotator(cube, rot);
    const Quat idle = scene.GetTransform(cube).Rotation;
    TickScene(scene, 0.5f);
    EXPECT_LT(std::abs(scene.GetTransform(cube).Rotation.Dot(idle)), 0.999f);
}

TEST(ViewportManipulator, SnapPositionToQuarterGrid) {
    const Vec3 snapped = SnapPositionToGrid({0.12f, 0.38f, -0.06f}, 0.25f);
    EXPECT_NEAR(snapped.x, 0.0f, 1e-5f);
    EXPECT_NEAR(snapped.y, 0.5f, 1e-5f);
    EXPECT_NEAR(snapped.z, 0.0f, 1e-5f);
}
