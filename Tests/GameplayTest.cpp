#include <gtest/gtest.h>

#include <Nova/Core/Input.h>
#include <Nova/Scene/Gameplay.h>
#include <Nova/Scene/Scene.h>
#include <Nova/Scene/SceneSerialization.h>
#include <Nova/Project/ProjectTemplate.h>
#include <Nova/Project/Workspace.h>

#include <cmath>
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
    EXPECT_TRUE(scene.Settings().EnableWaves);
}

TEST(Gameplay, SandboxLevelHasPlayerWithoutWaves) {
    Nova::Scene scene = Nova::Scene::CreateSandboxLevel();
    ASSERT_TRUE(scene.FindEntityByName("Player").IsValid());
    EXPECT_TRUE(scene.FindEntityByName("Ground").IsValid());
    EXPECT_TRUE(Nova::SceneHasFollowCamera(scene));
    EXPECT_FALSE(scene.Settings().EnableWaves);
    const Nova::GameplayHudSnapshot hud = Nova::QueryGameplayHud(scene);
    EXPECT_FALSE(hud.ShowCombatHud);
    EXPECT_EQ(hud.Phase, Nova::GameplayPhase::Combat);
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
    EXPECT_TRUE(scene.HasMeshRenderer(player));
    EXPECT_EQ(scene.GetMeshRenderer(player).AssetPath, "Assets/Characters/knight_armed.obj");
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

TEST(Gameplay, MeleeDoesNotHitThroughWall) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    Nova::SpawnGround(scene);
    Nova::Entity player = Nova::SpawnPlayer(scene);
    Nova::Entity bandit = Nova::SpawnEnemy(scene, {2.2f, 0.0f, 0.0f});
    Nova::Entity wall = scene.CreateEntity("Wall Block");
    Nova::MeshRendererComponent mesh;
    mesh.Primitive = Nova::MeshPrimitive::UnitCube;
    scene.AddMeshRenderer(wall, mesh);
    scene.AddCollider(wall);
    scene.GetTransform(wall).Position = {1.1f, 1.0f, 0.0f};
    scene.GetTransform(wall).Scale = {0.4f, 2.0f, 2.0f};
    scene.GetTransform(player).Position = {0.0f, 0.0f, 0.0f};
    scene.GetTransform(bandit).Position = {2.2f, 0.0f, 0.0f};

    const float hp0 = scene.GetCharacterController(bandit).Health;
    Nova::Input input;
    input.BeginFrame();
    input.OnKeyDown(Nova::KeyCode::F);
    Nova::TickScene(scene, 0.016f, &input);
    EXPECT_NEAR(scene.GetCharacterController(bandit).Health, hp0, 1e-3f);
}

TEST(Gameplay, DeadBanditRemovedAfterDelay) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    Nova::SpawnGround(scene);
    Nova::SpawnPlayer(scene);
    Nova::Entity bandit = Nova::SpawnEnemy(scene, {1.1f, 0.0f, 0.0f});
    scene.GetCharacterController(bandit).Dead = true;
    scene.GetCharacterController(bandit).Health = 0.0f;
    scene.GetCharacterController(bandit).CorpseTimer = 0.2f;
    Nova::TickScene(scene, 0.25f, nullptr);
    EXPECT_FALSE(scene.IsAlive(bandit));
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

TEST(Gameplay, HealthPickupHealsPlayer) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    Nova::SpawnGround(scene);
    Nova::Entity player = Nova::SpawnPlayer(scene);
    scene.GetCharacterController(player).Health = 80.0f;
    Nova::Entity herb = Nova::SpawnHealthPickup(scene, {0.4f, 0.0f, 0.0f});
    ASSERT_TRUE(scene.HasPickup(herb));
    ASSERT_TRUE(scene.HasCollider(scene.FindEntityByName("Ground")) == false);

    Nova::Input input;
    input.BeginFrame();
    Nova::TickScene(scene, 0.016f, &input);

    EXPECT_GT(scene.GetCharacterController(player).Health, 110.0f);
    EXPECT_TRUE(scene.GetPickup(herb).Taken);
}

TEST(Gameplay, MapSolidsHaveColliders) {
    Nova::Scene scene = Nova::Scene::CreatePlayableLevel();
    const Nova::Entity wall = scene.FindEntityByName("Wall North");
    ASSERT_TRUE(wall.IsValid());
    EXPECT_TRUE(scene.HasCollider(wall));
    EXPECT_TRUE(scene.FindEntityByName("Herb 1").IsValid());
    EXPECT_TRUE(scene.HasPickup(scene.FindEntityByName("Herb 1")));
}

TEST(Gameplay, WaveDoesNotStartWithoutDeadEnemies) {
    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    Nova::SpawnGround(scene);
    Nova::SpawnPlayer(scene);
    Nova::Input input;
    input.BeginFrame();
    Nova::TickScene(scene, 0.5f, &input);
    EXPECT_EQ(scene.Settings().Wave, 1);
    bool spawnedBandit = false;
    scene.ForEachEntity([&](Nova::Entity entity) {
        if (scene.HasCharacterController(entity) && scene.GetCharacterController(entity).Team == 1) {
            spawnedBandit = true;
        }
    });
    EXPECT_FALSE(spawnedBandit);
}

TEST(Gameplay, WaveStartsAfterClear) {
    Nova::Scene scene = Nova::Scene::CreatePlayableLevel();
    scene.ForEachEntity([&](Nova::Entity entity) {
        if (scene.HasCharacterController(entity) && scene.GetCharacterController(entity).Team == 1) {
            scene.GetCharacterController(entity).Dead = true;
            scene.GetCharacterController(entity).Health = 0.0f;
        }
    });
    Nova::Input input;
    for (int i = 0; i < 20; ++i) {
        input.BeginFrame();
        Nova::TickScene(scene, 0.1f, &input);
    }
    EXPECT_EQ(scene.Settings().Wave, 2);
    int alive = 0;
    scene.ForEachEntity([&](Nova::Entity entity) {
        if (scene.HasCharacterController(entity) && scene.GetCharacterController(entity).Team == 1 &&
            !scene.GetCharacterController(entity).Dead) {
            ++alive;
        }
    });
    EXPECT_GE(alive, 3);
}

TEST(Gameplay, PlaySimulationKeepsHeroOnMap) {
    Nova::Scene scene = Nova::Scene::CreatePlayableLevel();
    const Nova::Entity player = scene.FindEntityByName("Player");
    Nova::Input input;
    for (int i = 0; i < 40; ++i) {
        input.BeginFrame();
        input.OnKeyDown(Nova::KeyCode::W);
        Nova::TickScene(scene, 0.05f, &input);
    }
    const Nova::Vec3 pos = scene.GetTransform(player).Position;
    EXPECT_TRUE(std::isfinite(pos.x) && std::isfinite(pos.y) && std::isfinite(pos.z));
    EXPECT_LT(std::abs(pos.x), 13.5f);
    EXPECT_LT(std::abs(pos.z), 13.5f);
    EXPECT_NEAR(pos.y, 0.0f, 0.35f);
    const Nova::Entity camera = scene.FindPrimaryCamera();
    const Nova::Vec3 look = scene.GetCamera(camera).LookAtTarget;
    EXPECT_NEAR(look.x, pos.x, 0.35f);
    EXPECT_NEAR(look.z, pos.z, 0.35f);
}

TEST(Gameplay, ExportPlayableScenes) {
    const Nova::Scene empty = Nova::Scene::CreateEmptyLevel();
    const std::filesystem::path demo =
        std::filesystem::path(NOVA_SOURCE_DIR) / "Assets/Scenes/demo.scene.json";
    ASSERT_TRUE(Nova::SaveSceneToFile(empty, demo).Ok);
    EXPECT_FALSE(empty.FindEntityByName("Player").IsValid());

    const std::filesystem::path gameRoot = Nova::DefaultProjectsDirectory() / "KnightBandits";
    ASSERT_TRUE(Nova::CreateGameProject(gameRoot, "Knight Bandits",
                                        Nova::ProjectTemplateKind::KnightBandits, NOVA_SOURCE_DIR)
                    .Ok);
    Nova::Scene loaded;
    ASSERT_TRUE(Nova::LoadSceneFromFile(gameRoot / "Assets" / "Scenes" / "main.scene.json", loaded).Ok);
    EXPECT_TRUE(loaded.FindEntityByName("Player").IsValid());
    EXPECT_TRUE(loaded.Settings().EnableWaves);
}
