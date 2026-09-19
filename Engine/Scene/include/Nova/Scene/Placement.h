#pragma once

#include <Nova/Scene/Scene.h>
#include <Nova/Math/Vec.h>

namespace Nova {

enum class PlaceableKind : uint8_t {
    Cube = 0,
    Sphere,
    Plane,
    House,
    Tree,
    Crate,
    Wall,
    Road,
    Fence,
    Well,
    Knight,
    Bandit,
    Herb,
};

struct PlaceablePreview {
    Vec3 Center{};
    Vec3 HalfExtents{0.5f, 0.5f, 0.5f};
    Vec3 Color{0.85f, 0.78f, 0.35f};
};

const char* PlaceableKindLabel(PlaceableKind kind);
Entity SpawnPlaceable(Scene& scene, PlaceableKind kind, const Vec3& worldPosition,
                      float yawRadians = 0.0f);
void PlantOnGround(Scene& scene, Entity entity, const Vec3& xz, float grid = 0.5f);
PlaceablePreview QueryPlaceablePreview(PlaceableKind kind, const Vec3& xz, float yawRadians,
                                       float grid = 0.5f);
float PlaceableSpacing(PlaceableKind kind);
bool PlaceableAllowsStroke(PlaceableKind kind);
bool ShouldStampAlongStroke(const Vec3& lastStamp, const Vec3& next, float spacing);

} // namespace Nova
