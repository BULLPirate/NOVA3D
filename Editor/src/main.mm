#include <Nova/Core/Log.h>
#include <Nova/Core/Input.h>
#include <Nova/Platform/Window.h>
#include <Nova/Scene/Scene.h>
#include <Nova/Scene/SceneSerialization.h>
#include <Nova/Scene/SceneRendererBridge.h>
#include <Nova/Renderer/Renderer.h>
#include <Nova/Math/Math.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_metal.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <SDL3/SDL.h>

#include <cmath>
#include <filesystem>

namespace {

struct OrbitCamera {
    float YawRadians   = 0.0f;
    float PitchRadians = 0.25f;
    float Distance     = 3.3f;
    Nova::Vec3 Target{0.0f, 0.0f, 0.0f};
};

void SyncOrbitToScene(Nova::Scene& scene, const OrbitCamera& orbit) {
    Nova::Entity cameraEntity = scene.FindPrimaryCamera();
    if (!cameraEntity.IsValid() || !scene.HasCamera(cameraEntity)) return;

    Nova::CameraComponent& cam = scene.GetCamera(cameraEntity);
    cam.LookAtTarget = orbit.Target;

    const float cp = std::cos(orbit.PitchRadians);
    const float sp = std::sin(orbit.PitchRadians);
    const float cy = std::cos(orbit.YawRadians);
    const float sy = std::sin(orbit.YawRadians);

    Nova::Vec3& pos = scene.GetTransform(cameraEntity).Position;
    pos.x = orbit.Target.x + orbit.Distance * cp * sy;
    pos.y = orbit.Target.y + orbit.Distance * sp;
    pos.z = orbit.Target.z + orbit.Distance * cp * cy;
}

void InitOrbitFromScene(const Nova::Scene& scene, OrbitCamera& orbit) {
    Nova::Entity cameraEntity = scene.FindPrimaryCamera();
    if (!cameraEntity.IsValid() || !scene.HasCamera(cameraEntity)) return;

    const Nova::Transform& xform = scene.GetTransform(cameraEntity);
    orbit.Target = scene.GetCamera(cameraEntity).LookAtTarget;

    const Nova::Vec3 delta = xform.Position - orbit.Target;
    orbit.Distance = std::max(0.5f, delta.Length());
    if (orbit.Distance > 1e-4f) {
        orbit.PitchRadians = std::asin(delta.y / orbit.Distance);
        orbit.YawRadians = std::atan2(delta.x, delta.z);
    }
}

void ImGuiPassReady(void* renderPassDescriptor, void*) {
    ImGui_ImplMetal_NewFrame((__bridge MTLRenderPassDescriptor*)renderPassDescriptor);
}

void RotateTransform(Nova::Transform& transform, float deltaX, float deltaY) {
    const float sensitivity = 0.008f;
    const Nova::Quat qYaw =
        Nova::Quat::FromAxisAngle({0.0f, 1.0f, 0.0f}, deltaX * sensitivity);
    const Nova::Quat qPitch =
        Nova::Quat::FromAxisAngle({1.0f, 0.0f, 0.0f}, deltaY * sensitivity);
    transform.Rotation = (qYaw * qPitch * transform.Rotation).Normalized();
}

void ImGuiOverlay(void* commandBuffer, void* renderEncoder, void*) {
    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData) return;
    ImGui_ImplMetal_RenderDrawData(drawData,
                                   (__bridge id<MTLCommandBuffer>)commandBuffer,
                                   (__bridge id<MTLRenderCommandEncoder>)renderEncoder);
}

} // namespace

int main() {
    Nova::Log::Init();
    NOVA_LOG_INFO("NOVA3D Editor");

    const std::filesystem::path defaultScenePath =
        std::filesystem::path(NOVA_SOURCE_DIR) / "Assets/Scenes/demo.scene.json";

    Nova::Window window({"NOVA3D Editor", 1440, 900});
    if (!window.IsValid()) {
        Nova::Log::Shutdown();
        return 1;
    }

    Nova::Input input;
    auto renderer = Nova::CreateRenderer();
    if (!renderer->Init(window)) {
        Nova::Log::Shutdown();
        return 1;
    }
    renderer->SetClearColor(0.10f, 0.11f, 0.14f, 1.0f);
    renderer->SetRenderPassReadyCallback(ImGuiPassReady, nullptr);
    renderer->SetFrameOverlayCallback(ImGuiOverlay, nullptr);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForMetal(window.GetSDLWindow());
    ImGui_ImplMetal_Init((__bridge id<MTLDevice>)renderer->GetNativeDevice());

    Nova::Scene scene;
    std::filesystem::path scenePath = defaultScenePath;
    bool sceneDirty = false;

    if (Nova::SceneIOResult load = Nova::LoadSceneFromFile(scenePath, scene); load.Ok) {
        NOVA_LOG_INFO("Editor scene: {}", scenePath.string());
    } else {
        NOVA_LOG_WARN("Editor scene load failed: {}", load.Error);
        scene = Nova::Scene::CreateDemoLevel();
        sceneDirty = true;
    }

    OrbitCamera orbit;
    InitOrbitFromScene(scene, orbit);

    Nova::Entity selected{Nova::Entity::kInvalidEntity};
    bool viewportHovered = false;
    bool isPlaying = false;
    Nova::Scene playScene;
    while (!window.ShouldClose()) {
        input.BeginFrame();
        window.PollEvents(input, [](const SDL_Event& event) {
            ImGui_ImplSDL3_ProcessEvent(&event);
        });

        if (input.IsKeyPressed(Nova::KeyCode::Escape)) {
            if (isPlaying) {
                isPlaying = false;
            } else {
                break;
            }
        }

        ImGui_ImplSDL3_NewFrame();
        renderer->BeginFrame();
        ImGui::NewFrame();

        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal)) {
            if (Nova::SceneIOResult save = Nova::SaveSceneToFile(scene, scenePath); save.Ok) {
                sceneDirty = false;
                NOVA_LOG_INFO("Scene saved to {}", scenePath.string());
            } else {
                NOVA_LOG_ERROR("Save failed: {}", save.Error);
            }
        }

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Save Scene", "Cmd+S")) {
                    if (Nova::SceneIOResult save = Nova::SaveSceneToFile(scene, scenePath); save.Ok) {
                        sceneDirty = false;
                        NOVA_LOG_INFO("Scene saved to {}", scenePath.string());
                    } else {
                        NOVA_LOG_ERROR("Save failed: {}", save.Error);
                    }
                }
                if (ImGui::MenuItem("Reload")) {
                    Nova::Scene reloaded;
                    if (Nova::LoadSceneFromFile(scenePath, reloaded).Ok) {
                        scene = std::move(reloaded);
                        selected = Nova::Entity{};
                        InitOrbitFromScene(scene, orbit);
                        sceneDirty = false;
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Play")) {
                if (!isPlaying && ImGui::MenuItem("Play Scene")) {
                    playScene = Nova::CloneScene(scene);
                    isPlaying = true;
                }
                if (isPlaying && ImGui::MenuItem("Stop")) {
                    isPlaying = false;
                }
                ImGui::EndMenu();
            }
            if (isPlaying) {
                ImGui::Text(" | PLAY");
            } else if (sceneDirty) {
                ImGui::Text(" *");
            }
            ImGui::EndMainMenuBar();
        }

        Nova::Scene& activeScene = isPlaying ? playScene : scene;

        if (isPlaying) {
            const ImGuiViewport* vp = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(vp->WorkPos);
            ImGui::SetNextWindowSize(vp->WorkSize);
            ImGui::Begin("##PlayOverlay", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                             ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav);
            ImGui::End();

            ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + 12.0f, vp->WorkPos.y + 36.0f));
            ImGui::Begin("PlayMode", nullptr,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration);
            ImGui::TextUnformatted("Play Mode — Esc to stop");
            if (ImGui::Button("Stop")) {
                isPlaying = false;
            }
            ImGui::End();
        }

        if (isPlaying) {
            if (input.IsMouseButtonDown(Nova::MouseButton::Right)) {
                orbit.YawRadians += input.GetMouseDeltaX() * 0.005f;
                orbit.PitchRadians += input.GetMouseDeltaY() * 0.005f;
                orbit.PitchRadians = Nova::Clamp(orbit.PitchRadians, -1.4f, 1.4f);
            }
            if (input.GetScrollY() != 0.0f) {
                orbit.Distance =
                    Nova::Clamp(orbit.Distance - input.GetScrollY() * 0.25f, 0.8f, 30.0f);
            }
            SyncOrbitToScene(activeScene, orbit);
            uint32_t fbW = 0, fbH = 0;
            window.GetFramebufferSize(fbW, fbH);
            const float aspect = fbH > 0 ? static_cast<float>(fbW) / static_cast<float>(fbH)
                                         : 16.0f / 9.0f;
            Nova::RenderScene(activeScene, *renderer, aspect, -1.0f);
            ImGui::Render();
            renderer->BeginDrawing();
            renderer->EndFrame();
            continue;
        }

        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        const ImVec2 workPos = mainViewport->WorkPos;
        const ImVec2 workSize = mainViewport->WorkSize;
        constexpr float kSidePanelWidth = 280.0f;

        ImGui::SetNextWindowPos(workPos);
        ImGui::SetNextWindowSize(ImVec2(kSidePanelWidth, workSize.y));
        ImGui::Begin("Hierarchy", nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            if (ImGui::Button("Add Cube")) {
                Nova::Entity cube = scene.CreateEntity("Cube");
                scene.AddMeshRenderer(cube);
                selected = cube;
                sceneDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete") && selected.IsValid() && scene.IsAlive(selected)) {
                scene.DestroyEntity(selected);
                selected = Nova::Entity{};
                sceneDirty = true;
            }
            ImGui::Separator();
            scene.ForEachEntity([&](Nova::Entity entity) {
                const bool isSelected = selected.IsValid() && entity.Id == selected.Id;
                if (ImGui::Selectable(scene.GetName(entity).c_str(), isSelected)) {
                    selected = entity;
                }
            });
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(workPos.x + workSize.x - kSidePanelWidth, workPos.y));
        ImGui::SetNextWindowSize(ImVec2(kSidePanelWidth, workSize.y));
        ImGui::Begin("Inspector", nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            if (selected.IsValid() && scene.IsAlive(selected)) {
                ImGui::Text("Name: %s", scene.GetName(selected).c_str());
                Nova::Transform& xform = scene.GetTransform(selected);
                if (ImGui::DragFloat3("Position", &xform.Position.x, 0.02f)) {
                    sceneDirty = true;
                }
                if (ImGui::DragFloat3("Scale", &xform.Scale.x, 0.02f, 0.01f, 10.0f)) {
                    sceneDirty = true;
                }
                if (scene.HasMeshRenderer(selected)) {
                    ImGui::TextUnformatted("Mesh: UnitCube");
                }
                if (scene.HasCamera(selected)) {
                    ImGui::TextUnformatted("Component: Camera");
                }
                if (scene.HasDirectionalLight(selected)) {
                    Nova::DirectionalLightComponent& sun = scene.GetDirectionalLight(selected);
                    ImGui::TextUnformatted("Directional Light (Sun)");
                    if (ImGui::DragFloat3("Direction", &sun.Direction.x, 0.02f, -1.0f, 1.0f)) {
                        sun.Direction = sun.Direction.Normalized();
                        sceneDirty = true;
                    }
                    if (ImGui::DragFloat("Ambient", &sun.Ambient, 0.01f, 0.0f, 1.0f)) {
                        sceneDirty = true;
                    }
                }
            } else {
                ImGui::TextUnformatted("Select an entity in Hierarchy.");
            }
        ImGui::End();

        const float centerX = workPos.x + kSidePanelWidth;
        const float centerW = workSize.x - kSidePanelWidth * 2.0f;
        ImGui::SetNextWindowPos(ImVec2(centerX, workPos.y));
        ImGui::SetNextWindowSize(ImVec2(centerW, workSize.y));
        ImGui::Begin("Viewport", nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::TextUnformatted("Click gray area below, then: LMB rotate | Shift+LMB move | RMB camera");
            ImGui::Text("Scene: %s", scenePath.filename().string().c_str());
            const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
            ImVec2 size = canvasSize;
            if (size.x < 64.0f) size.x = 64.0f;
            if (size.y < 64.0f) size.y = 64.0f;
            ImGui::InvisibleButton("##viewport_canvas", size,
                                    ImGuiButtonFlags_MouseButtonLeft |
                                        ImGuiButtonFlags_MouseButtonRight |
                                        ImGuiButtonFlags_MouseButtonMiddle);
            viewportHovered = ImGui::IsItemHovered();
            if (viewportHovered) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
        ImGui::End();

        if (viewportHovered) {
            const float dx = input.GetMouseDeltaX();
            const float dy = input.GetMouseDeltaY();
            const bool shiftDown = input.IsKeyDown(Nova::KeyCode::LShift) ||
                                   input.IsKeyDown(Nova::KeyCode::RShift);

            if (selected.IsValid() && scene.IsAlive(selected)) {
                Nova::Transform& xform = scene.GetTransform(selected);
                if (input.IsMouseButtonDown(Nova::MouseButton::Left) && (dx != 0.0f || dy != 0.0f)) {
                    if (shiftDown) {
                        xform.Position.x += dx * 0.01f;
                        xform.Position.y -= dy * 0.01f;
                        sceneDirty = true;
                    } else if (scene.HasMeshRenderer(selected)) {
                        RotateTransform(xform, dx, dy);
                        sceneDirty = true;
                    }
                }
            }

            if (input.IsMouseButtonDown(Nova::MouseButton::Right)) {
                orbit.YawRadians += dx * 0.005f;
                orbit.PitchRadians += dy * 0.005f;
                orbit.PitchRadians = Nova::Clamp(orbit.PitchRadians, -1.4f, 1.4f);
                sceneDirty = true;
            }
            if (input.GetScrollY() != 0.0f) {
                orbit.Distance =
                    Nova::Clamp(orbit.Distance - input.GetScrollY() * 0.25f, 0.8f, 30.0f);
                sceneDirty = true;
            }
        }

        SyncOrbitToScene(activeScene, orbit);

        uint32_t fbW = 0, fbH = 0;
        window.GetFramebufferSize(fbW, fbH);
        const float aspect = fbH > 0 ? static_cast<float>(fbW) / static_cast<float>(fbH)
                                     : 16.0f / 9.0f;

        Nova::RenderScene(activeScene, *renderer, aspect, -1.0f);

        ImGui::Render();
        renderer->BeginDrawing();
        renderer->EndFrame();
    }

    ImGui_ImplMetal_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    renderer->Shutdown();
    Nova::Log::Shutdown();
    return 0;
}
