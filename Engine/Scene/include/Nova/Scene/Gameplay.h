#pragma once

#include <Nova/Scene/Scene.h>
#include <Nova/Core/EngineSettings.h>
#include <Nova/Core/Input.h>

#include <filesystem>
#include <string>

namespace Nova {

/// Spawns a controllable character. `meshAssetPath` empty = built-in capsule-like sphere.
Entity SpawnPlayer(Scene& scene, const std::string& meshAssetPath = {});
Entity SpawnEnemy(Scene& scene, const Vec3& position, const std::string& meshAssetPath = {});
Entity SpawnGround(Scene& scene);
Entity SpawnSimpleMap(Scene& scene);
Entity SpawnImportedMesh(Scene& scene, const std::string& name, const std::string& meshAssetPath);
void PlaceOnGround(Scene& scene, Entity entity);
void ApplyMeleeHit(Scene& scene, Entity attacker);
void EnsurePlayableCombatLevel(Scene& scene);
void AttachHumanoidVisual(Scene& scene, Entity root, bool knight);

struct GameplayHudSnapshot {
    float PlayerHealth = 0.0f;
    float PlayerMaxHealth = 0.0f;
    float TargetHealth = 0.0f;
    float TargetMaxHealth = 0.0f;
    int AliveEnemies = 0;
    bool PlayerAlive = false;
};

GameplayHudSnapshot QueryGameplayHud(const Scene& scene);

void TickScene(Scene& scene, float deltaSeconds, const Input* input = nullptr,
               EngineSettings* settings = nullptr,
               const std::filesystem::path& projectRoot = {},
               bool playJustStarted = false);

bool SceneHasFollowCamera(const Scene& scene);

} // namespace Nova
