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

} // namespace Nova
