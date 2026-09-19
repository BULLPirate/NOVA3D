#include <Nova/Renderer/Lighting.h>

#include <algorithm>
#include <cmath>

namespace Nova {

Vec3 LightDirectionTowardSurface(const Vec3& rayDirectionWorld) {
    const Vec3 ray = rayDirectionWorld.Normalized();
    if (ray.LengthSq() < 1e-8f) {
        return {0.0f, 1.0f, 0.0f};
    }
    return (ray * -1.0f).Normalized();
}

Mat4 ComputeDirectionalLightViewProjection(const Vec3& lightDirectionTowardLight,
                                           const Vec3& focus,
                                           float orthoHalfExtent,
                                           float nearPlane,
                                           float farPlane) {
    const Vec3 dir = lightDirectionTowardLight.Normalized();
    const float dist = std::max(18.0f, orthoHalfExtent * 1.8f);
    const Vec3 eye = focus + dir * dist;

    Vec3 up{0.0f, 1.0f, 0.0f};
    if (std::fabs(dir.Dot(up)) > 0.95f) {
        up = {1.0f, 0.0f, 0.0f};
    }

    const Mat4 view = Mat4::LookAt(eye, focus, up);
    const Mat4 proj = Mat4::OrthographicMetal(-orthoHalfExtent, orthoHalfExtent,
                                              -orthoHalfExtent, orthoHalfExtent,
                                              nearPlane, farPlane);
    return proj * view;
}

} // namespace Nova
