#pragma once

#include <Nova/Scene/Scene.h>
#include <Nova/Renderer/Renderer.h>

namespace Nova {

/// Maps scene components to the renderer for the current frame.
/// Pass timeSeconds < 0 to disable demo auto-rotation.
void RenderScene(const Scene& scene, IRenderer& renderer, float aspect, float timeSeconds);

} // namespace Nova
