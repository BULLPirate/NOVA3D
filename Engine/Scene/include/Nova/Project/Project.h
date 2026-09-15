#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Nova {

constexpr int kProjectFileVersion = 1;
constexpr const char* kProjectFileFormat = "nova.project";
constexpr const char* kProjectFolderName = ".nova";
constexpr const char* kProjectFileName = "project.json";

struct ProjectIOResult {
    bool Ok = false;
    std::string Error;
};

struct ProjectDescriptor {
    std::string Name = "Untitled";
    /// Absolute path to the project root (parent of `.nova/`).
    std::filesystem::path Root;
    /// Path relative to Root.
    std::filesystem::path StartupScene = "Assets/Scenes/demo.scene.json";
    std::filesystem::path LastOpenedScene = "Assets/Scenes/demo.scene.json";

    std::filesystem::path ProjectFilePath() const;
    std::filesystem::path ScenesDirectory() const;
    std::filesystem::path StartupSceneAbsolute() const;
    std::filesystem::path LastOpenedSceneAbsolute() const;
};

std::filesystem::path ProjectJsonPath(const std::filesystem::path& projectRoot);

bool IsNovaProjectRoot(const std::filesystem::path& projectRoot);

/// Accepts project root or path to `.nova/project.json`.
ProjectIOResult ResolveProjectRoot(std::filesystem::path pathIn, std::filesystem::path& outRoot);

ProjectIOResult LoadProject(const std::filesystem::path& projectRootOrFile, ProjectDescriptor& out);
ProjectIOResult SaveProject(const ProjectDescriptor& project);

/// Creates `Assets/Scenes` and `.nova` if missing.
ProjectIOResult EnsureProjectLayout(const std::filesystem::path& projectRoot);

/// Writes a new project file and folder layout; does not create scenes.
ProjectIOResult InitializeNewProject(const std::filesystem::path& projectRoot,
                                     const std::string& displayName);

/// Makes a path relative to Root when possible; otherwise returns absolute path.
std::filesystem::path MakeProjectRelativePath(const ProjectDescriptor& project,
                                              const std::filesystem::path& absolutePath);

/// Sorted list of `.scene.json` / `.json` scene files under `Assets/Scenes`.
std::vector<std::filesystem::path> ListProjectScenes(const ProjectDescriptor& project);

/// Sorted project-relative asset paths under `Assets/` (models, textures).
std::vector<std::filesystem::path> ListProjectAssets(const ProjectDescriptor& project);

} // namespace Nova
