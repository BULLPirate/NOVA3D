#include <gtest/gtest.h>
#include <Nova/Renderer/Mesh.h>

TEST(Mesh, UnitCubeTopology) {
    Nova::MeshData cube = Nova::CreateUnitCubeMesh();
    EXPECT_EQ(cube.Vertices.size(), 24u);
    EXPECT_EQ(cube.Indices.size(), 36u);
}
