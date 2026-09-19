#pragma once

#include <Nova/Project/Project.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace Nova {

enum class ProjectTemplateKind : uint8_t {
    Empty = 0,
    ThirdPerson = 1,
    KnightBandits = 2,
};

ProjectIOResult CreateGameProject(const std::filesystem::path& projectRoot, const std::string& name,
                                  ProjectTemplateKind kind,
                                  const std::filesystem::path& engineRoot = {});

/// Creates the folder if needed. For Knight Bandits, restores the combat scene when the
/// existing project is a leftover sandbox (so `run.sh knight` actually launches the game).
ProjectIOResult EnsureGameProject(const std::filesystem::path& projectRoot, const std::string& name,
                                  ProjectTemplateKind kind,
                                  const std::filesystem::path& engineRoot = {});

} // namespace Nova
