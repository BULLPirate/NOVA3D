#pragma once

#include <Nova/Scene/Scene.h>

#include <filesystem>
#include <string>

namespace Nova {

constexpr int kSceneFileVersion = 1;
constexpr const char* kSceneFileFormat = "nova.scene";

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

} // namespace Nova
