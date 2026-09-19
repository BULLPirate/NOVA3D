#include <Nova/Project/Workspace.h>
#include <Nova/Core/FileSystem.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>

namespace Nova {

namespace {

using json = nlohmann::json;

std::filesystem::path HomeDirectory() {
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home);
    }
    return std::filesystem::current_path();
}

} // namespace

std::filesystem::path UserWorkspaceFilePath() {
    return HomeDirectory() / "Library" / "Application Support" / "NOVA3D" / "workspace.json";
}

std::filesystem::path DefaultProjectsDirectory() {
    return HomeDirectory() / "Documents" / "NOVA3D" / "Projects";
}

bool LoadUserWorkspace(UserWorkspace& out) {
    out = UserWorkspace{};
    const FileIOResult read = ReadTextFile(UserWorkspaceFilePath());
    if (!read.Ok) {
        return false;
    }
    try {
        const json doc = json::parse(read.Text);
        out.ProfileName = doc.value("profileName", out.ProfileName);
        if (doc.contains("lastProject") && doc["lastProject"].is_string()) {
            out.LastProject = doc["lastProject"].get<std::string>();
        }
        if (doc.contains("recent") && doc["recent"].is_array()) {
            for (const json& item : doc["recent"]) {
                if (item.is_string()) {
                    out.RecentProjects.emplace_back(item.get<std::string>());
                }
            }
        }
    } catch (const json::exception&) {
        return false;
    }
    return true;
}

bool SaveUserWorkspace(const UserWorkspace& workspace) {
    json recent = json::array();
    for (const std::filesystem::path& path : workspace.RecentProjects) {
        recent.push_back(path.generic_string());
    }
    const json doc = {
        {"profileName", workspace.ProfileName},
        {"lastProject", workspace.LastProject.generic_string()},
        {"recent", recent},
    };
    std::string error;
    CreateDirectories(UserWorkspaceFilePath().parent_path(), error);
    return WriteTextFile(UserWorkspaceFilePath(), doc.dump(2)).Ok;
}

void RememberRecentProject(UserWorkspace& workspace, const std::filesystem::path& projectRoot) {
    if (projectRoot.empty()) {
        return;
    }
    workspace.LastProject = projectRoot;
    workspace.RecentProjects.erase(std::remove(workspace.RecentProjects.begin(),
                                               workspace.RecentProjects.end(), projectRoot),
                                   workspace.RecentProjects.end());
    workspace.RecentProjects.insert(workspace.RecentProjects.begin(), projectRoot);
    if (workspace.RecentProjects.size() > 12) {
        workspace.RecentProjects.resize(12);
    }
    SaveUserWorkspace(workspace);
}

} // namespace Nova
