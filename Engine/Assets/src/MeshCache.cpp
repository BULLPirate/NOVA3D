#include <Nova/Assets/MeshCache.h>

#include <Nova/Assets/ObjLoader.h>
#include <Nova/Core/Log.h>
#include <Nova/Renderer/Mesh.h>

namespace Nova {

MeshGpuHandle MeshAssetCache::Resolve(IRenderer& renderer,
                                      const std::filesystem::path& projectRoot,
                                      const std::string& relativeAssetPath) {
    if (relativeAssetPath.empty()) {
        return kDefaultMeshGpuHandle;
    }

    const auto found = m_Cache.find(relativeAssetPath);
    if (found != m_Cache.end()) {
        return found->second;
    }

    const std::filesystem::path absolute = projectRoot / relativeAssetPath;
    AssetLoadResult loaded = LoadObjMesh(absolute);
    if (!loaded.Ok) {
        NOVA_LOG_WARN("Mesh asset '{}': {} — using unit cube", relativeAssetPath, loaded.Error);
        m_Cache[relativeAssetPath] = kDefaultMeshGpuHandle;
        return kDefaultMeshGpuHandle;
    }

    const MeshGpuHandle handle = renderer.CreateGpuMesh(loaded.Mesh);
    m_Cache[relativeAssetPath] = handle;
    NOVA_LOG_INFO("Loaded mesh asset: {}", absolute.string());
    return handle;
}

void MeshAssetCache::Clear() { m_Cache.clear(); }

} // namespace Nova
