#include <Nova/Scene/SceneRendererBridge.h>

#include <Nova/Assets/MeshCache.h>
#include <Nova/Assets/TextureCache.h>
#include <Nova/Renderer/Camera.h>
#include <Nova/Renderer/Lighting.h>
#include <Nova/Renderer/Material.h>

#include <algorithm>

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

void RenderScene(const Scene& scene,
                 IRenderer& renderer,
                 float aspect,
                 const std::filesystem::path& projectRoot,
                 MeshAssetCache& meshCache,
                 TextureAssetCache& textureCache,
                 float opacityMultiplier) {
    Camera camera;
    if (BuildSceneCamera(scene, aspect, camera)) {
        renderer.SetCamera(camera);
    }

    const Vec3 clear = scene.Settings().ClearColor;
    renderer.SetClearColor(clear.x, clear.y, clear.z, 1.0f);

    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasDirectionalLight(entity)) return;
        const DirectionalLightComponent& src = scene.GetDirectionalLight(entity);
        DirectionalLight light;
        light.Direction = LightDirectionTowardSurface(src.Direction);
        light.Color = src.Color;
        light.Ambient = src.Ambient;
        renderer.SetDirectionalLight(light);
    });

    PointLight point = DisabledPointLight();
    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasPointLight(entity) || point.Range > 0.0f) return;
        const PointLightComponent& src = scene.GetPointLight(entity);
        const Mat4 world = scene.GetWorldMatrix(entity);
        const Vec4 origin = world * Vec4{0.0f, 0.0f, 0.0f, 1.0f};
        point.Position = {origin.x, origin.y, origin.z};
        point.Color = src.Color * src.Intensity;
        point.Range = src.Range;
    });
    renderer.SetPointLight(point);

    renderer.ClearMeshDraws();

    scene.ForEachEntity([&](Entity entity) {
        if (!scene.HasMeshRenderer(entity)) return;

        const Mat4 worldMatrix = scene.GetWorldMatrix(entity);
        const MeshRendererComponent& mesh = scene.GetMeshRenderer(entity);

        MeshGpuHandle gpuMesh = kDefaultMeshGpuHandle;
        if (!mesh.AssetPath.empty()) {
            gpuMesh = meshCache.Resolve(renderer, projectRoot, mesh.AssetPath);
        } else if (mesh.Primitive == MeshPrimitive::UnitPlane) {
            gpuMesh = kBuiltinPlaneMeshGpuHandle;
        } else if (mesh.Primitive == MeshPrimitive::UnitSphere) {
            gpuMesh = kBuiltinSphereMeshGpuHandle;
        }

        TextureGpuHandle gpuTex = kDefaultTextureGpuHandle;
        if (mesh.UseAlbedoTexture && !mesh.AlbedoTexturePath.empty()) {
            gpuTex = textureCache.Resolve(renderer, projectRoot, mesh.AlbedoTexturePath);
        }

        Material material;
        Vec3 tint = mesh.AlbedoColor;
        Entity root = entity;
        while (scene.GetParent(root).IsValid()) {
            root = scene.GetParent(root);
        }
        if (scene.HasCharacterController(root)) {
            const CharacterControllerComponent& character = scene.GetCharacterController(root);
            if (character.HurtTimer > 0.0f) {
                tint = {0.95f, 0.18f, 0.14f};
            } else if (character.AttackTimer > 0.18f && character.Team == 0) {
                tint = tint * 1.15f;
                tint.x = std::min(1.0f, tint.x + 0.12f);
            }
        }
        material.TintR = tint.x;
        material.TintG = tint.y;
        material.TintB = tint.z;
        material.TintA = std::clamp(mesh.Opacity * opacityMultiplier, 0.0f, 1.0f);
        material.UseAlbedoTexture = mesh.UseAlbedoTexture;
        material.ReceiveShadows = mesh.ReceiveShadows;

        renderer.EnqueueMeshDraw(worldMatrix, material, gpuMesh, gpuTex);
    });
}

} // namespace Nova
