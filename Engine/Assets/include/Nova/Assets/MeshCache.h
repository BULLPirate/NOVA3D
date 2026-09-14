#pragma once

#include <Nova/Renderer/Renderer.h>

#include <filesystem>
#include <string>
#include <unordered_map>

namespace Nova {

/// Resolves project-relative mesh paths to GPU mesh handles (OBJ for now).
class MeshAssetCache {
public:
    MeshGpuHandle Resolve(IRenderer& renderer,
                          const std::filesystem::path& projectRoot,
                          const std::string& relativeAssetPath);

    void Clear();

private:
    std::unordered_map<std::string, MeshGpuHandle> m_Cache;
};

} // namespace Nova
