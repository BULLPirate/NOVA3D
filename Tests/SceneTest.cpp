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

TEST(Scene, TransformRoundTrip) {
    Nova::Scene scene;
    Nova::Entity e = scene.CreateEntity("T");
    scene.GetTransform(e).Position = {1.0f, 2.0f, 3.0f};
    EXPECT_FLOAT_EQ(scene.GetTransform(e).Position.x, 1.0f);
}
