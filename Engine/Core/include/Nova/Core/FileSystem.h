#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace Nova {

struct FileIOResult {
    bool Ok = false;
    std::string Error;
    std::string Text;
};

bool FileExists(const std::filesystem::path& path);
bool CreateDirectories(const std::filesystem::path& path, std::string& error);

/// Project file first, then engine `Assets/` so editor primitives can use built-in meshes.
std::filesystem::path ResolveDataFile(const std::filesystem::path& projectRoot,
                                      const std::string& relativeAssetPath);

FileIOResult ReadTextFile(const std::filesystem::path& path);
FileIOResult WriteTextFile(const std::filesystem::path& path, std::string_view text);

} // namespace Nova
