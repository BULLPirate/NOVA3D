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

} // namespace

TEST(ViewportManipulator, RotateYTurnsFacingPointRight) {
    const Camera cam = MakeFrontCamera();
    ViewportGizmoSession session;
    BeginViewportGizmoSession(session, GizmoAxis::Y, 400.0f);
    Transform transform;
    ApplyViewportRotateAxis(session, transform, 80.0f, 0.0f, cam);
    const Vec3 moved = transform.Rotation.Rotate({0.0f, 0.0f, 1.0f});
    EXPECT_GT(moved.x, 0.05f);
    EXPECT_NEAR(moved.y, 0.0f, 0.08f);
}

TEST(ViewportManipulator, RotateXNodsFacingPointDown) {
    const Camera cam = MakeFrontCamera();
    ViewportGizmoSession session;
    BeginViewportGizmoSession(session, GizmoAxis::X, 400.0f);
    Transform transform;
    ApplyViewportRotateAxis(session, transform, 0.0f, 80.0f, cam);
    const Vec3 moved = transform.Rotation.Rotate({0.0f, 0.0f, 1.0f});
    EXPECT_LT(moved.y, -0.05f);
    EXPECT_NEAR(moved.x, 0.0f, 0.08f);
}

TEST(ViewportManipulator, WorldAxisStayWorldAfterObjectTurned) {
    const Camera cam = MakeFrontCamera();
    const Quat start = Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, Radians(90.0f));
    ViewportGizmoSession session;
    BeginViewportGizmoSession(session, GizmoAxis::X, 400.0f);
    Transform transform;
    transform.Rotation = start;
    ApplyViewportRotateAxis(session, transform, 0.0f, 90.0f, cam);
    const Quat delta = (transform.Rotation * start.Inverse()).Normalized();
    const Vec3 moved = delta.Rotate({0.0f, 0.0f, 1.0f});
    EXPECT_LT(moved.y, -0.05f);
    EXPECT_NEAR(moved.x, 0.0f, 0.08f);
}

TEST(ViewportManipulator, MoveXOnlyChangesX) {
    const Camera cam = MakeFrontCamera();
    ViewportGizmoSession session;
    BeginViewportGizmoSession(session, GizmoAxis::X, 400.0f);
    Transform transform;
    ApplyViewportMoveAxis(session, transform, 50.0f, 40.0f, cam);
    EXPECT_GT(transform.Position.x, 0.05f);
    EXPECT_NEAR(transform.Position.y, 0.0f, 1e-4f);
    EXPECT_NEAR(transform.Position.z, 0.0f, 1e-4f);
}

TEST(ViewportManipulator, ScaleYOnlyChangesY) {
    const Camera cam = MakeFrontCamera();
    ViewportGizmoSession session;
    BeginViewportGizmoSession(session, GizmoAxis::Y, 400.0f);
    Transform transform;
    ApplyViewportScaleAxis(session, transform, 0.0f, -40.0f, cam);
    EXPECT_NEAR(transform.Scale.x, 1.0f, 1e-4f);
    EXPECT_GT(transform.Scale.y, 1.05f);
    EXPECT_NEAR(transform.Scale.z, 1.0f, 1e-4f);
}

TEST(ViewportManipulator, InactiveSessionDoesNothing) {
    const Camera cam = MakeFrontCamera();
    ViewportGizmoSession session;
    Transform transform;
    ApplyViewportRotateAxis(session, transform, 80.0f, 80.0f, cam);
    EXPECT_FLOAT_EQ(transform.Rotation.w, 1.0f);
}

TEST(ViewportManipulator, SnapPositionToQuarterGrid) {
    const Vec3 snapped = SnapPositionToGrid({0.12f, 0.38f, -0.06f}, 0.25f);
    EXPECT_NEAR(snapped.x, 0.0f, 1e-5f);
    EXPECT_NEAR(snapped.y, 0.5f, 1e-5f);
    EXPECT_NEAR(snapped.z, 0.0f, 1e-5f);
}

TEST(ViewportManipulator, EditModeTickDoesNotSpinCubeWithoutPlay) {
    Scene scene;
    Entity cube = scene.CreateEntity("Cube");
    scene.AddMeshRenderer(cube);
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

TEST(ViewportManipulator, ObjectTurnYawsOnMouseX) {
    Transform transform;
    ApplyObjectTurn(transform, 80.0f, 0.0f, 400.0f);
    const Vec3 moved = transform.Rotation.Rotate({0.0f, 0.0f, 1.0f});
    EXPECT_GT(moved.x, 0.05f);
}

TEST(ViewportManipulator, FlyCameraDolliesInWithForwardWish) {
    float yaw = 0.0f;
    float pitch = 0.0f;
    float distance = 8.0f;
    Vec3 target{0.0f, 0.0f, 0.0f};
    FlyEditCamera(yaw, pitch, distance, target, 0.0f, 0.0f, 0.01f, 0.0f, 0.0f, 1.0f, 8.0f, 0.25f);
    EXPECT_LT(distance, 7.5f);
    EXPECT_NEAR(target.x, 0.0f, 1e-4f);
    EXPECT_NEAR(target.z, 0.0f, 1e-4f);
}

TEST(Scene, ShowAxesOffByDefault) {
    Scene scene = Scene::CreatePlayableLevel();
    EXPECT_FALSE(scene.GetShowAxes(scene.FindEntityByName("Player")));
    EXPECT_FALSE(scene.GetShowAxes(scene.FindEntityByName("Ground")));
}
