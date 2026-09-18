#include <Nova/Scene/Gameplay.h>
#include <Nova/Scene/Script.h>

#include <Nova/Math/Math.h>
#include <Nova/Math/Quat.h>
#include <Nova/Renderer/Camera.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace Nova {

namespace {

constexpr float kPlayerMaxHealth = 200.0f;
constexpr float kPlayerDamage = 50.0f;
constexpr float kEnemyMaxHealth = 32.0f;
constexpr float kEnemyDamage = 6.0f;

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
        bar = SpawnColored(scene, scene.GetName(owner) + " HP", MeshPrimitive::UnitCube, {0.0f, 1.95f, 0.0f},
                           {0.9f, 0.08f, 0.08f}, color);
        scene.SetParent(bar, owner);
    }
    if (!scene.HasMeshRenderer(bar)) {
        return;
    }
    scene.GetMeshRenderer(bar).AlbedoColor = color;
    scene.GetMeshRenderer(bar).UseAlbedoTexture = false;
    scene.GetMeshRenderer(bar).Opacity = character.Dead ? 0.0f : 1.0f;
    scene.GetTransform(bar).Position = {0.0f, 1.95f, 0.0f};
    scene.GetTransform(bar).Scale = {std::max(0.08f, 0.9f * ratio), 0.08f, 0.08f};
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
    if (scene.HasPlayerController(entity)) {
        scene.GetPlayerController(entity).Enabled = false;
    }
    scene.ForEachEntity([&](Entity child) {
        if (scene.GetParent(child).Id != entity.Id || !scene.HasMeshRenderer(child)) {
            return;
        }
        scene.GetMeshRenderer(child).Opacity = 0.35f;
        scene.GetMeshRenderer(child).AlbedoColor = {0.28f, 0.26f, 0.24f};
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

bool NameStartsWith(const std::string& name, const char* prefix) {
    return name.rfind(prefix, 0) == 0;
}

bool IsMapSolid(const Scene& scene, Entity entity) {
    if (!entity.IsValid() || !scene.HasMeshRenderer(entity) || scene.HasCharacterController(entity) ||
        scene.HasCamera(entity) || scene.GetParent(entity).IsValid()) {
        return false;
    }
    const MeshRendererComponent& mesh = scene.GetMeshRenderer(entity);
    if (mesh.Primitive == MeshPrimitive::UnitPlane) {
        return false;
    }
    const std::string& name = scene.GetName(entity);
    return NameStartsWith(name, "Wall") || NameStartsWith(name, "House") ||
           NameStartsWith(name, "Well") || NameStartsWith(name, "Crate");
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
        const float hx = box.Scale.x * 0.5f + radius;
        const float hz = box.Scale.z * 0.5f + radius;
        const float top = box.Position.y + box.Scale.y * 0.5f;
        if (xform.Position.y > top + 0.35f) {
            return;
        }
        const float dx = xform.Position.x - box.Position.x;
        const float dz = xform.Position.z - box.Position.z;
        if (std::abs(dx) >= hx || std::abs(dz) >= hz) {
            return;
        }
        const float px = hx - std::abs(dx);
        const float pz = hz - std::abs(dz);
        if (px < pz) {
            xform.Position.x += (dx >= 0.0f ? px : -px);
        } else {
            xform.Position.z += (dz >= 0.0f ? pz : -pz);
        }
    });
}

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
    follow.TargetName = "Player";
    follow.Distance = Clamp(follow.Distance, 5.4f, 9.0f);
    follow.PitchRadians = Clamp(follow.PitchRadians, 0.22f, 0.85f);
    if (follow.MouseSensitivity < 0.0005f || follow.MouseSensitivity > 0.02f) {
        follow.MouseSensitivity = 0.0032f;
    }
}

} // namespace

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
        AddPart(scene, root, prefix + "Sword", MeshPrimitive::UnitCube, {0.42f, 1.05f, 0.28f},
                {0.07f, 0.07f, 1.05f}, gold);
        AddPart(scene, root, prefix + "Shield", MeshPrimitive::UnitCube, {-0.42f, 0.95f, 0.02f},
                {0.06f, 0.48f, 0.36f}, steel);
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

Entity SpawnGround(Scene& scene) {
    Entity ground = SpawnColored(scene, "Ground", MeshPrimitive::UnitPlane, {0.0f, 0.0f, 0.0f},
                                 {30.0f, 1.0f, 30.0f}, {0.34f, 0.50f, 0.24f});
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
        SpawnColored(scene, name, MeshPrimitive::UnitCube, {pos.x, size.y * 0.5f, pos.z}, size,
                     {0.62f, 0.42f, 0.28f});
        SpawnColored(scene, name + " Roof", MeshPrimitive::UnitCube,
                     {pos.x, size.y + 0.28f, pos.z}, {size.x + 0.3f, 0.35f, size.z + 0.3f},
                     {0.42f, 0.16f, 0.14f});
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
    scene.Settings().ClearColor = {0.55f, 0.70f, 0.86f};
    return ground;
}

Entity SpawnPlayer(Scene& scene, const std::string& meshAssetPath) {
    Entity player = scene.CreateEntity("Player");
    if (!meshAssetPath.empty()) {
        MeshRendererComponent mesh;
        mesh.AssetPath = meshAssetPath;
        mesh.UseAlbedoTexture = false;
        mesh.AlbedoColor = {0.75f, 0.78f, 0.84f};
        mesh.Opacity = 1.0f;
        scene.AddMeshRenderer(player, mesh);
    } else {
        AttachHumanoidVisual(scene, player, true);
    }
    CharacterControllerComponent character;
    character.Height = 1.85f;
    character.Team = 0;
    character.Health = kPlayerMaxHealth;
    character.MaxHealth = kPlayerMaxHealth;
    character.AttackDamage = kPlayerDamage;
    character.AttackRange = 2.5f;
    character.FacingYaw = 0.0f;
    scene.AddCharacterController(player, character);
    scene.AddPlayerController(player);
    ScriptComponent script;
    script.AssetPath = "Assets/Scripts/player.ns";
    script.Source = DefaultPlayerScript();
    scene.AddScript(player, script);
    scene.GetTransform(player).Position = {0.0f, 0.0f, 0.0f};
    PlaceOnGround(scene, player);
    UpdateHealthBar(scene, player);
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
    if (!meshAssetPath.empty()) {
        MeshRendererComponent mesh;
        mesh.AssetPath = meshAssetPath;
        mesh.UseAlbedoTexture = false;
        mesh.AlbedoColor = {0.46f, 0.30f, 0.18f};
        scene.AddMeshRenderer(enemy, mesh);
    } else {
        AttachHumanoidVisual(scene, enemy, false);
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
    scene.AddCharacterController(enemy, character);
    scene.GetTransform(enemy).Position = position;
    PlaceOnGround(scene, enemy);
    UpdateHealthBar(scene, enemy);
    return enemy;
}

Entity SpawnImportedMesh(Scene& scene, const std::string& name, const std::string& meshAssetPath) {
    Entity entity = scene.CreateEntity(name);
    MeshRendererComponent mesh;
    mesh.AssetPath = meshAssetPath;
    mesh.UseAlbedoTexture = false;
    mesh.AlbedoColor = {0.78f, 0.74f, 0.66f};
    scene.AddMeshRenderer(entity, mesh);
    scene.GetTransform(entity).Position = {1.5f, 0.5f, 0.0f};
    PlaceOnGround(scene, entity);
    return entity;
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
    std::vector<Entity> victims;
    scene.ForEachEntity([&](Entity entity) {
        if (entity.Id == attacker.Id || !scene.HasCharacterController(entity)) {
            return;
        }
        const CharacterControllerComponent& other = scene.GetCharacterController(entity);
        if (other.Dead || other.Team == atk.Team) {
            return;
        }
        Vec3 delta = scene.GetTransform(entity).Position - origin;
        delta.y = 0.0f;
        if (delta.Length() <= atk.AttackRange) {
            victims.push_back(entity);
        }
    });
    for (Entity entity : victims) {
        CharacterControllerComponent& other = scene.GetCharacterController(entity);
        other.Health -= atk.AttackDamage;
        if (other.Health <= 0.0f) {
            MarkDead(scene, entity);
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
        AttachHumanoidVisual(scene, player, true);
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
}

GameplayHudSnapshot QueryGameplayHud(const Scene& scene) {
    GameplayHudSnapshot hud;
    const Entity player = scene.FindEntityByName("Player");
    if (player.IsValid() && scene.HasCharacterController(player)) {
        const CharacterControllerComponent& character = scene.GetCharacterController(player);
        hud.PlayerHealth = character.Health;
        hud.PlayerMaxHealth = character.MaxHealth;
        hud.PlayerAlive = !character.Dead;
    }
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
    return hud;
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
            // Mouse right looks right: camera orbits the opposite way around the hero.
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
        });
    }

    const Entity player = FindLocalPlayer(scene);
    if (player.IsValid() && input && scene.GetPlayerController(player).Enabled) {
        CharacterControllerComponent& character = scene.GetCharacterController(player);
        Transform& xform = scene.GetTransform(player);

        float camYaw = character.FacingYaw;
        scene.ForEachEntity([&](Entity entity) {
            if (scene.HasFollowCamera(entity)) {
                camYaw = scene.GetFollowCamera(entity).YawRadians;
            }
        });

        const float sinYaw = std::sin(camYaw);
        const float cosYaw = std::cos(camYaw);
        const Vec3 forward{-sinYaw, 0.0f, -cosYaw};
        const Vec3 right{cosYaw, 0.0f, -sinYaw};
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
            character.FacingYaw = camYaw;
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

        if (character.DodgeTimer > 0.12f) {
            xform.Position = xform.Position + character.DodgeVelocity * deltaSeconds;
        } else if (wish.LengthSq() > 1e-6f) {
            wish = wish.Normalized();
            xform.Position = xform.Position + wish * (speed * deltaSeconds);
            character.FacingYaw = std::atan2(wish.x, wish.z);
        }
        ResolveCharacterVsMap(scene, player, 0.38f);

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
    }

    if (player.IsValid()) {
        const Vec3 playerPos = scene.GetTransform(player).Position;
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
            if (dist > ai.AttackRange * 0.85f) {
                xform.Position = xform.Position + dir * (ai.MoveSpeed * deltaSeconds);
                ClampToArena(xform.Position);
                ResolveCharacterVsMap(scene, entity, 0.34f);
                PlaceOnGround(scene, entity);
            }
            ai.FacingYaw = std::atan2(dir.x, dir.z);
            xform.Rotation = Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, ai.FacingYaw);
            if (dist <= ai.AttackRange && ai.AiCooldown <= 0.0f) {
                ai.AiCooldown = 1.25f;
                ai.AttackTimer = 0.35f;
                ApplyMeleeHit(scene, entity);
            }
            UpdateHealthBar(scene, entity);
        }
    }

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
        Transform& camXform = scene.GetTransform(entity);
        camXform.Position =
            OrbitEyePosition(focus, follow.YawRadians, follow.PitchRadians, follow.Distance);
        scene.GetCamera(entity).LookAtTarget = focus;
    });
}

Scene Scene::CreatePlayableLevel() {
    Scene scene = CreateEmptyLevel();
    SpawnSimpleMap(scene);
    SpawnPlayer(scene);
    SpawnEnemy(scene, {6.5f, 0.0f, -3.5f});
    SpawnEnemy(scene, {-6.2f, 0.0f, 5.0f});
    SpawnEnemy(scene, {4.8f, 0.0f, 8.0f});
    SpawnEnemy(scene, {-3.5f, 0.0f, -7.2f});
    EnsureFollowCamera(scene);
    Entity camera = scene.FindPrimaryCamera();
    if (camera.IsValid()) {
        scene.GetTransform(camera).Position = {0.0f, 4.4f, 7.2f};
        scene.GetCamera(camera).LookAtTarget = {0.0f, 1.25f, 0.0f};
    }
    return scene;
}

} // namespace Nova
