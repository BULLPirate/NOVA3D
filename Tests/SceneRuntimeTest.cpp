#include <Nova/Scene/Scene.h>
#include <Nova/Scene/SceneRuntime.h>

#include <gtest/gtest.h>

TEST(SceneRuntime, RotatorChangesOrientation) {
    Nova::Scene scene;
    Nova::Entity e = scene.CreateEntity("Spin");
    scene.AddRotator(e);
    scene.GetRotator(e).AngularVelocity = {0.0f, 1.0f, 0.0f};

    const Nova::Quat before = scene.GetTransform(e).Rotation;
    Nova::TickScene(scene, 0.5f);
    const Nova::Quat after = scene.GetTransform(e).Rotation;
    EXPECT_LT(before.Dot(after), 0.999f);
}
