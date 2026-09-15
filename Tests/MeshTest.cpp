#include <gtest/gtest.h>
#include <Nova/Renderer/Mesh.h>

#include <cmath>

TEST(Mesh, UnitCubeTopology) {
    Nova::MeshData cube = Nova::CreateUnitCubeMesh();
    EXPECT_EQ(cube.Vertices.size(), 24u);
    EXPECT_EQ(cube.Indices.size(), 36u);
    EXPECT_TRUE(Nova::ValidateUnitCubeMesh(cube));
}

TEST(Mesh, TexturedCubeTopology) {
    Nova::TexturedMeshData cube = Nova::CreateUnitCubeTexturedMesh();
    EXPECT_EQ(cube.Vertices.size(), 24u);
    EXPECT_EQ(cube.Indices.size(), 36u);
    EXPECT_TRUE(Nova::ValidateUnitCubeTexturedMesh(cube));
}

TEST(Mesh, UnitPlaneTopology) {
    Nova::TexturedMeshData plane = Nova::CreateUnitPlaneTexturedMesh();
    EXPECT_EQ(plane.Vertices.size(), 4u);
    EXPECT_EQ(plane.Indices.size(), 6u);
    EXPECT_NEAR(plane.Vertices[0].ny, 1.0f, 1e-4f);
}

TEST(Mesh, TexturedCubeNormalsPerFace) {
    Nova::TexturedMeshData cube = Nova::CreateUnitCubeTexturedMesh();
    bool sawPositiveZ = false;
    for (const auto& v : cube.Vertices) {
        const float len = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz);
        EXPECT_NEAR(len, 1.0f, 1e-4f);
        if (v.nz > 0.9f && v.z > 0.4f) {
            sawPositiveZ = true;
            EXPECT_NEAR(v.nz, 1.0f, 1e-4f);
        }
    }
    EXPECT_TRUE(sawPositiveZ);
}
