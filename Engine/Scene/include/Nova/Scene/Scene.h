#pragma once

#include <Nova/Scene/Entity.h>
#include <Nova/Scene/Components.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace Nova {

/// In-memory scene graph (ECS-style storage, no hierarchy yet).
class Scene {
public:
    Entity CreateEntity(const std::string& name = "Entity");
    void DestroyEntity(Entity entity);
    /// Copies transform and all attached components. Name is set by caller after create.
    Entity DuplicateEntity(Entity source);
    bool IsAlive(Entity entity) const;

    const std::string& GetName(Entity entity) const;
    void SetName(Entity entity, const std::string& name);

    Transform& GetTransform(Entity entity);
    const Transform& GetTransform(Entity entity) const;

    bool HasMeshRenderer(Entity entity) const;
    MeshRendererComponent& GetMeshRenderer(Entity entity);
    const MeshRendererComponent& GetMeshRenderer(Entity entity) const;
    void AddMeshRenderer(Entity entity, MeshRendererComponent mesh = {});

    bool HasCamera(Entity entity) const;
    CameraComponent& GetCamera(Entity entity);
    const CameraComponent& GetCamera(Entity entity) const;
    void AddCamera(Entity entity, CameraComponent camera = {});

    bool HasDirectionalLight(Entity entity) const;
    DirectionalLightComponent& GetDirectionalLight(Entity entity);
    const DirectionalLightComponent& GetDirectionalLight(Entity entity) const;
    void AddDirectionalLight(Entity entity, DirectionalLightComponent light = {});

    bool HasRotator(Entity entity) const;
    RotatorComponent& GetRotator(Entity entity);
    const RotatorComponent& GetRotator(Entity entity) const;
    void AddRotator(Entity entity, RotatorComponent rotator = {});

    bool HasMover(Entity entity) const;
    MoverComponent& GetMover(Entity entity);
    const MoverComponent& GetMover(Entity entity) const;
    void AddMover(Entity entity, MoverComponent mover = {});

    Entity FindPrimaryCamera() const;
    /// Marks entity as the sole primary camera (must already have CameraComponent).
    void SetPrimaryCamera(Entity entity);

    void ForEachEntity(const std::function<void(Entity)>& fn) const;

    std::size_t EntityCount() const;

    /// Remove all entities (used before loading a scene file).
    void Clear();

    /// Default playable level: primary camera, sun, one cube.
    static Scene CreateDemoLevel();

private:
    struct EntityRecord {
        uint32_t Generation = 1;
        bool Alive = false;
        std::string Name;
        Transform LocalTransform;
        std::optional<MeshRendererComponent> Mesh;
        std::optional<CameraComponent> Camera;
        std::optional<DirectionalLightComponent> Light;
        std::optional<RotatorComponent> Rotator;
        std::optional<MoverComponent> Mover;
    };

    EntityRecord* GetRecord(Entity entity);
    const EntityRecord* GetRecord(Entity entity) const;

    std::vector<EntityRecord> m_Entities;
};

} // namespace Nova
