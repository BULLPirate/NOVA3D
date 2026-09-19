#include <Nova/Scene/Scene.h>

#include <stdexcept>

namespace Nova {

namespace {

constexpr uint32_t kIndexMask = 0x00FFFFFFu;
constexpr uint32_t kGenerationShift = 24u;

uint32_t PackEntityId(uint32_t index, uint32_t generation) {
    return (index & kIndexMask) | ((generation & 0xFFu) << kGenerationShift);
}

uint32_t EntityIndex(Entity entity) {
    return entity.Id & kIndexMask;
}

uint32_t EntityGeneration(Entity entity) {
    return entity.Id >> kGenerationShift;
}

} // namespace

std::size_t Scene::EntityCount() const {
    std::size_t count = 0;
    for (const EntityRecord& rec : m_Entities) {
        if (rec.Alive) ++count;
    }
    return count;
}

Entity Scene::CreateEntity(const std::string& name) {
    uint32_t index = UINT32_MAX;
    for (uint32_t i = 0; i < m_Entities.size(); ++i) {
        if (!m_Entities[i].Alive) {
            index = i;
            break;
        }
    }
    if (index == UINT32_MAX) {
        index = static_cast<uint32_t>(m_Entities.size());
        m_Entities.emplace_back();
    }

    EntityRecord& rec = m_Entities[index];
    rec.Alive = true;
    rec.Name = name;
    rec.ShowAxes = false;
    rec.LocalTransform = Transform{};
    rec.Mesh.reset();
    rec.Camera.reset();
    rec.Light.reset();
    rec.Rotator.reset();
    rec.Mover.reset();
    rec.PointLight.reset();
    rec.Character.reset();
    rec.Player.reset();
    rec.FollowCamera.reset();
    rec.Script.reset();
    rec.AudioSource.reset();
    rec.Parent = Entity{};

    return Entity{PackEntityId(index, rec.Generation)};
}

Entity Scene::DuplicateEntity(Entity source) {
    const EntityRecord* src = GetRecord(source);
    if (!src || !src->Alive) {
        return Entity{};
    }

    const std::string newName = MakeUniqueName(src->Name);
    const Transform xform = src->LocalTransform;
    const std::optional<MeshRendererComponent> mesh = src->Mesh;
    const std::optional<CameraComponent> camera = src->Camera;
    const std::optional<DirectionalLightComponent> light = src->Light;
    const std::optional<RotatorComponent> rotator = src->Rotator;
    const std::optional<MoverComponent> mover = src->Mover;
    const std::optional<PointLightComponent> point = src->PointLight;
    const std::optional<CharacterControllerComponent> character = src->Character;
    const std::optional<PlayerControllerComponent> player = src->Player;
    const std::optional<FollowCameraComponent> follow = src->FollowCamera;
    const std::optional<ScriptComponent> script = src->Script;
    const std::optional<AudioSourceComponent> audio = src->AudioSource;
    const std::optional<ColliderComponent> collider = src->Collider;
    const std::optional<PickupComponent> pickup = src->Pickup;

    Entity copy = CreateEntity(newName);
    GetTransform(copy) = xform;
    if (mesh) {
        AddMeshRenderer(copy, *mesh);
    }
    if (camera) {
        CameraComponent cam = *camera;
        cam.IsPrimary = false;
        AddCamera(copy, cam);
    }
    if (light) {
        AddDirectionalLight(copy, *light);
    }
    if (rotator) {
        AddRotator(copy, *rotator);
    }
    if (mover) {
        AddMover(copy, *mover);
    }
    if (point) {
        AddPointLight(copy, *point);
    }
    if (character) {
        AddCharacterController(copy, *character);
    }
    if (player) {
        AddPlayerController(copy, *player);
    }
    if (follow) {
        AddFollowCamera(copy, *follow);
    }
    if (script) {
        ScriptComponent copied = *script;
        copied.RanStart = false;
        AddScript(copy, copied);
    }
    if (audio) {
        AudioSourceComponent copied = *audio;
        copied.Started = false;
        AddAudioSource(copy, copied);
    }
    if (collider) {
        AddCollider(copy, *collider);
    }
    if (pickup) {
        PickupComponent copied = *pickup;
        copied.Taken = false;
        AddPickup(copy, copied);
    }
    SetParent(copy, src->Parent);
    SetShowAxes(copy, src->ShowAxes);

    std::vector<Entity> children;
    ForEachEntity([&](Entity entity) {
        if (entity.Id != source.Id && GetParent(entity).Id == source.Id) {
            children.push_back(entity);
        }
    });
    for (Entity child : children) {
        const Entity childCopy = DuplicateEntity(child);
        if (childCopy.IsValid()) {
            SetParent(childCopy, copy);
        }
    }
    return copy;
}

void Scene::DestroyEntity(Entity entity) {
    EntityRecord* rec = GetRecord(entity);
    if (!rec) return;
    std::vector<Entity> children;
    ForEachEntity([&](Entity e) {
        if (e.Id != entity.Id && GetParent(e).Id == entity.Id) {
            children.push_back(e);
        }
    });
    for (Entity child : children) {
        DestroyEntity(child);
    }
    rec = GetRecord(entity);
    if (!rec) {
        return;
    }
    rec->Alive = false;
    rec->Mesh.reset();
    rec->Camera.reset();
    rec->Light.reset();
    rec->Rotator.reset();
    rec->Mover.reset();
    rec->PointLight.reset();
    rec->Character.reset();
    rec->Player.reset();
    rec->FollowCamera.reset();
    rec->Script.reset();
    rec->AudioSource.reset();
    rec->Collider.reset();
    rec->Pickup.reset();
    if (rec->Generation < 255) {
        ++rec->Generation;
    }
}

bool Scene::IsAlive(Entity entity) const {
    return GetRecord(entity) != nullptr;
}

const std::string& Scene::GetName(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    if (!rec) {
        static const std::string kEmpty;
        return kEmpty;
    }
    return rec->Name;
}

void Scene::SetName(Entity entity, const std::string& name) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Name = name;
    }
}

bool Scene::GetShowAxes(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    return rec && rec->ShowAxes;
}

void Scene::SetShowAxes(Entity entity, bool show) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->ShowAxes = show;
    }
}

Transform& Scene::GetTransform(Entity entity) {
    EntityRecord* rec = GetRecord(entity);
    if (!rec) {
        throw std::out_of_range("Scene::GetTransform invalid entity");
    }
    return rec->LocalTransform;
}

const Transform& Scene::GetTransform(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    if (!rec) {
        throw std::out_of_range("Scene::GetTransform invalid entity");
    }
    return rec->LocalTransform;
}

bool Scene::HasMeshRenderer(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    return rec && rec->Mesh.has_value();
}

MeshRendererComponent& Scene::GetMeshRenderer(Entity entity) {
    EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Mesh) {
        throw std::out_of_range("Scene::GetMeshRenderer missing component");
    }
    return *rec->Mesh;
}

const MeshRendererComponent& Scene::GetMeshRenderer(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Mesh) {
        throw std::out_of_range("Scene::GetMeshRenderer missing component");
    }
    return *rec->Mesh;
}

void Scene::AddMeshRenderer(Entity entity, MeshRendererComponent mesh) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Mesh = mesh;
    }
}

void Scene::RemoveMeshRenderer(Entity entity) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Mesh.reset();
    }
}

bool Scene::HasCamera(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    return rec && rec->Camera.has_value();
}

CameraComponent& Scene::GetCamera(Entity entity) {
    EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Camera) {
        throw std::out_of_range("Scene::GetCamera missing component");
    }
    return *rec->Camera;
}

const CameraComponent& Scene::GetCamera(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Camera) {
        throw std::out_of_range("Scene::GetCamera missing component");
    }
    return *rec->Camera;
}

void Scene::AddCamera(Entity entity, CameraComponent camera) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Camera = camera;
    }
}

void Scene::RemoveCamera(Entity entity) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Camera.reset();
    }
}

bool Scene::HasDirectionalLight(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    return rec && rec->Light.has_value();
}

DirectionalLightComponent& Scene::GetDirectionalLight(Entity entity) {
    EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Light) {
        throw std::out_of_range("Scene::GetDirectionalLight missing component");
    }
    return *rec->Light;
}

const DirectionalLightComponent& Scene::GetDirectionalLight(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Light) {
        throw std::out_of_range("Scene::GetDirectionalLight missing component");
    }
    return *rec->Light;
}

void Scene::AddDirectionalLight(Entity entity, DirectionalLightComponent light) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Light = light;
    }
}

void Scene::RemoveDirectionalLight(Entity entity) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Light.reset();
    }
}

bool Scene::HasRotator(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    return rec && rec->Rotator.has_value();
}

RotatorComponent& Scene::GetRotator(Entity entity) {
    EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Rotator) {
        throw std::out_of_range("Scene::GetRotator missing component");
    }
    return *rec->Rotator;
}

const RotatorComponent& Scene::GetRotator(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Rotator) {
        throw std::out_of_range("Scene::GetRotator missing component");
    }
    return *rec->Rotator;
}

void Scene::AddRotator(Entity entity, RotatorComponent rotator) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Rotator = rotator;
    }
}

void Scene::RemoveRotator(Entity entity) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Rotator.reset();
    }
}

bool Scene::HasMover(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    return rec && rec->Mover.has_value();
}

MoverComponent& Scene::GetMover(Entity entity) {
    EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Mover) {
        throw std::out_of_range("Scene::GetMover missing component");
    }
    return *rec->Mover;
}

const MoverComponent& Scene::GetMover(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Mover) {
        throw std::out_of_range("Scene::GetMover missing component");
    }
    return *rec->Mover;
}

void Scene::AddMover(Entity entity, MoverComponent mover) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Mover = mover;
    }
}

void Scene::RemoveMover(Entity entity) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Mover.reset();
    }
}

bool Scene::HasPointLight(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    return rec && rec->PointLight.has_value();
}

PointLightComponent& Scene::GetPointLight(Entity entity) {
    EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->PointLight) {
        throw std::out_of_range("Scene::GetPointLight missing component");
    }
    return *rec->PointLight;
}

const PointLightComponent& Scene::GetPointLight(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->PointLight) {
        throw std::out_of_range("Scene::GetPointLight missing component");
    }
    return *rec->PointLight;
}

void Scene::AddPointLight(Entity entity, PointLightComponent light) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->PointLight = light;
    }
}

void Scene::RemovePointLight(Entity entity) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->PointLight.reset();
    }
}

bool Scene::HasCharacterController(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    return rec && rec->Character.has_value();
}

CharacterControllerComponent& Scene::GetCharacterController(Entity entity) {
    EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Character) {
        throw std::out_of_range("Scene::GetCharacterController missing component");
    }
    return *rec->Character;
}

const CharacterControllerComponent& Scene::GetCharacterController(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Character) {
        throw std::out_of_range("Scene::GetCharacterController missing component");
    }
    return *rec->Character;
}

void Scene::AddCharacterController(Entity entity, CharacterControllerComponent character) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Character = character;
    }
}

void Scene::RemoveCharacterController(Entity entity) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Character.reset();
    }
}

bool Scene::HasPlayerController(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    return rec && rec->Player.has_value();
}

PlayerControllerComponent& Scene::GetPlayerController(Entity entity) {
    EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Player) {
        throw std::out_of_range("Scene::GetPlayerController missing component");
    }
    return *rec->Player;
}

const PlayerControllerComponent& Scene::GetPlayerController(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Player) {
        throw std::out_of_range("Scene::GetPlayerController missing component");
    }
    return *rec->Player;
}

void Scene::AddPlayerController(Entity entity, PlayerControllerComponent player) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Player = player;
    }
}

void Scene::RemovePlayerController(Entity entity) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Player.reset();
    }
}

bool Scene::HasFollowCamera(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    return rec && rec->FollowCamera.has_value();
}

FollowCameraComponent& Scene::GetFollowCamera(Entity entity) {
    EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->FollowCamera) {
        throw std::out_of_range("Scene::GetFollowCamera missing component");
    }
    return *rec->FollowCamera;
}

const FollowCameraComponent& Scene::GetFollowCamera(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->FollowCamera) {
        throw std::out_of_range("Scene::GetFollowCamera missing component");
    }
    return *rec->FollowCamera;
}

void Scene::AddFollowCamera(Entity entity, FollowCameraComponent follow) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->FollowCamera = follow;
    }
}

void Scene::RemoveFollowCamera(Entity entity) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->FollowCamera.reset();
    }
}

bool Scene::HasScript(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    return rec && rec->Script.has_value();
}

ScriptComponent& Scene::GetScript(Entity entity) {
    EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Script) {
        throw std::out_of_range("Scene::GetScript missing component");
    }
    return *rec->Script;
}

const ScriptComponent& Scene::GetScript(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Script) {
        throw std::out_of_range("Scene::GetScript missing component");
    }
    return *rec->Script;
}

void Scene::AddScript(Entity entity, ScriptComponent script) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Script = script;
    }
}

void Scene::RemoveScript(Entity entity) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Script.reset();
    }
}

bool Scene::HasAudioSource(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    return rec && rec->AudioSource.has_value();
}

AudioSourceComponent& Scene::GetAudioSource(Entity entity) {
    EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->AudioSource) {
        throw std::out_of_range("Scene::GetAudioSource missing component");
    }
    return *rec->AudioSource;
}

const AudioSourceComponent& Scene::GetAudioSource(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->AudioSource) {
        throw std::out_of_range("Scene::GetAudioSource missing component");
    }
    return *rec->AudioSource;
}

void Scene::AddAudioSource(Entity entity, AudioSourceComponent source) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->AudioSource = source;
    }
}

void Scene::RemoveAudioSource(Entity entity) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->AudioSource.reset();
    }
}

bool Scene::HasCollider(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    return rec && rec->Collider.has_value();
}

ColliderComponent& Scene::GetCollider(Entity entity) {
    EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Collider) {
        throw std::out_of_range("Scene::GetCollider missing component");
    }
    return *rec->Collider;
}

const ColliderComponent& Scene::GetCollider(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Collider) {
        throw std::out_of_range("Scene::GetCollider missing component");
    }
    return *rec->Collider;
}

void Scene::AddCollider(Entity entity, ColliderComponent collider) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Collider = collider;
    }
}

void Scene::RemoveCollider(Entity entity) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Collider.reset();
    }
}

bool Scene::HasPickup(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    return rec && rec->Pickup.has_value();
}

PickupComponent& Scene::GetPickup(Entity entity) {
    EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Pickup) {
        throw std::out_of_range("Scene::GetPickup missing component");
    }
    return *rec->Pickup;
}

const PickupComponent& Scene::GetPickup(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Pickup) {
        throw std::out_of_range("Scene::GetPickup missing component");
    }
    return *rec->Pickup;
}

void Scene::AddPickup(Entity entity, PickupComponent pickup) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Pickup = pickup;
    }
}

void Scene::RemovePickup(Entity entity) {
    if (EntityRecord* rec = GetRecord(entity)) {
        rec->Pickup.reset();
    }
}

SceneSettings& Scene::Settings() {
    return m_Settings;
}

const SceneSettings& Scene::Settings() const {
    return m_Settings;
}

Entity Scene::GetParent(Entity entity) const {
    const EntityRecord* rec = GetRecord(entity);
    if (!rec || !rec->Parent.IsValid()) {
        return Entity{};
    }
    if (!IsAlive(rec->Parent)) {
        return Entity{};
    }
    return rec->Parent;
}

void Scene::SetParent(Entity child, Entity parent) {
    EntityRecord* childRec = GetRecord(child);
    if (!childRec) {
        return;
    }
    if (!parent.IsValid()) {
        childRec->Parent = Entity{};
        return;
    }
    if (!IsAlive(parent) || child.Id == parent.Id) {
        return;
    }
    Entity walk = parent;
    while (walk.IsValid()) {
        if (walk.Id == child.Id) {
            return;
        }
        walk = GetParent(walk);
    }
    childRec->Parent = parent;
}

Mat4 Scene::GetWorldMatrix(Entity entity) const {
    const Transform& local = GetTransform(entity);
    const Mat4 localMatrix = local.ToMatrix();
    const Entity parent = GetParent(entity);
    if (!parent.IsValid()) {
        return localMatrix;
    }
    return GetWorldMatrix(parent) * localMatrix;
}

Entity Scene::FindEntityByName(const std::string& name) const {
    Entity found{};
    ForEachEntity([&](Entity e) {
        if (found.IsValid()) {
            return;
        }
        if (GetName(e) == name) {
            found = e;
        }
    });
    return found;
}

std::string Scene::MakeUniqueName(const std::string& base) const {
    if (!FindEntityByName(base).IsValid()) {
        return base;
    }
    for (int i = 2; i < 10000; ++i) {
        const std::string candidate = base + " " + std::to_string(i);
        if (!FindEntityByName(candidate).IsValid()) {
            return candidate;
        }
    }
    return base + " Copy";
}

void Scene::SetPrimaryCamera(Entity entity) {
    if (!HasCamera(entity)) {
        return;
    }
    ForEachEntity([&](Entity e) {
        if (HasCamera(e)) {
            GetCamera(e).IsPrimary = (e.Id == entity.Id);
        }
    });
}

Entity Scene::FindPrimaryCamera() const {
    Entity found{Entity::kInvalidEntity};
    for (uint32_t i = 0; i < m_Entities.size(); ++i) {
        const EntityRecord& rec = m_Entities[i];
        if (!rec.Alive || !rec.Camera) continue;
        Entity e{PackEntityId(i, rec.Generation)};
        if (rec.Camera->IsPrimary) {
            return e;
        }
        if (!found.IsValid()) {
            found = e;
        }
    }
    return found;
}

void Scene::ForEachEntity(const std::function<void(Entity)>& fn) const {
    for (uint32_t i = 0; i < m_Entities.size(); ++i) {
        const EntityRecord& rec = m_Entities[i];
        if (!rec.Alive) continue;
        fn(Entity{PackEntityId(i, rec.Generation)});
    }
}

void Scene::Clear() {
    m_Entities.clear();
    m_Settings = {};
}

Scene Scene::CreateDemoLevel() {
    return CreateEmptyLevel();
}

Scene Scene::CreateEmptyLevel() {
    Scene scene;

    Entity sun = scene.CreateEntity("Sun");
    DirectionalLightComponent light;
    light.Direction = Vec3{0.45f, -0.88f, 0.15f}.Normalized();
    light.Ambient = 0.40f;
    scene.AddDirectionalLight(sun, light);

    Entity cam = scene.CreateEntity("Main Camera");
    CameraComponent camera;
    camera.IsPrimary = true;
    camera.LookAtTarget = {0.0f, 0.8f, 0.0f};
    scene.AddCamera(cam, camera);
    scene.GetTransform(cam).Position = {7.5f, 5.4f, 11.0f};

    return scene;
}

Scene::EntityRecord* Scene::GetRecord(Entity entity) {
    if (!entity.IsValid()) return nullptr;
    const uint32_t index = EntityIndex(entity);
    if (index >= m_Entities.size()) return nullptr;
    EntityRecord& rec = m_Entities[index];
    if (!rec.Alive || EntityGeneration(entity) != rec.Generation) {
        return nullptr;
    }
    return &rec;
}

const Scene::EntityRecord* Scene::GetRecord(Entity entity) const {
    return const_cast<Scene*>(this)->GetRecord(entity);
}

} // namespace Nova
