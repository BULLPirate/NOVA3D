#pragma once

#include <Nova/Renderer/Mesh.h>

#include <filesystem>
#include <string>

namespace Nova {

struct AssetLoadResult {
    bool Ok = false;
    std::string Error;
    TexturedMeshData Mesh;
};

AssetLoadResult LoadObjMesh(const std::filesystem::path& path);

} // namespace Nova
