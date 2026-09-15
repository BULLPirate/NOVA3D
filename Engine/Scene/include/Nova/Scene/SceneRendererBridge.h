#pragma once

#include <Nova/Scene/Scene.h>
#include <Nova/Renderer/Renderer.h>
#include <Nova/Renderer/Camera.h>

#include <filesystem>

namespace Nova {

class MeshAssetCache;
class TextureAssetCache;

/// Maps scene components to the renderer for the current frame.
void RenderScene(const Scene& scene,
                 IRenderer& renderer,
                 float aspect,
                 const std::filesystem::path& projectRoot,
                 MeshAssetCache& meshCache,
                 TextureAssetCache& textureCache);

/// Fills camera from the scene primary camera (after orbit sync). Returns false if none.
bool BuildSceneCamera(const Scene& scene, float aspect, Camera& outCamera);

} // namespace Nova
