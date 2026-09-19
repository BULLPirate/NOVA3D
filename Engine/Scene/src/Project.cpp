#include <Nova/Project/Project.h>
#include <Nova/Core/EngineSettings.h>
#include <Nova/Core/FileSystem.h>
#include <Nova/Scene/Script.h>
#include <Nova/Scene/Scene.h>
#include <Nova/Scene/SceneSerialization.h>

#include <nlohmann/json.hpp>

#include <algorithm>

namespace Nova {

namespace {

using json = nlohmann::json;

} // namespace

std::filesystem::path ProjectJsonPath(const std::filesystem::path& projectRoot) {
    return projectRoot / kProjectFolderName / kProjectFileName;
}

std::filesystem::path ProjectDescriptor::ProjectFilePath() const {
    return ProjectJsonPath(Root);
}

std::filesystem::path ProjectDescriptor::ScenesDirectory() const {
    return Root / "Assets" / "Scenes";
}

std::filesystem::path ProjectDescriptor::PrefabsDirectory() const {
    return Root / "Assets" / "Prefabs";
}

std::filesystem::path ProjectDescriptor::StartupSceneAbsolute() const {
    return Root / StartupScene;
}

std::filesystem::path ProjectDescriptor::LastOpenedSceneAbsolute() const {
    return Root / LastOpenedScene;
}

bool IsNovaProjectRoot(const std::filesystem::path& projectRoot) {
    return std::filesystem::is_regular_file(ProjectJsonPath(projectRoot));
}

ProjectIOResult ResolveProjectRoot(std::filesystem::path pathIn, std::filesystem::path& outRoot) {
    ProjectIOResult result;
    if (pathIn.empty()) {
        result.Error = "empty path";
        return result;
    }

    std::error_code ec;
    pathIn = std::filesystem::weakly_canonical(pathIn, ec);
    if (ec) {
        pathIn = std::filesystem::absolute(pathIn);
    }

    if (pathIn.filename() == kProjectFileName &&
        pathIn.parent_path().filename() == kProjectFolderName) {
        outRoot = pathIn.parent_path().parent_path();
        result.Ok = true;
        return result;
    }

    if (IsNovaProjectRoot(pathIn)) {
        outRoot = pathIn;
        result.Ok = true;
        return result;
    }

    result.Error = "not a NOVA3D project (missing .nova/project.json)";
    return result;
}

ProjectIOResult LoadProject(const std::filesystem::path& projectRootOrFile,
                            ProjectDescriptor& out) {
    ProjectIOResult result;
    std::filesystem::path root;
    if (ProjectIOResult resolve = ResolveProjectRoot(projectRootOrFile, root); !resolve.Ok) {
        return resolve;
    }

    const std::filesystem::path jsonPath = ProjectJsonPath(root);
    const FileIOResult read = ReadTextFile(jsonPath);
    if (!read.Ok) {
        result.Error = read.Error;
        return result;
    }

    json doc;
    try {
        doc = json::parse(read.Text);
    } catch (const json::exception& ex) {
        result.Error = ex.what();
        return result;
    }

    if (!doc.contains("format") || doc["format"].get<std::string>() != kProjectFileFormat) {
        result.Error = "invalid or missing format";
        return result;
    }
    if (!doc.contains("version") || doc["version"].get<int>() != kProjectFileVersion) {
        result.Error = "unsupported project version";
        return result;
    }

    out.Root = root;
    out.Name = doc.value("name", "Untitled");
    out.StartupScene = doc.value("startupScene", "Assets/Scenes/demo.scene.json");
    out.LastOpenedScene = doc.value("lastOpenedScene", out.StartupScene.generic_string());

    result.Ok = true;
    return result;
}

ProjectIOResult SaveProject(const ProjectDescriptor& project) {
    ProjectIOResult result;
    if (project.Root.empty()) {
        result.Error = "project root is empty";
        return result;
    }

    if (ProjectIOResult layout = EnsureProjectLayout(project.Root); !layout.Ok) {
        return layout;
    }

    json doc = {
        {"format", kProjectFileFormat},
        {"version", kProjectFileVersion},
        {"name", project.Name},
        {"startupScene", project.StartupScene.generic_string()},
        {"lastOpenedScene", project.LastOpenedScene.generic_string()},
    };

    const std::filesystem::path jsonPath = ProjectJsonPath(project.Root);
    const FileIOResult write = WriteTextFile(jsonPath, doc.dump(2));
    if (!write.Ok) {
        result.Error = write.Error;
        return result;
    }
    result.Ok = true;
    return result;
}

ProjectIOResult EnsureProjectLayout(const std::filesystem::path& projectRoot) {
    ProjectIOResult result;
    std::error_code ec;
    std::filesystem::create_directories(projectRoot / "Assets" / "Scenes", ec);
    std::filesystem::create_directories(projectRoot / "Assets" / "Prefabs", ec);
    std::filesystem::create_directories(projectRoot / "Assets" / "Models", ec);
    std::filesystem::create_directories(projectRoot / "Assets" / "Textures", ec);
    std::filesystem::create_directories(projectRoot / "Assets" / "Audio", ec);
    std::filesystem::create_directories(projectRoot / "Assets" / "Environment", ec);
    std::filesystem::create_directories(projectRoot / "Assets" / "Scripts", ec);
    std::filesystem::create_directories(projectRoot / "Assets" / "Characters", ec);
    std::filesystem::create_directories(projectRoot / "Assets" / "Items", ec);
    std::filesystem::create_directories(projectRoot / kProjectFolderName, ec);
    if (ec) {
        result.Error = ec.message();
        return result;
    }
    result.Ok = true;
    return result;
}

ProjectIOResult InitializeNewProject(const std::filesystem::path& projectRoot,
                                     const std::string& displayName) {
    ProjectIOResult result;
    if (projectRoot.empty()) {
        result.Error = "empty project root";
        return result;
    }

    if (ProjectIOResult layout = EnsureProjectLayout(projectRoot); !layout.Ok) {
        return layout;
    }

    SaveEngineSettings(projectRoot, EngineSettings::Defaults());
    WriteTextFile(projectRoot / "Assets" / "Scripts" / "player.ns", DefaultPlayerScript());
    WriteTextFile(projectRoot / "Assets" / "Scripts" / "world.ns", DefaultContentScript());
    WriteTextFile(projectRoot / "Assets" / "Scripts" / "game.ns", DefaultGameScript());
    std::string dirError;
    CreateDirectories(projectRoot / "Saves", dirError);
    CreateDirectories(projectRoot / "Assets" / "UI", dirError);

    Scene empty = Scene::CreateEmptyLevel();
    SaveSceneToFile(empty, projectRoot / "Assets" / "Scenes" / "main.scene.json");

    ProjectDescriptor project;
    project.Root = projectRoot;
    project.Name = displayName.empty() ? projectRoot.filename().string() : displayName;
    project.StartupScene = "Assets/Scenes/main.scene.json";
    project.LastOpenedScene = project.StartupScene;

    return SaveProject(project);
}

std::filesystem::path MakeProjectRelativePath(const ProjectDescriptor& project,
                                                const std::filesystem::path& absolutePath) {
    std::error_code ec;
    const std::filesystem::path abs =
        std::filesystem::weakly_canonical(absolutePath, ec);
    const std::filesystem::path root =
        std::filesystem::weakly_canonical(project.Root, ec);

    std::filesystem::path rel = abs.lexically_relative(root);
    if (!rel.empty() && rel.native()[0] != '.') {
        return rel;
    }
    return abs;
}

std::vector<std::filesystem::path> ListProjectScenes(const ProjectDescriptor& project) {
    std::vector<std::filesystem::path> scenes;
    const std::filesystem::path scenesDir = project.ScenesDirectory();
    std::error_code ec;
    if (!std::filesystem::is_directory(scenesDir, ec)) {
        return scenes;
    }

    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(scenesDir, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::filesystem::path file = entry.path();
        if (file.extension() == ".json") {
            scenes.push_back(MakeProjectRelativePath(project, file));
        }
    }

    std::sort(scenes.begin(), scenes.end());
    return scenes;
}

std::vector<std::filesystem::path> ListProjectPrefabs(const ProjectDescriptor& project) {
    std::vector<std::filesystem::path> prefabs;
    const std::filesystem::path dir = project.PrefabsDirectory();
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        return prefabs;
    }

    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::filesystem::path file = entry.path();
        if (file.extension() == ".json") {
            prefabs.push_back(MakeProjectRelativePath(project, file));
        }
    }

    std::sort(prefabs.begin(), prefabs.end());
    return prefabs;
}

std::vector<std::filesystem::path> ListProjectAssets(const ProjectDescriptor& project) {
    std::vector<std::filesystem::path> assets;
    const std::filesystem::path assetsDir = project.Root / "Assets";
    std::error_code ec;
    if (!std::filesystem::is_directory(assetsDir, ec)) {
        return assets;
    }

    for (const std::filesystem::directory_entry& entry :
         std::filesystem::recursive_directory_iterator(assetsDir, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::filesystem::path file = entry.path();
        const std::string ext = file.extension().string();
        if (ext == ".png" || ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".wav" ||
            ext == ".ns") {
            assets.push_back(MakeProjectRelativePath(project, file));
        }
    }

    std::sort(assets.begin(), assets.end());
    return assets;
}

} // namespace Nova
