#include <Nova/Scene/SceneRendererBridge.h>

#include <Nova/Renderer/Camera.h>
#include <Nova/Renderer/Lighting.h>
#include <Nova/Renderer/Material.h>

namespace Nova {

bool BuildSceneCamera(const Scene& scene, float aspect, Camera& outCamera) {
    Entity cameraEntity = scene.FindPrimaryCamera();
    if (!cameraEntity.IsValid() || !scene.HasCamera(cameraEntity)) {
        return false;
    }

    const Transform& camXform = scene.GetTransform(cameraEntity);
    const CameraComponent& camComp = scene.GetCamera(cameraEntity);

    outCamera.Position = camXform.Position;
    outCamera.Target = camComp.LookAtTarget;
    outCamera.FovYRadians = camComp.FovYRadians;
    outCamera.Aspect = aspect;
    outCamera.NearPlane = camComp.NearPlane;
    outCamera.FarPlane = camComp.FarPlane;
    return true;
}

void RenderScene(const Scene& scene, IRenderer& renderer, float aspect, float timeSeconds) {
    Camera camera;
    if (BuildSceneCamera(scene, aspect, camera)) {
        renderer.SetCamera(camera);
    }

    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasDirectionalLight(entity)) return;
        const DirectionalLightComponent& src = scene.GetDirectionalLight(entity);
        DirectionalLight light;
        light.Direction = LightDirectionTowardSurface(src.Direction);
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
            model = model * Mat4::RotateY(-timeSeconds * 0.8f) * Mat4::RotateX(-timeSeconds * 0.35f);
        }

        Material material;
        material.ReceiveShadows = mesh.ReceiveShadows;
        renderer.EnqueueMeshDraw(model, material);
    });
}

} // namespace Nova
