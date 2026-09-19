#include <gtest/gtest.h>

#include <cmath>

#include <Nova/Scene/Placement.h>
#include <Nova/Scene/Gameplay.h>
#include <Nova/Scene/ScenePicking.h>
#include <Nova/Renderer/Camera.h>
#include <Editor/ViewportManipulator.h>

TEST(Placement, SpawnsSpherePrimitive) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    const Nova::Entity ball =
        Nova::SpawnPlaceable(scene, Nova::PlaceableKind::Sphere, {1.0f, 0.0f, 2.0f});
    ASSERT_TRUE(ball.IsValid());
    EXPECT_TRUE(scene.HasMeshRenderer(ball));
    EXPECT_EQ(scene.GetMeshRenderer(ball).Primitive, Nova::MeshPrimitive::UnitSphere);
}

TEST(Placement, SpawnsHouseWithCollider) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    Nova::SpawnGround(scene);
    const Nova::Entity house = Nova::SpawnPlaceable(scene, Nova::PlaceableKind::House, {2.0f, 0.0f, 3.0f});
    ASSERT_TRUE(house.IsValid());
    EXPECT_TRUE(scene.HasCollider(house));
    EXPECT_NEAR(scene.GetTransform(house).Position.x, 2.0f, 0.26f);
    EXPECT_NEAR(scene.GetTransform(house).Position.z, 3.0f, 0.26f);
}

TEST(Placement, UniqueNamesOnRepeat) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    const Nova::Entity a = Nova::SpawnPlaceable(scene, Nova::PlaceableKind::Cube, {0.0f, 0.0f, 0.0f});
    const Nova::Entity b = Nova::SpawnPlaceable(scene, Nova::PlaceableKind::Cube, {1.0f, 0.0f, 0.0f});
    EXPECT_NE(scene.GetName(a), scene.GetName(b));
}

TEST(Placement, HouseKeepsRoofAsChild) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    Nova::SpawnGround(scene);
    const Nova::Entity house = Nova::SpawnPlaceable(scene, Nova::PlaceableKind::House, {0.0f, 0.0f, 0.0f});
    int children = 0;
    scene.ForEachEntity([&](Nova::Entity entity) {
        if (scene.GetParent(entity).Id == house.Id) {
            ++children;
        }
    });
    EXPECT_GE(children, 1);
    const Nova::Vec3 before = scene.GetTransform(house).Position;
    scene.GetTransform(house).Position.x += 4.0f;
    bool roofFollowed = false;
    scene.ForEachEntity([&](Nova::Entity entity) {
        if (scene.GetParent(entity).Id != house.Id) {
            return;
        }
        const Nova::Mat4 world = scene.GetWorldMatrix(entity);
        EXPECT_NEAR(world.m[3][0], before.x + 4.0f, 0.8f);
        roofFollowed = true;
    });
    EXPECT_TRUE(roofFollowed);
}

TEST(Placement, YawTurnsWall) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    const Nova::Entity wall =
        Nova::SpawnPlaceable(scene, Nova::PlaceableKind::Wall, {0.0f, 0.0f, 0.0f}, 1.5707963f);
    EXPECT_GT(std::abs(scene.GetTransform(wall).Rotation.y), 0.5f);
}

TEST(Placement, StrokeSpacing) {
    EXPECT_FALSE(Nova::ShouldStampAlongStroke({0.0f, 0.0f, 0.0f}, {0.4f, 0.0f, 0.0f}, 1.0f));
    EXPECT_TRUE(Nova::ShouldStampAlongStroke({0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, 1.0f));
    EXPECT_FALSE(Nova::PlaceableAllowsStroke(Nova::PlaceableKind::Knight));
    EXPECT_TRUE(Nova::PlaceableAllowsStroke(Nova::PlaceableKind::Road));
}

TEST(Placement, PreviewFollowsYaw) {
    const Nova::PlaceablePreview a =
        Nova::QueryPlaceablePreview(Nova::PlaceableKind::Wall, {0.0f, 0.0f, 0.0f}, 0.0f);
    const Nova::PlaceablePreview b =
        Nova::QueryPlaceablePreview(Nova::PlaceableKind::Wall, {0.0f, 0.0f, 0.0f}, 1.5707963f);
    EXPECT_GT(a.HalfExtents.x, a.HalfExtents.z);
    EXPECT_GT(b.HalfExtents.z, b.HalfExtents.x);
}

TEST(Placement, KnightAndBanditAreCharacters) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    Nova::SpawnGround(scene);
    const Nova::Entity knight = Nova::SpawnPlaceable(scene, Nova::PlaceableKind::Knight, {0.0f, 0.0f, 0.0f});
    const Nova::Entity bandit = Nova::SpawnPlaceable(scene, Nova::PlaceableKind::Bandit, {2.0f, 0.0f, 0.0f});
    EXPECT_TRUE(scene.HasPlayerController(knight));
    EXPECT_TRUE(Nova::SceneHasFollowCamera(scene));
    EXPECT_TRUE(scene.HasCharacterController(bandit));
    EXPECT_EQ(scene.GetCharacterController(bandit).Team, 1);
    EXPECT_EQ(scene.GetMeshRenderer(knight).AssetPath, "Assets/Characters/knight_armed.obj");
    EXPECT_EQ(scene.GetMeshRenderer(bandit).AssetPath, "Assets/Characters/bandit.obj");
}

TEST(ScenePicking, RayHitsGroundPlane) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    Nova::SpawnGround(scene);
    Nova::Ray ray;
    ray.Origin = {0.0f, 8.0f, 0.0f};
    ray.Direction = {0.0f, -1.0f, 0.0f};
    Nova::Vec3 hit{};
    ASSERT_TRUE(Nova::RayHitGround(scene, ray, hit));
    EXPECT_NEAR(hit.y, 0.0f, 0.05f);
}

TEST(ViewportManipulator, OrbitLookTurnsYawNotTarget) {
    float yaw = 0.0f;
    float pitch = 0.4f;
    Nova::Editor::ApplyOrbitLook(yaw, pitch, 80.0f, 0.0f, 0.01f);
    EXPECT_GT(yaw, 0.5f);
    EXPECT_NEAR(pitch, 0.4f, 1e-3f);
}

TEST(Scene, MakeUniqueNameIncrements) {
    Nova::Scene scene;
    scene.CreateEntity("Cube");
    EXPECT_EQ(scene.MakeUniqueName("Cube"), "Cube 2");
    EXPECT_EQ(scene.MakeUniqueName("Lamp"), "Lamp");
}
