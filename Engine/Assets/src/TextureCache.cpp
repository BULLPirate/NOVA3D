#include <Nova/Assets/TextureCache.h>

#include <Nova/Assets/PngLoader.h>
#include <Nova/Core/Log.h>
#include <Nova/Core/FileSystem.h>

namespace Nova {

TextureGpuHandle TextureAssetCache::Resolve(IRenderer& renderer,
                                              const std::filesystem::path& projectRoot,
                                              const std::string& relativePath) {
    if (relativePath.empty()) {
        return kDefaultTextureGpuHandle;
    }

    const auto found = m_Cache.find(relativePath);
    if (found != m_Cache.end()) {
        return found->second;
    }

    const std::filesystem::path absolute = ResolveDataFile(projectRoot, relativePath);
    PngLoadResult loaded = LoadPngImage(absolute);
    if (!loaded.Ok) {
        NOVA_LOG_WARN("Texture '{}': {} — using default", relativePath, loaded.Error);
        m_Cache[relativePath] = kDefaultTextureGpuHandle;
        return kDefaultTextureGpuHandle;
    }

    const TextureGpuHandle handle = renderer.CreateGpuTexture(loaded.Image);
    m_Cache[relativePath] = handle;
    NOVA_LOG_INFO("Loaded texture: {}", absolute.string());
    return handle;
}

void TextureAssetCache::Clear() { m_Cache.clear(); }

} // namespace Nova
