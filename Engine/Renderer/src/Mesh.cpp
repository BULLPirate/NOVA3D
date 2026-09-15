#include <Nova/Renderer/Mesh.h>
#include <Nova/Math/Vec.h>

#include <cmath>

namespace Nova {

namespace {

constexpr float kHalf = 0.5f;

Vec3 TriangleNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
    return (b - a).Cross(c - a);
}

bool TriangleFacesOutward(const Vec3& a, const Vec3& b, const Vec3& c) {
    const Vec3 n = TriangleNormal(a, b, c);
    const Vec3 center = (a + b + c) / 3.0f;
    return n.Dot(center) > 0.0f;
}

void AddFace(std::vector<ColoredVertex>& verts,
             std::vector<uint32_t>& indices,
             const ColoredVertex& a, const ColoredVertex& b,
             const ColoredVertex& c, const ColoredVertex& d) {
    const uint32_t base = static_cast<uint32_t>(verts.size());
    verts.push_back(a);
    verts.push_back(b);
    verts.push_back(c);
    verts.push_back(d);
    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
    indices.push_back(base + 0);
}

void AddFace(std::vector<TexturedVertex>& verts,
             std::vector<uint32_t>& indices,
             const TexturedVertex& a, const TexturedVertex& b,
             const TexturedVertex& c, const TexturedVertex& d) {
    const uint32_t base = static_cast<uint32_t>(verts.size());
    verts.push_back(a);
    verts.push_back(b);
    verts.push_back(c);
    verts.push_back(d);
    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
    indices.push_back(base + 0);
}

ColoredVertex CV(const Vec3& p, float r, float g, float b) {
    return {p.x, p.y, p.z, r, g, b, 1.0f};
}

TexturedVertex TV(const Vec3& p, const Vec3& n, float u, float v) {
    return {p.x, p.y, p.z, n.x, n.y, n.z, u, v};
}

/// CCW quad on a cube face. bitangent = normal × tangent (right-handed).
void AddCubeFaceColored(std::vector<ColoredVertex>& verts,
                        std::vector<uint32_t>& indices,
                        const Vec3& normal, const Vec3& tangent,
                        float r, float g, float b) {
    const Vec3 bitangent = normal.Cross(tangent);
    const Vec3 center = normal * kHalf;

    auto corner = [&](float su, float sv) {
        return center + tangent * (su * kHalf) + bitangent * (sv * kHalf);
    };

    // CCW when viewed from outside (along +normal).
    const Vec3 p0 = corner(-1.0f, -1.0f);
    const Vec3 p1 = corner( 1.0f, -1.0f);
    const Vec3 p2 = corner( 1.0f,  1.0f);
    const Vec3 p3 = corner(-1.0f,  1.0f);

    AddFace(verts, indices,
            CV(p0, r, g, b), CV(p1, r, g, b), CV(p2, r, g, b), CV(p3, r, g, b));
}

void AddCubeFaceTextured(std::vector<TexturedVertex>& verts,
                         std::vector<uint32_t>& indices,
                         const Vec3& normal, const Vec3& tangent) {
    const Vec3 bitangent = normal.Cross(tangent);
    const Vec3 center = normal * kHalf;

    auto corner = [&](float su, float sv) {
        return center + tangent * (su * kHalf) + bitangent * (sv * kHalf);
    };

    const Vec3 p0 = corner(-1.0f, -1.0f);
    const Vec3 p1 = corner( 1.0f, -1.0f);
    const Vec3 p2 = corner( 1.0f,  1.0f);
    const Vec3 p3 = corner(-1.0f,  1.0f);

    const float u0 = 0.0f, u1 = 1.0f, v0 = 0.0f, v1 = 1.0f;
    AddFace(verts, indices,
            TV(p0, normal, u0, v0), TV(p1, normal, u1, v0),
            TV(p2, normal, u1, v1), TV(p3, normal, u0, v1));
}

void BuildCubeFacesColored(MeshData& mesh) {
    AddCubeFaceColored(mesh.Vertices, mesh.Indices, {0, 0, 1}, {1, 0, 0}, 0.52f, 0.60f, 0.78f);
    AddCubeFaceColored(mesh.Vertices, mesh.Indices, {0, 0, -1}, {-1, 0, 0}, 0.58f, 0.54f, 0.68f);
    AddCubeFaceColored(mesh.Vertices, mesh.Indices, {1, 0, 0}, {0, 0, -1}, 0.62f, 0.48f, 0.48f);
    AddCubeFaceColored(mesh.Vertices, mesh.Indices, {-1, 0, 0}, {0, 0, 1}, 0.48f, 0.58f, 0.58f);
    AddCubeFaceColored(mesh.Vertices, mesh.Indices, {0, 1, 0}, {1, 0, 0}, 0.58f, 0.70f, 0.55f);
    AddCubeFaceColored(mesh.Vertices, mesh.Indices, {0, -1, 0}, {1, 0, 0}, 0.45f, 0.45f, 0.50f);
}

void BuildCubeFacesTextured(TexturedMeshData& mesh) {
    AddCubeFaceTextured(mesh.Vertices, mesh.Indices, {0, 0, 1}, {1, 0, 0});
    AddCubeFaceTextured(mesh.Vertices, mesh.Indices, {0, 0, -1}, {-1, 0, 0});
    AddCubeFaceTextured(mesh.Vertices, mesh.Indices, {1, 0, 0}, {0, 0, -1});
    AddCubeFaceTextured(mesh.Vertices, mesh.Indices, {-1, 0, 0}, {0, 0, 1});
    AddCubeFaceTextured(mesh.Vertices, mesh.Indices, {0, 1, 0}, {1, 0, 0});
    AddCubeFaceTextured(mesh.Vertices, mesh.Indices, {0, -1, 0}, {1, 0, 0});
}

template<typename Vertex>
Vec3 PositionOf(const Vertex& v) {
    return {v.x, v.y, v.z};
}

template<typename Mesh>
bool AllTrianglesFaceOutward(const Mesh& mesh) {
    for (size_t i = 0; i + 2 < mesh.Indices.size(); i += 3) {
        const Vec3 a = PositionOf(mesh.Vertices[mesh.Indices[i + 0]]);
        const Vec3 b = PositionOf(mesh.Vertices[mesh.Indices[i + 1]]);
        const Vec3 c = PositionOf(mesh.Vertices[mesh.Indices[i + 2]]);
        if (!TriangleFacesOutward(a, b, c)) {
            return false;
        }
    }
    return true;
}

} // namespace

MeshData CreateUnitCubeMesh() {
    MeshData mesh;
    BuildCubeFacesColored(mesh);
    return mesh;
}

TexturedMeshData CreateUnitCubeTexturedMesh() {
    TexturedMeshData mesh;
    BuildCubeFacesTextured(mesh);
    return mesh;
}

TexturedMeshData CreateUnitPlaneTexturedMesh() {
    TexturedMeshData mesh;
    const float h = 0.5f;
    mesh.Vertices = {
        {-h, 0.0f, -h, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
        {h, 0.0f, -h, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f},
        {h, 0.0f, h, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f},
        {-h, 0.0f, h, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
    };
    mesh.Indices = {0, 1, 2, 0, 2, 3};
    return mesh;
}

bool ValidateUnitCubeMesh(const MeshData& mesh) {
    if (mesh.Vertices.size() != 24 || mesh.Indices.size() != 36) {
        return false;
    }
    return AllTrianglesFaceOutward(mesh);
}

bool ValidateUnitCubeTexturedMesh(const TexturedMeshData& mesh) {
    if (mesh.Vertices.size() != 24 || mesh.Indices.size() != 36) {
        return false;
    }
    return AllTrianglesFaceOutward(mesh);
}

} // namespace Nova
