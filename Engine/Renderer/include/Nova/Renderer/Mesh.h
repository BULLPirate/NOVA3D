#pragma once

#include <cstdint>
#include <vector>

namespace Nova {

/// Colored vertex — temporary format until materials exist.
struct ColoredVertex {
    float x, y, z;
    float r, g, b, a;
};

/// CPU-side mesh: uploaded to GPU by the renderer.
struct MeshData {
    std::vector<ColoredVertex> Vertices;
    std::vector<uint32_t>      Indices;
};

/// Engine test primitive (unit cube centered at origin).
MeshData CreateUnitCubeMesh();

struct TexturedVertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};

struct TexturedMeshData {
    std::vector<TexturedVertex> Vertices;
    std::vector<uint32_t>       Indices;
};

/// Same cube topology with UVs for texture sampling.
TexturedMeshData CreateUnitCubeTexturedMesh();
/// 1x1 quad on XZ (Y up), centered at origin.
TexturedMeshData CreateUnitPlaneTexturedMesh();

/// Geometry checks (outward-facing triangles, 24 verts / 36 indices).
bool ValidateUnitCubeMesh(const MeshData& mesh);
bool ValidateUnitCubeTexturedMesh(const TexturedMeshData& mesh);

} // namespace Nova
