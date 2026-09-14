#pragma once

#include <filesystem>

namespace Nova::Editor {

/// Path to `Nova3D.app` next to NovaEditor in the build tree.
std::filesystem::path ResolveNova3DAppBundle();

/// Launches the standalone runtime with `--scene` (macOS `open`).
bool LaunchGameWithScene(const std::filesystem::path& scenePath);

} // namespace Nova::Editor
