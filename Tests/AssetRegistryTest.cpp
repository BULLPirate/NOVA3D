#include <gtest/gtest.h>

#include <Nova/Assets/AssetRegistry.h>

TEST(AssetRegistry, SamePathSameId) {
    Nova::AssetRegistry registry;
    const Nova::AssetId a = registry.RegisterPath("Assets/Models/hero.gltf");
    const Nova::AssetId b = registry.RegisterPath("Assets/Models/hero.gltf");
    const Nova::AssetId c = registry.RegisterPath("Assets/Models/crate.obj");
    EXPECT_FALSE(a.IsNil());
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    ASSERT_TRUE(registry.PathFor(a).has_value());
    EXPECT_EQ(*registry.PathFor(a), "Assets/Models/hero.gltf");
    EXPECT_EQ(*registry.IdFor("Assets/Models/crate.obj"), c);
    EXPECT_EQ(registry.Count(), 2u);
}

TEST(AssetRegistry, EmptyPathIsNil) {
    Nova::AssetRegistry registry;
    EXPECT_TRUE(registry.RegisterPath("").IsNil());
    EXPECT_EQ(registry.Count(), 0u);
}
