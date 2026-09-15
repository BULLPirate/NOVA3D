#pragma once

#include <Nova/Assets/ObjLoader.h>

#include <filesystem>

namespace Nova {

/// Loads mesh geometry from OBJ, glTF, or GLB (project-relative path resolved by caller).
AssetLoadResult LoadMeshAsset(const std::filesystem::path& absolutePath);

} // namespace Nova
