#include <Nova/Scene/Gameplay.h>
#include <Nova/Scene/Script.h>
#include <Nova/Scene/Collision.h>

#include <Nova/Math/Math.h>
#include <Nova/Math/Quat.h>
#include <Nova/Renderer/Camera.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

namespace Nova {

namespace {

constexpr float kPlayerMaxHealth = 200.0f;
constexpr float kPlayerDamage = 50.0f;
constexpr float kEnemyMaxHealth = 32.0f;
constexpr float kEnemyDamage = 6.0f;

bool NameStartsWith(const std::string& name, const char* prefix) {
    return name.rfind(prefix, 0) == 0;
}

float GroundHeightUnder(const Scene& scene, const Vec3& position) {
    float ground = 0.0f;
    bool found = false;
    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasMeshRenderer(entity)) {
            return;
        }
        const MeshRendererComponent& mesh = scene.GetMeshRenderer(entity);
        if (mesh.Primitive != MeshPrimitive::UnitPlane) {
            return;
        }
        const float y = scene.GetTransform(entity).Position.y;
        if (!found || y > ground) {
            ground = y;
            found = true;
        }
        (void)position;
    });
    return ground;
}

Entity FindLocalPlayer(const Scene& scene) {
    Entity found{};
    scene.ForEachEntity([&](Entity entity) {
        if (found.IsValid()) {
            return;
        }
        if (scene.HasPlayerController(entity) && scene.GetPlayerController(entity).Enabled &&
            scene.HasCharacterController(entity) && !scene.GetCharacterController(entity).Dead) {
            found = entity;
        }
    });
    return found;
}

Entity SpawnColored(Scene& scene, const std::string& name, MeshPrimitive primitive, const Vec3& pos,
                    const Vec3& scale, const Vec3& color) {
    if (Entity existing = scene.FindEntityByName(name); existing.IsValid()) {
        return existing;
    }
    Entity entity = scene.CreateEntity(name);
    MeshRendererComponent mesh;
    mesh.Primitive = primitive;
    mesh.UseAlbedoTexture = false;
    mesh.AlbedoColor = color;
    mesh.Opacity = 1.0f;
    scene.AddMeshRenderer(entity, mesh);
    scene.GetTransform(entity).Position = pos;
    scene.GetTransform(entity).Scale = scale;
    if (NameStartsWith(name, "Wall") || NameStartsWith(name, "House") || NameStartsWith(name, "Well") ||
        NameStartsWith(name, "Crate") || NameStartsWith(name, "Tree") || NameStartsWith(name, "Fence")) {
        scene.AddCollider(entity);
    }
    return entity;
}

void AddPart(Scene& scene, Entity parent, const std::string& name, MeshPrimitive primitive,
             const Vec3& localPos, const Vec3& scale, const Vec3& color) {
    Entity part = SpawnColored(scene, name, primitive, localPos, scale, color);
    scene.SetParent(part, parent);
}

void UpdateHealthBar(Scene& scene, Entity owner) {
    if (!scene.HasCharacterController(owner)) {
        return;
    }
    const CharacterControllerComponent& character = scene.GetCharacterController(owner);
    Entity bar{};
    scene.ForEachEntity([&](Entity entity) {
        if (bar.IsValid()) {
            return;
        }
        if (scene.GetParent(entity).Id == owner.Id && scene.GetName(entity) == scene.GetName(owner) + " HP") {
            bar = entity;
        }
    });
    const Vec3 color = character.Team == 0 ? Vec3{0.25f, 0.82f, 0.32f} : Vec3{0.86f, 0.18f, 0.16f};
    const float ratio = character.MaxHealth > 0.0f
                            ? Clamp(character.Health / character.MaxHealth, 0.0f, 1.0f)
                            : 0.0f;
    if (!bar.IsValid()) {
        bar = SpawnColored(scene, scene.GetName(owner) + " HP", MeshPrimitive::UnitCube, {0.0f, 2.15f, 0.0f},
                           {1.15f, 0.12f, 0.12f}, color);
        scene.SetParent(bar, owner);
    }
    if (!scene.HasMeshRenderer(bar)) {
        return;
    }
    scene.GetMeshRenderer(bar).AlbedoColor = color;
    scene.GetMeshRenderer(bar).UseAlbedoTexture = false;
    scene.GetMeshRenderer(bar).Opacity = character.Dead ? 0.0f : 1.0f;
    scene.GetTransform(bar).Position = {0.0f, 2.15f, 0.0f};
    scene.GetTransform(bar).Scale = {std::max(0.12f, 1.15f * ratio), 0.12f, 0.12f};
}

void ClampToArena(Vec3& position) {
    position.x = Clamp(position.x, -13.5f, 13.5f);
    position.z = Clamp(position.z, -13.5f, 13.5f);
}

void MarkDead(Scene& scene, Entity entity) {
    if (!scene.HasCharacterController(entity)) {
        return;
    }
    CharacterControllerComponent& character = scene.GetCharacterController(entity);
    character.Dead = true;
    character.Health = 0.0f;
    character.CorpseTimer = scene.HasPlayerController(entity) ? 0.0f : 4.0f;
    if (scene.HasPlayerController(entity)) {
        scene.GetPlayerController(entity).Enabled = false;
    }
    auto grayMesh = [&](Entity meshEntity) {
        if (!scene.HasMeshRenderer(meshEntity)) {
            return;
        }
        scene.GetMeshRenderer(meshEntity).Opacity = 0.35f;
        scene.GetMeshRenderer(meshEntity).AlbedoColor = {0.28f, 0.26f, 0.24f};
        scene.GetMeshRenderer(meshEntity).UseAlbedoTexture = false;
    };
    grayMesh(entity);
    scene.ForEachEntity([&](Entity child) {
        if (scene.GetParent(child).Id != entity.Id) {
            return;
        }
        grayMesh(child);
    });
}

Entity FindFollowCamera(const Scene& scene) {
    Entity found{};
    scene.ForEachEntity([&](Entity entity) {
        if (found.IsValid()) {
            return;
        }
        if (scene.HasFollowCamera(entity) && scene.HasCamera(entity)) {
            found = entity;
        }
    });
    return found;
}

bool IsMapSolid(const Scene& scene, Entity entity) {
    if (!entity.IsValid() || scene.HasCharacterController(entity) || scene.HasCamera(entity) ||
        scene.HasPickup(entity) || scene.GetParent(entity).IsValid()) {
        return false;
    }
    if (scene.HasCollider(entity)) {
        return scene.GetCollider(entity).Solid;
    }
    if (!scene.HasMeshRenderer(entity)) {
        return false;
    }
    if (scene.GetMeshRenderer(entity).Primitive == MeshPrimitive::UnitPlane) {
        return false;
    }
    const std::string& name = scene.GetName(entity);
    return NameStartsWith(name, "Wall") || NameStartsWith(name, "House") ||
           NameStartsWith(name, "Well") || NameStartsWith(name, "Crate") ||
           NameStartsWith(name, "Tree") || NameStartsWith(name, "Fence");
}

void CollectMapSolids(const Scene& scene, std::vector<Aabb>& solids) {
    scene.ForEachEntity([&](Entity entity) {
        if (!IsMapSolid(scene, entity)) {
            return;
        }
        const Transform& box = scene.GetTransform(entity);
        Vec3 size = box.Scale;
        if (scene.HasCollider(entity)) {
            const Vec3 custom = scene.GetCollider(entity).Size;
            if (custom.LengthSq() > 1e-6f) {
                size = custom;
            }
        }
        solids.push_back(AabbFromCenterScale(box.Position, size));
    });
}

void ResolveCharacterVsMap(Scene& scene, Entity mover, float radius) {
    if (!mover.IsValid()) {
        return;
    }
    Transform& xform = scene.GetTransform(mover);
    scene.ForEachEntity([&](Entity entity) {
        if (!IsMapSolid(scene, entity)) {
            return;
        }
        const Transform& box = scene.GetTransform(entity);
        Vec3 size = box.Scale;
        if (scene.HasCollider(entity)) {
            const Vec3 custom = scene.GetCollider(entity).Size;
            if (custom.LengthSq() > 1e-6f) {
                size = custom;
            }
        }
        ResolveCircleAabbXZ(xform.Position, radius, AabbFromCenterScale(box.Position, size));
    });
}

void SeparateCharacters(Scene& scene) {
    std::vector<Entity> bodies;
    scene.ForEachEntity([&](Entity entity) {
        if (scene.HasCharacterController(entity) && !scene.GetCharacterController(entity).Dead) {
            bodies.push_back(entity);
        }
    });
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            SeparateCirclesXZ(scene.GetTransform(bodies[i]).Position,
                              scene.GetTransform(bodies[j]).Position,
                              scene.GetCharacterController(bodies[i]).Radius,
                              scene.GetCharacterController(bodies[j]).Radius);
        }
    }
}

Vec3 CameraLookXZ(float camYaw) {
    return {-std::sin(camYaw), 0.0f, -std::cos(camYaw)};
}

float FollowCameraYaw(const Scene& scene, float fallback) {
    float yaw = fallback;
    scene.ForEachEntity([&](Entity entity) {
        if (scene.HasFollowCamera(entity)) {
            yaw = scene.GetFollowCamera(entity).YawRadians;
        }
    });
    return yaw;
}

void AnimateHumanoidWalk(Scene& scene, Entity root, float phase, bool moving) {
    const float bob = moving ? std::sin(phase) * 0.08f : 0.0f;
    scene.ForEachEntity([&](Entity child) {
        if (scene.GetParent(child).Id != root.Id) {
            return;
        }
        const std::string& name = scene.GetName(child);
        Transform& local = scene.GetTransform(child);
        if (name.find("LegL") != std::string::npos) {
            local.Position.y = 0.41f + bob;
        } else if (name.find("LegR") != std::string::npos) {
            local.Position.y = 0.41f - bob;
        } else if (name.find("ArmL") != std::string::npos) {
            local.Position.z = 0.04f - bob * 0.45f;
        } else if (name.find("ArmR") != std::string::npos) {
            local.Position.z = 0.08f + bob * 0.45f;
        }
    });
}

void BillboardHealthBars(Scene& scene, const Vec3& cameraPos) {
    scene.ForEachEntity([&](Entity entity) {
        const std::string& name = scene.GetName(entity);
        if (name.size() < 3 || name.substr(name.size() - 3) != " HP") {
            return;
        }
        const Entity parent = scene.GetParent(entity);
        if (!parent.IsValid()) {
            return;
        }
        const Vec3 origin = scene.GetTransform(parent).Position;
        const Vec3 toCam = cameraPos - origin;
        const float worldYaw = std::atan2(toCam.x, toCam.z);
        float parentYaw = 0.0f;
        if (scene.HasCharacterController(parent)) {
            parentYaw = scene.GetCharacterController(parent).FacingYaw;
        }
        scene.GetTransform(entity).Rotation =
            Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, worldYaw - parentYaw);
    });
}

void CollectPickups(Scene& scene, Entity player) {
    if (!player.IsValid() || !scene.HasCharacterController(player) ||
        scene.GetCharacterController(player).Dead) {
        return;
    }
    CharacterControllerComponent& character = scene.GetCharacterController(player);
    const Vec3 pos = scene.GetTransform(player).Position;
    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasPickup(entity)) {
            return;
        }
        PickupComponent& pickup = scene.GetPickup(entity);
        if (pickup.Taken) {
            return;
        }
        Vec3 delta = scene.GetTransform(entity).Position - pos;
        delta.y = 0.0f;
        if (delta.Length() > 1.15f) {
            return;
        }
        character.Health = std::min(character.MaxHealth, character.Health + pickup.Heal);
        pickup.Taken = true;
        if (scene.HasMeshRenderer(entity)) {
            scene.GetMeshRenderer(entity).Opacity = 0.0f;
        }
        UpdateHealthBar(scene, player);
    });
}

bool SolidBlocksSegment(const Scene& scene, const Vec3& from, const Vec3& to) {
    std::vector<Aabb> solids;
    CollectMapSolids(scene, solids);
    for (const Aabb& box : solids) {
        float t = 1.0f;
        if (SegmentHitsAabb(from, to, box, t) && t > 0.04f && t < 0.96f) {
            return true;
        }
    }
    return false;
}

void UnstickCharacter(Scene& scene, Entity entity) {
    if (!entity.IsValid() || !scene.HasCharacterController(entity)) {
        return;
    }
    const float radius = scene.GetCharacterController(entity).Radius;
    ResolveCharacterVsMap(scene, entity, radius);
    PlaceOnGround(scene, entity);
    Vec3& pos = scene.GetTransform(entity).Position;
    if (std::abs(pos.x) > 4.2f) {
        pos.x *= 0.45f;
        ResolveCharacterVsMap(scene, entity, radius);
        PlaceOnGround(scene, entity);
    }
}

std::string UniqueGameplayName(const Scene& scene, const std::string& base) {
    if (!scene.FindEntityByName(base).IsValid()) {
        return base;
    }
    for (int i = 2; i < 1000; ++i) {
        const std::string candidate = base + " " + std::to_string(i);
        if (!scene.FindEntityByName(candidate).IsValid()) {
            return candidate;
        }
    }
    return base + " Copy";
}

void MaybeStartNextWave(Scene& scene, float deltaSeconds) {
    if (!scene.Settings().EnableWaves) {
        return;
    }
    int alive = 0;
    int dead = 0;
    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasCharacterController(entity) || scene.GetCharacterController(entity).Team != 1) {
            return;
        }
        if (scene.GetCharacterController(entity).Dead) {
            ++dead;
        } else {
            ++alive;
        }
    });
    if (alive > 0) {
        scene.Settings().PendingWave = false;
        scene.Settings().WaveTimer = 1.4f;
        return;
    }
    if (dead == 0 || scene.Settings().Wave >= 5) {
        scene.Settings().PendingWave = false;
        return;
    }
    if (!scene.Settings().PendingWave) {
        scene.Settings().PendingWave = true;
        scene.Settings().WaveTimer = 1.4f;
        return;
    }
    scene.Settings().WaveTimer -= deltaSeconds;
    if (scene.Settings().WaveTimer > 0.0f) {
        return;
    }
    scene.Settings().PendingWave = false;
    scene.Settings().Wave += 1;
    scene.Settings().Score += 25;
    const int count = 2 + scene.Settings().Wave;
    const Vec3 spots[] = {{0.0f, 0.0f, -6.0f}, {0.0f, 0.0f, 6.0f}, {4.5f, 0.0f, 0.0f},
                          {-4.5f, 0.0f, 0.0f}, {3.2f, 0.0f, 4.0f}, {-3.2f, 0.0f, -4.0f}};
    for (int i = 0; i < count; ++i) {
        Entity spawned = SpawnEnemy(scene, spots[static_cast<std::size_t>(i) % 6]);
        UnstickCharacter(scene, spawned);
    }
}

void ReviveCharacter(Scene& scene, Entity entity) {
    if (!entity.IsValid() || !scene.HasCharacterController(entity)) {
        return;
    }
    CharacterControllerComponent& character = scene.GetCharacterController(entity);
    character.Dead = false;
    character.Health = character.MaxHealth;
    character.HurtTimer = 0.0f;
    character.KnockbackVelocity = {};
    if (scene.HasPlayerController(entity)) {
        scene.GetPlayerController(entity).Enabled = true;
    }
    auto restoreMesh = [&](Entity meshEntity) {
        if (!scene.HasMeshRenderer(meshEntity)) {
            return;
        }
        MeshRendererComponent& mesh = scene.GetMeshRenderer(meshEntity);
        mesh.Opacity = 1.0f;
        const std::string& name = scene.GetName(meshEntity);
        if (!mesh.AssetPath.empty()) {
            mesh.UseAlbedoTexture = !mesh.AlbedoTexturePath.empty();
            mesh.AlbedoColor = {1.0f, 1.0f, 1.0f};
            return;
        }
        if (name.find("Face") != std::string::npos) {
            mesh.AlbedoColor = {0.86f, 0.68f, 0.52f};
        } else if (name.find("Sword") != std::string::npos || name.find("Crest") != std::string::npos) {
            mesh.AlbedoColor = {0.82f, 0.64f, 0.22f};
        } else if (name.find("Cape") != std::string::npos) {
            mesh.AlbedoColor = {0.18f, 0.22f, 0.48f};
        } else if (name.find("Hood") != std::string::npos) {
            mesh.AlbedoColor = {0.55f, 0.14f, 0.16f};
        } else if (name.find("Club") != std::string::npos) {
            mesh.AlbedoColor = {0.42f, 0.28f, 0.14f};
        } else if (name.find("Leg") != std::string::npos) {
            mesh.AlbedoColor = character.Team == 0 ? Vec3{0.22f, 0.24f, 0.30f} : Vec3{0.46f, 0.30f, 0.18f};
        } else if (name.find("HP") != std::string::npos) {
            mesh.AlbedoColor = character.Team == 0 ? Vec3{0.25f, 0.82f, 0.32f} : Vec3{0.86f, 0.18f, 0.16f};
        } else {
            mesh.AlbedoColor = character.Team == 0 ? Vec3{0.72f, 0.76f, 0.84f} : Vec3{0.46f, 0.30f, 0.18f};
        }
    };
    restoreMesh(entity);
    scene.ForEachEntity([&](Entity child) {
        if (scene.GetParent(child).Id != entity.Id) {
            return;
        }
        restoreMesh(child);
    });
    PlaceOnGround(scene, entity);
    UpdateHealthBar(scene, entity);
}

} // namespace

void EnsureFollowCamera(Scene& scene) {
    Entity camera = scene.FindPrimaryCamera();
    if (!camera.IsValid()) {
        return;
    }
    if (!scene.HasFollowCamera(camera)) {
        FollowCameraComponent follow;
        follow.TargetName = "Player";
        follow.Distance = 6.8f;
        follow.PitchRadians = 0.42f;
        follow.YawRadians = 0.0f;
        follow.MouseSensitivity = 0.0032f;
        scene.AddFollowCamera(camera, follow);
        return;
    }
    FollowCameraComponent& follow = scene.GetFollowCamera(camera);
    if (follow.TargetName.empty()) {
        follow.TargetName = "Player";
    }
    follow.PitchRadians = Clamp(follow.PitchRadians, 0.12f, 1.15f);
    if (follow.MouseSensitivity < 0.0005f || follow.MouseSensitivity > 0.02f) {
        follow.MouseSensitivity = 0.0032f;
    }
}

void AttachHumanoidVisual(Scene& scene, Entity root, bool knight) {
    bool hasBody = false;
    scene.ForEachEntity([&](Entity entity) {
        if (scene.GetParent(entity).Id == root.Id &&
            scene.GetName(entity) == scene.GetName(root) + " Body") {
            hasBody = true;
        }
    });
    if (hasBody) {
        return;
    }

    const Vec3 steel{0.72f, 0.76f, 0.84f};
    const Vec3 dark{0.22f, 0.24f, 0.30f};
    const Vec3 gold{0.82f, 0.64f, 0.22f};
    const Vec3 skin{0.86f, 0.68f, 0.52f};
    const Vec3 leather{0.46f, 0.30f, 0.18f};
    const Vec3 cloth{0.55f, 0.14f, 0.16f};
    const Vec3 wood{0.42f, 0.28f, 0.14f};

    const std::string prefix = scene.GetName(root) + " ";
    if (knight) {
        AddPart(scene, root, prefix + "Body", MeshPrimitive::UnitCube, {0.0f, 1.08f, 0.04f},
                {0.46f, 0.58f, 0.30f}, steel);
        AddPart(scene, root, prefix + "Head", MeshPrimitive::UnitSphere, {0.0f, 1.52f, 0.04f},
                {0.30f, 0.30f, 0.30f}, steel);
        AddPart(scene, root, prefix + "Face", MeshPrimitive::UnitSphere, {0.0f, 1.48f, 0.12f},
                {0.16f, 0.14f, 0.10f}, skin);
        AddPart(scene, root, prefix + "LegL", MeshPrimitive::UnitCube, {-0.12f, 0.42f, 0.02f},
                {0.16f, 0.72f, 0.16f}, dark);
        AddPart(scene, root, prefix + "LegR", MeshPrimitive::UnitCube, {0.12f, 0.42f, 0.02f},
                {0.16f, 0.72f, 0.16f}, dark);
        AddPart(scene, root, prefix + "ArmL", MeshPrimitive::UnitCube, {-0.34f, 1.08f, 0.04f},
                {0.14f, 0.52f, 0.14f}, steel);
        AddPart(scene, root, prefix + "ArmR", MeshPrimitive::UnitCube, {0.34f, 1.08f, 0.08f},
                {0.14f, 0.52f, 0.14f}, steel);
        AddPart(scene, root, prefix + "Sword", MeshPrimitive::UnitCube, {0.46f, 1.05f, 0.42f},
                {0.08f, 0.08f, 1.15f}, gold);
        AddPart(scene, root, prefix + "Shield", MeshPrimitive::UnitCube, {-0.46f, 0.98f, 0.02f},
                {0.08f, 0.55f, 0.42f}, steel);
        AddPart(scene, root, prefix + "Cape", MeshPrimitive::UnitCube, {0.0f, 1.05f, -0.18f},
                {0.42f, 0.7f, 0.08f}, {0.18f, 0.22f, 0.48f});
        AddPart(scene, root, prefix + "Crest", MeshPrimitive::UnitCube, {0.0f, 1.72f, 0.04f},
                {0.06f, 0.22f, 0.16f}, gold);
    } else {
        AddPart(scene, root, prefix + "Body", MeshPrimitive::UnitCube, {0.0f, 1.02f, 0.02f},
                {0.40f, 0.52f, 0.26f}, leather);
        AddPart(scene, root, prefix + "Head", MeshPrimitive::UnitSphere, {0.0f, 1.46f, 0.02f},
                {0.26f, 0.26f, 0.26f}, skin);
        AddPart(scene, root, prefix + "Hood", MeshPrimitive::UnitSphere, {0.0f, 1.54f, -0.02f},
                {0.32f, 0.22f, 0.32f}, cloth);
        AddPart(scene, root, prefix + "LegL", MeshPrimitive::UnitCube, {-0.10f, 0.40f, 0.02f},
                {0.14f, 0.68f, 0.14f}, leather);
        AddPart(scene, root, prefix + "LegR", MeshPrimitive::UnitCube, {0.10f, 0.40f, 0.02f},
                {0.14f, 0.68f, 0.14f}, leather);
        AddPart(scene, root, prefix + "ArmL", MeshPrimitive::UnitCube, {-0.30f, 1.02f, 0.04f},
                {0.12f, 0.48f, 0.12f}, leather);
        AddPart(scene, root, prefix + "ArmR", MeshPrimitive::UnitCube, {0.30f, 1.02f, 0.08f},
                {0.12f, 0.48f, 0.12f}, leather);
        AddPart(scene, root, prefix + "Club", MeshPrimitive::UnitCube, {0.38f, 1.10f, 0.22f},
                {0.10f, 0.10f, 0.72f}, wood);
    }
}

void SetEntityMeshAsset(Scene& scene, Entity entity, const std::string& meshAssetPath,
                        const std::string& texturePath) {
    if (!entity.IsValid() || !scene.IsAlive(entity) || meshAssetPath.empty()) {
        return;
    }
    std::vector<Entity> extras;
    scene.ForEachEntity([&](Entity child) {
        if (scene.GetParent(child).Id != entity.Id) {
            return;
        }
        const std::string& name = scene.GetName(child);
        if (name.size() >= 3 && name.substr(name.size() - 3) == " HP") {
            return;
        }
        extras.push_back(child);
    });
    for (Entity child : extras) {
        scene.DestroyEntity(child);
    }
    MeshRendererComponent mesh;
    mesh.AssetPath = meshAssetPath;
    mesh.AlbedoTexturePath = texturePath;
    mesh.UseAlbedoTexture = !texturePath.empty();
    mesh.AlbedoColor = {1.0f, 1.0f, 1.0f};
    mesh.Opacity = 1.0f;
    if (scene.HasMeshRenderer(entity)) {
        scene.GetMeshRenderer(entity) = mesh;
    } else {
        scene.AddMeshRenderer(entity, mesh);
    }
    scene.GetTransform(entity).Scale = {1.0f, 1.0f, 1.0f};
}

Entity SpawnGround(Scene& scene) {
    Entity ground = SpawnColored(scene, "Ground", MeshPrimitive::UnitPlane, {0.0f, 0.0f, 0.0f},
                                 {30.0f, 1.0f, 30.0f}, {0.34f, 0.50f, 0.24f});
    if (scene.HasMeshRenderer(ground)) {
        MeshRendererComponent& mesh = scene.GetMeshRenderer(ground);
        mesh.AlbedoTexturePath = "Assets/Textures/ground.png";
        mesh.UseAlbedoTexture = true;
        mesh.ReceiveShadows = true;
    }
    return ground;
}

void PlaceOnGround(Scene& scene, Entity entity) {
    if (!entity.IsValid() || !scene.IsAlive(entity)) {
        return;
    }
    Transform& xform = scene.GetTransform(entity);
    float feet = 0.0f;
    if (scene.HasCharacterController(entity)) {
        feet = scene.GetCharacterController(entity).FootOffset;
    }
    xform.Position.y = GroundHeightUnder(scene, xform.Position) + feet;
}

Entity SpawnSimpleMap(Scene& scene) {
    Entity ground = SpawnGround(scene);
    SpawnColored(scene, "Road", MeshPrimitive::UnitPlane, {0.0f, 0.02f, 0.0f}, {4.2f, 1.0f, 22.0f},
                 {0.45f, 0.36f, 0.24f});
    SpawnColored(scene, "Wall North", MeshPrimitive::UnitCube, {0.0f, 1.3f, -14.5f},
                 {30.0f, 2.6f, 0.55f}, {0.52f, 0.50f, 0.46f});
    SpawnColored(scene, "Wall South", MeshPrimitive::UnitCube, {0.0f, 1.3f, 14.5f},
                 {30.0f, 2.6f, 0.55f}, {0.52f, 0.50f, 0.46f});
    SpawnColored(scene, "Wall West", MeshPrimitive::UnitCube, {-14.5f, 1.3f, 0.0f},
                 {0.55f, 2.6f, 30.0f}, {0.52f, 0.50f, 0.46f});
    SpawnColored(scene, "Wall East", MeshPrimitive::UnitCube, {14.5f, 1.3f, 0.0f},
                 {0.55f, 2.6f, 30.0f}, {0.52f, 0.50f, 0.46f});

    auto house = [&](const std::string& name, const Vec3& pos, const Vec3& size) {
        Entity body = SpawnColored(scene, name, MeshPrimitive::UnitCube,
                                   {pos.x, size.y * 0.5f, pos.z}, size, {0.62f, 0.42f, 0.28f});
        Entity roof = SpawnColored(scene, name + " Roof", MeshPrimitive::UnitCube,
                                   {0.0f, size.y * 0.5f + 0.28f, 0.0f},
                                   {size.x + 0.3f, 0.35f, size.z + 0.3f}, {0.42f, 0.16f, 0.14f});
        scene.SetParent(roof, body);
    };
    house("House A", {-8.5f, 0.0f, -6.0f}, {3.4f, 2.2f, 3.0f});
    house("House B", {8.8f, 0.0f, -5.2f}, {3.0f, 2.0f, 3.4f});
    house("House C", {-7.8f, 0.0f, 7.4f}, {3.6f, 2.4f, 3.2f});
    SpawnColored(scene, "Well", MeshPrimitive::UnitCube, {2.2f, 0.45f, 2.4f},
                 {1.1f, 0.9f, 1.1f}, {0.48f, 0.48f, 0.50f});
    SpawnColored(scene, "Crate", MeshPrimitive::UnitCube, {-2.4f, 0.4f, 4.2f}, {0.8f, 0.8f, 0.8f},
                 {0.40f, 0.28f, 0.16f});
    SpawnColored(scene, "Crate 2", MeshPrimitive::UnitCube, {5.4f, 0.35f, 6.0f}, {0.7f, 0.7f, 0.7f},
                 {0.40f, 0.28f, 0.16f});
    auto tree = [&](const std::string& name, const Vec3& pos) {
        Entity trunk = SpawnColored(scene, name, MeshPrimitive::UnitCube, {pos.x, 0.9f, pos.z},
                                    {0.32f, 1.8f, 0.32f}, {0.38f, 0.24f, 0.12f});
        Entity crown = SpawnColored(scene, name + " Crown", MeshPrimitive::UnitSphere,
                                    {0.0f, 1.25f, 0.0f}, {1.6f, 1.3f, 1.6f}, {0.20f, 0.48f, 0.18f});
        scene.SetParent(crown, trunk);
    };
    tree("Tree 1", {-11.0f, 0.0f, 1.5f});
    tree("Tree 2", {11.2f, 0.0f, 3.0f});
    tree("Tree 3", {1.5f, 0.0f, -10.5f});
    tree("Tree 4", {-4.0f, 0.0f, 11.0f});
    SpawnColored(scene, "Fence 1", MeshPrimitive::UnitCube, {0.0f, 0.45f, -8.8f}, {3.4f, 0.9f, 0.16f},
                 {0.40f, 0.28f, 0.16f});
    SpawnColored(scene, "Fence 2", MeshPrimitive::UnitCube, {-3.2f, 0.45f, 3.6f}, {0.16f, 0.9f, 2.4f},
                 {0.40f, 0.28f, 0.16f});
    SpawnColored(scene, "Grass A", MeshPrimitive::UnitCube, {3.6f, 0.08f, -2.4f}, {0.9f, 0.12f, 0.7f},
                 {0.28f, 0.58f, 0.20f});
    SpawnColored(scene, "Grass B", MeshPrimitive::UnitCube, {-4.8f, 0.08f, -2.0f}, {0.8f, 0.12f, 0.8f},
                 {0.26f, 0.54f, 0.18f});
    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasMeshRenderer(entity)) {
            return;
        }
        MeshRendererComponent& mesh = scene.GetMeshRenderer(entity);
        const std::string& name = scene.GetName(entity);
        if (name == "Ground") {
            mesh.AlbedoTexturePath = "Assets/Textures/ground.png";
            mesh.UseAlbedoTexture = true;
            mesh.ReceiveShadows = true;
        } else if (name.rfind("Wall", 0) == 0 || name.rfind("House", 0) == 0 || name == "Well" ||
                   name.rfind("Fence", 0) == 0 || name == "Road" ||
                   name.find("Roof") != std::string::npos) {
            mesh.AlbedoTexturePath = "Assets/Textures/stone.png";
            mesh.UseAlbedoTexture = true;
        }
    });
    scene.ForEachEntity([&](Entity entity) {
        if (scene.GetParent(entity).IsValid() || !scene.HasMeshRenderer(entity)) {
            return;
        }
        const std::string& name = scene.GetName(entity);
        if (name.rfind("Tree", 0) == 0) {
            SetEntityMeshAsset(scene, entity, "Assets/Characters/tree.obj", {});
            scene.GetTransform(entity).Position.y = 0.0f;
        } else if (name.rfind("Crate", 0) == 0) {
            SetEntityMeshAsset(scene, entity, "Assets/Characters/crate.obj",
                               "Assets/Textures/stone.png");
            scene.GetTransform(entity).Position.y = 0.0f;
        }
    });
    scene.Settings().ClearColor = {0.55f, 0.70f, 0.86f};
    return ground;
}

Entity SpawnPlayer(Scene& scene, const std::string& meshAssetPath) {
    Entity player = scene.CreateEntity(UniqueGameplayName(scene, "Player"));
    if (!meshAssetPath.empty()) {
        SetEntityMeshAsset(scene, player, meshAssetPath, "Assets/Textures/characters.png");
    } else {
        SetEntityMeshAsset(scene, player, "Assets/Characters/knight_armed.obj",
                           "Assets/Textures/characters.png");
    }
    CharacterControllerComponent character;
    character.Height = 1.85f;
    character.Team = 0;
    character.Health = kPlayerMaxHealth;
    character.MaxHealth = kPlayerMaxHealth;
    character.AttackDamage = kPlayerDamage;
    character.AttackRange = 2.5f;
    character.FacingYaw = 0.0f;
    character.Radius = 0.40f;
    scene.AddCharacterController(player, character);
    scene.AddPlayerController(player);
    if (!scene.HasPointLight(player)) {
        PointLightComponent torch;
        torch.Color = {1.0f, 0.86f, 0.62f};
        torch.Intensity = 2.1f;
        torch.Range = 9.0f;
        scene.AddPointLight(player, torch);
    }
    ScriptComponent script;
    script.AssetPath = "Assets/Scripts/player.ns";
    script.Source = DefaultPlayerScript();
    scene.AddScript(player, script);
    scene.GetTransform(player).Position = {0.0f, 0.0f, 0.0f};
    PlaceOnGround(scene, player);
    UpdateHealthBar(scene, player);
    EnsureFollowCamera(scene);
    return player;
}

Entity SpawnEnemy(Scene& scene, const Vec3& position, const std::string& meshAssetPath) {
    int banditIndex = 1;
    scene.ForEachEntity([&](Entity entity) {
        if (scene.GetName(entity).rfind("Bandit", 0) == 0 &&
            !scene.GetParent(entity).IsValid()) {
            ++banditIndex;
        }
    });
    Entity enemy = scene.CreateEntity("Bandit " + std::to_string(banditIndex));
    std::string meshPath = meshAssetPath;
    std::string texPath;
    if (meshPath.empty()) {
        scene.ForEachEntity([&](Entity entity) {
            if (!meshPath.empty() || !scene.HasCharacterController(entity) ||
                scene.GetCharacterController(entity).Team != 1 || !scene.HasMeshRenderer(entity)) {
                return;
            }
            const MeshRendererComponent& existing = scene.GetMeshRenderer(entity);
            if (!existing.AssetPath.empty()) {
                meshPath = existing.AssetPath;
                texPath = existing.AlbedoTexturePath;
            }
        });
    } else {
        texPath = "Assets/Textures/characters.png";
    }
    if (!meshPath.empty()) {
        if (texPath.empty()) {
            texPath = "Assets/Textures/characters.png";
        }
        SetEntityMeshAsset(scene, enemy, meshPath, texPath);
    } else {
        SetEntityMeshAsset(scene, enemy, "Assets/Characters/bandit.obj",
                           "Assets/Textures/characters.png");
    }
    CharacterControllerComponent character;
    character.MoveSpeed = 3.2f;
    character.Height = 1.75f;
    character.Team = 1;
    character.Health = kEnemyMaxHealth;
    character.MaxHealth = kEnemyMaxHealth;
    character.AttackDamage = kEnemyDamage;
    character.AttackRange = 1.65f;
    character.DetectRange = 15.0f;
    character.Radius = 0.34f;
    scene.AddCharacterController(enemy, character);
    scene.GetTransform(enemy).Position = position;
    PlaceOnGround(scene, enemy);
    UnstickCharacter(scene, enemy);
    UpdateHealthBar(scene, enemy);
    return enemy;
}

Entity SpawnImportedMesh(Scene& scene, const std::string& name, const std::string& meshAssetPath) {
    Entity entity = scene.CreateEntity(name);
    MeshRendererComponent mesh;
    mesh.AssetPath = meshAssetPath;
    mesh.AlbedoColor = {1.0f, 1.0f, 1.0f};
    mesh.UseAlbedoTexture = false;
    const auto dot = meshAssetPath.find_last_of('.');
    if (dot != std::string::npos) {
        mesh.AlbedoTexturePath = meshAssetPath.substr(0, dot) + ".png";
        mesh.UseAlbedoTexture = true;
    }
    scene.AddMeshRenderer(entity, mesh);
    scene.AddCollider(entity);
    scene.GetTransform(entity).Position = {0.0f, 0.0f, 0.0f};
    PlaceOnGround(scene, entity);
    return entity;
}

Entity SpawnHealthPickup(Scene& scene, const Vec3& position) {
    int index = 1;
    scene.ForEachEntity([&](Entity entity) {
        if (NameStartsWith(scene.GetName(entity), "Herb")) {
            ++index;
        }
    });
    Entity herb = SpawnColored(scene, "Herb " + std::to_string(index), MeshPrimitive::UnitSphere,
                               position, {0.28f, 0.28f, 0.28f}, {0.22f, 0.82f, 0.38f});
    scene.RemoveCollider(herb);
    PickupComponent pickup;
    pickup.Heal = 40.0f;
    scene.AddPickup(herb, pickup);
    PlaceOnGround(scene, herb);
    scene.GetTransform(herb).Position.y += 0.35f;
    return herb;
}

bool SceneHasFollowCamera(const Scene& scene) { return FindFollowCamera(scene).IsValid(); }

void ApplyMeleeHit(Scene& scene, Entity attacker) {
    if (!attacker.IsValid() || !scene.HasCharacterController(attacker)) {
        return;
    }
    const CharacterControllerComponent& atk = scene.GetCharacterController(attacker);
    if (atk.Dead) {
        return;
    }
    const Vec3 origin = scene.GetTransform(attacker).Position;
    const Vec3 body{std::sin(atk.FacingYaw), 0.0f, std::cos(atk.FacingYaw)};
    const Vec3 aim = scene.HasPlayerController(attacker)
                         ? CameraLookXZ(FollowCameraYaw(scene, atk.FacingYaw))
                         : body;
    std::vector<Entity> nearby;
    scene.ForEachEntity([&](Entity entity) {
        if (entity.Id == attacker.Id || !scene.HasCharacterController(entity)) {
            return;
        }
        const CharacterControllerComponent& other = scene.GetCharacterController(entity);
        if (other.Dead || other.Team == atk.Team || other.HurtTimer > 0.0f) {
            return;
        }
        Vec3 delta = scene.GetTransform(entity).Position - origin;
        delta.y = 0.0f;
        if (delta.Length() <= atk.AttackRange) {
            nearby.push_back(entity);
        }
    });
    if (nearby.empty()) {
        return;
    }
    std::vector<Entity> victims;
    for (Entity entity : nearby) {
        Vec3 delta = scene.GetTransform(entity).Position - origin;
        delta.y = 0.0f;
        const Vec3 dir = delta.LengthSq() > 1e-6f ? delta.Normalized() : aim;
        const Vec3 from = origin + Vec3{0.0f, 1.1f, 0.0f};
        const Vec3 to = scene.GetTransform(entity).Position + Vec3{0.0f, 1.1f, 0.0f};
        if ((dir.Dot(aim) > 0.12f || dir.Dot(body) > 0.12f) &&
            !SolidBlocksSegment(scene, from, to)) {
            victims.push_back(entity);
        }
    }
    if (victims.empty()) {
        for (Entity entity : nearby) {
            const Vec3 from = origin + Vec3{0.0f, 1.1f, 0.0f};
            const Vec3 to = scene.GetTransform(entity).Position + Vec3{0.0f, 1.1f, 0.0f};
            if (!SolidBlocksSegment(scene, from, to)) {
                victims.push_back(entity);
                break;
            }
        }
    }
    const float stun = atk.Team == 0 ? 0.12f : 0.45f;
    const float knock = atk.Team == 0 ? 7.5f : 3.2f;
    for (Entity entity : victims) {
        CharacterControllerComponent& other = scene.GetCharacterController(entity);
        other.Health -= atk.AttackDamage;
        other.HurtTimer = stun;
        Vec3 away = scene.GetTransform(entity).Position - origin;
        away.y = 0.0f;
        if (away.LengthSq() > 1e-6f) {
            other.KnockbackVelocity = away.Normalized() * knock;
        }
        if (other.Health <= 0.0f) {
            MarkDead(scene, entity);
            if (atk.Team == 0) {
                scene.Settings().Score += 10;
            }
        }
        UpdateHealthBar(scene, entity);
    }
}

void EnsurePlayableCombatLevel(Scene& scene) {
    if (!scene.FindEntityByName("Ground").IsValid()) {
        SpawnSimpleMap(scene);
    } else if (!scene.FindEntityByName("House A").IsValid()) {
        SpawnSimpleMap(scene);
    }
    Entity player = scene.FindEntityByName("Player");
    if (!player.IsValid() || !scene.HasCharacterController(player)) {
        player = SpawnPlayer(scene);
    }
    if (player.IsValid()) {
        if (!scene.HasMeshRenderer(player) || scene.GetMeshRenderer(player).AssetPath.empty()) {
            SetEntityMeshAsset(scene, player, "Assets/Characters/knight_armed.obj",
                               "Assets/Textures/characters.png");
        }
        if (!scene.HasPlayerController(player)) {
            scene.AddPlayerController(player);
        }
        CharacterControllerComponent& stats = scene.GetCharacterController(player);
        if (stats.MaxHealth < 150.0f) {
            stats.MaxHealth = 200.0f;
            stats.Health = 200.0f;
        }
        if (stats.AttackDamage < 40.0f) {
            stats.AttackDamage = 50.0f;
        }
        stats.Team = 0;
        stats.Radius = std::max(stats.Radius, 0.40f);
        if (!scene.HasPointLight(player)) {
            PointLightComponent torch;
            torch.Color = {1.0f, 0.86f, 0.62f};
            torch.Intensity = 2.1f;
            torch.Range = 9.0f;
            scene.AddPointLight(player, torch);
        }
    }
    bool hasEnemy = false;
    scene.ForEachEntity([&](Entity entity) {
        if (scene.HasCharacterController(entity) && scene.GetCharacterController(entity).Team == 1 &&
            !scene.GetCharacterController(entity).Dead) {
            hasEnemy = true;
        }
    });
    if (!hasEnemy) {
        SpawnEnemy(scene, {6.5f, 0.0f, -3.5f});
        SpawnEnemy(scene, {-6.2f, 0.0f, 5.0f});
        SpawnEnemy(scene, {4.8f, 0.0f, 8.0f});
        SpawnEnemy(scene, {-3.5f, 0.0f, -7.2f});
    }
    EnsureFollowCamera(scene);
    scene.ForEachEntity([&](Entity entity) {
        if (IsMapSolid(scene, entity) && !scene.HasCollider(entity)) {
            scene.AddCollider(entity);
        }
    });
    bool hasHerb = false;
    scene.ForEachEntity([&](Entity entity) {
        if (scene.HasPickup(entity) && !scene.GetPickup(entity).Taken) {
            hasHerb = true;
        }
    });
    if (!hasHerb) {
        SpawnHealthPickup(scene, {-1.6f, 0.0f, 2.2f});
        SpawnHealthPickup(scene, {3.4f, 0.0f, -1.8f});
        SpawnHealthPickup(scene, {0.0f, 0.0f, -4.5f});
    }
    if (scene.Settings().Wave < 1) {
        scene.Settings().Wave = 1;
    }
}

GameplayHudSnapshot QueryGameplayHud(const Scene& scene) {
    GameplayHudSnapshot hud;
    Entity player{};
    scene.ForEachEntity([&](Entity entity) {
        if (scene.HasPlayerController(entity) && scene.HasCharacterController(entity)) {
            player = entity;
        }
    });
    if (!player.IsValid()) {
        player = scene.FindEntityByName("Player");
    }
    if (player.IsValid() && scene.HasCharacterController(player)) {
        const CharacterControllerComponent& character = scene.GetCharacterController(player);
        hud.PlayerHealth = character.Health;
        hud.PlayerMaxHealth = character.MaxHealth;
        hud.PlayerAlive = !character.Dead;
    }
    hud.Wave = scene.Settings().Wave;
    hud.Score = scene.Settings().Score;
    hud.WaveCountdown = std::max(0.0f, scene.Settings().WaveTimer);
    const Vec3 playerPos =
        player.IsValid() ? scene.GetTransform(player).Position : Vec3{0.0f, 0.0f, 0.0f};
    float best = 1.0e9f;
    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasCharacterController(entity) || scene.GetCharacterController(entity).Team != 1) {
            return;
        }
        const CharacterControllerComponent& enemy = scene.GetCharacterController(entity);
        if (!enemy.Dead) {
            ++hud.AliveEnemies;
        }
        const float dist = (scene.GetTransform(entity).Position - playerPos).Length();
        if (dist < best && dist < 8.0f) {
            best = dist;
            hud.TargetHealth = enemy.Health;
            hud.TargetMaxHealth = enemy.MaxHealth;
        }
    });
    hud.ShowCombatHud = scene.Settings().EnableWaves;
    if (hud.PlayerMaxHealth <= 0.0f) {
        hud.Phase = GameplayPhase::Combat;
        hud.PlayerAlive = false;
    } else if (!hud.PlayerAlive) {
        hud.Phase = GameplayPhase::Dead;
    } else if (hud.ShowCombatHud && hud.AliveEnemies == 0 && hud.Wave >= 5) {
        hud.Phase = GameplayPhase::Victory;
    } else if (hud.ShowCombatHud && hud.AliveEnemies == 0 && scene.Settings().PendingWave) {
        hud.Phase = GameplayPhase::NextWave;
    } else {
        hud.Phase = GameplayPhase::Combat;
    }
    return hud;
}

std::string FormatGameplayHudTitle(const GameplayHudSnapshot& hud) {
    std::ostringstream title;
    title << "NOVA3D";
    if (hud.PlayerMaxHealth > 0.0f) {
        title << "  HP " << static_cast<int>(hud.PlayerHealth) << "/"
              << static_cast<int>(hud.PlayerMaxHealth);
    }
    if (hud.ShowCombatHud) {
        title << "  Wave " << hud.Wave << "  Score " << hud.Score << "  Bandits "
              << hud.AliveEnemies;
    }
    if (hud.Phase == GameplayPhase::Dead) {
        title << "  — R to stand";
    } else if (hud.Phase == GameplayPhase::Victory) {
        title << "  — village clear";
    } else if (hud.Phase == GameplayPhase::NextWave) {
        title << "  — next wave";
    }
    return title.str();
}

const char* GameplayHudStatusLine(const GameplayHudSnapshot& hud) {
    if (!hud.ShowCombatHud) {
        if (hud.PlayerMaxHealth <= 0.0f) {
            return "Пустая сцена. Расставьте объекты в редакторе, затем Play.";
        }
        if (!hud.PlayerAlive) {
            return "Падение. R — встать.";
        }
        return "Мышь — обзор   WASD — ходьба   Пробел — прыжок   F8 — сейв   F9 — загрузка";
    }
    switch (hud.Phase) {
    case GameplayPhase::Dead:
        return "Падение. R — встать.";
    case GameplayPhase::Victory:
        return "Деревня зачищена. R — заново. Esc — выход.";
    case GameplayPhase::NextWave:
        return "Следующая волна…";
    case GameplayPhase::Combat:
    default:
        return "Мышь — обзор   WASD — ходьба   F/ЛКМ — удар   Shift — бег   R — встать   F8/F9 — сейв";
    }
}

void TickScene(Scene& scene, float deltaSeconds, const Input* input, EngineSettings* settings,
               const std::filesystem::path& projectRoot, bool playJustStarted) {
    if (deltaSeconds <= 0.0f && !playJustStarted) {
        return;
    }

    EngineSettings localSettings = EngineSettings::Defaults();
    EngineSettings& cfg = settings ? *settings : localSettings;
    TickScripts(scene, input, cfg, projectRoot, playJustStarted);

    if (deltaSeconds <= 0.0f) {
        return;
    }

    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasRotator(entity)) {
            return;
        }
        const RotatorComponent& rot = scene.GetRotator(entity);
        Transform& xform = scene.GetTransform(entity);
        const Quat qx = Quat::FromAxisAngle({1.0f, 0.0f, 0.0f}, rot.AngularVelocity.x * deltaSeconds);
        const Quat qy = Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, rot.AngularVelocity.y * deltaSeconds);
        const Quat qz = Quat::FromAxisAngle({0.0f, 0.0f, 1.0f}, rot.AngularVelocity.z * deltaSeconds);
        if (rot.LocalSpace) {
            xform.Rotation = (xform.Rotation * qy * qx * qz).Normalized();
        } else {
            xform.Rotation = (qy * qx * qz * xform.Rotation).Normalized();
        }
    });

    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasMover(entity)) {
            return;
        }
        scene.GetTransform(entity).Position =
            scene.GetTransform(entity).Position + scene.GetMover(entity).Velocity * deltaSeconds;
    });

    if (input) {
        scene.ForEachEntity([&](Entity entity) {
            if (!scene.HasFollowCamera(entity)) {
                return;
            }
            FollowCameraComponent& follow = scene.GetFollowCamera(entity);
            if (cfg.MouseLook) {
                follow.YawRadians -=
                    input->GetMouseDeltaX() * follow.MouseSensitivity * cfg.MouseSensitivity;
                float lookY = input->GetMouseDeltaY() * follow.MouseSensitivity * cfg.MouseSensitivity;
                if (!cfg.InvertY) {
                    lookY = -lookY;
                }
                follow.PitchRadians = Clamp(follow.PitchRadians + lookY, 0.12f, 1.15f);
                if (input->GetScrollY() != 0.0f) {
                    follow.Distance = Clamp(follow.Distance - input->GetScrollY() * 0.35f, 3.2f, 14.0f);
                }
            }
        });
    }

    const Entity namedPlayer = scene.FindEntityByName("Player");
    if (namedPlayer.IsValid() && scene.HasCharacterController(namedPlayer) && input) {
        const bool dead = scene.GetCharacterController(namedPlayer).Dead;
        if (dead && (input->IsKeyPressed(KeyCode::R) || cfg.IsActionPressed(*input, "interact"))) {
            ReviveCharacter(scene, namedPlayer);
        }
    }

    const Entity player = FindLocalPlayer(scene);
    if (player.IsValid() && input && scene.GetPlayerController(player).Enabled) {
        CharacterControllerComponent& character = scene.GetCharacterController(player);
        Transform& xform = scene.GetTransform(player);

        const float camYaw = FollowCameraYaw(scene, character.FacingYaw);
        const Vec3 forward = CameraLookXZ(camYaw);
        const Vec3 right{forward.z, 0.0f, -forward.x};
        Vec3 wish{};
        if (cfg.IsActionDown(*input, "forward") || input->IsKeyDown(KeyCode::Up)) {
            wish = wish + forward;
        }
        if (cfg.IsActionDown(*input, "back") || input->IsKeyDown(KeyCode::Down)) {
            wish = wish - forward;
        }
        if (cfg.IsActionDown(*input, "right") || input->IsKeyDown(KeyCode::Right)) {
            wish = wish + right;
        }
        if (cfg.IsActionDown(*input, "left") || input->IsKeyDown(KeyCode::Left)) {
            wish = wish - right;
        }

        character.Crouching = cfg.IsActionDown(*input, "crouch");
        character.DodgeTimer = std::max(0.0f, character.DodgeTimer - deltaSeconds);
        character.AttackTimer = std::max(0.0f, character.AttackTimer - deltaSeconds);

        if (character.Grounded && cfg.IsActionPressed(*input, "dodge") &&
            character.DodgeTimer <= 0.0f) {
            const Vec3 dodgeDir = wish.LengthSq() > 1e-6f ? wish.Normalized() : forward;
            character.DodgeVelocity = dodgeDir * character.DodgeSpeed;
            character.DodgeTimer = 0.28f;
            character.Grounded = false;
            character.VerticalVelocity = character.JumpSpeed * 0.22f;
        }

        const bool attackPressed = cfg.IsActionPressed(*input, "attack") ||
                                   input->IsMouseButtonPressed(MouseButton::Left);
        if (attackPressed && character.AttackTimer <= 0.0f) {
            character.AttackTimer = 0.4f;
            character.FacingYaw = std::atan2(forward.x, forward.z);
            ApplyMeleeHit(scene, player);
        }

        float speed = character.MoveSpeed;
        if (character.Crouching) {
            speed *= character.CrouchMultiplier;
        } else if (cfg.IsActionDown(*input, "sprint")) {
            speed *= character.SprintMultiplier;
        }
        if (!character.Grounded) {
            speed *= character.AirControl;
        }

        const bool moving = wish.LengthSq() > 1e-6f;
        if (character.DodgeTimer > 0.12f) {
            xform.Position = xform.Position + character.DodgeVelocity * deltaSeconds;
            character.WalkPhase += 14.0f * deltaSeconds;
        } else if (moving) {
            wish = wish.Normalized();
            xform.Position = xform.Position + wish * (speed * deltaSeconds);
            character.FacingYaw = std::atan2(wish.x, wish.z);
            character.WalkPhase += 10.0f * deltaSeconds;
        }
        ResolveCharacterVsMap(scene, player, character.Radius);

        const float slash =
            character.AttackTimer > 0.0f
                ? std::sin((0.4f - character.AttackTimer) / 0.4f * 3.14159265f) * 0.7f
                : 0.0f;
        xform.Rotation = Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, character.FacingYaw + slash);

        const float ground = GroundHeightUnder(scene, xform.Position) + character.FootOffset;
        if (character.Grounded && cfg.IsActionPressed(*input, "jump") && !character.Crouching) {
            character.VerticalVelocity = character.JumpSpeed;
            character.Grounded = false;
        }
        character.VerticalVelocity -= character.Gravity * deltaSeconds;
        xform.Position.y += character.VerticalVelocity * deltaSeconds;
        ClampToArena(xform.Position);
        if (xform.Position.y <= ground) {
            xform.Position.y = ground;
            character.VerticalVelocity = 0.0f;
            character.Grounded = true;
        } else {
            character.Grounded = false;
        }
        UpdateHealthBar(scene, player);
        AnimateHumanoidWalk(scene, player, character.WalkPhase, moving);
        CollectPickups(scene, player);
    }

    const Entity combatTarget = player.IsValid() ? player : namedPlayer;
    if (combatTarget.IsValid()) {
        const Vec3 playerPos = scene.GetTransform(combatTarget).Position;
        std::vector<Entity> enemies;
        scene.ForEachEntity([&](Entity entity) {
            if (scene.HasCharacterController(entity) &&
                scene.GetCharacterController(entity).Team == 1) {
                enemies.push_back(entity);
            }
        });
        for (Entity entity : enemies) {
            CharacterControllerComponent& ai = scene.GetCharacterController(entity);
            if (ai.Dead) {
                UpdateHealthBar(scene, entity);
                continue;
            }
            ai.AttackTimer = std::max(0.0f, ai.AttackTimer - deltaSeconds);
            ai.AiCooldown = std::max(0.0f, ai.AiCooldown - deltaSeconds);
            Transform& xform = scene.GetTransform(entity);
            Vec3 toPlayer = playerPos - xform.Position;
            toPlayer.y = 0.0f;
            const float dist = toPlayer.Length();
            if (dist > ai.DetectRange || dist < 1e-4f) {
                UpdateHealthBar(scene, entity);
                continue;
            }
            const Vec3 dir = toPlayer * (1.0f / dist);
            const bool chasing = dist > ai.AttackRange * 0.85f;
            if (chasing) {
                const Vec3 stepProbe = xform.Position + Vec3{0.0f, 1.1f, 0.0f} + dir * 1.1f;
                Vec3 step = dir;
                if (SolidBlocksSegment(scene, xform.Position + Vec3{0.0f, 1.1f, 0.0f}, stepProbe)) {
                    step = Vec3{-dir.z, 0.0f, dir.x};
                }
                xform.Position = xform.Position + step * (ai.MoveSpeed * deltaSeconds);
                ClampToArena(xform.Position);
                ResolveCharacterVsMap(scene, entity, ai.Radius);
                PlaceOnGround(scene, entity);
                ai.WalkPhase += 9.0f * deltaSeconds;
            }
            ai.FacingYaw = std::atan2(dir.x, dir.z);
            xform.Rotation = Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, ai.FacingYaw);
            AnimateHumanoidWalk(scene, entity, ai.WalkPhase, chasing);
            const Vec3 eye = xform.Position + Vec3{0.0f, 1.2f, 0.0f};
            const Vec3 targetEye = playerPos + Vec3{0.0f, 1.2f, 0.0f};
            const bool canSee = !SolidBlocksSegment(scene, eye, targetEye);
            if (dist <= ai.AttackRange && ai.AiCooldown <= 0.0f && canSee) {
                ai.AiCooldown = 1.25f;
                ai.AttackTimer = 0.35f;
                ApplyMeleeHit(scene, entity);
            }
            UpdateHealthBar(scene, entity);
        }
        MaybeStartNextWave(scene, deltaSeconds);
    }

    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasPickup(entity) || scene.GetPickup(entity).Taken) {
            return;
        }
        PickupComponent& pickup = scene.GetPickup(entity);
        pickup.Hover += deltaSeconds;
        Transform& herb = scene.GetTransform(entity);
        herb.Position.y = GroundHeightUnder(scene, herb.Position) + 0.38f +
                          0.08f * std::sin(pickup.Hover * 3.0f);
        herb.Rotation = Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, pickup.Hover * 1.6f);
    });

    std::vector<Entity> corpses;
    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasCharacterController(entity)) {
            return;
        }
        CharacterControllerComponent& character = scene.GetCharacterController(entity);
        if (character.Dead && character.CorpseTimer > 0.0f) {
            character.CorpseTimer -= deltaSeconds;
            if (character.CorpseTimer <= 0.0f && !scene.HasPlayerController(entity)) {
                corpses.push_back(entity);
            }
        }
        character.HurtTimer = std::max(0.0f, character.HurtTimer - deltaSeconds);
        if (character.KnockbackVelocity.LengthSq() > 1e-5f) {
            Transform& xform = scene.GetTransform(entity);
            xform.Position = xform.Position + character.KnockbackVelocity * deltaSeconds;
            character.KnockbackVelocity = character.KnockbackVelocity * std::max(0.0f, 1.0f - 8.0f * deltaSeconds);
            if (character.KnockbackVelocity.LengthSq() < 0.05f) {
                character.KnockbackVelocity = {};
            }
            ResolveCharacterVsMap(scene, entity, character.Radius);
            ClampToArena(xform.Position);
        }
    });
    for (Entity corpse : corpses) {
        scene.DestroyEntity(corpse);
    }
    SeparateCharacters(scene);

    std::vector<Aabb> solids;
    CollectMapSolids(scene, solids);
    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasFollowCamera(entity) || !scene.HasCamera(entity)) {
            return;
        }
        const FollowCameraComponent& follow = scene.GetFollowCamera(entity);
        Entity target = scene.FindEntityByName(follow.TargetName);
        if (!target.IsValid()) {
            target = FindLocalPlayer(scene);
        }
        if (!target.IsValid()) {
            return;
        }
        const Vec3 focus = scene.GetTransform(target).Position + Vec3{0.0f, 1.25f, 0.0f};
        const Vec3 desired =
            OrbitEyePosition(focus, follow.YawRadians, follow.PitchRadians, follow.Distance);
        Transform& camXform = scene.GetTransform(entity);
        camXform.Position = CameraEyeAvoidingSolids(focus, desired, solids, 0.32f);
        scene.GetCamera(entity).LookAtTarget = focus;
        BillboardHealthBars(scene, camXform.Position);
    });
}

Scene Scene::CreateSandboxLevel() {
    Scene scene = CreateEmptyLevel();
    SpawnGround(scene);
    SpawnPlayer(scene);
    EnsureFollowCamera(scene);
    scene.Settings().EnableWaves = false;
    Entity camera = scene.FindPrimaryCamera();
    if (camera.IsValid()) {
        scene.GetTransform(camera).Position = {0.0f, 4.4f, 7.2f};
        scene.GetCamera(camera).LookAtTarget = {0.0f, 1.25f, 0.0f};
    }
    return scene;
}

Scene Scene::CreatePlayableLevel() {
    Scene scene = CreateEmptyLevel();
    SpawnSimpleMap(scene);
    SpawnPlayer(scene);
    SpawnEnemy(scene, {6.5f, 0.0f, -3.5f});
    SpawnEnemy(scene, {-6.2f, 0.0f, 5.0f});
    SpawnEnemy(scene, {4.8f, 0.0f, 8.0f});
    SpawnEnemy(scene, {-3.5f, 0.0f, -7.2f});
    SpawnHealthPickup(scene, {-1.6f, 0.0f, 2.2f});
    SpawnHealthPickup(scene, {3.4f, 0.0f, -1.8f});
    SpawnHealthPickup(scene, {0.0f, 0.0f, -4.5f});
    EnsureFollowCamera(scene);
    scene.Settings().EnableWaves = true;
    Entity camera = scene.FindPrimaryCamera();
    if (camera.IsValid()) {
        scene.GetTransform(camera).Position = {0.0f, 4.4f, 7.2f};
        scene.GetCamera(camera).LookAtTarget = {0.0f, 1.25f, 0.0f};
    }
    return scene;
}

} // namespace Nova
