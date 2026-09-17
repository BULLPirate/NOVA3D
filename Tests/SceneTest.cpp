#include <gtest/gtest.h>
#include <Nova/Scene/Scene.h>

TEST(Scene, CreateDestroyEntity) {
    Nova::Scene scene;
    Nova::Entity a = scene.CreateEntity("A");
    EXPECT_TRUE(scene.IsAlive(a));
    EXPECT_EQ(scene.GetName(a), "A");
    scene.DestroyEntity(a);
    EXPECT_FALSE(scene.IsAlive(a));
}

TEST(Scene, StaleHandleAfterDestroy) {
    Nova::Scene scene;
    Nova::Entity a = scene.CreateEntity("A");
    scene.DestroyEntity(a);
    EXPECT_FALSE(scene.IsAlive(a));
    Nova::Entity b = scene.CreateEntity("B");
    EXPECT_TRUE(scene.IsAlive(b));
    EXPECT_NE(a.Id, b.Id);
}

TEST(Scene, EmptyLevelHasCameraAndLightOnly) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    int meshCount = 0;
    scene.ForEachEntity([&](Nova::Entity e) {
        if (scene.HasMeshRenderer(e)) {
            ++meshCount;
        }
    });
    EXPECT_EQ(meshCount, 0);
    EXPECT_TRUE(scene.FindPrimaryCamera().IsValid());
}

TEST(Scene, DemoLevelHasCameraCubeAndLight) {
    Nova::Scene scene = Nova::Scene::CreateDemoLevel();
    Nova::Entity camera = scene.FindPrimaryCamera();
    EXPECT_TRUE(camera.IsValid());

    int meshCount = 0;
    int lightCount = 0;
    scene.ForEachEntity([&](Nova::Entity e) {
        if (scene.HasMeshRenderer(e)) ++meshCount;
        if (scene.HasDirectionalLight(e)) ++lightCount;
    });
    EXPECT_EQ(meshCount, 1);
    EXPECT_EQ(lightCount, 1);
}

TEST(Scene, ClearRemovesEntities) {
    Nova::Scene scene = Nova::Scene::CreateDemoLevel();
    EXPECT_EQ(scene.EntityCount(), 3u);
    scene.Clear();
    EXPECT_EQ(scene.EntityCount(), 0u);
}

TEST(Scene, DuplicateEntityCopiesPointLight) {
    Nova::Scene scene;
    Nova::Entity lamp = scene.CreateEntity("Lamp");
    Nova::PointLightComponent light;
    light.Range = 3.0f;
    scene.AddPointLight(lamp, light);

    Nova::Entity copy = scene.DuplicateEntity(lamp);
    EXPECT_TRUE(scene.HasPointLight(copy));
    EXPECT_FLOAT_EQ(scene.GetPointLight(copy).Range, 3.0f);
}

TEST(Scene, ClearResetsEnvironment) {
    Nova::Scene scene;
    scene.Settings().ClearColor = {1.0f, 0.0f, 0.0f};
    scene.Clear();
    EXPECT_NEAR(scene.Settings().ClearColor.x, 0.10f, 1e-4f);
}

TEST(Scene, DuplicateEntityCopiesComponents) {
    Nova::Scene scene;
    Nova::Entity cube = scene.CreateEntity("Cube");
    scene.AddMeshRenderer(cube);
    scene.GetTransform(cube).Position = {1.0f, 2.0f, 3.0f};

    Nova::Entity copy = scene.DuplicateEntity(cube);
    EXPECT_TRUE(copy.IsValid());
    EXPECT_TRUE(scene.HasMeshRenderer(copy));
    EXPECT_FLOAT_EQ(scene.GetTransform(copy).Position.y, 2.0f);
    EXPECT_FALSE(scene.HasCamera(copy));
}

TEST(Scene, SetPrimaryCamera) {
    Nova::Scene scene;
    Nova::Entity a = scene.CreateEntity("CamA");
    Nova::Entity b = scene.CreateEntity("CamB");
    scene.AddCamera(a, {});
    scene.AddCamera(b, {});
    scene.GetCamera(a).IsPrimary = true;

    scene.SetPrimaryCamera(b);
    EXPECT_FALSE(scene.GetCamera(a).IsPrimary);
    EXPECT_TRUE(scene.GetCamera(b).IsPrimary);
    EXPECT_EQ(scene.FindPrimaryCamera().Id, b.Id);
}

TEST(Scene, ParentWorldMatrixCombinesTranslation) {
    Nova::Scene scene;
    Nova::Entity parent = scene.CreateEntity("Parent");
    scene.GetTransform(parent).Position = {1.0f, 0.0f, 0.0f};
    Nova::Entity child = scene.CreateEntity("Child");
    scene.GetTransform(child).Position = {0.0f, 1.0f, 0.0f};
    scene.SetParent(child, parent);

    const Nova::Mat4 world = scene.GetWorldMatrix(child);
    EXPECT_NEAR(world.m[3][0], 1.0f, 0.01f);
    EXPECT_NEAR(world.m[3][1], 1.0f, 0.01f);
    EXPECT_NEAR(world.m[3][2], 0.0f, 0.01f);
}

TEST(Scene, TransformRoundTrip) {
    Nova::Scene scene;
    Nova::Entity e = scene.CreateEntity("T");
    scene.GetTransform(e).Position = {1.0f, 2.0f, 3.0f};
    EXPECT_FLOAT_EQ(scene.GetTransform(e).Position.x, 1.0f);
}
