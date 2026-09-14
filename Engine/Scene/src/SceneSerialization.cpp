#include <Nova/Scene/SceneSerialization.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>

namespace Nova {

namespace {

using json = nlohmann::json;

bool NearlyEqual(float a, float b, float eps) {
    return std::fabs(a - b) <= eps;
}

bool Vec3Near(const Vec3& a, const Vec3& b, float eps) {
    return NearlyEqual(a.x, b.x, eps) && NearlyEqual(a.y, b.y, eps) &&
           NearlyEqual(a.z, b.z, eps);
}

bool QuatNear(const Quat& a, const Quat& b, float eps) {
    const float dot = std::fabs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
    const float dotNeg = std::fabs(a.x * -b.x + a.y * -b.y + a.z * -b.z + a.w * -b.w);
    return (1.0f - std::max(dot, dotNeg)) <= eps;
}

json Vec3ToJson(const Vec3& v) {
    return json::array({v.x, v.y, v.z});
}

bool Vec3FromJson(const json& j, Vec3& out, std::string& error) {
    if (!j.is_array() || j.size() != 3) {
        error = "vec3 must be a 3-element array";
        return false;
    }
    out.x = j[0].get<float>();
    out.y = j[1].get<float>();
    out.z = j[2].get<float>();
    return true;
}

json QuatToJson(const Quat& q) {
    return json::array({q.x, q.y, q.z, q.w});
}

bool QuatFromJson(const json& j, Quat& out, std::string& error) {
    if (!j.is_array() || j.size() != 4) {
        error = "quat must be a 4-element array";
        return false;
    }
    out.x = j[0].get<float>();
    out.y = j[1].get<float>();
    out.z = j[2].get<float>();
    out.w = j[3].get<float>();
    if (!std::isfinite(out.x) || !std::isfinite(out.y) || !std::isfinite(out.z) ||
        !std::isfinite(out.w)) {
        error = "quat contains non-finite values";
        return false;
    }
    const float len = out.Length();
    if (len < 1e-6f) {
        error = "quat length is zero";
        return false;
    }
    out = out.Normalized();
    return true;
}

json TransformToJson(const Transform& t) {
    json j;
    j["position"] = Vec3ToJson(t.Position);
    j["rotation"] = QuatToJson(t.Rotation);
    j["scale"] = Vec3ToJson(t.Scale);
    return j;
}

bool TransformFromJson(const json& j, Transform& out, std::string& error) {
    if (!j.is_object()) {
        error = "transform must be an object";
        return false;
    }
    if (j.contains("position")) {
        if (!Vec3FromJson(j["position"], out.Position, error)) return false;
    }
    if (j.contains("rotation")) {
        if (!QuatFromJson(j["rotation"], out.Rotation, error)) return false;
    }
    if (j.contains("scale")) {
        if (!Vec3FromJson(j["scale"], out.Scale, error)) return false;
    }
    return true;
}

const char* MeshPrimitiveToString(MeshPrimitive p) {
    switch (p) {
    case MeshPrimitive::UnitCube: return "UnitCube";
    }
    return "UnitCube";
}

bool MeshPrimitiveFromString(const std::string& s, MeshPrimitive& out) {
    if (s == "UnitCube") {
        out = MeshPrimitive::UnitCube;
        return true;
    }
    return false;
}

json EntityToJson(const Scene& scene, Entity entity) {
    json j;
    j["name"] = scene.GetName(entity);
    j["transform"] = TransformToJson(scene.GetTransform(entity));

    if (scene.HasMeshRenderer(entity)) {
        const MeshRendererComponent& mesh = scene.GetMeshRenderer(entity);
        j["meshRenderer"] = {
            {"primitive", MeshPrimitiveToString(mesh.Primitive)},
            {"receiveShadows", mesh.ReceiveShadows},
        };
    }

    if (scene.HasCamera(entity)) {
        const CameraComponent& cam = scene.GetCamera(entity);
        j["camera"] = {
            {"primary", cam.IsPrimary},
            {"lookAt", Vec3ToJson(cam.LookAtTarget)},
            {"fovYRadians", cam.FovYRadians},
            {"near", cam.NearPlane},
            {"far", cam.FarPlane},
        };
    }

    if (scene.HasDirectionalLight(entity)) {
        const DirectionalLightComponent& light = scene.GetDirectionalLight(entity);
        j["directionalLight"] = {
            {"direction", Vec3ToJson(light.Direction)},
            {"color", Vec3ToJson(light.Color)},
            {"ambient", light.Ambient},
        };
    }

    return j;
}

bool EntityFromJson(const json& entityJson, Scene& scene, std::string& error) {
    if (!entityJson.is_object()) {
        error = "entity must be an object";
        return false;
    }
    if (!entityJson.contains("name") || !entityJson["name"].is_string()) {
        error = "entity missing name";
        return false;
    }

    Entity entity = scene.CreateEntity(entityJson["name"].get<std::string>());

    if (entityJson.contains("transform")) {
        if (!TransformFromJson(entityJson["transform"], scene.GetTransform(entity), error)) {
            return false;
        }
    }

    if (entityJson.contains("meshRenderer")) {
        const json& meshJson = entityJson["meshRenderer"];
        if (!meshJson.is_object()) {
            error = "meshRenderer must be an object";
            return false;
        }
        MeshRendererComponent mesh;
        if (meshJson.contains("primitive")) {
            if (!MeshPrimitiveFromString(meshJson["primitive"].get<std::string>(),
                                         mesh.Primitive)) {
                error = "unknown mesh primitive";
                return false;
            }
        }
        if (meshJson.contains("receiveShadows")) {
            mesh.ReceiveShadows = meshJson["receiveShadows"].get<bool>();
        }
        scene.AddMeshRenderer(entity, mesh);
    }

    if (entityJson.contains("camera")) {
        const json& camJson = entityJson["camera"];
        if (!camJson.is_object()) {
            error = "camera must be an object";
            return false;
        }
        CameraComponent cam;
        if (camJson.contains("primary")) {
            cam.IsPrimary = camJson["primary"].get<bool>();
        }
        if (camJson.contains("lookAt")) {
            if (!Vec3FromJson(camJson["lookAt"], cam.LookAtTarget, error)) return false;
        }
        if (camJson.contains("fovYRadians")) {
            cam.FovYRadians = camJson["fovYRadians"].get<float>();
        }
        if (camJson.contains("near")) {
            cam.NearPlane = camJson["near"].get<float>();
        }
        if (camJson.contains("far")) {
            cam.FarPlane = camJson["far"].get<float>();
        }
        scene.AddCamera(entity, cam);
    }

    if (entityJson.contains("directionalLight")) {
        const json& lightJson = entityJson["directionalLight"];
        if (!lightJson.is_object()) {
            error = "directionalLight must be an object";
            return false;
        }
        DirectionalLightComponent light;
        if (lightJson.contains("direction")) {
            if (!Vec3FromJson(lightJson["direction"], light.Direction, error)) return false;
            light.Direction = light.Direction.Normalized();
        }
        if (lightJson.contains("color")) {
            if (!Vec3FromJson(lightJson["color"], light.Color, error)) return false;
        }
        if (lightJson.contains("ambient")) {
            light.Ambient = lightJson["ambient"].get<float>();
        }
        scene.AddDirectionalLight(entity, light);
    }

    return true;
}

struct EntitySnapshot {
    std::string Name;
    Transform Transform;
    std::optional<MeshRendererComponent> Mesh;
    std::optional<CameraComponent> Camera;
    std::optional<DirectionalLightComponent> Light;
};

std::vector<EntitySnapshot> SnapshotScene(const Scene& scene) {
    std::vector<EntitySnapshot> snapshots;
    scene.ForEachEntity([&](Entity entity) {
        EntitySnapshot snap;
        snap.Name = scene.GetName(entity);
        snap.Transform = scene.GetTransform(entity);
        if (scene.HasMeshRenderer(entity)) snap.Mesh = scene.GetMeshRenderer(entity);
        if (scene.HasCamera(entity)) snap.Camera = scene.GetCamera(entity);
        if (scene.HasDirectionalLight(entity)) snap.Light = scene.GetDirectionalLight(entity);
        snapshots.push_back(std::move(snap));
    });
    return snapshots;
}

} // namespace

std::string SerializeSceneToString(const Scene& scene) {
    json root;
    root["format"] = kSceneFileFormat;
    root["version"] = kSceneFileVersion;
    json entities = json::array();
    scene.ForEachEntity([&](Entity entity) { entities.push_back(EntityToJson(scene, entity)); });
    root["entities"] = entities;
    return root.dump(2);
}

SceneIOResult DeserializeSceneFromString(const std::string& jsonText, Scene& outScene) {
    SceneIOResult result;
    try {
        json root = json::parse(jsonText);

        if (!root.contains("format") || root["format"].get<std::string>() != kSceneFileFormat) {
            result.Error = "unsupported scene format";
            return result;
        }
        if (!root.contains("version") || !root["version"].is_number_integer()) {
            result.Error = "missing scene version";
            return result;
        }
        const int version = root["version"].get<int>();
        if (version != kSceneFileVersion) {
            result.Error = "unsupported scene version";
            return result;
        }
        if (!root.contains("entities") || !root["entities"].is_array()) {
            result.Error = "entities must be an array";
            return result;
        }

        outScene.Clear();
        for (const json& entityJson : root["entities"]) {
            if (!EntityFromJson(entityJson, outScene, result.Error)) {
                outScene.Clear();
                return result;
            }
        }

        result.Ok = true;
        return result;
    } catch (const json::exception& ex) {
        result.Error = ex.what();
        outScene.Clear();
        return result;
    }
}

SceneIOResult SaveSceneToFile(const Scene& scene, const std::filesystem::path& path) {
    SceneIOResult result;
    std::ofstream file(path);
    if (!file.is_open()) {
        result.Error = "failed to open file for writing";
        return result;
    }
    file << SerializeSceneToString(scene);
    if (!file.good()) {
        result.Error = "failed while writing scene file";
        return result;
    }
    result.Ok = true;
    return result;
}

SceneIOResult LoadSceneFromFile(const std::filesystem::path& path, Scene& outScene) {
    SceneIOResult result;
    std::ifstream file(path);
    if (!file.is_open()) {
        result.Error = "failed to open file for reading";
        return result;
    }
    std::string contents((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    if (!file.good() && !file.eof()) {
        result.Error = "failed while reading scene file";
        return result;
    }
    return DeserializeSceneFromString(contents, outScene);
}

bool ScenesEquivalent(const Scene& a, const Scene& b, float epsilon) {
    if (a.EntityCount() != b.EntityCount()) {
        return false;
    }

    auto snapsA = SnapshotScene(a);
    auto snapsB = SnapshotScene(b);
    if (snapsA.size() != snapsB.size()) {
        return false;
    }

    auto byName = [](const EntitySnapshot& s) { return s.Name; };
    std::sort(snapsA.begin(), snapsA.end(),
              [&](const EntitySnapshot& x, const EntitySnapshot& y) {
                  return byName(x) < byName(y);
              });
    std::sort(snapsB.begin(), snapsB.end(),
              [&](const EntitySnapshot& x, const EntitySnapshot& y) {
                  return byName(x) < byName(y);
              });

    for (size_t i = 0; i < snapsA.size(); ++i) {
        const EntitySnapshot& sa = snapsA[i];
        const EntitySnapshot& sb = snapsB[i];
        if (sa.Name != sb.Name) return false;
        if (!Vec3Near(sa.Transform.Position, sb.Transform.Position, epsilon)) return false;
        if (!Vec3Near(sa.Transform.Scale, sb.Transform.Scale, epsilon)) return false;
        if (!QuatNear(sa.Transform.Rotation, sb.Transform.Rotation, epsilon)) return false;

        if (static_cast<bool>(sa.Mesh) != static_cast<bool>(sb.Mesh)) return false;
        if (sa.Mesh && (sa.Mesh->Primitive != sb.Mesh->Primitive ||
                        sa.Mesh->ReceiveShadows != sb.Mesh->ReceiveShadows)) {
            return false;
        }

        if (static_cast<bool>(sa.Camera) != static_cast<bool>(sb.Camera)) return false;
        if (sa.Camera) {
            if (sa.Camera->IsPrimary != sb.Camera->IsPrimary) return false;
            if (!Vec3Near(sa.Camera->LookAtTarget, sb.Camera->LookAtTarget, epsilon)) {
                return false;
            }
            if (!NearlyEqual(sa.Camera->FovYRadians, sb.Camera->FovYRadians, epsilon)) {
                return false;
            }
            if (!NearlyEqual(sa.Camera->NearPlane, sb.Camera->NearPlane, epsilon)) return false;
            if (!NearlyEqual(sa.Camera->FarPlane, sb.Camera->FarPlane, epsilon)) return false;
        }

        if (static_cast<bool>(sa.Light) != static_cast<bool>(sb.Light)) return false;
        if (sa.Light) {
            if (!Vec3Near(sa.Light->Direction, sb.Light->Direction, epsilon)) return false;
            if (!Vec3Near(sa.Light->Color, sb.Light->Color, epsilon)) return false;
            if (!NearlyEqual(sa.Light->Ambient, sb.Light->Ambient, epsilon)) return false;
        }
    }

    return true;
}

} // namespace Nova
