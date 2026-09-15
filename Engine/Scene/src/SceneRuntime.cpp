#include <Nova/Scene/SceneRuntime.h>

#include <Nova/Math/Quat.h>

namespace Nova {

void TickScene(Scene& scene, float deltaSeconds) {
    if (deltaSeconds <= 0.0f) {
        return;
    }

    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasRotator(entity)) {
            return;
        }

        const RotatorComponent& rot = scene.GetRotator(entity);
        Transform& xform = scene.GetTransform(entity);

        const Quat qx = Quat::FromAxisAngle({1.0f, 0.0f, 0.0f}, rot.AngularVelocity.x * deltaSeconds);
        const Quat qy = Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, rot.AngularVelocity.y * deltaSeconds);
        const Quat qz = Quat::FromAxisAngle({0.0f, 0.0f, 1.0f}, rot.AngularVelocity.z * deltaSeconds);

        if (rot.LocalSpace) {
            xform.Rotation = (xform.Rotation * qy * qx * qz).Normalized();
        } else {
            xform.Rotation = (qy * qx * qz * xform.Rotation).Normalized();
        }
    });

    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasMover(entity)) {
            return;
        }
        const MoverComponent& mover = scene.GetMover(entity);
        Transform& xform = scene.GetTransform(entity);
        xform.Position = xform.Position + mover.Velocity * deltaSeconds;
    });
}

} // namespace Nova
