#pragma once

#include <cstdint>

#include <Nova/Math/Transform.h>
#include <Nova/Math/Math.h>
#include <Nova/Assets/AssetId.h>

#include <string>

namespace Nova {

enum class MeshPrimitive : uint8_t {
    UnitCube = 0,
    UnitPlane = 1,
    UnitSphere = 2,
};

struct MeshRendererComponent {
    MeshPrimitive Primitive = MeshPrimitive::UnitCube;
    /// Project-relative path, e.g. Assets/Models/hero.obj. Empty = use Primitive.
    std::string AssetPath;
    /// Stable id from AssetRegistry; empty Guid if the mesh is a builtin primitive.
    AssetId MeshAssetId;
    Vec3 AlbedoColor{1.0f, 1.0f, 1.0f};
    /// 1 = fully visible. Play always uses 1 unless this is lowered per-entity.
    float Opacity = 1.0f;
    /// Project-relative PNG, e.g. Assets/Textures/brick.png
    std::string AlbedoTexturePath;
    AssetId AlbedoTextureId;
    bool UseAlbedoTexture = true;
    bool ReceiveShadows = true;
};

/// Continuous rotation (radians per second per axis).
struct RotatorComponent {
    Vec3 AngularVelocity{0.0f, 0.0f, 0.0f};
    bool LocalSpace = true;
};

/// Linear motion in world space (units per second).
struct MoverComponent {
    Vec3 Velocity{0.0f, 0.0f, 0.0f};
};

/// Camera uses entity Transform for position/rotation; looks at LookAtTarget.
struct CameraComponent {
    bool IsPrimary = false;
    Vec3 LookAtTarget{0.0f, 0.0f, 0.0f};
    float FovYRadians = Radians(60.0f);
    float NearPlane   = 0.1f;
    float FarPlane    = 100.0f;
};

struct DirectionalLightComponent {
    /// World-space direction **light rays travel** (from sun toward the scene).
    Vec3 Direction{0.45f, -0.88f, 0.15f};
    Vec3 Color{1.0f, 0.98f, 0.92f};
    float Ambient = 0.42f;
};

struct PointLightComponent {
    Vec3 Color{1.0f, 0.85f, 0.55f};
    float Intensity = 1.5f;
    float Range = 6.0f;
};

struct SceneSettings {
    Vec3 ClearColor{0.52f, 0.66f, 0.80f};
    int Wave = 1;
    int Score = 0;
    bool PendingWave = false;
    float WaveTimer = 0.0f;
    /// Game-mode flag stored in the project's scene, not in the engine default.
    bool EnableWaves = false;
};

struct ColliderComponent {
    bool Solid = true;
    /// Zero means use the entity Transform scale.
    Vec3 Size{0.0f, 0.0f, 0.0f};
};

struct PickupComponent {
    float Heal = 40.0f;
    bool Taken = false;
    float Hover = 0.0f;
};

struct CharacterControllerComponent {
    float MoveSpeed = 6.0f;
    float JumpSpeed = 6.5f;
    float Gravity = 20.0f;
    float Height = 1.85f;
    float FootOffset = 0.0f;
    float SprintMultiplier = 1.75f;
    float CrouchMultiplier = 0.5f;
    float AirControl = 0.42f;
    float DodgeSpeed = 13.0f;
    float VerticalVelocity = 0.0f;
    float DodgeTimer = 0.0f;
    float AttackTimer = 0.0f;
    Vec3 DodgeVelocity{0.0f, 0.0f, 0.0f};
    bool Grounded = true;
    bool Crouching = false;
    /// 0 = player/friendly, 1 = enemy.
    int Team = 0;
    float Health = 100.0f;
    float MaxHealth = 100.0f;
    float AttackDamage = 28.0f;
    float AttackRange = 2.35f;
    float DetectRange = 16.0f;
    float AiCooldown = 0.0f;
    bool Dead = false;
    /// Yaw the body faces. Camera look is separate.
    float FacingYaw = 0.0f;
    float Radius = 0.36f;
    float HurtTimer = 0.0f;
    Vec3 KnockbackVelocity{0.0f, 0.0f, 0.0f};
    float WalkPhase = 0.0f;
    float CorpseTimer = 0.0f;
};

struct PlayerControllerComponent {
    bool Enabled = true;
};

struct FollowCameraComponent {
    std::string TargetName = "Player";
    float Distance = 8.0f;
    float Height = 3.2f;
    float YawRadians = 0.0f;
    float PitchRadians = 0.35f;
    float MouseSensitivity = 0.0045f;
};

/// Inline NovaScript or a project file under Assets/Scripts.
struct ScriptComponent {
    std::string Source;
    std::string AssetPath;
    bool RanStart = false;
};

struct AudioSourceComponent {
    std::string SoundId = "confirm";
    bool PlayOnStart = true;
    bool Started = false;
};

} // namespace Nova
