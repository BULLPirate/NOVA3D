#include <Nova/Scene/SceneRendererBridge.h>

#include <Nova/Renderer/Camera.h>
#include <Nova/Renderer/Lighting.h>
#include <Nova/Renderer/Material.h>

namespace Nova {

void RenderScene(const Scene& scene, IRenderer& renderer, float aspect, float timeSeconds) {
    Entity cameraEntity = scene.FindPrimaryCamera();
    if (cameraEntity.IsValid() && scene.HasCamera(cameraEntity)) {
        const Transform& camXform = scene.GetTransform(cameraEntity);
        const CameraComponent& camComp = scene.GetCamera(cameraEntity);

        Camera camera;
        camera.Position = camXform.Position;
        camera.Target = camComp.LookAtTarget;
        camera.FovYRadians = camComp.FovYRadians;
        camera.Aspect = aspect;
        camera.NearPlane = camComp.NearPlane;
        camera.FarPlane = camComp.FarPlane;
        renderer.SetCamera(camera);
    }

    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasDirectionalLight(entity)) return;
        const DirectionalLightComponent& src = scene.GetDirectionalLight(entity);
        DirectionalLight light;
        light.Direction = src.Direction.Normalized();
        light.Color = src.Color;
        light.Ambient = src.Ambient;
        renderer.SetDirectionalLight(light);
    });

    renderer.ClearMeshDraws();

    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasMeshRenderer(entity)) return;

        const Transform& meshXform = scene.GetTransform(entity);
        const MeshRendererComponent& mesh = scene.GetMeshRenderer(entity);

        Mat4 model = meshXform.ToMatrix();
        if (timeSeconds >= 0.0f) {
            model = model * Mat4::RotateY(timeSeconds * 0.8f) * Mat4::RotateX(timeSeconds * 0.35f);
        }

        Material material;
        material.ReceiveShadows = mesh.ReceiveShadows;
        renderer.EnqueueMeshDraw(model, material);
    });
}

} // namespace Nova
