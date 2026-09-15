#pragma once

#include <Nova/Renderer/Renderer.h>

#include <filesystem>
#include <string>
#include <unordered_map>

namespace Nova {

class TextureAssetCache {
public:
    TextureGpuHandle Resolve(IRenderer& renderer,
                             const std::filesystem::path& projectRoot,
                             const std::string& relativePath);

    void Clear();

private:
    std::unordered_map<std::string, TextureGpuHandle> m_Cache;
};

} // namespace Nova
