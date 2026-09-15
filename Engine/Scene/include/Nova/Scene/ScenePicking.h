#pragma once

#include <Nova/Scene/Entity.h>
#include <Nova/Scene/Components.h>
#include <Nova/Renderer/Camera.h>
#include <Nova/Math/Vec.h>
#include <Nova/Math/Mat4.h>

namespace Nova {

class Scene;

struct Aabb {
    Vec3 Min{-0.5f, -0.5f, -0.5f};
    Vec3 Max{0.5f, 0.5f, 0.5f};
};

bool RayIntersectsAabb(const Ray& ray, const Aabb& box, float& outDistance);
Aabb LocalBoundsForMesh(const MeshRendererComponent& mesh);
Aabb WorldAabbFromLocal(const Mat4& world, const Aabb& local);

/// Closest mesh under the cursor. Invalid entity if none.
Entity PickSceneMesh(const Scene& scene, const Ray& worldRay);

} // namespace Nova
