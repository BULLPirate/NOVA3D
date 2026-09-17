#pragma once

#include <Nova/Scene/Scene.h>

#include <filesystem>
#include <string>

namespace Nova {

constexpr int kSceneFileVersion = 1;
constexpr const char* kSceneFileFormat = "nova.scene";
constexpr int kPrefabFileVersion = 1;
constexpr const char* kPrefabFileFormat = "nova.prefab";

struct SceneIOResult {
    bool Ok = false;
    std::string Error;
};

SceneIOResult SaveSceneToFile(const Scene& scene, const std::filesystem::path& path);
SceneIOResult LoadSceneFromFile(const std::filesystem::path& path, Scene& outScene);

/// Round-trip helpers for tests.
std::string SerializeSceneToString(const Scene& scene);
SceneIOResult DeserializeSceneFromString(const std::string& json, Scene& outScene);

/// Compare scene contents (ignores entity ids / order by name).
bool ScenesEquivalent(const Scene& a, const Scene& b, float epsilon = 1e-4f);

/// Deep copy via JSON round-trip (used for Play Mode).
Scene CloneScene(const Scene& source);

/// Entity plus descendants. Parent links that leave the subtree are omitted.
std::string SerializePrefabToString(const Scene& scene, Entity root);
SceneIOResult SavePrefabToFile(const Scene& scene, Entity root, const std::filesystem::path& path);
SceneIOResult InstantiatePrefabFromString(const std::string& json, Scene& dest, Entity& outRoot);
SceneIOResult InstantiatePrefabFromFile(const std::filesystem::path& path, Scene& dest, Entity& outRoot);

} // namespace Nova
