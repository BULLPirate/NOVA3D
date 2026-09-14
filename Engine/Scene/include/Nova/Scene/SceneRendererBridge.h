#pragma once

#include <Nova/Scene/Scene.h>
#include <Nova/Renderer/Renderer.h>
#include <Nova/Renderer/Camera.h>

namespace Nova {

/// Maps scene components to the renderer for the current frame.
/// Pass timeSeconds < 0 to disable demo auto-rotation.
void RenderScene(const Scene& scene, IRenderer& renderer, float aspect, float timeSeconds);

/// Fills camera from the scene primary camera (after orbit sync). Returns false if none.
bool BuildSceneCamera(const Scene& scene, float aspect, Camera& outCamera);

} // namespace Nova
