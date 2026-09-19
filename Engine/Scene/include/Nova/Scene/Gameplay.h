#pragma once

#include <Nova/Scene/Scene.h>
#include <Nova/Core/EngineSettings.h>
#include <Nova/Core/Input.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace Nova {

/// Spawns a controllable character. Empty `meshAssetPath` uses `Assets/Characters/knight_armed.obj`.
Entity SpawnPlayer(Scene& scene, const std::string& meshAssetPath = {});
Entity SpawnEnemy(Scene& scene, const Vec3& position, const std::string& meshAssetPath = {});
Entity SpawnGround(Scene& scene);
Entity SpawnSimpleMap(Scene& scene);
Entity SpawnImportedMesh(Scene& scene, const std::string& name, const std::string& meshAssetPath);
Entity SpawnHealthPickup(Scene& scene, const Vec3& position);
void PlaceOnGround(Scene& scene, Entity entity);
void ApplyMeleeHit(Scene& scene, Entity attacker);
void EnsureFollowCamera(Scene& scene);
void EnsurePlayableCombatLevel(Scene& scene);
void AttachHumanoidVisual(Scene& scene, Entity root, bool knight);
void SetEntityMeshAsset(Scene& scene, Entity entity, const std::string& meshAssetPath,
                        const std::string& texturePath = {});

enum class GameplayPhase : uint8_t { Combat = 0, Dead, NextWave, Victory };

struct GameplayHudSnapshot {
    float PlayerHealth = 0.0f;
    float PlayerMaxHealth = 0.0f;
    float TargetHealth = 0.0f;
    float TargetMaxHealth = 0.0f;
    int AliveEnemies = 0;
    bool PlayerAlive = false;
    int Wave = 1;
    int Score = 0;
    GameplayPhase Phase = GameplayPhase::Combat;
    float WaveCountdown = 0.0f;
    bool ShowCombatHud = false;
};

GameplayHudSnapshot QueryGameplayHud(const Scene& scene);
std::string FormatGameplayHudTitle(const GameplayHudSnapshot& hud);
const char* GameplayHudStatusLine(const GameplayHudSnapshot& hud);

void TickScene(Scene& scene, float deltaSeconds, const Input* input = nullptr,
               EngineSettings* settings = nullptr,
               const std::filesystem::path& projectRoot = {},
               bool playJustStarted = false);

bool SceneHasFollowCamera(const Scene& scene);

} // namespace Nova
