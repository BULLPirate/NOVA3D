#pragma once

#include <Nova/Project/Project.h>

#include <filesystem>

namespace Nova::Editor {

/// Path to `Nova3D.app` next to NovaEditor in the build tree.
std::filesystem::path ResolveNova3DAppBundle();

/// Launches the standalone runtime for an open game project.
bool LaunchGame(const Nova::ProjectDescriptor& project, const std::filesystem::path& scenePath = {});

} // namespace Nova::Editor
