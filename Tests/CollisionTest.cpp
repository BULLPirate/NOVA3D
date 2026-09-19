#include <gtest/gtest.h>

#include <Nova/Scene/Collision.h>
#include <Nova/Scene/Gameplay.h>
#include <Nova/Scene/Scene.h>
#include <Nova/Core/Input.h>

#include <string>

TEST(Collision, CirclePushedOutOfAabb) {
    Nova::Aabb box = Nova::AabbFromCenterScale({0.0f, 0.5f, 0.0f}, {2.0f, 1.0f, 2.0f});
    Nova::Vec3 pos{0.2f, 0.0f, 0.0f};
    EXPECT_TRUE(Nova::ResolveCircleAabbXZ(pos, 0.4f, box));
    EXPECT_GT(std::abs(pos.x), 1.3f);
}

TEST(Collision, CirclesSeparate) {
    Nova::Vec3 a{0.0f, 0.0f, 0.0f};
    Nova::Vec3 b{0.2f, 0.0f, 0.0f};
    Nova::SeparateCirclesXZ(a, b, 0.4f, 0.4f);
    const float dist = (b - a).Length();
    EXPECT_NEAR(dist, 0.8f, 0.02f);
}

TEST(Collision, CameraPullsInBeforeWall) {
    const Nova::Aabb wall = Nova::AabbFromCenterScale({0.0f, 1.0f, 2.0f}, {4.0f, 2.0f, 0.4f});
    const Nova::Vec3 focus{0.0f, 1.0f, 0.0f};
    const Nova::Vec3 desired{0.0f, 1.0f, 6.0f};
    const Nova::Vec3 eye = Nova::CameraEyeAvoidingSolids(focus, desired, {wall}, 0.2f);
    EXPECT_LT(eye.z, 2.0f);
    EXPECT_GT(eye.z, 0.2f);
}

TEST(Gameplay, RespawnRestoresPlayer) {
    Nova::Scene scene = Nova::Scene::CreatePlayableLevel();
    Nova::Entity player = scene.FindEntityByName("Player");
    scene.GetCharacterController(player).Health = 0.0f;
    scene.GetCharacterController(player).Dead = true;
    scene.GetPlayerController(player).Enabled = false;

    Nova::Input input;
    input.BeginFrame();
    input.OnKeyDown(Nova::KeyCode::R);
    Nova::TickScene(scene, 0.016f, &input);

    EXPECT_FALSE(scene.GetCharacterController(player).Dead);
    EXPECT_GT(scene.GetCharacterController(player).Health, 150.0f);
    EXPECT_TRUE(scene.GetPlayerController(player).Enabled);
}

TEST(Gameplay, HudTitleListsHealth) {
    Nova::Scene scene = Nova::Scene::CreatePlayableLevel();
    const std::string title = Nova::FormatGameplayHudTitle(Nova::QueryGameplayHud(scene));
    EXPECT_NE(title.find("HP"), std::string::npos);
    EXPECT_NE(title.find("200"), std::string::npos);
    EXPECT_EQ(Nova::QueryGameplayHud(scene).Phase, Nova::GameplayPhase::Combat);
}

TEST(Gameplay, HudMarksVictoryAtWaveFive) {
    Nova::Scene scene = Nova::Scene::CreatePlayableLevel();
    scene.Settings().Wave = 5;
    scene.ForEachEntity([&](Nova::Entity entity) {
        if (scene.HasCharacterController(entity) && scene.GetCharacterController(entity).Team == 1) {
            scene.GetCharacterController(entity).Dead = true;
            scene.GetCharacterController(entity).Health = 0.0f;
        }
    });
    const Nova::GameplayHudSnapshot hud = Nova::QueryGameplayHud(scene);
    EXPECT_EQ(hud.Phase, Nova::GameplayPhase::Victory);
    EXPECT_NE(std::string(Nova::GameplayHudStatusLine(hud)).find("зачищена"), std::string::npos);
}
