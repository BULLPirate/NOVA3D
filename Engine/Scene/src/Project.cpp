#include <Nova/Project/Project.h>

#include <nlohmann/json.hpp>

#include <fstream>

namespace Nova {

namespace {

using json = nlohmann::json;

std::string PathToUtf8(const std::filesystem::path& p) {
    return p.generic_string();
}

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
    std::ifstream in(jsonPath);
    if (!in) {
        result.Error = "cannot open " + PathToUtf8(jsonPath);
        return result;
    }

    json doc;
    try {
        in >> doc;
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
    std::ofstream out(jsonPath);
    if (!out) {
        result.Error = "cannot write " + PathToUtf8(jsonPath);
        return result;
    }
    out << doc.dump(2);
    result.Ok = true;
    return result;
}

ProjectIOResult EnsureProjectLayout(const std::filesystem::path& projectRoot) {
    ProjectIOResult result;
    std::error_code ec;
    std::filesystem::create_directories(projectRoot / "Assets" / "Scenes", ec);
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

} // namespace Nova
