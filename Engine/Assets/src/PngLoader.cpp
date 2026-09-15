#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <Nova/Assets/PngLoader.h>

namespace Nova {

PngLoadResult LoadPngImage(const std::filesystem::path& path) {
    PngLoadResult result;
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels =
        stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        result.Error = stbi_failure_reason() ? stbi_failure_reason() : "stbi_load failed";
        return result;
    }

    result.Image.Width = static_cast<uint32_t>(width);
    result.Image.Height = static_cast<uint32_t>(height);
    result.Image.Pixels.assign(pixels, pixels + static_cast<size_t>(width) * height * 4);
    stbi_image_free(pixels);

    result.Ok = true;
    return result;
}

} // namespace Nova
