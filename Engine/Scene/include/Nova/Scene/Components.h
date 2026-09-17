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
    /// Project-relative PNG, e.g. Assets/Textures/brick.png
    std::string AlbedoTexturePath;
    AssetId AlbedoTextureId;
    bool UseAlbedoTexture = true;
    bool ReceiveShadows = false;
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
    Vec3 Color{1.0f, 0.98f, 0.95f};
    float Ambient = 0.12f;
};

struct PointLightComponent {
    Vec3 Color{1.0f, 0.85f, 0.55f};
    float Intensity = 1.5f;
    float Range = 6.0f;
};

struct SceneSettings {
    Vec3 ClearColor{0.10f, 0.11f, 0.14f};
};

} // namespace Nova
