#pragma once

#include <Nova/Renderer/Texture.h>

#include <filesystem>
#include <string>

namespace Nova {

struct PngLoadResult {
    bool Ok = false;
    std::string Error;
    ImageRGBA Image;
};

PngLoadResult LoadPngImage(const std::filesystem::path& path);

} // namespace Nova
