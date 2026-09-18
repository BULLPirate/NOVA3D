#include <Nova/Scene/ScenePicking.h>

#include <Nova/Scene/Scene.h>
#include <Nova/Math/Mat4.h>

#include <limits>
#include <cmath>

namespace Nova {

bool RayIntersectsAabb(const Ray& ray, const Aabb& box, float& outDistance) {
    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::max();
    const Vec3 origin = ray.Origin;
    const Vec3 dir = ray.Direction;
    const float bounds[2][3] = {
        {box.Min.x, box.Min.y, box.Min.z},
        {box.Max.x, box.Max.y, box.Max.z},
    };
    const float originA[3] = {origin.x, origin.y, origin.z};
    const float dirA[3] = {dir.x, dir.y, dir.z};

    for (int i = 0; i < 3; ++i) {
        if (std::fabs(dirA[i]) < 1e-8f) {
            if (originA[i] < bounds[0][i] || originA[i] > bounds[1][i]) {
                return false;
            }
            continue;
        }
        float t0 = (bounds[0][i] - originA[i]) / dirA[i];
        float t1 = (bounds[1][i] - originA[i]) / dirA[i];
        if (t0 > t1) {
            const float tmp = t0;
            t0 = t1;
            t1 = tmp;
        }
        tMin = t0 > tMin ? t0 : tMin;
        tMax = t1 < tMax ? t1 : tMax;
        if (tMax < tMin) {
            return false;
        }
    }

    outDistance = tMin;
    return tMax >= 0.0f;
}

Aabb LocalBoundsForMesh(const MeshRendererComponent& mesh) {
    if (!mesh.AssetPath.empty()) {
        return {{-0.5f, 0.0f, -0.5f}, {0.7f, 2.0f, 0.55f}};
    }
    if (mesh.Primitive == MeshPrimitive::UnitPlane) {
        return {{-0.5f, -0.02f, -0.5f}, {0.5f, 0.02f, 0.5f}};
    }
    return {{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
}

Aabb WorldAabbFromLocal(const Mat4& world, const Aabb& local) {
    const Vec3 corners[8] = {
        {local.Min.x, local.Min.y, local.Min.z}, {local.Max.x, local.Min.y, local.Min.z},
        {local.Min.x, local.Max.y, local.Min.z}, {local.Max.x, local.Max.y, local.Min.z},
        {local.Min.x, local.Min.y, local.Max.z}, {local.Max.x, local.Min.y, local.Max.z},
        {local.Min.x, local.Max.y, local.Max.z}, {local.Max.x, local.Max.y, local.Max.z},
    };
    Aabb worldBox;
    worldBox.Min = {1e9f, 1e9f, 1e9f};
    worldBox.Max = {-1e9f, -1e9f, -1e9f};
    for (const Vec3& c : corners) {
        const Vec3 p = world.TransformPoint(c);
        worldBox.Min.x = p.x < worldBox.Min.x ? p.x : worldBox.Min.x;
        worldBox.Min.y = p.y < worldBox.Min.y ? p.y : worldBox.Min.y;
        worldBox.Min.z = p.z < worldBox.Min.z ? p.z : worldBox.Min.z;
        worldBox.Max.x = p.x > worldBox.Max.x ? p.x : worldBox.Max.x;
        worldBox.Max.y = p.y > worldBox.Max.y ? p.y : worldBox.Max.y;
        worldBox.Max.z = p.z > worldBox.Max.z ? p.z : worldBox.Max.z;
    }
    return worldBox;
}

Entity PickSceneMesh(const Scene& scene, const Ray& worldRay) {
    Entity best{};
    float bestDist = std::numeric_limits<float>::max();
    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasMeshRenderer(entity)) {
            return;
        }
        const Aabb local = LocalBoundsForMesh(scene.GetMeshRenderer(entity));
        const Aabb world = WorldAabbFromLocal(scene.GetWorldMatrix(entity), local);
        float dist = 0.0f;
        if (!RayIntersectsAabb(worldRay, world, dist) || dist < 0.0f) {
            return;
        }
        if (dist < bestDist) {
            bestDist = dist;
            best = entity;
        }
    });
    return best;
}

bool RayHitYPlane(const Ray& ray, float planeY, Vec3& outHit) {
    if (std::fabs(ray.Direction.y) < 1e-6f) {
        return false;
    }
    const float t = (planeY - ray.Origin.y) / ray.Direction.y;
    if (t < 0.0f) {
        return false;
    }
    outHit = ray.Origin + ray.Direction * t;
    return true;
}

} // namespace Nova
