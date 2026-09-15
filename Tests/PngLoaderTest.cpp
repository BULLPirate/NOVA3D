#include <Nova/Assets/PngLoader.h>

#include <gtest/gtest.h>

#include <filesystem>

TEST(PngLoader, LoadsBundledChecker) {
    const std::filesystem::path path =
        std::filesystem::path(NOVA_SOURCE_DIR) / "Assets/Textures/checker.png";
    ASSERT_TRUE(std::filesystem::exists(path));

    const Nova::PngLoadResult result = Nova::LoadPngImage(path);
    ASSERT_TRUE(result.Ok) << result.Error;
    EXPECT_TRUE(result.Image.IsValid());
}
