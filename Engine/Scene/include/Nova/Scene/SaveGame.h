#pragma once

#include <Nova/Project/Project.h>
#include <Nova/Scene/SceneSerialization.h>

#include <string>
#include <vector>

namespace Nova {

std::filesystem::path SavesDirectory(const ProjectDescriptor& project);
std::vector<std::string> ListSaveSlots(const ProjectDescriptor& project);
SceneIOResult SaveGameSlot(const ProjectDescriptor& project, const std::string& slot,
                           const Scene& scene);
SceneIOResult LoadGameSlot(const ProjectDescriptor& project, const std::string& slot, Scene& out);

} // namespace Nova
