#include <Nova/Renderer/Camera.h>

namespace Nova {

bool ProjectWorldToViewport(const Mat4& viewProjection,
                            const Vec3& world,
                            float viewportWidth,
                            float viewportHeight,
                            float& outX,
                            float& outY) {
    if (viewportWidth <= 0.0f || viewportHeight <= 0.0f) {
        return false;
    }

    const Vec4 clip = viewProjection * Vec4{world.x, world.y, world.z, 1.0f};
    if (clip.w <= 1e-5f) {
        return false;
    }

    const float invW = 1.0f / clip.w;
    const float ndcX = clip.x * invW;
    const float ndcY = clip.y * invW;

    outX = (ndcX * 0.5f + 0.5f) * viewportWidth;
    outY = (1.0f - (ndcY * 0.5f + 0.5f)) * viewportHeight;
    return true;
}

} // namespace Nova
