#include <Nova/Renderer/Texture.h>

namespace Nova {

ImageRGBA CreateCheckerboardImage(uint32_t size, uint32_t cells) {
    ImageRGBA img;
    img.Width = size;
    img.Height = size;
    img.Pixels.resize(static_cast<size_t>(size) * size * 4);

    const uint32_t cell = size / cells;
    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            const bool dark = ((x / cell) + (y / cell)) % 2 == 0;
            const uint8_t c = dark ? 72 : 210;
            const size_t i = (static_cast<size_t>(y) * size + x) * 4;
            img.Pixels[i + 0] = c;
            img.Pixels[i + 1] = c;
            img.Pixels[i + 2] = c;
            img.Pixels[i + 3] = 255;
        }
    }
    return img;
}

} // namespace Nova
