#include <gtest/gtest.h>

#include <Nova/Core/Input.h>
#include <Nova/Scene/Gameplay.h>
#include <Nova/Scene/Scene.h>
#include <Nova/Scene/SceneSerialization.h>

#include <cstdlib>
#include <filesystem>

TEST(Gameplay, PlayableLevelHasPlayerGroundAndFollowCamera) {
    Nova::Scene scene = Nova::Scene::CreatePlayableLevel();
    const Nova::Entity player = scene.FindEntityByName("Player");
    ASSERT_TRUE(player.IsValid());
    EXPECT_TRUE(scene.HasCharacterController(player));
    EXPECT_TRUE(scene.HasPlayerController(player));
    EXPECT_TRUE(scene.FindEntityByName("Ground").IsValid());
    EXPECT_TRUE(Nova::SceneHasFollowCamera(scene));
}

TEST(Gameplay, WasdMovesPlayerForward) {
    Nova::Scene scene = Nova::Scene::CreatePlayableLevel();
    const Nova::Entity player = scene.FindEntityByName("Player");
    const float z0 = scene.GetTransform(player).Position.z;

    Nova::Input input;
    input.BeginFrame();
    input.OnKeyDown(Nova::KeyCode::W);
    Nova::TickScene(scene, 0.25f, &input);

    EXPECT_LT(scene.GetTransform(player).Position.z, z0 - 0.2f);
}

TEST(Gameplay, IdleTickDoesNotMovePlayerWithoutInput) {
    Nova::Scene scene = Nova::Scene::CreatePlayableLevel();
    const Nova::Entity player = scene.FindEntityByName("Player");
    const Nova::Vec3 before = scene.GetTransform(player).Position;
    Nova::TickScene(scene, 0.25f, nullptr);
    EXPECT_NEAR(scene.GetTransform(player).Position.x, before.x, 1e-4f);
    EXPECT_NEAR(scene.GetTransform(player).Position.z, before.z, 1e-4f);
}

TEST(Gameplay, SpawnPlayerWithMeshPath) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    const Nova::Entity player = Nova::SpawnPlayer(scene, "Assets/Models/pyramid.obj");
    EXPECT_EQ(scene.GetMeshRenderer(player).AssetPath, "Assets/Models/pyramid.obj");
    EXPECT_TRUE(scene.HasPlayerController(player));
}

TEST(Gameplay, DefaultKnightAndMap) {
    Nova::Scene scene = Nova::Scene::CreatePlayableLevel();
    const Nova::Entity player = scene.FindEntityByName("Player");
    ASSERT_TRUE(player.IsValid());
    EXPECT_NEAR(scene.GetCharacterController(player).Health, 200.0f, 1e-3f);
    EXPECT_GT(scene.GetCharacterController(player).AttackDamage, 40.0f);
    bool hasBody = false;
    scene.ForEachEntity([&](Nova::Entity entity) {
        if (scene.GetParent(entity).Id == player.Id && scene.GetName(entity) == "Player Body") {
            hasBody = true;
        }
    });
    EXPECT_TRUE(hasBody);
    EXPECT_TRUE(scene.FindEntityByName("Wall North").IsValid());
    EXPECT_TRUE(scene.FindEntityByName("House A").IsValid());
    EXPECT_TRUE(scene.FindEntityByName("Bandit 1").IsValid());
    const Nova::Entity bandit = scene.FindEntityByName("Bandit 1");
    ASSERT_TRUE(bandit.IsValid());
    EXPECT_LT(scene.GetCharacterController(bandit).Health,
              scene.GetCharacterController(player).Health);
    EXPECT_LT(scene.GetCharacterController(bandit).AttackDamage,
              scene.GetCharacterController(player).AttackDamage);
}

TEST(Gameplay, SprintMovesFasterThanWalk) {
    Nova::Scene walk = Nova::Scene::CreatePlayableLevel();
    Nova::Scene sprint = Nova::Scene::CreatePlayableLevel();
    const float zWalk0 = walk.GetTransform(walk.FindEntityByName("Player")).Position.z;
    const float zSprint0 = sprint.GetTransform(sprint.FindEntityByName("Player")).Position.z;

    Nova::Input walkIn;
    walkIn.BeginFrame();
    walkIn.OnKeyDown(Nova::KeyCode::W);
    Nova::TickScene(walk, 0.2f, &walkIn);

    Nova::EngineSettings settings = Nova::EngineSettings::Defaults();
    Nova::Input sprintIn;
    sprintIn.BeginFrame();
    sprintIn.OnKeyDown(Nova::KeyCode::W);
    sprintIn.OnKeyDown(Nova::KeyCode::LShift);
    Nova::TickScene(sprint, 0.2f, &sprintIn, &settings);

    const float walkDist = zWalk0 - walk.GetTransform(walk.FindEntityByName("Player")).Position.z;
    const float sprintDist =
        zSprint0 - sprint.GetTransform(sprint.FindEntityByName("Player")).Position.z;
    EXPECT_GT(sprintDist, walkDist * 1.2f);
}

TEST(Gameplay, AttackDamagesNearbyBandit) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    Nova::SpawnGround(scene);
    Nova::Entity player = Nova::SpawnPlayer(scene);
    Nova::Entity bandit = Nova::SpawnEnemy(scene, {1.1f, 0.0f, 0.0f});
    scene.GetTransform(player).Position = {0.0f, 0.0f, 0.0f};
    scene.GetTransform(bandit).Position = {1.1f, 0.0f, 0.0f};

    const float hp0 = scene.GetCharacterController(bandit).Health;
    Nova::Input input;
    input.BeginFrame();
    input.OnKeyDown(Nova::KeyCode::F);
    Nova::TickScene(scene, 0.016f, &input);

    EXPECT_LT(scene.GetCharacterController(bandit).Health, hp0);

    input.BeginFrame();
    input.OnKeyUp(Nova::KeyCode::F);
    Nova::TickScene(scene, 0.45f, &input);
    input.BeginFrame();
    input.OnKeyDown(Nova::KeyCode::F);
    Nova::TickScene(scene, 0.016f, &input);
    EXPECT_TRUE(scene.GetCharacterController(bandit).Dead);
}

TEST(Gameplay, EnemyWalksTowardPlayer) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    Nova::SpawnGround(scene);
    Nova::SpawnPlayer(scene);
    Nova::Entity bandit = Nova::SpawnEnemy(scene, {8.0f, 0.0f, 0.0f});
    const float x0 = scene.GetTransform(bandit).Position.x;
    Nova::TickScene(scene, 0.4f, nullptr);
    EXPECT_LT(scene.GetTransform(bandit).Position.x, x0 - 0.5f);
}

TEST(Gameplay, LookDoesNotSpinIdlePlayer) {
    Nova::Scene scene = Nova::Scene::CreatePlayableLevel();
    const Nova::Entity player = scene.FindEntityByName("Player");
    const Nova::Quat rot0 = scene.GetTransform(player).Rotation;
    float yaw0 = 0.0f;
    scene.ForEachEntity([&](Nova::Entity entity) {
        if (scene.HasFollowCamera(entity)) {
            yaw0 = scene.GetFollowCamera(entity).YawRadians;
        }
    });

    Nova::Input input;
    input.BeginFrame();
    input.OnMouseMove(0.0f, 0.0f, 90.0f, 0.0f);
    Nova::TickScene(scene, 0.016f, &input);

    EXPECT_NEAR(scene.GetTransform(player).Rotation.w, rot0.w, 1e-4f);
    scene.ForEachEntity([&](Nova::Entity entity) {
        if (scene.HasFollowCamera(entity)) {
            EXPECT_LT(scene.GetFollowCamera(entity).YawRadians, yaw0 - 0.2f);
        }
    });
}

TEST(Gameplay, FollowCameraLooksAtHeroChest) {
    Nova::Scene scene = Nova::Scene::CreatePlayableLevel();
    Nova::TickScene(scene, 0.016f, nullptr);
    const Nova::Entity player = scene.FindEntityByName("Player");
    const Nova::Entity camera = scene.FindPrimaryCamera();
    ASSERT_TRUE(player.IsValid());
    ASSERT_TRUE(camera.IsValid());
    const Nova::Vec3 focus = scene.GetCamera(camera).LookAtTarget;
    const Nova::Vec3 feet = scene.GetTransform(player).Position;
    EXPECT_NEAR(focus.x, feet.x, 0.05f);
    EXPECT_NEAR(focus.z, feet.z, 0.05f);
    EXPECT_NEAR(focus.y, feet.y + 1.25f, 0.05f);
}

TEST(Gameplay, EnsurePlayableFillsEmptyScene) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    Nova::EnsurePlayableCombatLevel(scene);
    EXPECT_TRUE(scene.FindEntityByName("Player").IsValid());
    EXPECT_TRUE(scene.FindEntityByName("Ground").IsValid());
    EXPECT_TRUE(scene.FindEntityByName("Bandit 1").IsValid());
    EXPECT_TRUE(Nova::SceneHasFollowCamera(scene));
}

TEST(Gameplay, ExportPlayableScenes) {
    const Nova::Scene scene = Nova::Scene::CreatePlayableLevel();
    const std::filesystem::path demo =
        std::filesystem::path(NOVA_SOURCE_DIR) / "Assets/Scenes/demo.scene.json";
    ASSERT_TRUE(Nova::SaveSceneToFile(scene, demo).Ok);

    const std::filesystem::path gameRoot =
        std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : ".") / "Desktop" /
        "KnightBandits";
    std::error_code ec;
    std::filesystem::create_directories(gameRoot / "Assets" / "Scenes", ec);
    const Nova::SceneIOResult saved =
        Nova::SaveSceneToFile(scene, gameRoot / "Assets" / "Scenes" / "arena.scene.json");
    EXPECT_TRUE(saved.Ok) << saved.Error;
}
