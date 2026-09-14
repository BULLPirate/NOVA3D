#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Nova::Editor {

struct FileFilter {
    std::string Description;
    std::vector<std::string> Extensions; // e.g. "json"
};

std::optional<std::filesystem::path> ShowOpenFolderDialog(const char* title);
std::optional<std::filesystem::path> ShowOpenFileDialog(const char* title,
                                                        const std::vector<FileFilter>& filters);
std::optional<std::filesystem::path> ShowSaveFileDialog(const char* title,
                                                        const std::filesystem::path& defaultPath,
                                                        const std::vector<FileFilter>& filters);

} // namespace Nova::Editor
