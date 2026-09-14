#pragma once

#include <cstdint>

namespace Nova {

/// Pixel rectangle for 3D drawing (origin top-left of the framebuffer).
struct RenderViewport {
    uint32_t X = 0;
    uint32_t Y = 0;
    uint32_t Width = 0;
    uint32_t Height = 0;
    bool Active = false;
};

} // namespace Nova
