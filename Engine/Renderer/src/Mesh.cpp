#include <Nova/Renderer/Mesh.h>

namespace Nova {

namespace {

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

ColoredVertex V(float x, float y, float z, float r, float g, float b) {
    return {x, y, z, r, g, b, 1.0f};
}

} // namespace

MeshData CreateUnitCubeMesh() {
    // 24 vertices (4 per face), one calm color per face — no corner color bleeding.
    // Muted palette, similar brightness — easier to read shape while spinning.
    const float r = 0.62f, g = 0.48f, b = 0.48f; // +X right — dusty rose
    const float l = 0.48f, t = 0.58f, u = 0.58f; // -X left — slate teal
    const float yu = 0.58f, yv = 0.70f, yw = 0.55f; // +Y top — sage
    const float yd = 0.45f, ye = 0.45f, yf = 0.50f; // -Y bottom — cool gray
    const float fz = 0.52f, fy = 0.60f, fx = 0.78f; // +Z front — soft blue
    const float bz = 0.58f, by = 0.54f, bx = 0.68f; // -Z back — soft purple

    MeshData mesh;

    // +Z front
    AddFace(mesh.Vertices, mesh.Indices,
            V(-0.5f, -0.5f,  0.5f, fz, fy, fx),
            V( 0.5f, -0.5f,  0.5f, fz, fy, fx),
            V( 0.5f,  0.5f,  0.5f, fz, fy, fx),
            V(-0.5f,  0.5f,  0.5f, fz, fy, fx));

    // -Z back
    AddFace(mesh.Vertices, mesh.Indices,
            V( 0.5f, -0.5f, -0.5f, bz, by, bx),
            V(-0.5f, -0.5f, -0.5f, bz, by, bx),
            V(-0.5f,  0.5f, -0.5f, bz, by, bx),
            V( 0.5f,  0.5f, -0.5f, bz, by, bx));

    // +X right
    AddFace(mesh.Vertices, mesh.Indices,
            V(0.5f, -0.5f,  0.5f, r, g, b),
            V(0.5f, -0.5f, -0.5f, r, g, b),
            V(0.5f,  0.5f, -0.5f, r, g, b),
            V(0.5f,  0.5f,  0.5f, r, g, b));

    // -X left
    AddFace(mesh.Vertices, mesh.Indices,
            V(-0.5f, -0.5f, -0.5f, l, t, u),
            V(-0.5f, -0.5f,  0.5f, l, t, u),
            V(-0.5f,  0.5f,  0.5f, l, t, u),
            V(-0.5f,  0.5f, -0.5f, l, t, u));

    // +Y top
    AddFace(mesh.Vertices, mesh.Indices,
            V(-0.5f, 0.5f,  0.5f, yu, yv, yw),
            V( 0.5f, 0.5f,  0.5f, yu, yv, yw),
            V( 0.5f, 0.5f, -0.5f, yu, yv, yw),
            V(-0.5f, 0.5f, -0.5f, yu, yv, yw));

    // -Y bottom
    AddFace(mesh.Vertices, mesh.Indices,
            V(-0.5f, -0.5f, -0.5f, yd, ye, yf),
            V( 0.5f, -0.5f, -0.5f, yd, ye, yf),
            V( 0.5f, -0.5f,  0.5f, yd, ye, yf),
            V(-0.5f, -0.5f,  0.5f, yd, ye, yf));

    return mesh;
}

} // namespace Nova
