#pragma once

#include <Nova/Scene/Entity.h>
#include <Nova/Scene/Components.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace Nova {

/// In-memory scene graph (ECS-style storage with optional parent links).
class Scene {
public:
    Entity CreateEntity(const std::string& name = "Entity");
    void DestroyEntity(Entity entity);
    /// Copies transform and all attached components. Name is set by caller after create.
    Entity DuplicateEntity(Entity source);
    bool IsAlive(Entity entity) const;

    const std::string& GetName(Entity entity) const;
    void SetName(Entity entity, const std::string& name);
    bool GetShowAxes(Entity entity) const;
    void SetShowAxes(Entity entity, bool show);

    Transform& GetTransform(Entity entity);
    const Transform& GetTransform(Entity entity) const;

    Entity GetParent(Entity entity) const;
    void SetParent(Entity child, Entity parent);
    Mat4 GetWorldMatrix(Entity entity) const;

    Entity FindEntityByName(const std::string& name) const;

    bool HasMeshRenderer(Entity entity) const;
    MeshRendererComponent& GetMeshRenderer(Entity entity);
    const MeshRendererComponent& GetMeshRenderer(Entity entity) const;
    void AddMeshRenderer(Entity entity, MeshRendererComponent mesh = {});
    void RemoveMeshRenderer(Entity entity);

    bool HasCamera(Entity entity) const;
    CameraComponent& GetCamera(Entity entity);
    const CameraComponent& GetCamera(Entity entity) const;
    void AddCamera(Entity entity, CameraComponent camera = {});
    void RemoveCamera(Entity entity);

    bool HasDirectionalLight(Entity entity) const;
    DirectionalLightComponent& GetDirectionalLight(Entity entity);
    const DirectionalLightComponent& GetDirectionalLight(Entity entity) const;
    void AddDirectionalLight(Entity entity, DirectionalLightComponent light = {});
    void RemoveDirectionalLight(Entity entity);

    bool HasRotator(Entity entity) const;
    RotatorComponent& GetRotator(Entity entity);
    const RotatorComponent& GetRotator(Entity entity) const;
    void AddRotator(Entity entity, RotatorComponent rotator = {});
    void RemoveRotator(Entity entity);

    bool HasMover(Entity entity) const;
    MoverComponent& GetMover(Entity entity);
    const MoverComponent& GetMover(Entity entity) const;
    void AddMover(Entity entity, MoverComponent mover = {});
    void RemoveMover(Entity entity);

    bool HasPointLight(Entity entity) const;
    PointLightComponent& GetPointLight(Entity entity);
    const PointLightComponent& GetPointLight(Entity entity) const;
    void AddPointLight(Entity entity, PointLightComponent light = {});
    void RemovePointLight(Entity entity);

    bool HasCharacterController(Entity entity) const;
    CharacterControllerComponent& GetCharacterController(Entity entity);
    const CharacterControllerComponent& GetCharacterController(Entity entity) const;
    void AddCharacterController(Entity entity, CharacterControllerComponent character = {});
    void RemoveCharacterController(Entity entity);

    bool HasPlayerController(Entity entity) const;
    PlayerControllerComponent& GetPlayerController(Entity entity);
    const PlayerControllerComponent& GetPlayerController(Entity entity) const;
    void AddPlayerController(Entity entity, PlayerControllerComponent player = {});
    void RemovePlayerController(Entity entity);

    bool HasFollowCamera(Entity entity) const;
    FollowCameraComponent& GetFollowCamera(Entity entity);
    const FollowCameraComponent& GetFollowCamera(Entity entity) const;
    void AddFollowCamera(Entity entity, FollowCameraComponent follow = {});
    void RemoveFollowCamera(Entity entity);

    bool HasScript(Entity entity) const;
    ScriptComponent& GetScript(Entity entity);
    const ScriptComponent& GetScript(Entity entity) const;
    void AddScript(Entity entity, ScriptComponent script = {});
    void RemoveScript(Entity entity);

    bool HasAudioSource(Entity entity) const;
    AudioSourceComponent& GetAudioSource(Entity entity);
    const AudioSourceComponent& GetAudioSource(Entity entity) const;
    void AddAudioSource(Entity entity, AudioSourceComponent source = {});
    void RemoveAudioSource(Entity entity);

    SceneSettings& Settings();
    const SceneSettings& Settings() const;

    Entity FindPrimaryCamera() const;
    /// Marks entity as the sole primary camera (must already have CameraComponent).
    void SetPrimaryCamera(Entity entity);

    void ForEachEntity(const std::function<void(Entity)>& fn) const;

    std::size_t EntityCount() const;

    /// Remove all entities (used before loading a scene file).
    void Clear();

    /// Playable game level: ground, player character, follow camera, sun.
    static Scene CreatePlayableLevel();
    static Scene CreateDemoLevel();
    /// Sun + primary camera only (no meshes).
    static Scene CreateEmptyLevel();

private:
    struct EntityRecord {
        uint32_t Generation = 1;
        bool Alive = false;
        std::string Name;
        bool ShowAxes = false;
        Transform LocalTransform;
        Entity Parent{};
        std::optional<MeshRendererComponent> Mesh;
        std::optional<CameraComponent> Camera;
        std::optional<DirectionalLightComponent> Light;
        std::optional<RotatorComponent> Rotator;
        std::optional<MoverComponent> Mover;
        std::optional<PointLightComponent> PointLight;
        std::optional<CharacterControllerComponent> Character;
        std::optional<PlayerControllerComponent> Player;
        std::optional<FollowCameraComponent> FollowCamera;
        std::optional<ScriptComponent> Script;
        std::optional<AudioSourceComponent> AudioSource;
    };

    EntityRecord* GetRecord(Entity entity);
    const EntityRecord* GetRecord(Entity entity) const;

    std::vector<EntityRecord> m_Entities;
    SceneSettings m_Settings{};
};

} // namespace Nova
