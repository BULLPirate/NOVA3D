#pragma once

#include <Nova/Assets/ObjLoader.h>

#include <filesystem>

namespace Nova {

AssetLoadResult LoadGltfMesh(const std::filesystem::path& path);

} // namespace Nova
