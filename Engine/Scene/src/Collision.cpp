#include <Nova/Scene/Collision.h>

#include <algorithm>
#include <cmath>

namespace Nova {

Aabb AabbFromCenterScale(const Vec3& center, const Vec3& scale) {
    const Vec3 half{scale.x * 0.5f, scale.y * 0.5f, scale.z * 0.5f};
    return {center - half, center + half};
}

bool ResolveCircleAabbXZ(Vec3& position, float radius, const Aabb& box) {
    const float hx = (box.Max.x - box.Min.x) * 0.5f + radius;
    const float hz = (box.Max.z - box.Min.z) * 0.5f + radius;
    const float cx = (box.Min.x + box.Max.x) * 0.5f;
    const float cz = (box.Min.z + box.Max.z) * 0.5f;
    if (position.y > box.Max.y + 0.35f || position.y + 1.6f < box.Min.y) {
        return false;
    }
    const float dx = position.x - cx;
    const float dz = position.z - cz;
    if (std::abs(dx) >= hx || std::abs(dz) >= hz) {
        return false;
    }
    const float px = hx - std::abs(dx);
    const float pz = hz - std::abs(dz);
    if (px < pz) {
        position.x += dx >= 0.0f ? px : -px;
    } else {
        position.z += dz >= 0.0f ? pz : -pz;
    }
    return true;
}

void SeparateCirclesXZ(Vec3& a, Vec3& b, float radiusA, float radiusB) {
    Vec3 delta{b.x - a.x, 0.0f, b.z - a.z};
    const float minDist = radiusA + radiusB;
    const float distSq = delta.LengthSq();
    if (distSq >= minDist * minDist || distSq < 1e-8f) {
        if (distSq < 1e-8f) {
            a.x -= radiusA * 0.5f;
            b.x += radiusB * 0.5f;
        }
        return;
    }
    const float dist = std::sqrt(distSq);
    const float push = (minDist - dist) * 0.5f;
    delta = delta * (1.0f / dist);
    a.x -= delta.x * push;
    a.z -= delta.z * push;
    b.x += delta.x * push;
    b.z += delta.z * push;
}

bool SegmentHitsAabb(const Vec3& a, const Vec3& b, const Aabb& box, float& tHit) {
    const Vec3 dir = b - a;
    float tMin = 0.0f;
    float tMax = 1.0f;
    auto slab = [&](float origin, float vel, float bmin, float bmax) {
        if (std::abs(vel) < 1e-7f) {
            return origin >= bmin && origin <= bmax;
        }
        float t0 = (bmin - origin) / vel;
        float t1 = (bmax - origin) / vel;
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        tMin = std::max(tMin, t0);
        tMax = std::min(tMax, t1);
        return tMin <= tMax;
    };
    if (!slab(a.x, dir.x, box.Min.x, box.Max.x) || !slab(a.y, dir.y, box.Min.y, box.Max.y) ||
        !slab(a.z, dir.z, box.Min.z, box.Max.z)) {
        return false;
    }
    tHit = tMin;
    return tMin >= 0.0f && tMin <= 1.0f;
}

Vec3 CameraEyeAvoidingSolids(const Vec3& focus, const Vec3& desired,
                             const std::vector<Aabb>& solids, float skin) {
    float best = 1.0f;
    for (const Aabb& box : solids) {
        float t = 1.0f;
        if (SegmentHitsAabb(focus, desired, box, t) && t < best) {
            best = t;
        }
    }
    if (best >= 0.999f) {
        return desired;
    }
    best = std::max(0.12f, best - skin / std::max(0.001f, (desired - focus).Length()));
    return focus + (desired - focus) * best;
}

} // namespace Nova
