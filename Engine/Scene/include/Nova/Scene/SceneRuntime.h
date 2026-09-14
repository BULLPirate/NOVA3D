#pragma once

#include <Nova/Scene/Scene.h>

namespace Nova {

/// Advances gameplay / simulation components (call before render in play mode).
void TickScene(Scene& scene, float deltaSeconds);

} // namespace Nova
