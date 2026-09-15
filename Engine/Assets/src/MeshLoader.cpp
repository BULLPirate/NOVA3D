#include <Nova/Assets/MeshLoader.h>

#include <Nova/Assets/GltfLoader.h>
#include <Nova/Assets/ObjLoader.h>

#include <algorithm>
#include <cctype>

namespace Nova {

namespace {

std::string LowerExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

} // namespace

AssetLoadResult LoadMeshAsset(const std::filesystem::path& absolutePath) {
    const std::string ext = LowerExtension(absolutePath);
    if (ext == ".gltf" || ext == ".glb") {
        return LoadGltfMesh(absolutePath);
    }
    if (ext == ".obj") {
        return LoadObjMesh(absolutePath);
    }

    AssetLoadResult result;
    result.Error = "unsupported mesh extension (use .obj, .gltf, .glb)";
    return result;
}

} // namespace Nova
