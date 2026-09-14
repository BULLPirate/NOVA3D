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
