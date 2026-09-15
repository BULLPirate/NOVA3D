#include <Nova/Core/Log.h>
#include <Nova/Core/Input.h>
#include <Nova/Platform/MacApp.h>
#include <Nova/Platform/Window.h>
#include <Nova/Project/Project.h>
#include <Nova/Scene/Scene.h>
#include <Nova/Scene/SceneSerialization.h>
#include <Nova/Assets/MeshCache.h>
#include <Nova/Scene/SceneRendererBridge.h>
#include <Nova/Scene/SceneRuntime.h>

#include "FileDialog.h"
#include "GameLauncher.h"
#include <Nova/Renderer/Renderer.h>
#include <Nova/Renderer/RenderViewport.h>
#include <Nova/Renderer/Camera.h>
#include <Nova/Math/Math.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_metal.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <SDL3/SDL.h>

#include <cstdio>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

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

void ApplyOrbitPosition(const OrbitCamera& orbit, Nova::Vec3& position) {
    const float cp = std::cos(orbit.PitchRadians);
    const float sp = std::sin(orbit.PitchRadians);
    const float cy = std::cos(orbit.YawRadians);
    const float sy = std::sin(orbit.YawRadians);
    position.x = orbit.Target.x + orbit.Distance * cp * sy;
    position.y = orbit.Target.y + orbit.Distance * sp;
    position.z = orbit.Target.z + orbit.Distance * cp * cy;
}

std::string UniqueEntityName(const Nova::Scene& scene, const char* baseName) {
    auto exists = [&](const std::string& name) {
        bool found = false;
        scene.ForEachEntity([&](Nova::Entity e) {
            if (scene.GetName(e) == name) {
                found = true;
            }
        });
        return found;
    };
    if (!exists(baseName)) {
        return baseName;
    }
    for (int i = 2; i < 1000; ++i) {
        const std::string candidate = std::string(baseName) + " " + std::to_string(i);
        if (!exists(candidate)) {
            return candidate;
        }
    }
    return std::string(baseName) + " (copy)";
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

const std::vector<Nova::Editor::FileFilter> kSceneFileFilters = {
    {"NOVA Scene", {"json"}},
};

enum class ViewportTool { Move, Rotate };

enum class MoveAxis { Free, X, Y, Z };

struct FrameViewport {
    Nova::RenderViewport Gpu{};
    ImVec2 CanvasMin{};
    ImVec2 CanvasSize{};
    float Aspect = 16.0f / 9.0f;
};

void DrawTranslationGizmo(ImDrawList* drawList,
                          const FrameViewport& vp,
                          const Nova::Mat4& viewProj,
                          const Nova::Vec3& origin,
                          float axisLength) {
    auto toScreen = [&](const Nova::Vec3& world, ImVec2& out) {
        float sx = 0.0f;
        float sy = 0.0f;
        if (!Nova::ProjectWorldToViewport(viewProj, world, vp.CanvasSize.x, vp.CanvasSize.y, sx,
                                          sy)) {
            return false;
        }
        out.x = vp.CanvasMin.x + sx;
        out.y = vp.CanvasMin.y + sy;
        return true;
    };

    ImVec2 center;
    if (!toScreen(origin, center)) {
        return;
    }

    const Nova::Vec3 axes[3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
    const ImU32 colors[3] = {IM_COL32(230, 80, 80, 255), IM_COL32(90, 220, 100, 255),
                             IM_COL32(90, 140, 255, 255)};
    for (int i = 0; i < 3; ++i) {
        ImVec2 tip;
        if (!toScreen(origin + axes[i] * axisLength, tip)) {
            continue;
        }
        drawList->AddLine(center, tip, colors[i], 3.0f);
        drawList->AddCircleFilled(tip, 5.0f, colors[i]);
    }
}

enum class PendingNavigation {
    None,
    OpenProject,
    NewProject,
    OpenScene,
};

bool SaveSceneToDisk(Nova::Scene& scene,
                     const std::filesystem::path& path,
                     Nova::ProjectDescriptor& project,
                     bool& sceneDirty) {
    if (Nova::SceneIOResult save = Nova::SaveSceneToFile(scene, path); !save.Ok) {
        NOVA_LOG_ERROR("Save failed: {}", save.Error);
        return false;
    }
    sceneDirty = false;
    project.LastOpenedScene = Nova::MakeProjectRelativePath(project, path);
    if (Nova::ProjectIOResult pr = Nova::SaveProject(project); !pr.Ok) {
        NOVA_LOG_WARN("Project metadata save failed: {}", pr.Error);
    }
    NOVA_LOG_INFO("Scene saved to {}", path.string());
    return true;
}

bool LoadSceneFromDisk(const std::filesystem::path& path,
                       Nova::Scene& scene,
                       Nova::ProjectDescriptor& project,
                       Nova::Entity& selected,
                       OrbitCamera& orbit,
                       bool& sceneDirty) {
    Nova::Scene loaded;
    if (Nova::SceneIOResult load = Nova::LoadSceneFromFile(path, loaded); !load.Ok) {
        NOVA_LOG_ERROR("Load failed: {}", load.Error);
        return false;
    }
    scene = std::move(loaded);
    selected = Nova::Entity{};
    InitOrbitFromScene(scene, orbit);
    sceneDirty = false;
    project.LastOpenedScene = Nova::MakeProjectRelativePath(project, path);
    if (Nova::ProjectIOResult pr = Nova::SaveProject(project); !pr.Ok) {
        NOVA_LOG_WARN("Project metadata save failed: {}", pr.Error);
    }
    NOVA_LOG_INFO("Scene loaded from {}", path.string());
    return true;
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
    Nova::EnsureMacOSApplicationReady();

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

    Nova::ProjectDescriptor project;
    const std::filesystem::path devRoot = NOVA_SOURCE_DIR;
    if (Nova::ProjectIOResult pr = Nova::LoadProject(devRoot, project); !pr.Ok) {
        NOVA_LOG_WARN("No .nova project at source root: {}", pr.Error);
        project.Root = devRoot;
        project.Name = "NOVA3D";
        Nova::EnsureProjectLayout(project.Root);
    }

    Nova::Scene scene;
    std::filesystem::path scenePath = project.LastOpenedSceneAbsolute();
    bool sceneDirty = false;
    OrbitCamera orbit;
    Nova::Entity selected{Nova::Entity::kInvalidEntity};
    bool broughtWindowForward = false;

    if (!std::filesystem::exists(scenePath)) {
        scenePath = project.StartupSceneAbsolute();
    }
    if (!LoadSceneFromDisk(scenePath, scene, project, selected, orbit, sceneDirty)) {
        scene = Nova::Scene::CreateDemoLevel();
        sceneDirty = true;
        Nova::SaveSceneToFile(scene, scenePath);
        InitOrbitFromScene(scene, orbit);
    }
    bool viewportHovered = false;
    FrameViewport frameViewport;
    ViewportTool viewportTool = ViewportTool::Rotate;
    MoveAxis moveAxis = MoveAxis::Free;
    char renameBuffer[128] = {};
    bool isPlaying = false;
    Nova::Scene playScene;
    Uint64 playStartTicks = 0;
    Uint64 playLastTickMs = 0;
    Nova::MeshAssetCache meshCache;
    auto startPlay = [&]() {
        playScene = Nova::CloneScene(scene);
        playStartTicks = SDL_GetTicks();
        playLastTickMs = playStartTicks;
        isPlaying = true;
    };
    auto stopPlay = [&]() { isPlaying = false; };
    PendingNavigation pendingNav = PendingNavigation::None;
    bool showUnsavedPrompt = false;
    while (!window.ShouldClose()) {
        input.BeginFrame();
        window.PollEvents(input, [](const SDL_Event& event) {
            ImGui_ImplSDL3_ProcessEvent(&event);
        });

        if (input.IsKeyPressed(Nova::KeyCode::Escape)) {
            if (isPlaying) {
                stopPlay();
            } else {
                break;
            }
        }

        ImGui_ImplSDL3_NewFrame();
        renderer->BeginFrame();
        ImGui::NewFrame();

        if (!broughtWindowForward) {
            window.BringToFront();
            broughtWindowForward = true;
        }

        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal)) {
            SaveSceneToDisk(scene, scenePath, project, sceneDirty);
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_G,
                            ImGuiInputFlags_RouteGlobal)) {
            if (sceneDirty) {
                SaveSceneToDisk(scene, scenePath, project, sceneDirty);
            }
            Nova::Editor::LaunchGameWithScene(scenePath);
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D, ImGuiInputFlags_RouteGlobal)) {
            if (selected.IsValid() && scene.IsAlive(selected)) {
                Nova::Entity dup = scene.DuplicateEntity(selected);
                scene.SetName(dup, UniqueEntityName(scene, scene.GetName(selected).c_str()).c_str());
                selected = dup;
                sceneDirty = true;
            }
        }

        auto runPendingNavigation = [&]() {
            switch (pendingNav) {
            case PendingNavigation::OpenProject: {
                if (auto folder = Nova::Editor::ShowOpenFolderDialog("Open NOVA3D Project")) {
                    Nova::ProjectDescriptor opened;
                    if (Nova::LoadProject(*folder, opened).Ok) {
                        project = opened;
                        scenePath = project.LastOpenedSceneAbsolute();
                        if (!std::filesystem::exists(scenePath)) {
                            scenePath = project.StartupSceneAbsolute();
                        }
                        if (!LoadSceneFromDisk(scenePath, scene, project, selected, orbit,
                                               sceneDirty)) {
                            scene = Nova::Scene::CreateDemoLevel();
                            sceneDirty = true;
                            SaveSceneToDisk(scene, scenePath, project, sceneDirty);
                        }
                    }
                }
                break;
            }
            case PendingNavigation::NewProject: {
                if (auto folder = Nova::Editor::ShowOpenFolderDialog("Create Project Folder")) {
                    if (Nova::InitializeNewProject(*folder, folder->filename().string()).Ok) {
                        Nova::Scene demo = Nova::Scene::CreateDemoLevel();
                        const std::filesystem::path mainScene =
                            *folder / "Assets/Scenes/main.scene.json";
                        Nova::SaveSceneToFile(demo, mainScene);
                        Nova::LoadProject(*folder, project);
                        scenePath = mainScene;
                        LoadSceneFromDisk(scenePath, scene, project, selected, orbit, sceneDirty);
                    }
                }
                break;
            }
            case PendingNavigation::OpenScene: {
                if (auto file = Nova::Editor::ShowOpenFileDialog("Open Scene", kSceneFileFilters)) {
                    scenePath = *file;
                    LoadSceneFromDisk(scenePath, scene, project, selected, orbit, sceneDirty);
                }
                break;
            }
            case PendingNavigation::None:
                break;
            }
            pendingNav = PendingNavigation::None;
        };

        if (showUnsavedPrompt) {
            ImGui::OpenPopup("Unsaved Changes");
        }
        if (ImGui::BeginPopupModal("Unsaved Changes", &showUnsavedPrompt,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Save changes before continuing?");
            if (ImGui::Button("Save", ImVec2(120, 0))) {
                SaveSceneToDisk(scene, scenePath, project, sceneDirty);
                showUnsavedPrompt = false;
                runPendingNavigation();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard", ImVec2(120, 0))) {
                sceneDirty = false;
                showUnsavedPrompt = false;
                runPendingNavigation();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                pendingNav = PendingNavigation::None;
                showUnsavedPrompt = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        auto requestNavigation = [&](PendingNavigation nav) {
            if (sceneDirty) {
                pendingNav = nav;
                showUnsavedPrompt = true;
            } else {
                pendingNav = nav;
                runPendingNavigation();
            }
        };

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open Project...")) {
                    requestNavigation(PendingNavigation::OpenProject);
                }
                if (ImGui::MenuItem("New Project...")) {
                    requestNavigation(PendingNavigation::NewProject);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Open Scene...")) {
                    requestNavigation(PendingNavigation::OpenScene);
                }
                if (ImGui::MenuItem("Save Scene", "Cmd+S")) {
                    SaveSceneToDisk(scene, scenePath, project, sceneDirty);
                }
                if (ImGui::MenuItem("Save Scene As...")) {
                    std::filesystem::path defaultPath = project.ScenesDirectory() / "scene.json";
                    if (!scenePath.empty()) {
                        defaultPath = scenePath;
                    }
                    if (auto file = Nova::Editor::ShowSaveFileDialog("Save Scene As", defaultPath,
                                                                     kSceneFileFilters)) {
                        scenePath = *file;
                        SaveSceneToDisk(scene, scenePath, project, sceneDirty);
                    }
                }
                if (ImGui::MenuItem("Reload")) {
                    LoadSceneFromDisk(scenePath, scene, project, selected, orbit, sceneDirty);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Play")) {
                if (!isPlaying && ImGui::MenuItem("Play Scene", "F5")) {
                    startPlay();
                }
                if (isPlaying && ImGui::MenuItem("Stop", "Esc")) {
                    stopPlay();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Run Standalone Game", "Cmd+Shift+G")) {
                    if (sceneDirty) {
                        SaveSceneToDisk(scene, scenePath, project, sceneDirty);
                    }
                    Nova::Editor::LaunchGameWithScene(scenePath);
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (isPlaying) {
                if (ImGui::Button("Stop")) {
                    stopPlay();
                }
            } else if (ImGui::Button("Play")) {
                startPlay();
            }
            ImGui::Separator();
            ImGui::Text(" | %s", project.Name.c_str());
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
                stopPlay();
            }
            ImGui::End();
        }

        if (!isPlaying &&
            ImGui::Shortcut(ImGuiKey_F5, ImGuiInputFlags_RouteGlobal | ImGuiInputFlags_RouteOverFocused)) {
            startPlay();
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
            Nova::RenderViewport fullVp;
            fullVp.Active = false;
            renderer->SetRenderViewport(fullVp);
            uint32_t fbW = 0, fbH = 0;
            window.GetFramebufferSize(fbW, fbH);
            const float aspect = fbH > 0 ? static_cast<float>(fbW) / static_cast<float>(fbH)
                                         : 16.0f / 9.0f;
            const Uint64 nowMs = SDL_GetTicks();
            const float dt = static_cast<float>(nowMs - playLastTickMs) * 0.001f;
            playLastTickMs = nowMs;
            Nova::TickScene(playScene, dt);
            Nova::RenderScene(activeScene, *renderer, aspect, project.Root, meshCache);
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
            if (ImGui::Button("Cube")) {
                Nova::Entity cube = scene.CreateEntity(UniqueEntityName(scene, "Cube").c_str());
                scene.AddMeshRenderer(cube);
                selected = cube;
                sceneDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Sun")) {
                Nova::Entity sun = scene.CreateEntity(UniqueEntityName(scene, "Sun").c_str());
                Nova::DirectionalLightComponent light;
                light.Direction = light.Direction.Normalized();
                scene.AddDirectionalLight(sun, light);
                selected = sun;
                sceneDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Camera")) {
                Nova::Entity cam = scene.CreateEntity(UniqueEntityName(scene, "Camera").c_str());
                Nova::CameraComponent camera;
                const Nova::Entity primary = scene.FindPrimaryCamera();
                camera.IsPrimary = !primary.IsValid();
                camera.LookAtTarget = orbit.Target;
                scene.AddCamera(cam, camera);
                ApplyOrbitPosition(orbit, scene.GetTransform(cam).Position);
                if (camera.IsPrimary) {
                    scene.SetPrimaryCamera(cam);
                }
                selected = cam;
                sceneDirty = true;
            }
            if (ImGui::Button("Dup") && selected.IsValid() && scene.IsAlive(selected)) {
                Nova::Entity dup = scene.DuplicateEntity(selected);
                scene.SetName(dup, UniqueEntityName(scene, scene.GetName(selected).c_str()).c_str());
                selected = dup;
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
                std::string label = scene.GetName(entity);
                if (scene.HasCamera(entity) && scene.GetCamera(entity).IsPrimary) {
                    label += " [Cam]";
                }
                if (scene.HasDirectionalLight(entity)) {
                    label += " [Light]";
                }
                if (scene.HasMeshRenderer(entity)) {
                    label += " [Mesh]";
                }
                if (scene.HasRotator(entity)) {
                    label += " [Rotator]";
                }
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selected = entity;
                    renameBuffer[0] = '\0';
                }
            });
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(workPos.x + workSize.x - kSidePanelWidth, workPos.y));
        ImGui::SetNextWindowSize(ImVec2(kSidePanelWidth, workSize.y));
        ImGui::Begin("Inspector", nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            if (selected.IsValid() && scene.IsAlive(selected)) {
                const std::string& currentName = scene.GetName(selected);
                if (renameBuffer[0] == '\0' ||
                    std::string(renameBuffer) != currentName) {
                    std::snprintf(renameBuffer, sizeof(renameBuffer), "%s", currentName.c_str());
                }
                if (ImGui::InputText("Name", renameBuffer, sizeof(renameBuffer))) {
                    scene.SetName(selected, renameBuffer);
                    sceneDirty = true;
                }
                Nova::Transform& xform = scene.GetTransform(selected);
                if (ImGui::DragFloat3("Position", &xform.Position.x, 0.02f)) {
                    sceneDirty = true;
                }
                if (ImGui::DragFloat3("Scale", &xform.Scale.x, 0.02f, 0.01f, 10.0f)) {
                    sceneDirty = true;
                }
                if (scene.HasMeshRenderer(selected)) {
                    Nova::MeshRendererComponent& mesh = scene.GetMeshRenderer(selected);
                    ImGui::TextUnformatted("Mesh Renderer");
                    char assetBuf[256] = {};
                    std::snprintf(assetBuf, sizeof(assetBuf), "%s", mesh.AssetPath.c_str());
                    if (ImGui::InputText("Asset (OBJ/glTF)", assetBuf, sizeof(assetBuf))) {
                        mesh.AssetPath = assetBuf;
                        sceneDirty = true;
                    }
                    ImGui::TextUnformatted("Empty asset = UnitCube primitive");
                }
                if (scene.HasRotator(selected)) {
                    Nova::RotatorComponent& rot = scene.GetRotator(selected);
                    ImGui::TextUnformatted("Rotator");
                    if (ImGui::DragFloat3("Angular Vel", &rot.AngularVelocity.x, 0.02f)) {
                        sceneDirty = true;
                    }
                    if (ImGui::Checkbox("Local Space", &rot.LocalSpace)) {
                        sceneDirty = true;
                    }
                } else if (ImGui::Button("Add Rotator")) {
                    scene.AddRotator(selected);
                    sceneDirty = true;
                }
                if (scene.HasMover(selected)) {
                    Nova::MoverComponent& mover = scene.GetMover(selected);
                    ImGui::TextUnformatted("Mover");
                    if (ImGui::DragFloat3("Velocity", &mover.Velocity.x, 0.02f)) {
                        sceneDirty = true;
                    }
                } else if (ImGui::Button("Add Mover")) {
                    scene.AddMover(selected);
                    sceneDirty = true;
                }
                if (scene.HasCamera(selected)) {
                    Nova::CameraComponent& cam = scene.GetCamera(selected);
                    ImGui::TextUnformatted("Camera");
                    if (ImGui::Checkbox("Primary", &cam.IsPrimary)) {
                        if (cam.IsPrimary) {
                            scene.SetPrimaryCamera(selected);
                        }
                        sceneDirty = true;
                    }
                    if (ImGui::DragFloat3("Look At", &cam.LookAtTarget.x, 0.02f)) {
                        sceneDirty = true;
                    }
                    float fovDeg = Nova::Degrees(cam.FovYRadians);
                    if (ImGui::DragFloat("FOV (deg)", &fovDeg, 0.5f, 20.0f, 120.0f)) {
                        cam.FovYRadians = Nova::Radians(fovDeg);
                        sceneDirty = true;
                    }
                }
                if (scene.HasDirectionalLight(selected)) {
                    Nova::DirectionalLightComponent& sun = scene.GetDirectionalLight(selected);
                    ImGui::TextUnformatted("Directional Light (Sun)");
                    ImGui::TextUnformatted("Direction = ray travel (sun → scene)");
                    if (ImGui::DragFloat3("Ray Direction", &sun.Direction.x, 0.02f, -1.0f, 1.0f)) {
                        sun.Direction = sun.Direction.Normalized();
                        sceneDirty = true;
                    }
                    if (ImGui::ColorEdit3("Color", &sun.Color.x)) {
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
            if (ImGui::RadioButton("Move (M)", viewportTool == ViewportTool::Move)) {
                viewportTool = ViewportTool::Move;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Rotate (R)", viewportTool == ViewportTool::Rotate)) {
                viewportTool = ViewportTool::Rotate;
            }
            if (viewportTool == ViewportTool::Move) {
                ImGui::SameLine();
                if (ImGui::RadioButton("Free", moveAxis == MoveAxis::Free)) moveAxis = MoveAxis::Free;
                ImGui::SameLine();
                if (ImGui::RadioButton("X", moveAxis == MoveAxis::X)) moveAxis = MoveAxis::X;
                ImGui::SameLine();
                if (ImGui::RadioButton("Y", moveAxis == MoveAxis::Y)) moveAxis = MoveAxis::Y;
                ImGui::SameLine();
                if (ImGui::RadioButton("Z", moveAxis == MoveAxis::Z)) moveAxis = MoveAxis::Z;
            }
            ImGui::TextUnformatted("Gray area: LMB tool | RMB orbit | wheel zoom");
            const std::string relScene =
                Nova::MakeProjectRelativePath(project, scenePath).generic_string();
            ImGui::Text("Project: %s", project.Root.filename().string().c_str());
            ImGui::Text("Scene: %s", relScene.c_str());
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

            const ImVec2 canvasMin = ImGui::GetItemRectMin();
            const ImVec2 canvasMax = ImGui::GetItemRectMax();
            const ImVec2 canvasPxSize = ImVec2(canvasMax.x - canvasMin.x, canvasMax.y - canvasMin.y);
            const ImVec2 fbScale = ImGui::GetIO().DisplayFramebufferScale;
            frameViewport.CanvasMin = canvasMin;
            frameViewport.CanvasSize = canvasPxSize;
            frameViewport.Gpu.X = static_cast<uint32_t>(canvasMin.x * fbScale.x);
            frameViewport.Gpu.Y = static_cast<uint32_t>(canvasMin.y * fbScale.y);
            frameViewport.Gpu.Width = static_cast<uint32_t>(canvasPxSize.x * fbScale.x);
            frameViewport.Gpu.Height = static_cast<uint32_t>(canvasPxSize.y * fbScale.y);
            frameViewport.Gpu.Active =
                frameViewport.Gpu.Width >= 8 && frameViewport.Gpu.Height >= 8;
            frameViewport.Aspect =
                canvasPxSize.y > 0.0f ? canvasPxSize.x / canvasPxSize.y : 16.0f / 9.0f;

            if (selected.IsValid() && scene.IsAlive(selected) &&
                viewportTool == ViewportTool::Move) {
                Nova::Camera cam;
                if (Nova::BuildSceneCamera(scene, frameViewport.Aspect, cam)) {
                    const float axisLen =
                        std::max(0.25f, scene.GetTransform(selected).Scale.x * 0.75f);
                    DrawTranslationGizmo(ImGui::GetWindowDrawList(), frameViewport,
                                           cam.GetViewProjectionMatrix(),
                                           scene.GetTransform(selected).Position, axisLen);
                }
            }
        ImGui::End();

        if (ImGui::Shortcut(ImGuiKey_M, ImGuiInputFlags_RouteGlobal)) {
            viewportTool = ViewportTool::Move;
        }
        if (ImGui::Shortcut(ImGuiKey_R, ImGuiInputFlags_RouteGlobal)) {
            viewportTool = ViewportTool::Rotate;
        }

        if (viewportHovered) {
            const float dx = input.GetMouseDeltaX();
            const float dy = input.GetMouseDeltaY();

            if (selected.IsValid() && scene.IsAlive(selected)) {
                Nova::Transform& xform = scene.GetTransform(selected);
                if (input.IsMouseButtonDown(Nova::MouseButton::Left) && (dx != 0.0f || dy != 0.0f)) {
                    if (viewportTool == ViewportTool::Move) {
                        const float s = 0.01f;
                        switch (moveAxis) {
                        case MoveAxis::X:
                            xform.Position.x += dx * s;
                            break;
                        case MoveAxis::Y:
                            xform.Position.y -= dy * s;
                            break;
                        case MoveAxis::Z:
                            xform.Position.z -= dy * s;
                            break;
                        case MoveAxis::Free:
                            xform.Position.x += dx * s;
                            xform.Position.y -= dy * s;
                            break;
                        }
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

        if (frameViewport.Gpu.Active) {
            renderer->SetRenderViewport(frameViewport.Gpu);
        } else {
            Nova::RenderViewport fullVp;
            fullVp.Active = false;
            renderer->SetRenderViewport(fullVp);
        }

        float renderAspect = frameViewport.Aspect;
        if (!frameViewport.Gpu.Active) {
            uint32_t fbW = 0, fbH = 0;
            window.GetFramebufferSize(fbW, fbH);
            renderAspect = fbH > 0 ? static_cast<float>(fbW) / static_cast<float>(fbH)
                                   : 16.0f / 9.0f;
        }
        Nova::RenderScene(activeScene, *renderer, renderAspect, project.Root, meshCache);

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
