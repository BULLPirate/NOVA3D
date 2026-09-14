#include <gtest/gtest.h>
#include <Nova/Renderer/Texture.h>

TEST(Texture, CheckerboardValid) {
    Nova::ImageRGBA img = Nova::CreateCheckerboardImage(64, 8);
    EXPECT_TRUE(img.IsValid());
    EXPECT_EQ(img.Width, 64u);
    EXPECT_EQ(img.Height, 64u);
}

TEST(Texture, CheckerboardAlternates) {
    Nova::ImageRGBA img = Nova::CreateCheckerboardImage(64, 8);
    // Pixel (0,0) vs (8,0) — neighboring checker cells.
    const size_t a = 0;
    const size_t b = static_cast<size_t>(8) * 4;
    EXPECT_NE(img.Pixels[a], img.Pixels[b]);
}
