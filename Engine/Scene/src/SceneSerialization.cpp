#include <Nova/Scene/SceneSerialization.h>
#include <Nova/Core/Guid.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

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
    case MeshPrimitive::UnitPlane: return "UnitPlane";
    case MeshPrimitive::UnitSphere: return "UnitSphere";
    }
    return "UnitCube";
}

bool MeshPrimitiveFromString(const std::string& s, MeshPrimitive& out) {
    if (s == "UnitCube") {
        out = MeshPrimitive::UnitCube;
        return true;
    }
    if (s == "UnitPlane") {
        out = MeshPrimitive::UnitPlane;
        return true;
    }
    if (s == "UnitSphere") {
        out = MeshPrimitive::UnitSphere;
        return true;
    }
    return false;
}

json EntityToJson(const Scene& scene, Entity entity, const std::unordered_set<uint32_t>* subtreeIds) {
    json j;
    j["name"] = scene.GetName(entity);
    j["transform"] = TransformToJson(scene.GetTransform(entity));
    const Entity parent = scene.GetParent(entity);
    if (parent.IsValid()) {
        const bool parentInSubtree =
            subtreeIds == nullptr || subtreeIds->count(parent.Id) != 0;
        if (parentInSubtree) {
            j["parent"] = scene.GetName(parent);
        }
    }

    if (scene.HasMeshRenderer(entity)) {
        const MeshRendererComponent& mesh = scene.GetMeshRenderer(entity);
        json meshJson = {
            {"primitive", MeshPrimitiveToString(mesh.Primitive)},
            {"receiveShadows", mesh.ReceiveShadows},
        };
        if (!mesh.AssetPath.empty()) {
            meshJson["asset"] = mesh.AssetPath;
        }
        if (!mesh.MeshAssetId.IsNil()) {
            meshJson["assetId"] = mesh.MeshAssetId.ToString();
        }
        meshJson["albedoColor"] = Vec3ToJson(mesh.AlbedoColor);
        if (!mesh.AlbedoTexturePath.empty()) {
            meshJson["albedoTexture"] = mesh.AlbedoTexturePath;
        }
        if (!mesh.AlbedoTextureId.IsNil()) {
            meshJson["albedoTextureId"] = mesh.AlbedoTextureId.ToString();
        }
        meshJson["useAlbedoTexture"] = mesh.UseAlbedoTexture;
        j["meshRenderer"] = meshJson;
    }

    if (scene.HasRotator(entity)) {
        const RotatorComponent& rot = scene.GetRotator(entity);
        j["rotator"] = {
            {"angularVelocity", Vec3ToJson(rot.AngularVelocity)},
            {"localSpace", rot.LocalSpace},
        };
    }

    if (scene.HasMover(entity)) {
        const MoverComponent& mover = scene.GetMover(entity);
        j["mover"] = {{"velocity", Vec3ToJson(mover.Velocity)}};
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

    if (scene.HasPointLight(entity)) {
        const PointLightComponent& light = scene.GetPointLight(entity);
        j["pointLight"] = {
            {"color", Vec3ToJson(light.Color)},
            {"intensity", light.Intensity},
            {"range", light.Range},
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
        if (meshJson.contains("asset")) {
            mesh.AssetPath = meshJson["asset"].get<std::string>();
        }
        if (meshJson.contains("assetId")) {
            if (!Guid::TryParse(meshJson["assetId"].get<std::string>(), mesh.MeshAssetId)) {
                error = "invalid mesh assetId";
                return false;
            }
        }
        if (meshJson.contains("albedoColor")) {
            if (!Vec3FromJson(meshJson["albedoColor"], mesh.AlbedoColor, error)) return false;
        }
        if (meshJson.contains("albedoTexture")) {
            mesh.AlbedoTexturePath = meshJson["albedoTexture"].get<std::string>();
        }
        if (meshJson.contains("albedoTextureId")) {
            if (!Guid::TryParse(meshJson["albedoTextureId"].get<std::string>(),
                                mesh.AlbedoTextureId)) {
                error = "invalid albedoTextureId";
                return false;
            }
        }
        if (meshJson.contains("useAlbedoTexture")) {
            mesh.UseAlbedoTexture = meshJson["useAlbedoTexture"].get<bool>();
        }
        scene.AddMeshRenderer(entity, mesh);
    }

    if (entityJson.contains("rotator")) {
        const json& rotJson = entityJson["rotator"];
        if (!rotJson.is_object()) {
            error = "rotator must be an object";
            return false;
        }
        RotatorComponent rot;
        if (rotJson.contains("angularVelocity")) {
            if (!Vec3FromJson(rotJson["angularVelocity"], rot.AngularVelocity, error)) return false;
        }
        if (rotJson.contains("localSpace")) {
            rot.LocalSpace = rotJson["localSpace"].get<bool>();
        }
        scene.AddRotator(entity, rot);
    }

    if (entityJson.contains("mover")) {
        const json& moverJson = entityJson["mover"];
        if (!moverJson.is_object()) {
            error = "mover must be an object";
            return false;
        }
        MoverComponent mover;
        if (moverJson.contains("velocity")) {
            if (!Vec3FromJson(moverJson["velocity"], mover.Velocity, error)) return false;
        }
        scene.AddMover(entity, mover);
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

    if (entityJson.contains("pointLight")) {
        const json& lightJson = entityJson["pointLight"];
        if (!lightJson.is_object()) {
            error = "pointLight must be an object";
            return false;
        }
        PointLightComponent light;
        if (lightJson.contains("color")) {
            if (!Vec3FromJson(lightJson["color"], light.Color, error)) return false;
        }
        if (lightJson.contains("intensity")) {
            light.Intensity = lightJson["intensity"].get<float>();
        }
        if (lightJson.contains("range")) {
            light.Range = lightJson["range"].get<float>();
        }
        scene.AddPointLight(entity, light);
    }

    return true;
}

struct EntitySnapshot {
    std::string Name;
    std::string ParentName;
    Transform Transform;
    std::optional<MeshRendererComponent> Mesh;
    std::optional<CameraComponent> Camera;
    std::optional<DirectionalLightComponent> Light;
    std::optional<PointLightComponent> PointLight;
    std::optional<RotatorComponent> Rotator;
    std::optional<MoverComponent> Mover;
};

std::vector<EntitySnapshot> SnapshotScene(const Scene& scene) {
    std::vector<EntitySnapshot> snapshots;
    scene.ForEachEntity([&](Entity entity) {
        EntitySnapshot snap;
        snap.Name = scene.GetName(entity);
        const Entity parent = scene.GetParent(entity);
        if (parent.IsValid()) {
            snap.ParentName = scene.GetName(parent);
        }
        snap.Transform = scene.GetTransform(entity);
        if (scene.HasMeshRenderer(entity)) snap.Mesh = scene.GetMeshRenderer(entity);
        if (scene.HasCamera(entity)) snap.Camera = scene.GetCamera(entity);
        if (scene.HasDirectionalLight(entity)) snap.Light = scene.GetDirectionalLight(entity);
        if (scene.HasPointLight(entity)) snap.PointLight = scene.GetPointLight(entity);
        if (scene.HasRotator(entity)) snap.Rotator = scene.GetRotator(entity);
        if (scene.HasMover(entity)) snap.Mover = scene.GetMover(entity);
        snapshots.push_back(std::move(snap));
    });
    return snapshots;
}

void CollectSubtree(const Scene& scene, Entity root, std::vector<Entity>& out) {
    out.push_back(root);
    scene.ForEachEntity([&](Entity entity) {
        if (scene.GetParent(entity).Id == root.Id) {
            CollectSubtree(scene, entity, out);
        }
    });
}

std::string UniqueEntityNameInScene(const Scene& scene, const std::string& base) {
    auto exists = [&](const std::string& name) { return scene.FindEntityByName(name).IsValid(); };
    if (!exists(base)) {
        return base;
    }
    for (int i = 2; i < 10000; ++i) {
        const std::string candidate = base + " " + std::to_string(i);
        if (!exists(candidate)) {
            return candidate;
        }
    }
    return base + " Copy";
}

Entity CopyEntityInto(const Scene& src, Entity source, Scene& dest, const std::string& name) {
    Entity copy = dest.CreateEntity(name);
    dest.GetTransform(copy) = src.GetTransform(source);
    if (src.HasMeshRenderer(source)) {
        dest.AddMeshRenderer(copy, src.GetMeshRenderer(source));
    }
    if (src.HasCamera(source)) {
        CameraComponent cam = src.GetCamera(source);
        cam.IsPrimary = false;
        dest.AddCamera(copy, cam);
    }
    if (src.HasDirectionalLight(source)) {
        dest.AddDirectionalLight(copy, src.GetDirectionalLight(source));
    }
    if (src.HasPointLight(source)) {
        dest.AddPointLight(copy, src.GetPointLight(source));
    }
    if (src.HasRotator(source)) {
        dest.AddRotator(copy, src.GetRotator(source));
    }
    if (src.HasMover(source)) {
        dest.AddMover(copy, src.GetMover(source));
    }
    return copy;
}

} // namespace

std::string SerializeSceneToString(const Scene& scene) {
    json root;
    root["format"] = kSceneFileFormat;
    root["version"] = kSceneFileVersion;
    root["environment"] = {{"clearColor", Vec3ToJson(scene.Settings().ClearColor)}};
    json entities = json::array();
    scene.ForEachEntity([&](Entity entity) { entities.push_back(EntityToJson(scene, entity, nullptr)); });
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
        std::vector<std::pair<std::string, std::string>> parentLinks;
        for (const json& entityJson : root["entities"]) {
            if (!EntityFromJson(entityJson, outScene, result.Error)) {
                outScene.Clear();
                return result;
            }
            if (entityJson.contains("parent") && entityJson["parent"].is_string()) {
                const std::string childName = entityJson["name"].get<std::string>();
                parentLinks.emplace_back(childName, entityJson["parent"].get<std::string>());
            }
        }
        for (const auto& link : parentLinks) {
            const Entity child = outScene.FindEntityByName(link.first);
            const Entity parent = outScene.FindEntityByName(link.second);
            if (!child.IsValid() || !parent.IsValid()) {
                result.Error = "unknown parent entity: " + link.second;
                outScene.Clear();
                return result;
            }
            outScene.SetParent(child, parent);
        }

        if (root.contains("environment") && root["environment"].is_object()) {
            const json& env = root["environment"];
            if (env.contains("clearColor")) {
                if (!Vec3FromJson(env["clearColor"], outScene.Settings().ClearColor, result.Error)) {
                    outScene.Clear();
                    return result;
                }
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

Scene CloneScene(const Scene& source) {
    Scene copy;
    const std::string json = SerializeSceneToString(source);
    SceneIOResult result = DeserializeSceneFromString(json, copy);
    if (!result.Ok) {
        copy.Clear();
    }
    return copy;
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
        if (sa.ParentName != sb.ParentName) return false;
        if (!Vec3Near(sa.Transform.Position, sb.Transform.Position, epsilon)) return false;
        if (!Vec3Near(sa.Transform.Scale, sb.Transform.Scale, epsilon)) return false;
        if (!QuatNear(sa.Transform.Rotation, sb.Transform.Rotation, epsilon)) return false;

        if (static_cast<bool>(sa.Mesh) != static_cast<bool>(sb.Mesh)) return false;
        if (sa.Mesh && (sa.Mesh->Primitive != sb.Mesh->Primitive ||
                        sa.Mesh->ReceiveShadows != sb.Mesh->ReceiveShadows ||
                        sa.Mesh->AssetPath != sb.Mesh->AssetPath ||
                        sa.Mesh->MeshAssetId != sb.Mesh->MeshAssetId ||
                        sa.Mesh->AlbedoTexturePath != sb.Mesh->AlbedoTexturePath ||
                        sa.Mesh->AlbedoTextureId != sb.Mesh->AlbedoTextureId ||
                        sa.Mesh->UseAlbedoTexture != sb.Mesh->UseAlbedoTexture ||
                        !Vec3Near(sa.Mesh->AlbedoColor, sb.Mesh->AlbedoColor, epsilon))) {
            return false;
        }

        if (static_cast<bool>(sa.Rotator) != static_cast<bool>(sb.Rotator)) return false;
        if (sa.Rotator) {
            if (!Vec3Near(sa.Rotator->AngularVelocity, sb.Rotator->AngularVelocity, epsilon)) {
                return false;
            }
            if (sa.Rotator->LocalSpace != sb.Rotator->LocalSpace) return false;
        }

        if (static_cast<bool>(sa.Mover) != static_cast<bool>(sb.Mover)) return false;
        if (sa.Mover && !Vec3Near(sa.Mover->Velocity, sb.Mover->Velocity, epsilon)) {
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

        if (static_cast<bool>(sa.PointLight) != static_cast<bool>(sb.PointLight)) return false;
        if (sa.PointLight) {
            if (!Vec3Near(sa.PointLight->Color, sb.PointLight->Color, epsilon)) return false;
            if (!NearlyEqual(sa.PointLight->Intensity, sb.PointLight->Intensity, epsilon)) {
                return false;
            }
            if (!NearlyEqual(sa.PointLight->Range, sb.PointLight->Range, epsilon)) return false;
        }
    }

    return Vec3Near(a.Settings().ClearColor, b.Settings().ClearColor, epsilon);
}

std::string SerializePrefabToString(const Scene& scene, Entity root) {
    json doc;
    doc["format"] = kPrefabFileFormat;
    doc["version"] = kPrefabFileVersion;
    if (!scene.IsAlive(root)) {
        doc["root"] = "";
        doc["entities"] = json::array();
        return doc.dump(2);
    }

    std::vector<Entity> subtree;
    CollectSubtree(scene, root, subtree);
    std::unordered_set<uint32_t> ids;
    for (const Entity entity : subtree) {
        ids.insert(entity.Id);
    }

    doc["root"] = scene.GetName(root);
    json entities = json::array();
    for (const Entity entity : subtree) {
        entities.push_back(EntityToJson(scene, entity, &ids));
    }
    doc["entities"] = entities;
    return doc.dump(2);
}

SceneIOResult SavePrefabToFile(const Scene& scene, Entity root, const std::filesystem::path& path) {
    SceneIOResult result;
    if (!scene.IsAlive(root)) {
        result.Error = "invalid prefab root";
        return result;
    }
    std::ofstream file(path);
    if (!file.is_open()) {
        result.Error = "failed to open file for writing";
        return result;
    }
    file << SerializePrefabToString(scene, root);
    if (!file.good()) {
        result.Error = "failed while writing prefab file";
        return result;
    }
    result.Ok = true;
    return result;
}

SceneIOResult InstantiatePrefabFromString(const std::string& jsonText, Scene& dest, Entity& outRoot) {
    SceneIOResult result;
    outRoot = Entity{};
    try {
        json root = json::parse(jsonText);
        if (!root.contains("format") || root["format"].get<std::string>() != kPrefabFileFormat) {
            result.Error = "unsupported prefab format";
            return result;
        }
        if (!root.contains("version") || root["version"].get<int>() != kPrefabFileVersion) {
            result.Error = "unsupported prefab version";
            return result;
        }
        if (!root.contains("entities") || !root["entities"].is_array()) {
            result.Error = "entities must be an array";
            return result;
        }

        Scene temp;
        std::vector<std::pair<std::string, std::string>> parentLinks;
        for (const json& entityJson : root["entities"]) {
            if (!EntityFromJson(entityJson, temp, result.Error)) {
                return result;
            }
            if (entityJson.contains("parent") && entityJson["parent"].is_string()) {
                parentLinks.emplace_back(entityJson["name"].get<std::string>(),
                                         entityJson["parent"].get<std::string>());
            }
        }
        for (const auto& link : parentLinks) {
            const Entity child = temp.FindEntityByName(link.first);
            const Entity parent = temp.FindEntityByName(link.second);
            if (!child.IsValid() || !parent.IsValid()) {
                result.Error = "unknown parent entity: " + link.second;
                return result;
            }
            temp.SetParent(child, parent);
        }

        std::string rootName;
        if (root.contains("root") && root["root"].is_string()) {
            rootName = root["root"].get<std::string>();
        }
        Entity tempRoot = temp.FindEntityByName(rootName);
        if (!tempRoot.IsValid() && temp.EntityCount() > 0) {
            temp.ForEachEntity([&](Entity entity) {
                if (!tempRoot.IsValid() && !temp.GetParent(entity).IsValid()) {
                    tempRoot = entity;
                }
            });
        }
        if (!tempRoot.IsValid()) {
            result.Error = "prefab has no root entity";
            return result;
        }

        std::unordered_map<std::string, Entity> nameMap;
        temp.ForEachEntity([&](Entity entity) {
            const std::string original = temp.GetName(entity);
            const std::string unique = UniqueEntityNameInScene(dest, original);
            nameMap[original] = CopyEntityInto(temp, entity, dest, unique);
        });

        for (const auto& link : parentLinks) {
            dest.SetParent(nameMap[link.first], nameMap[link.second]);
        }

        const std::string originalRootName = temp.GetName(tempRoot);
        outRoot = nameMap[originalRootName];
        result.Ok = true;
        return result;
    } catch (const json::exception& ex) {
        result.Error = ex.what();
        return result;
    }
}

SceneIOResult InstantiatePrefabFromFile(const std::filesystem::path& path, Scene& dest, Entity& outRoot) {
    SceneIOResult result;
    std::ifstream file(path);
    if (!file.is_open()) {
        result.Error = "failed to open file for reading";
        return result;
    }
    const std::string contents((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    return InstantiatePrefabFromString(contents, dest, outRoot);
}

} // namespace Nova
