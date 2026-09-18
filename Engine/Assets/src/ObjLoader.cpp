#include <Nova/Assets/ObjLoader.h>

#include <Nova/Math/Vec.h>

#include <fstream>
#include <sstream>
#include <vector>

namespace Nova {

namespace {

struct FaceCorner {
    int Position = 0;
    int TexCoord = 0;
};

int ResolveIndex(int idx, int count) {
    if (idx < 0) {
        return count + idx;
    }
    return idx - 1;
}

} // namespace

AssetLoadResult LoadObjMesh(const std::filesystem::path& path) {
    AssetLoadResult result;
    std::ifstream in(path);
    if (!in) {
        result.Error = "cannot open obj file";
        return result;
    }

    std::vector<Vec3> positions;
    std::vector<Vec3> uvs;
    std::string line;
    while (std::getline(in, line)) {
        if (line.size() < 2 || line[0] == '#') {
            continue;
        }
        if (line.rfind("v ", 0) == 0) {
            std::istringstream iss(line.substr(2));
            Vec3 v;
            iss >> v.x >> v.y >> v.z;
            positions.push_back(v);
        } else if (line.rfind("vt ", 0) == 0) {
            std::istringstream iss(line.substr(3));
            Vec3 t;
            iss >> t.x >> t.y;
            uvs.push_back(t);
        } else if (line.rfind("f ", 0) == 0) {
            std::istringstream iss(line.substr(2));
            std::vector<FaceCorner> face;
            std::string token;
            while (iss >> token) {
                FaceCorner corner;
                const size_t first = token.find('/');
                if (first == std::string::npos) {
                    corner.Position = std::stoi(token);
                } else {
                    corner.Position = std::stoi(token.substr(0, first));
                    const size_t second = token.find('/', first + 1);
                    const std::string uvPart = token.substr(first + 1, second == std::string::npos
                                                                          ? std::string::npos
                                                                          : second - first - 1);
                    if (!uvPart.empty()) {
                        corner.TexCoord = std::stoi(uvPart);
                    }
                }
                const int pos = ResolveIndex(corner.Position, static_cast<int>(positions.size()));
                if (pos < 0 || pos >= static_cast<int>(positions.size())) {
                    result.Error = "face index out of range";
                    return result;
                }
                corner.Position = pos;
                if (corner.TexCoord != 0) {
                    const int uv = ResolveIndex(corner.TexCoord, static_cast<int>(uvs.size()));
                    corner.TexCoord = (uv >= 0 && uv < static_cast<int>(uvs.size())) ? uv + 1 : 0;
                }
                face.push_back(corner);
            }
            if (face.size() < 3) {
                continue;
            }
            for (size_t i = 1; i + 1 < face.size(); ++i) {
                const Vec3 a = positions[static_cast<size_t>(face[0].Position)];
                const Vec3 b = positions[static_cast<size_t>(face[i].Position)];
                const Vec3 c = positions[static_cast<size_t>(face[i + 1].Position)];
                const Vec3 n = (b - a).Cross(c - a).Normalized();

                const auto emit = [&](const FaceCorner& corner, const Vec3& p) {
                    TexturedVertex v;
                    v.x = p.x;
                    v.y = p.y;
                    v.z = p.z;
                    v.nx = n.x;
                    v.ny = n.y;
                    v.nz = n.z;
                    v.u = 0.0f;
                    v.v = 0.0f;
                    if (corner.TexCoord > 0) {
                        const Vec3& t = uvs[static_cast<size_t>(corner.TexCoord - 1)];
                        v.u = t.x;
                        v.v = t.y;
                    }
                    result.Mesh.Vertices.push_back(v);
                    result.Mesh.Indices.push_back(static_cast<uint32_t>(result.Mesh.Indices.size()));
                };
                emit(face[0], a);
                emit(face[i], b);
                emit(face[i + 1], c);
            }
        }
    }

    if (result.Mesh.Vertices.empty()) {
        result.Error = "obj contained no triangles";
        return result;
    }

    result.Ok = true;
    return result;
}

} // namespace Nova
