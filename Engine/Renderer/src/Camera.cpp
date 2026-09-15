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

bool ViewportPointToRay(const Camera& camera,
                        float pixelX,
                        float pixelY,
                        float viewportWidth,
                        float viewportHeight,
                        Ray& outRay) {
    if (viewportWidth <= 1e-4f || viewportHeight <= 1e-4f) {
        return false;
    }

    const float ndcX = (pixelX / viewportWidth) * 2.0f - 1.0f;
    const float ndcY = 1.0f - (pixelY / viewportHeight) * 2.0f;
    const Mat4 invViewProj = camera.GetViewProjectionMatrix().Inverse();

    const Vec3 nearPoint = invViewProj.TransformPoint({ndcX, ndcY, 0.0f});
    const Vec3 farPoint = invViewProj.TransformPoint({ndcX, ndcY, 1.0f});
    const Vec3 dir = farPoint - nearPoint;
    if (dir.LengthSq() < 1e-12f) {
        return false;
    }

    outRay.Origin = camera.Position;
    outRay.Direction = dir.Normalized();
    return true;
}

} // namespace Nova
