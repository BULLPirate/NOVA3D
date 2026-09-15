#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include <Nova/Assets/GltfLoader.h>

#include <Nova/Math/Vec.h>

#include <cstring>
#include <vector>

namespace Nova {

namespace {

Vec3 ReadVec3(const float* data) { return {data[0], data[1], data[2]}; }

void GenerateFlatNormal(const Vec3& a, const Vec3& b, const Vec3& c, float& nx, float& ny, float& nz) {
    const Vec3 n = (b - a).Cross(c - a).Normalized();
    nx = n.x;
    ny = n.y;
    nz = n.z;
}

bool AppendPrimitive(const cgltf_primitive& primitive, TexturedMeshData& out, std::string& error) {
    if (primitive.type != cgltf_primitive_type_triangles) {
        return true;
    }

    const cgltf_accessor* posAccessor = nullptr;
    const cgltf_accessor* normAccessor = nullptr;
    const cgltf_accessor* uvAccessor = nullptr;

    for (size_t i = 0; i < primitive.attributes_count; ++i) {
        const cgltf_attribute& attr = primitive.attributes[i];
        if (attr.type == cgltf_attribute_type_position) {
            posAccessor = attr.data;
        } else if (attr.type == cgltf_attribute_type_normal) {
            normAccessor = attr.data;
        } else if (attr.type == cgltf_attribute_type_texcoord) {
            uvAccessor = attr.data;
        }
    }

    if (!posAccessor || posAccessor->type != cgltf_type_vec3) {
        error = "gltf primitive missing POSITION vec3";
        return false;
    }

    if (primitive.indices) {
        const cgltf_accessor* indices = primitive.indices;
        const size_t indexCount = indices->count;
        std::vector<float> positions(posAccessor->count * 3);
        std::vector<float> normals;
        if (normAccessor) {
            normals.resize(normAccessor->count * 3);
            cgltf_accessor_unpack_floats(normAccessor, normals.data(), normals.size());
        }
        std::vector<float> uvs;
        if (uvAccessor) {
            uvs.resize(uvAccessor->count * 2);
            cgltf_accessor_unpack_floats(uvAccessor, uvs.data(), uvs.size());
        }

        cgltf_accessor_unpack_floats(posAccessor, positions.data(), positions.size());

        for (size_t i = 0; i + 2 < indexCount; i += 3) {
            const cgltf_size i0 = cgltf_accessor_read_index(indices, i + 0);
            const cgltf_size i1 = cgltf_accessor_read_index(indices, i + 1);
            const cgltf_size i2 = cgltf_accessor_read_index(indices, i + 2);

            const Vec3 p0 = ReadVec3(&positions[i0 * 3]);
            const Vec3 p1 = ReadVec3(&positions[i1 * 3]);
            const Vec3 p2 = ReadVec3(&positions[i2 * 3]);

            float fnx = 0.0f, fny = 0.0f, fnz = 1.0f;
            if (!normAccessor) {
                GenerateFlatNormal(p0, p1, p2, fnx, fny, fnz);
            }

            const auto emitCorner = [&](cgltf_size srcIndex, const Vec3& pos) {
                TexturedVertex v;
                v.x = pos.x;
                v.y = pos.y;
                v.z = pos.z;
                if (normAccessor && srcIndex < normAccessor->count) {
                    v.nx = normals[srcIndex * 3 + 0];
                    v.ny = normals[srcIndex * 3 + 1];
                    v.nz = normals[srcIndex * 3 + 2];
                } else {
                    v.nx = fnx;
                    v.ny = fny;
                    v.nz = fnz;
                }
                if (uvAccessor && srcIndex < uvAccessor->count) {
                    v.u = uvs[srcIndex * 2 + 0];
                    v.v = uvs[srcIndex * 2 + 1];
                } else {
                    v.u = 0.0f;
                    v.v = 0.0f;
                }
                out.Vertices.push_back(v);
                out.Indices.push_back(static_cast<uint32_t>(out.Indices.size()));
            };

            emitCorner(i0, p0);
            emitCorner(i1, p1);
            emitCorner(i2, p2);
        }
    } else {
        if (posAccessor->count % 3 != 0) {
            error = "non-indexed primitive count is not a multiple of 3";
            return false;
        }
        std::vector<float> positions(posAccessor->count * 3);
        cgltf_accessor_unpack_floats(posAccessor, positions.data(), positions.size());
        for (size_t i = 0; i < posAccessor->count; i += 3) {
            const Vec3 p0 = ReadVec3(&positions[(i + 0) * 3]);
            const Vec3 p1 = ReadVec3(&positions[(i + 1) * 3]);
            const Vec3 p2 = ReadVec3(&positions[(i + 2) * 3]);
            float fnx = 0.0f, fny = 0.0f, fnz = 1.0f;
            GenerateFlatNormal(p0, p1, p2, fnx, fny, fnz);
            const auto emit = [&](const Vec3& p) {
                TexturedVertex v{p.x, p.y, p.z, fnx, fny, fnz, 0.0f, 0.0f};
                out.Vertices.push_back(v);
                out.Indices.push_back(static_cast<uint32_t>(out.Indices.size()));
            };
            emit(p0);
            emit(p1);
            emit(p2);
        }
    }

    return true;
}

} // namespace

AssetLoadResult LoadGltfMesh(const std::filesystem::path& path) {
    AssetLoadResult result;
    cgltf_options options{};
    cgltf_data* data = nullptr;

    const std::string pathUtf8 = path.string();
    cgltf_result parseResult = cgltf_parse_file(&options, pathUtf8.c_str(), &data);
    if (parseResult != cgltf_result_success || !data) {
        result.Error = "cgltf_parse_file failed";
        return result;
    }

    parseResult = cgltf_load_buffers(&options, data, pathUtf8.c_str());
    if (parseResult != cgltf_result_success) {
        cgltf_free(data);
        result.Error = "cgltf_load_buffers failed";
        return result;
    }

    for (size_t m = 0; m < data->meshes_count; ++m) {
        const cgltf_mesh& mesh = data->meshes[m];
        for (size_t p = 0; p < mesh.primitives_count; ++p) {
            if (!AppendPrimitive(mesh.primitives[p], result.Mesh, result.Error)) {
                cgltf_free(data);
                return result;
            }
        }
    }

    cgltf_free(data);

    if (result.Mesh.Vertices.empty()) {
        result.Error = "gltf contained no triangle geometry";
        return result;
    }

    result.Ok = true;
    return result;
}

} // namespace Nova
