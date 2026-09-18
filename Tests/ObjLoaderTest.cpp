#include <Nova/Assets/ObjLoader.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

TEST(ObjLoader, LoadsBundledPyramid) {
    const std::filesystem::path path =
        std::filesystem::path(NOVA_SOURCE_DIR) / "Assets/Models/pyramid.obj";
    ASSERT_TRUE(std::filesystem::exists(path));

    const Nova::AssetLoadResult result = Nova::LoadObjMesh(path);
    ASSERT_TRUE(result.Ok) << result.Error;
    EXPECT_GE(result.Mesh.Vertices.size(), 3u);
    EXPECT_GE(result.Mesh.Indices.size(), 3u);
}

TEST(ObjLoader, ReadsUvMappedKnight) {
    const std::filesystem::path path =
        std::filesystem::path(NOVA_SOURCE_DIR) / "Assets/Characters/knight_armed.obj";
    ASSERT_TRUE(std::filesystem::exists(path));
    const Nova::AssetLoadResult result = Nova::LoadObjMesh(path);
    ASSERT_TRUE(result.Ok) << result.Error;
    EXPECT_GT(result.Mesh.Vertices.size(), 500u);
    bool hasUv = false;
    for (const Nova::TexturedVertex& v : result.Mesh.Vertices) {
        if (v.u > 0.01f || v.v > 0.01f) {
            hasUv = true;
            break;
        }
    }
    EXPECT_TRUE(hasUv);
}
