#pragma once

#include <Nova/Math/Math.h>

#include <vector>

namespace Nova {

struct Aabb {
    Vec3 Min{0.0f, 0.0f, 0.0f};
    Vec3 Max{0.0f, 0.0f, 0.0f};
};

Aabb AabbFromCenterScale(const Vec3& center, const Vec3& scale);

/// Push a circle on XZ out of an AABB. Returns true if it overlapped.
bool ResolveCircleAabbXZ(Vec3& position, float radius, const Aabb& box);

/// Separate two circles on XZ. Both positions are moved.
void SeparateCirclesXZ(Vec3& a, Vec3& b, float radiusA, float radiusB);

/// First hit of segment A→B against AABB. `t` in [0, 1] if true.
bool SegmentHitsAabb(const Vec3& a, const Vec3& b, const Aabb& box, float& t);

/// Pull the camera toward `focus` if the orbit ray hits any solid.
Vec3 CameraEyeAvoidingSolids(const Vec3& focus, const Vec3& desired,
                             const std::vector<Aabb>& solids, float skin = 0.28f);

} // namespace Nova
