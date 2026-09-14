#pragma once

#include <cstdint>
#include <vector>

namespace Nova {

/// CPU RGBA8 image — uploaded to GPU by the Metal backend.
struct ImageRGBA {
    uint32_t Width  = 0;
    uint32_t Height = 0;
    std::vector<uint8_t> Pixels; // size = Width * Height * 4

    bool IsValid() const {
        return Width > 0 && Height > 0 &&
               Pixels.size() == static_cast<size_t>(Width) * Height * 4;
    }
};

/// Built-in debug pattern (no external files).
ImageRGBA CreateCheckerboardImage(uint32_t size, uint32_t cells);

} // namespace Nova
