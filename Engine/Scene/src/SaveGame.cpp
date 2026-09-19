#include <Nova/Scene/SaveGame.h>
#include <Nova/Scene/SceneSerialization.h>
#include <Nova/Core/FileSystem.h>

#include <algorithm>

namespace Nova {

std::filesystem::path SavesDirectory(const ProjectDescriptor& project) {
    return project.Root / "Saves";
}

std::vector<std::string> ListSaveSlots(const ProjectDescriptor& project) {
    std::vector<std::string> slots;
    std::error_code ec;
    const std::filesystem::path dir = SavesDirectory(project);
    if (!std::filesystem::exists(dir, ec)) {
        return slots;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() == ".json") {
            slots.push_back(entry.path().stem().string());
        }
    }
    std::sort(slots.begin(), slots.end());
    return slots;
}

SceneIOResult SaveGameSlot(const ProjectDescriptor& project, const std::string& slot,
                           const Scene& scene) {
    SceneIOResult result;
    if (slot.empty()) {
        result.Error = "empty save slot";
        return result;
    }
    std::string error;
    if (!CreateDirectories(SavesDirectory(project), error)) {
        result.Error = error;
        return result;
    }
    return SaveSceneToFile(scene, SavesDirectory(project) / (slot + ".json"));
}

SceneIOResult LoadGameSlot(const ProjectDescriptor& project, const std::string& slot, Scene& out) {
    return LoadSceneFromFile(SavesDirectory(project) / (slot + ".json"), out);
}

} // namespace Nova
