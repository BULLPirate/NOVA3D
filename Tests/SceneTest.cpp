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

TEST(Scene, TransformRoundTrip) {
    Nova::Scene scene;
    Nova::Entity e = scene.CreateEntity("T");
    scene.GetTransform(e).Position = {1.0f, 2.0f, 3.0f};
    EXPECT_FLOAT_EQ(scene.GetTransform(e).Position.x, 1.0f);
}
