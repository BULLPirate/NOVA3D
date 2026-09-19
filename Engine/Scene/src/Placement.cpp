#include <Nova/Scene/Placement.h>
#include <Nova/Scene/Gameplay.h>

#include <cmath>

namespace Nova {

namespace {

float Snap(float value, float grid) {
    if (grid <= 1e-6f) {
        return value;
    }
    return std::round(value / grid) * grid;
}

Entity MakeSolid(Scene& scene, const std::string& base, MeshPrimitive primitive, const Vec3& pos,
                 const Vec3& scale, const Vec3& color) {
    Entity entity = scene.CreateEntity(scene.MakeUniqueName(base));
    MeshRendererComponent mesh;
    mesh.Primitive = primitive;
    mesh.UseAlbedoTexture = false;
    mesh.AlbedoColor = color;
    mesh.Opacity = 1.0f;
    scene.AddMeshRenderer(entity, mesh);
    scene.AddCollider(entity);
    scene.GetTransform(entity).Position = pos;
    scene.GetTransform(entity).Scale = scale;
    return entity;
}

Entity MakeChild(Scene& scene, Entity parent, const std::string& base, MeshPrimitive primitive,
                 const Vec3& localPos, const Vec3& scale, const Vec3& color) {
    Entity child = MakeSolid(scene, base, primitive, localPos, scale, color);
    scene.SetParent(child, parent);
    return child;
}

void PaintAlbedo(Scene& scene, Entity entity, const char* texturePath) {
    if (!entity.IsValid() || !scene.HasMeshRenderer(entity)) {
        return;
    }
    MeshRendererComponent& mesh = scene.GetMeshRenderer(entity);
    mesh.AlbedoTexturePath = texturePath;
    mesh.UseAlbedoTexture = true;
}

void YawExtents(float yaw, float& halfX, float& halfZ) {
    const float c = std::abs(std::cos(yaw));
    const float s = std::abs(std::sin(yaw));
    const float x = halfX * c + halfZ * s;
    const float z = halfX * s + halfZ * c;
    halfX = x;
    halfZ = z;
}

} // namespace

const char* PlaceableKindLabel(PlaceableKind kind) {
    switch (kind) {
    case PlaceableKind::Cube:
        return "Cube";
    case PlaceableKind::Sphere:
        return "Sphere";
    case PlaceableKind::Plane:
        return "Ground";
    case PlaceableKind::House:
        return "House";
    case PlaceableKind::Tree:
        return "Tree";
    case PlaceableKind::Crate:
        return "Crate";
    case PlaceableKind::Wall:
        return "Wall";
    case PlaceableKind::Road:
        return "Road";
    case PlaceableKind::Fence:
        return "Fence";
    case PlaceableKind::Well:
        return "Well";
    case PlaceableKind::Knight:
        return "Player";
    case PlaceableKind::Bandit:
        return "NPC";
    case PlaceableKind::Herb:
        return "Herb";
    }
    return "Cube";
}

void PlantOnGround(Scene& scene, Entity entity, const Vec3& xz, float grid) {
    if (!entity.IsValid() || !scene.IsAlive(entity)) {
        return;
    }
    Transform& xform = scene.GetTransform(entity);
    xform.Position.x = Snap(xz.x, grid);
    xform.Position.z = Snap(xz.z, grid);
    if (scene.HasCharacterController(entity) || scene.HasPickup(entity)) {
        PlaceOnGround(scene, entity);
    }
    if (scene.HasPickup(entity)) {
        xform.Position.y += 0.35f;
    }
}

float PlaceableSpacing(PlaceableKind kind) {
    switch (kind) {
    case PlaceableKind::House:
        return 3.6f;
    case PlaceableKind::Tree:
        return 2.2f;
    case PlaceableKind::Wall:
        return 3.8f;
    case PlaceableKind::Road:
        return 3.6f;
    case PlaceableKind::Fence:
        return 2.1f;
    case PlaceableKind::Well:
        return 1.6f;
    case PlaceableKind::Plane:
        return 10.0f;
    case PlaceableKind::Knight:
    case PlaceableKind::Bandit:
        return 2.4f;
    case PlaceableKind::Herb:
        return 1.3f;
    case PlaceableKind::Crate:
        return 1.0f;
    case PlaceableKind::Cube:
    case PlaceableKind::Sphere:
    default:
        return 1.0f;
    }
}

bool PlaceableAllowsStroke(PlaceableKind kind) {
    return kind != PlaceableKind::Knight && kind != PlaceableKind::Bandit &&
           kind != PlaceableKind::Plane;
}

bool ShouldStampAlongStroke(const Vec3& lastStamp, const Vec3& next, float spacing) {
    const float dx = next.x - lastStamp.x;
    const float dz = next.z - lastStamp.z;
    return dx * dx + dz * dz >= spacing * spacing;
}

PlaceablePreview QueryPlaceablePreview(PlaceableKind kind, const Vec3& xz, float yawRadians,
                                       float grid) {
    PlaceablePreview preview;
    preview.Center = {Snap(xz.x, grid), 0.5f, Snap(xz.z, grid)};
    preview.HalfExtents = {0.5f, 0.5f, 0.5f};
    preview.Color = {0.92f, 0.82f, 0.28f};
    switch (kind) {
    case PlaceableKind::House:
        preview.Center.y = 1.25f;
        preview.HalfExtents = {1.75f, 1.35f, 1.65f};
        preview.Color = {0.72f, 0.48f, 0.30f};
        break;
    case PlaceableKind::Tree:
        preview.Center.y = 1.4f;
        preview.HalfExtents = {0.85f, 1.5f, 0.85f};
        preview.Color = {0.22f, 0.55f, 0.22f};
        break;
    case PlaceableKind::Crate:
        preview.Center.y = 0.4f;
        preview.HalfExtents = {0.4f, 0.4f, 0.4f};
        preview.Color = {0.48f, 0.32f, 0.18f};
        break;
    case PlaceableKind::Wall:
        preview.Center.y = 1.3f;
        preview.HalfExtents = {2.0f, 1.3f, 0.25f};
        preview.Color = {0.62f, 0.60f, 0.54f};
        break;
    case PlaceableKind::Road:
        preview.Center.y = 0.03f;
        preview.HalfExtents = {1.2f, 0.03f, 2.0f};
        preview.Color = {0.50f, 0.40f, 0.26f};
        break;
    case PlaceableKind::Fence:
        preview.Center.y = 0.45f;
        preview.HalfExtents = {1.1f, 0.45f, 0.10f};
        preview.Color = {0.42f, 0.30f, 0.18f};
        break;
    case PlaceableKind::Well:
        preview.Center.y = 0.45f;
        preview.HalfExtents = {0.55f, 0.45f, 0.55f};
        preview.Color = {0.52f, 0.52f, 0.56f};
        break;
    case PlaceableKind::Plane:
        preview.Center.y = 0.0f;
        preview.HalfExtents = {6.0f, 0.02f, 6.0f};
        preview.Color = {0.34f, 0.52f, 0.24f};
        break;
    case PlaceableKind::Sphere:
        preview.Center.y = 0.5f;
        preview.HalfExtents = {0.5f, 0.5f, 0.5f};
        preview.Color = {0.55f, 0.72f, 0.86f};
        break;
    case PlaceableKind::Knight:
    case PlaceableKind::Bandit:
        preview.Center.y = 0.9f;
        preview.HalfExtents = {0.4f, 0.95f, 0.4f};
        preview.Color = kind == PlaceableKind::Knight ? Vec3{0.55f, 0.62f, 0.80f}
                                                     : Vec3{0.62f, 0.28f, 0.22f};
        break;
    case PlaceableKind::Herb:
        preview.Center.y = 0.45f;
        preview.HalfExtents = {0.25f, 0.25f, 0.25f};
        preview.Color = {0.28f, 0.72f, 0.32f};
        break;
    case PlaceableKind::Cube:
    default:
        break;
    }
    YawExtents(yawRadians, preview.HalfExtents.x, preview.HalfExtents.z);
    return preview;
}

Entity SpawnPlaceable(Scene& scene, PlaceableKind kind, const Vec3& worldPosition,
                      float yawRadians) {
    if (kind != PlaceableKind::Plane && !scene.FindEntityByName("Ground").IsValid()) {
        SpawnGround(scene);
    }
    Entity spawned{};
    switch (kind) {
    case PlaceableKind::Plane: {
        spawned = scene.CreateEntity(scene.MakeUniqueName("Ground"));
        MeshRendererComponent mesh;
        mesh.Primitive = MeshPrimitive::UnitPlane;
        mesh.UseAlbedoTexture = false;
        mesh.AlbedoColor = {0.34f, 0.50f, 0.24f};
        scene.AddMeshRenderer(spawned, mesh);
        scene.GetTransform(spawned).Scale = {12.0f, 1.0f, 12.0f};
        PaintAlbedo(scene, spawned, "Assets/Textures/ground.png");
        break;
    }
    case PlaceableKind::House: {
        spawned = MakeSolid(scene, "House", MeshPrimitive::UnitCube,
                            {worldPosition.x, 1.1f, worldPosition.z}, {3.2f, 2.2f, 3.0f},
                            {0.62f, 0.42f, 0.28f});
        PaintAlbedo(scene, spawned, "Assets/Textures/stone.png");
        Entity roof = MakeChild(scene, spawned, "Roof", MeshPrimitive::UnitCube, {0.0f, 1.38f, 0.0f},
                                {3.5f, 0.35f, 3.3f}, {0.42f, 0.16f, 0.14f});
        PaintAlbedo(scene, roof, "Assets/Textures/stone.png");
        break;
    }
    case PlaceableKind::Tree: {
        spawned = scene.CreateEntity(scene.MakeUniqueName("Tree"));
        SetEntityMeshAsset(scene, spawned, "Assets/Characters/tree.obj", {});
        scene.AddCollider(spawned);
        scene.GetTransform(spawned).Position = {worldPosition.x, 0.0f, worldPosition.z};
        break;
    }
    case PlaceableKind::Crate:
        spawned = scene.CreateEntity(scene.MakeUniqueName("Crate"));
        SetEntityMeshAsset(scene, spawned, "Assets/Characters/crate.obj",
                           "Assets/Textures/stone.png");
        scene.AddCollider(spawned);
        scene.GetTransform(spawned).Position = {worldPosition.x, 0.0f, worldPosition.z};
        break;
    case PlaceableKind::Wall:
        spawned = MakeSolid(scene, "Wall", MeshPrimitive::UnitCube,
                            {worldPosition.x, 1.3f, worldPosition.z}, {4.0f, 2.6f, 0.45f},
                            {0.52f, 0.50f, 0.46f});
        PaintAlbedo(scene, spawned, "Assets/Textures/stone.png");
        break;
    case PlaceableKind::Road:
        spawned = MakeSolid(scene, "Road", MeshPrimitive::UnitPlane,
                            {worldPosition.x, 0.02f, worldPosition.z}, {2.4f, 1.0f, 4.0f},
                            {0.45f, 0.36f, 0.24f});
        scene.RemoveCollider(spawned);
        PaintAlbedo(scene, spawned, "Assets/Textures/ground.png");
        break;
    case PlaceableKind::Fence:
        spawned = MakeSolid(scene, "Fence", MeshPrimitive::UnitCube,
                            {worldPosition.x, 0.45f, worldPosition.z}, {2.2f, 0.9f, 0.16f},
                            {0.40f, 0.28f, 0.16f});
        PaintAlbedo(scene, spawned, "Assets/Textures/stone.png");
        break;
    case PlaceableKind::Well:
        spawned = MakeSolid(scene, "Well", MeshPrimitive::UnitCube,
                            {worldPosition.x, 0.45f, worldPosition.z}, {1.1f, 0.9f, 1.1f},
                            {0.48f, 0.48f, 0.50f});
        PaintAlbedo(scene, spawned, "Assets/Textures/stone.png");
        break;
    case PlaceableKind::Knight:
        spawned = SpawnPlayer(scene);
        break;
    case PlaceableKind::Bandit:
        spawned = SpawnEnemy(scene, worldPosition);
        break;
    case PlaceableKind::Herb:
        spawned = SpawnHealthPickup(scene, worldPosition);
        break;
    case PlaceableKind::Sphere:
        spawned = MakeSolid(scene, "Sphere", MeshPrimitive::UnitSphere,
                            {worldPosition.x, 0.5f, worldPosition.z}, {1.0f, 1.0f, 1.0f},
                            {0.62f, 0.74f, 0.86f});
        break;
    case PlaceableKind::Cube:
    default:
        spawned = MakeSolid(scene, "Cube", MeshPrimitive::UnitCube,
                            {worldPosition.x, 0.5f, worldPosition.z}, {1.0f, 1.0f, 1.0f},
                            {0.78f, 0.74f, 0.66f});
        break;
    }
    PlantOnGround(scene, spawned, worldPosition);
    if (spawned.IsValid() && scene.IsAlive(spawned) && std::abs(yawRadians) > 1e-5f) {
        scene.GetTransform(spawned).Rotation = Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, yawRadians);
        if (scene.HasCharacterController(spawned)) {
            scene.GetCharacterController(spawned).FacingYaw = yawRadians;
        }
    }
    return spawned;
}

} // namespace Nova
