#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Nova {

struct UserWorkspace {
    std::string ProfileName = "Player";
    std::vector<std::filesystem::path> RecentProjects;
    std::filesystem::path LastProject;
};

std::filesystem::path UserWorkspaceFilePath();
std::filesystem::path DefaultProjectsDirectory();

bool LoadUserWorkspace(UserWorkspace& out);
bool SaveUserWorkspace(const UserWorkspace& workspace);
void RememberRecentProject(UserWorkspace& workspace, const std::filesystem::path& projectRoot);

} // namespace Nova
