#include <Nova/Assets/GltfLoader.h>
#include <Nova/Assets/MeshLoader.h>

#include <gtest/gtest.h>

#include <filesystem>

TEST(GltfLoader, LoadsBundledTriangle) {
    const std::filesystem::path path =
        std::filesystem::path(NOVA_SOURCE_DIR) / "Assets/Models/triangle.gltf";
    ASSERT_TRUE(std::filesystem::exists(path));

    const Nova::AssetLoadResult result = Nova::LoadGltfMesh(path);
    ASSERT_TRUE(result.Ok) << result.Error;
    EXPECT_GE(result.Mesh.Vertices.size(), 3u);
    EXPECT_GE(result.Mesh.Indices.size(), 3u);
}

TEST(MeshLoader, DispatchesByExtension) {
    const std::filesystem::path gltf =
        std::filesystem::path(NOVA_SOURCE_DIR) / "Assets/Models/triangle.gltf";
    const Nova::AssetLoadResult gltfResult = Nova::LoadMeshAsset(gltf);
    EXPECT_TRUE(gltfResult.Ok) << gltfResult.Error;
}
