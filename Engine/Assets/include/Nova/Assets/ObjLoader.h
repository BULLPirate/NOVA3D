#pragma once

#include <Nova/Renderer/Mesh.h>
#include <Nova/Math/Vec.h>

#include <filesystem>
#include <string>

namespace Nova {

struct AssetLoadResult {
    bool Ok = false;
    std::string Error;
    TexturedMeshData Mesh;
    Vec3 MaterialBaseColor{1.0f, 1.0f, 1.0f};
    bool HasMaterialBaseColor = false;
};

AssetLoadResult LoadObjMesh(const std::filesystem::path& path);

} // namespace Nova
