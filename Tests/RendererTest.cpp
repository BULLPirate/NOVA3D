#include <gtest/gtest.h>
#include <Nova/Renderer/Renderer.h>

TEST(Renderer, FactoryCreatesUninitializedBackend) {
    auto renderer = Nova::CreateRenderer();
    ASSERT_NE(renderer, nullptr);
    EXPECT_FALSE(renderer->IsInitialized());
}
