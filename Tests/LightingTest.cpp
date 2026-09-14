#include <gtest/gtest.h>
#include <Nova/Renderer/Lighting.h>

#include <cmath>

TEST(Lighting, DefaultDirectionalLightIsNormalized) {
    Nova::DirectionalLight light = Nova::DefaultDirectionalLight();
    EXPECT_NEAR(light.Direction.Length(), 1.0f, 1e-5f);
    EXPECT_GT(light.Ambient, 0.0f);
    EXPECT_LT(light.Ambient, 1.0f);
}

TEST(Lighting, DirectionalLightViewProjectionFinite) {
    const Nova::Mat4 m = Nova::ComputeDirectionalLightViewProjection(
        {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 2.0f, 0.5f, 20.0f);
    const Nova::Vec4 origin = m * Nova::Vec4{0.0f, 0.0f, 0.0f, 1.0f};
    EXPECT_TRUE(std::isfinite(origin.x));
    EXPECT_TRUE(std::isfinite(origin.y));
    EXPECT_TRUE(std::isfinite(origin.z));
    EXPECT_TRUE(std::isfinite(origin.w));
}
