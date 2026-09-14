#include <Nova/Assets/ObjLoader.h>

#include <Nova/Math/Vec.h>

#include <fstream>
#include <sstream>
namespace Nova {

namespace {

Vec3 ReadVec3(const std::string& line, size_t startIndex) {
    std::istringstream iss(line.substr(startIndex));
    Vec3 v;
    iss >> v.x >> v.y >> v.z;
    return v;
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
    std::string line;
    while (std::getline(in, line)) {
        if (line.size() < 2 || line[0] == '#') {
            continue;
        }
        if (line.rfind("v ", 0) == 0) {
            positions.push_back(ReadVec3(line, 2));
        } else if (line.rfind("f ", 0) == 0) {
            std::istringstream iss(line.substr(2));
            std::vector<uint32_t> face;
            std::string token;
            while (iss >> token) {
                const size_t slash = token.find('/');
                const int idx = std::stoi(slash == std::string::npos ? token : token.substr(0, slash));
                const int resolved = idx < 0 ? static_cast<int>(positions.size()) + idx + 1 : idx;
                if (resolved < 1 || resolved > static_cast<int>(positions.size())) {
                    result.Error = "face index out of range";
                    return result;
                }
                face.push_back(static_cast<uint32_t>(resolved - 1));
            }
            if (face.size() < 3) {
                continue;
            }
            for (size_t i = 1; i + 1 < face.size(); ++i) {
                const Vec3 a = positions[face[0]];
                const Vec3 b = positions[face[i]];
                const Vec3 c = positions[face[i + 1]];
                const Vec3 n = (b - a).Cross(c - a).Normalized();

                const auto pushCorner = [&](const Vec3& p) {
                    TexturedVertex v;
                    v.x = p.x;
                    v.y = p.y;
                    v.z = p.z;
                    v.nx = n.x;
                    v.ny = n.y;
                    v.nz = n.z;
                    v.u = 0.0f;
                    v.v = 0.0f;
                    result.Mesh.Vertices.push_back(v);
                    result.Mesh.Indices.push_back(static_cast<uint32_t>(result.Mesh.Indices.size()));
                };
                pushCorner(a);
                pushCorner(b);
                pushCorner(c);
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
