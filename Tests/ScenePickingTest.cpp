#include <gtest/gtest.h>

#include <Nova/Scene/Scene.h>
#include <Nova/Scene/ScenePicking.h>
#include <Nova/Renderer/Camera.h>

TEST(ScenePicking, RayHitsCenteredCube) {
    Nova::Ray ray;
    ray.Origin = {0.0f, 0.0f, 5.0f};
    ray.Direction = {0.0f, 0.0f, -1.0f};
    Nova::Aabb box{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
    float dist = 0.0f;
    EXPECT_TRUE(Nova::RayIntersectsAabb(ray, box, dist));
    EXPECT_NEAR(dist, 4.5f, 0.05f);
}

TEST(ScenePicking, RayMissesOffsetCube) {
    Nova::Ray ray;
    ray.Origin = {3.0f, 0.0f, 5.0f};
    ray.Direction = {0.0f, 0.0f, -1.0f};
    Nova::Aabb box{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
    float dist = 0.0f;
    EXPECT_FALSE(Nova::RayIntersectsAabb(ray, box, dist));
}

TEST(ScenePicking, PicksFrontMeshInDemo) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    Nova::Entity cube = scene.CreateEntity("Cube");
    scene.AddMeshRenderer(cube);
    Nova::Camera cam;
    cam.Position = {0.0f, 0.35f, 3.2f};
    cam.Target = {0.0f, 0.0f, 0.0f};
    cam.Aspect = 1.0f;

    Nova::Ray ray;
    ASSERT_TRUE(Nova::ViewportPointToRay(cam, 200.0f, 200.0f, 400.0f, 400.0f, ray));
    const Nova::Entity hit = Nova::PickSceneMesh(scene, ray);
    ASSERT_TRUE(hit.IsValid());
    EXPECT_EQ(scene.GetName(hit), "Cube");
}
