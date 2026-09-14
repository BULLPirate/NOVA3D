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
    rec.LocalTransform = Transform{};
    rec.Mesh.reset();
    rec.Camera.reset();
    rec.Light.reset();

    return Entity{PackEntityId(index, rec.Generation)};
}

Entity Scene::DuplicateEntity(Entity source) {
    const EntityRecord* src = GetRecord(source);
    if (!src || !src->Alive) {
        return Entity{};
    }

    const std::string newName = src->Name + " Copy";
    const Transform xform = src->LocalTransform;
    const std::optional<MeshRendererComponent> mesh = src->Mesh;
    const std::optional<CameraComponent> camera = src->Camera;
    const std::optional<DirectionalLightComponent> light = src->Light;

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
    return copy;
}

void Scene::DestroyEntity(Entity entity) {
    EntityRecord* rec = GetRecord(entity);
    if (!rec) return;
    rec->Alive = false;
    rec->Mesh.reset();
    rec->Camera.reset();
    rec->Light.reset();
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
}

Scene Scene::CreateDemoLevel() {
    Scene scene;

    Entity sun = scene.CreateEntity("Sun");
    DirectionalLightComponent light;
    light.Direction = Vec3{0.45f, -0.88f, 0.15f}.Normalized();
    scene.AddDirectionalLight(sun, light);

    Entity cam = scene.CreateEntity("Main Camera");
    CameraComponent camera;
    camera.IsPrimary = true;
    camera.LookAtTarget = {0.0f, 0.0f, 0.0f};
    scene.AddCamera(cam, camera);
    scene.GetTransform(cam).Position = {0.0f, 0.35f, 3.2f};

    Entity cube = scene.CreateEntity("Cube");
    scene.AddMeshRenderer(cube, {});
    scene.GetTransform(cube).Position = {0.0f, 0.0f, 0.0f};

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
