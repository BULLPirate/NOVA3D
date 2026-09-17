#include <Nova/Core/Log.h>
#include <Nova/Core/Input.h>
#include <Nova/Core/Time.h>
#include <Nova/Platform/MacApp.h>
#include <Nova/Platform/Window.h>
#include <Nova/Project/Project.h>
#include <Nova/Scene/Scene.h>
#include <Nova/Scene/SceneSerialization.h>
#include <Nova/Assets/MeshCache.h>
#include <Nova/Assets/MeshLoader.h>
#include <Nova/Assets/TextureCache.h>
#include <Nova/Scene/SceneRendererBridge.h>
#include <Nova/Scene/ScenePicking.h>
#include <Nova/Scene/SceneRuntime.h>
#include <Nova/Plugins/BuiltinPlugins.h>
#include <Nova/Plugins/PluginRegistry.h>
#include <Nova/Plugins/ServiceHub.h>

#include <Editor/ViewportManipulator.h>
#include <Editor/EditorHistory.h>
#include <Editor/EditorLogSink.h>

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

Nova::Vec3 CameraForward(const Nova::Camera& camera) {
    return (camera.Target - camera.Position).Normalized();
}

Nova::Vec3 CameraRight(const Nova::Camera& camera) {
    return CameraForward(camera).Cross(camera.Up).Normalized();
}

Nova::Vec3 CameraUp(const Nova::Camera& camera) {
    return CameraRight(camera).Cross(CameraForward(camera)).Normalized();
}

float ViewportRotationRadiansPerPixel(float viewportHeightPx) {
    return 2.8f / std::max(viewportHeightPx, 160.0f);
}

void PanOrbitTarget(OrbitCamera& orbit,
                    float deltaX,
                    float deltaY,
                    const Nova::Camera& camera,
                    float viewportHeightPx) {
    const Nova::Vec3 right = CameraRight(camera);
    const Nova::Vec3 up = CameraUp(camera);
    const float scale = orbit.Distance * 2.0f / std::max(viewportHeightPx, 64.0f);
    orbit.Target = orbit.Target - right * (deltaX * scale) + up * (deltaY * scale);
}

void OrbitFromMouseDelta(OrbitCamera& orbit, float deltaX, float deltaY, float radiansPerPixel) {
    orbit.YawRadians += deltaX * radiansPerPixel;
    orbit.PitchRadians += deltaY * radiansPerPixel;
    orbit.PitchRadians = Nova::Clamp(orbit.PitchRadians, -1.55f, 1.55f);
}

Nova::Vec3 WorldPositionFromMatrix(const Nova::Mat4& world) {
    return {world.m[3][0], world.m[3][1], world.m[3][2]};
}

int EntityHierarchyDepth(const Nova::Scene& scene, Nova::Entity entity) {
    int depth = 0;
    Nova::Entity parent = scene.GetParent(entity);
    while (parent.IsValid()) {
        ++depth;
        parent = scene.GetParent(parent);
    }
    return depth;
}

void FocusOrbitOn(const Nova::Scene& scene, Nova::Entity entity, OrbitCamera& orbit) {
    if (!entity.IsValid() || !scene.IsAlive(entity)) {
        return;
    }
    orbit.Target = WorldPositionFromMatrix(scene.GetWorldMatrix(entity));
    orbit.Distance = Nova::Clamp(orbit.Distance, 1.5f, 12.0f);
}

const std::vector<Nova::Editor::FileFilter> kSceneFileFilters = {
    {"NOVA Scene", {"json"}},
};
const std::vector<Nova::Editor::FileFilter> kPrefabFileFilters = {
    {"NOVA Prefab", {"json"}},
};

enum class ViewportTool { Move, Rotate, Scale };

struct ViewportScaleSession {
    bool Active = false;
    Nova::Vec3 StartScale{1.0f, 1.0f, 1.0f};
    float StartMouseY = 0.0f;
};

enum class MoveAxis { Free, X, Y, Z };

struct FrameViewport {
    Nova::RenderViewport Gpu{};
    ImVec2 CanvasMin{};
    ImVec2 CanvasSize{};
    float Aspect = 16.0f / 9.0f;
    bool CanvasActive = false;
};

void DrawViewportGroundGrid(ImDrawList* drawList,
                          const FrameViewport& vp,
                          const Nova::Mat4& viewProj) {
    constexpr int kHalfLines = 8;
    constexpr float kStep = 0.5f;
    const ImU32 major = IM_COL32(80, 85, 95, 140);
    const ImU32 minor = IM_COL32(55, 58, 68, 90);

    auto drawSegment = [&](const Nova::Vec3& a, const Nova::Vec3& b, ImU32 color) {
        float ax = 0.0f;
        float ay = 0.0f;
        float bx = 0.0f;
        float by = 0.0f;
        if (!Nova::ProjectWorldToViewport(viewProj, a, vp.CanvasSize.x, vp.CanvasSize.y, ax, ay)) {
            return;
        }
        if (!Nova::ProjectWorldToViewport(viewProj, b, vp.CanvasSize.x, vp.CanvasSize.y, bx, by)) {
            return;
        }
        drawList->AddLine(ImVec2(vp.CanvasMin.x + ax, vp.CanvasMin.y + ay),
                          ImVec2(vp.CanvasMin.x + bx, vp.CanvasMin.y + by), color, 1.0f);
    };

    for (int i = -kHalfLines; i <= kHalfLines; ++i) {
        const float x = static_cast<float>(i) * kStep;
        const float z0 = static_cast<float>(-kHalfLines) * kStep;
        const float z1 = static_cast<float>(kHalfLines) * kStep;
        const ImU32 color = (i == 0) ? major : minor;
        drawSegment({x, 0.0f, z0}, {x, 0.0f, z1}, color);
    }
    for (int i = -kHalfLines; i <= kHalfLines; ++i) {
        const float z = static_cast<float>(i) * kStep;
        const float x0 = static_cast<float>(-kHalfLines) * kStep;
        const float x1 = static_cast<float>(kHalfLines) * kStep;
        const ImU32 color = (i == 0) ? major : minor;
        drawSegment({x0, 0.0f, z}, {x1, 0.0f, z}, color);
    }
}

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
    SwitchScene,
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
    Nova::Editor::AttachEditorLogSink();
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
    bool showAssetsPanel = true;
    bool showConsolePanel = true;
    Nova::Editor::ViewportRotateSession rotateSession;
    ViewportScaleSession scaleSession;
    bool moveHistoryPushed = false;
    Nova::Editor::EditorHistory history;
    std::vector<std::filesystem::path> projectAssetsCache;
    bool projectAssetsStale = true;
    bool isPlaying = false;
    Nova::Scene playScene;
    Nova::Clock clock;
    Nova::PluginRegistry plugins;
    Nova::ServiceHub services;
    Nova::RegisterBuiltinPlugins(plugins, services, project.Root / ".nova" / "storage");
    NOVA_LOG_INFO("Plugins loaded: {}", plugins.Count());
    Nova::MeshAssetCache meshCache;
    Nova::TextureAssetCache textureCache;
    auto startPlay = [&]() {
        playScene = Nova::CloneScene(scene);
        clock.Reset();
        isPlaying = true;
    };
    auto stopPlay = [&]() { isPlaying = false; };
    auto invalidateProjectCaches = [&]() {
        meshCache.Clear();
        textureCache.Clear();
        projectAssetsStale = true;
    };
    auto refreshProjectAssets = [&]() {
        if (projectAssetsStale) {
            projectAssetsCache = Nova::ListProjectAssets(project);
            projectAssetsStale = false;
        }
    };
    PendingNavigation pendingNav = PendingNavigation::None;
    std::filesystem::path pendingScenePath;
    bool showUnsavedPrompt = false;
    while (!window.ShouldClose()) {
        input.BeginFrame();
        window.PollEvents(input, [](const SDL_Event& event) {
            ImGui_ImplSDL3_ProcessEvent(&event);
        });
        clock.Tick();

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

        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal) && !isPlaying) {
            if (history.CanUndo()) {
                const std::string keepName =
                    selected.IsValid() && scene.IsAlive(selected) ? scene.GetName(selected)
                                                                 : std::string();
                if (history.Undo(scene)) {
                    selected = keepName.empty() ? Nova::Entity{} : scene.FindEntityByName(keepName);
                    sceneDirty = true;
                    rotateSession.Active = false;
                    scaleSession.Active = false;
                }
            }
        }
        if ((ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal) ||
             ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z,
                             ImGuiInputFlags_RouteGlobal)) &&
            !isPlaying) {
            if (history.CanRedo()) {
                const std::string keepName =
                    selected.IsValid() && scene.IsAlive(selected) ? scene.GetName(selected)
                                                                 : std::string();
                if (history.Redo(scene)) {
                    selected = keepName.empty() ? Nova::Entity{} : scene.FindEntityByName(keepName);
                    sceneDirty = true;
                }
            }
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
                history.Push(scene);
                Nova::Entity dup = scene.DuplicateEntity(selected);
                scene.SetName(dup, UniqueEntityName(scene, scene.GetName(selected).c_str()).c_str());
                selected = dup;
                sceneDirty = true;
            }
        }
        if (ImGui::Shortcut(ImGuiKey_Delete, ImGuiInputFlags_RouteGlobal) && !isPlaying) {
            if (selected.IsValid() && scene.IsAlive(selected)) {
                history.Push(scene);
                scene.DestroyEntity(selected);
                selected = Nova::Entity{};
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
                        invalidateProjectCaches();
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
                        invalidateProjectCaches();
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
            case PendingNavigation::SwitchScene: {
                if (!pendingScenePath.empty()) {
                    scenePath = pendingScenePath;
                    LoadSceneFromDisk(scenePath, scene, project, selected, orbit, sceneDirty);
                    pendingScenePath.clear();
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
                pendingScenePath.clear();
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
                if (ImGui::BeginMenu("Scenes in Project")) {
                    for (const std::filesystem::path& rel : Nova::ListProjectScenes(project)) {
                        const std::string label = rel.filename().string();
                        const bool isCurrent =
                            Nova::MakeProjectRelativePath(project, scenePath).generic_string() ==
                            rel.generic_string();
                        if (ImGui::MenuItem(label.c_str(), nullptr, isCurrent)) {
                            const std::filesystem::path abs = project.Root / rel;
                            if (isCurrent) {
                                continue;
                            }
                            if (sceneDirty) {
                                pendingScenePath = abs;
                                pendingNav = PendingNavigation::SwitchScene;
                                showUnsavedPrompt = true;
                            } else {
                                scenePath = abs;
                                LoadSceneFromDisk(scenePath, scene, project, selected, orbit,
                                                  sceneDirty);
                            }
                        }
                    }
                    ImGui::EndMenu();
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
                if (ImGui::MenuItem("New Empty Scene")) {
                    if (sceneDirty) {
                        pendingNav = PendingNavigation::None;
                        showUnsavedPrompt = true;
                    } else {
                        scene = Nova::Scene::CreateEmptyLevel();
                        selected = Nova::Entity{};
                        sceneDirty = true;
                        InitOrbitFromScene(scene, orbit);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Save Prefab...", nullptr,
                                    false, selected.IsValid() && scene.IsAlive(selected))) {
                    const std::filesystem::path defaultPath =
                        project.PrefabsDirectory() /
                        (scene.GetName(selected) + ".prefab.json");
                    std::error_code ec;
                    std::filesystem::create_directories(project.PrefabsDirectory(), ec);
                    if (auto file = Nova::Editor::ShowSaveFileDialog("Save Prefab", defaultPath,
                                                                     kPrefabFileFilters)) {
                        history.Push(scene);
                        const Nova::SceneIOResult io =
                            Nova::SavePrefabToFile(scene, selected, *file);
                        if (!io.Ok) {
                            NOVA_LOG_ERROR("Save prefab failed: {}", io.Error);
                        }
                    }
                }
                if (ImGui::BeginMenu("Prefabs in Project")) {
                    const auto prefabs = Nova::ListProjectPrefabs(project);
                    if (prefabs.empty()) {
                        ImGui::MenuItem("(none)", nullptr, false, false);
                    }
                    for (const std::filesystem::path& rel : prefabs) {
                        if (ImGui::MenuItem(rel.filename().string().c_str())) {
                            history.Push(scene);
                            Nova::Entity spawned{};
                            const Nova::SceneIOResult io =
                                Nova::InstantiatePrefabFromFile(project.Root / rel, scene, spawned);
                            if (io.Ok) {
                                selected = spawned;
                                sceneDirty = true;
                            } else {
                                NOVA_LOG_ERROR("Instantiate prefab failed: {}", io.Error);
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Instantiate Prefab...")) {
                    if (auto file = Nova::Editor::ShowOpenFileDialog("Instantiate Prefab",
                                                                     kPrefabFileFilters)) {
                        history.Push(scene);
                        Nova::Entity spawned{};
                        const Nova::SceneIOResult io =
                            Nova::InstantiatePrefabFromFile(*file, scene, spawned);
                        if (io.Ok) {
                            selected = spawned;
                            sceneDirty = true;
                        } else {
                            NOVA_LOG_ERROR("Instantiate prefab failed: {}", io.Error);
                        }
                    }
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
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Assets", nullptr, &showAssetsPanel);
                ImGui::MenuItem("Console", nullptr, &showConsolePanel);
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
                uint32_t fbW = 0, fbH = 0;
                window.GetFramebufferSize(fbW, fbH);
                const float rpp = ViewportRotationRadiansPerPixel(static_cast<float>(fbH));
                OrbitFromMouseDelta(orbit, input.GetMouseDeltaX(), input.GetMouseDeltaY(), rpp);
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
            Nova::TickScene(playScene, clock.DeltaSeconds());
            Nova::RenderScene(activeScene, *renderer, aspect, project.Root, meshCache, textureCache);
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
            if (ImGui::Button("Empty")) {
                history.Push(scene);
                Nova::Entity empty = scene.CreateEntity(UniqueEntityName(scene, "Entity").c_str());
                selected = empty;
                sceneDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Mesh")) {
                history.Push(scene);
                Nova::Entity cube = scene.CreateEntity(UniqueEntityName(scene, "Mesh").c_str());
                scene.AddMeshRenderer(cube);
                selected = cube;
                sceneDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Plane")) {
                history.Push(scene);
                Nova::Entity plane = scene.CreateEntity(UniqueEntityName(scene, "Plane").c_str());
                Nova::MeshRendererComponent mesh;
                mesh.Primitive = Nova::MeshPrimitive::UnitPlane;
                scene.AddMeshRenderer(plane, mesh);
                selected = plane;
                sceneDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Sphere")) {
                history.Push(scene);
                Nova::Entity sphere = scene.CreateEntity(UniqueEntityName(scene, "Sphere").c_str());
                Nova::MeshRendererComponent mesh;
                mesh.Primitive = Nova::MeshPrimitive::UnitSphere;
                scene.AddMeshRenderer(sphere, mesh);
                selected = sphere;
                sceneDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Sun")) {
                history.Push(scene);
                Nova::Entity sun = scene.CreateEntity(UniqueEntityName(scene, "Sun").c_str());
                Nova::DirectionalLightComponent light;
                light.Direction = light.Direction.Normalized();
                scene.AddDirectionalLight(sun, light);
                selected = sun;
                sceneDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Camera")) {
                history.Push(scene);
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
            ImGui::SameLine();
            if (ImGui::Button("Point")) {
                history.Push(scene);
                Nova::Entity lamp = scene.CreateEntity(UniqueEntityName(scene, "Point Light").c_str());
                scene.AddPointLight(lamp);
                scene.GetTransform(lamp).Position = {0.8f, 1.2f, 0.6f};
                selected = lamp;
                sceneDirty = true;
            }
            if (ImGui::Button("Dup") && selected.IsValid() && scene.IsAlive(selected)) {
                history.Push(scene);
                Nova::Entity dup = scene.DuplicateEntity(selected);
                scene.SetName(dup, UniqueEntityName(scene, scene.GetName(selected).c_str()).c_str());
                selected = dup;
                sceneDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete") && selected.IsValid() && scene.IsAlive(selected)) {
                history.Push(scene);
                scene.DestroyEntity(selected);
                selected = Nova::Entity{};
                sceneDirty = true;
            }
            ImGui::Separator();
            scene.ForEachEntity([&](Nova::Entity entity) {
                const bool isSelected = selected.IsValid() && entity.Id == selected.Id;
                const int depth = EntityHierarchyDepth(scene, entity);
                std::string label(depth * 2, ' ');
                label += scene.GetName(entity);
                if (scene.HasCamera(entity) && scene.GetCamera(entity).IsPrimary) {
                    label += " [Cam]";
                }
                if (scene.HasDirectionalLight(entity)) {
                    label += " [Light]";
                }
                if (scene.HasPointLight(entity)) {
                    label += " [Point]";
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
            ImGui::TextUnformatted("Scene Environment");
            if (ImGui::ColorEdit3("Clear Color", &scene.Settings().ClearColor.x)) {
                sceneDirty = true;
            }
            ImGui::Separator();
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
                ImGui::Separator();
                ImGui::TextUnformatted("Transform");
                Nova::Transform& xform = scene.GetTransform(selected);
                if (ImGui::DragFloat3("Position", &xform.Position.x, 0.02f)) {
                    sceneDirty = true;
                }
                if (rotateSession.Active) {
                    ImGui::TextUnformatted("Rotation: (viewport drag)");
                } else {
                    Nova::Vec3 eulerDeg = xform.Rotation.ToEulerYXZRadians();
                    eulerDeg.x = Nova::Degrees(eulerDeg.x);
                    eulerDeg.y = Nova::Degrees(eulerDeg.y);
                    eulerDeg.z = Nova::Degrees(eulerDeg.z);
                    if (ImGui::DragFloat3("Rotation (deg)", &eulerDeg.x, 0.5f, -360.0f, 360.0f)) {
                        xform.Rotation = Nova::Quat::FromEulerYXZRadians(
                            {Nova::Radians(eulerDeg.x), Nova::Radians(eulerDeg.y),
                             Nova::Radians(eulerDeg.z)});
                        sceneDirty = true;
                    }
                }
                if (ImGui::DragFloat3("Scale", &xform.Scale.x, 0.02f, 0.01f, 10.0f)) {
                    sceneDirty = true;
                }
                const Nova::Entity currentParent = scene.GetParent(selected);
                const std::string parentLabel =
                    currentParent.IsValid() ? scene.GetName(currentParent) : "(none)";
                if (ImGui::BeginCombo("Parent", parentLabel.c_str())) {
                    if (ImGui::Selectable("(none)", !currentParent.IsValid())) {
                        scene.SetParent(selected, Nova::Entity{});
                        sceneDirty = true;
                    }
                    scene.ForEachEntity([&](Nova::Entity candidate) {
                        if (candidate.Id == selected.Id) {
                            return;
                        }
                        const bool picked =
                            currentParent.IsValid() && candidate.Id == currentParent.Id;
                        if (ImGui::Selectable(scene.GetName(candidate).c_str(), picked)) {
                            scene.SetParent(selected, candidate);
                            sceneDirty = true;
                        }
                    });
                    ImGui::EndCombo();
                }
                if (scene.HasMeshRenderer(selected)) {
                    Nova::MeshRendererComponent& mesh = scene.GetMeshRenderer(selected);
                    ImGui::TextUnformatted("Mesh Renderer");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove")) {
                        scene.RemoveMeshRenderer(selected);
                        sceneDirty = true;
                    }
                    char assetBuf[256] = {};
                    std::snprintf(assetBuf, sizeof(assetBuf), "%s", mesh.AssetPath.c_str());
                    if (ImGui::InputText("Asset (OBJ/glTF)", assetBuf, sizeof(assetBuf))) {
                        mesh.AssetPath = assetBuf;
                        sceneDirty = true;
                    }
                    int primIndex = static_cast<int>(mesh.Primitive);
                    const char* primLabels[] = {"UnitCube", "UnitPlane", "UnitSphere"};
                    if (ImGui::Combo("Primitive", &primIndex, primLabels, 3)) {
                        mesh.Primitive = static_cast<Nova::MeshPrimitive>(primIndex);
                        sceneDirty = true;
                    }
                    ImGui::TextUnformatted("Empty asset = built-in primitive mesh");
                    if (ImGui::ColorEdit3("Albedo", &mesh.AlbedoColor.x)) {
                        sceneDirty = true;
                    }
                    char texBuf[256] = {};
                    std::snprintf(texBuf, sizeof(texBuf), "%s", mesh.AlbedoTexturePath.c_str());
                    if (ImGui::InputText("Albedo PNG", texBuf, sizeof(texBuf))) {
                        mesh.AlbedoTexturePath = texBuf;
                        sceneDirty = true;
                    }
                    if (ImGui::Checkbox("Sample Texture", &mesh.UseAlbedoTexture)) {
                        sceneDirty = true;
                    }
                } else if (ImGui::Button("Add Mesh Renderer")) {
                    scene.AddMeshRenderer(selected);
                    sceneDirty = true;
                }
                if (scene.HasRotator(selected)) {
                    Nova::RotatorComponent& rot = scene.GetRotator(selected);
                    ImGui::TextUnformatted("Rotator");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove")) {
                        scene.RemoveRotator(selected);
                        sceneDirty = true;
                    }
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
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove")) {
                        scene.RemoveMover(selected);
                        sceneDirty = true;
                    }
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
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove")) {
                        scene.RemoveCamera(selected);
                        sceneDirty = true;
                    }
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
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove")) {
                        scene.RemoveDirectionalLight(selected);
                        sceneDirty = true;
                    }
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
                if (scene.HasPointLight(selected)) {
                    Nova::PointLightComponent& lamp = scene.GetPointLight(selected);
                    ImGui::TextUnformatted("Point Light");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove##point")) {
                        scene.RemovePointLight(selected);
                        sceneDirty = true;
                    }
                    if (ImGui::ColorEdit3("Point Color", &lamp.Color.x)) {
                        sceneDirty = true;
                    }
                    if (ImGui::DragFloat("Intensity", &lamp.Intensity, 0.05f, 0.0f, 20.0f)) {
                        sceneDirty = true;
                    }
                    if (ImGui::DragFloat("Range", &lamp.Range, 0.05f, 0.1f, 40.0f)) {
                        sceneDirty = true;
                    }
                } else if (ImGui::Button("Add Point Light")) {
                    scene.AddPointLight(selected);
                    sceneDirty = true;
                }
            } else {
                ImGui::TextUnformatted("Select an entity in Hierarchy.");
            }
        ImGui::End();

        if (showAssetsPanel) {
            ImGui::SetNextWindowSize(ImVec2(420.0f, 260.0f), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Assets", &showAssetsPanel)) {
                ImGui::TextUnformatted("Click to assign to selected Mesh (if any).");
                refreshProjectAssets();
                for (const std::filesystem::path& rel : projectAssetsCache) {
                    const std::string path = rel.generic_string();
                    if (ImGui::Selectable(path.c_str())) {
                        if (selected.IsValid() && scene.IsAlive(selected) &&
                            scene.HasMeshRenderer(selected)) {
                            Nova::MeshRendererComponent& mesh = scene.GetMeshRenderer(selected);
                            const std::string ext = rel.extension().string();
                            if (ext == ".png") {
                                mesh.AlbedoTexturePath = path;
                                mesh.UseAlbedoTexture = true;
                            } else {
                                mesh.AssetPath = path;
                                const Nova::AssetLoadResult loaded =
                                    Nova::LoadMeshAsset(project.Root / rel);
                                if (loaded.Ok && loaded.HasMaterialBaseColor) {
                                    mesh.AlbedoColor = loaded.MaterialBaseColor;
                                }
                            }
                            sceneDirty = true;
                        }
                    }
                }
            }
            ImGui::End();
        }

        if (showConsolePanel) {
            ImGui::SetNextWindowSize(ImVec2(520.0f, 180.0f), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Console", &showConsolePanel)) {
                if (ImGui::Button("Clear")) {
                    Nova::Editor::ClearEditorLog();
                }
                ImGui::Separator();
                ImGui::BeginChild("##log", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
                                  ImGuiWindowFlags_HorizontalScrollbar);
                const std::vector<std::string> lines = Nova::Editor::CopyEditorLogLines();
                for (const std::string& line : lines) {
                    ImGui::TextUnformatted(line.c_str());
                }
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 8.0f) {
                    ImGui::SetScrollHereY(1.0f);
                }
                ImGui::EndChild();
            }
            ImGui::End();
        }

        const float centerX = workPos.x + kSidePanelWidth;
        const float centerW = workSize.x - kSidePanelWidth * 2.0f;
        ImGui::SetNextWindowPos(ImVec2(centerX, workPos.y));
        ImGui::SetNextWindowSize(ImVec2(centerW, workSize.y));
        ImGui::Begin("Viewport", nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);
            if (ImGui::RadioButton("Move (M)", viewportTool == ViewportTool::Move)) {
                viewportTool = ViewportTool::Move;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Rotate (R)", viewportTool == ViewportTool::Rotate)) {
                viewportTool = ViewportTool::Rotate;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Scale (S)", viewportTool == ViewportTool::Scale)) {
                viewportTool = ViewportTool::Scale;
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
                ImGui::SameLine();
                ImGui::TextUnformatted("Shift = snap 0.25");
            }
            ImGui::TextUnformatted(
                "LMB rotate: screen tumble (any face, any click) | RMB camera | S scale | Cmd+Z");
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
                if (viewportTool == ViewportTool::Rotate) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                } else {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }
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

            frameViewport.CanvasActive = ImGui::IsItemActive();

            Nova::Camera viewportCameraInPanel;
            const bool hasCamInPanel =
                Nova::BuildSceneCamera(scene, frameViewport.Aspect, viewportCameraInPanel);
            if (hasCamInPanel) {
                DrawViewportGroundGrid(ImGui::GetWindowDrawList(), frameViewport,
                                       viewportCameraInPanel.GetViewProjectionMatrix());
            }

            const ImVec2 mousePos = ImGui::GetIO().MousePos;
            const float mouseLocalX = mousePos.x - frameViewport.CanvasMin.x;
            const float mouseLocalY = mousePos.y - frameViewport.CanvasMin.y;

            if (hasCamInPanel && ImGui::IsItemActivated() &&
                ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                Nova::Ray pickRay;
                if (Nova::ViewportPointToRay(viewportCameraInPanel, mouseLocalX, mouseLocalY,
                                             frameViewport.CanvasSize.x,
                                             frameViewport.CanvasSize.y, pickRay)) {
                    const Nova::Entity hit = Nova::PickSceneMesh(scene, pickRay);
                    if (hit.IsValid()) {
                        selected = hit;
                        renameBuffer[0] = '\0';
                    }
                }
            }

            if (selected.IsValid() && scene.IsAlive(selected) && !scene.HasCamera(selected) &&
                hasCamInPanel && frameViewport.CanvasActive) {
                Nova::Transform& xform = scene.GetTransform(selected);
                if (viewportTool == ViewportTool::Rotate) {
                    if (!rotateSession.Active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        history.Push(scene);
                        Nova::Editor::BeginViewportRotateSession(
                            rotateSession, frameViewport.CanvasSize.y);
                    } else if (rotateSession.Active) {
                        const Nova::Quat before = xform.Rotation;
                        Nova::Editor::ApplyViewportRotateDrag(
                            rotateSession, xform, ImGui::GetIO().MouseDelta.x,
                            ImGui::GetIO().MouseDelta.y, viewportCameraInPanel);
                        if (before.Dot(xform.Rotation) < 0.99999f) {
                            sceneDirty = true;
                        }
                    }
                } else if (viewportTool == ViewportTool::Scale) {
                    if (ImGui::IsItemActivated()) {
                        history.Push(scene);
                        scaleSession.Active = true;
                        scaleSession.StartScale = xform.Scale;
                        scaleSession.StartMouseY = mouseLocalY;
                    }
                    if (scaleSession.Active) {
                        const float factor =
                            std::max(0.05f, 1.0f + (scaleSession.StartMouseY - mouseLocalY) * 0.01f);
                        xform.Scale = scaleSession.StartScale * factor;
                        sceneDirty = true;
                    }
                }
            }

            if (selected.IsValid() && scene.IsAlive(selected) &&
                viewportTool == ViewportTool::Move && hasCamInPanel) {
                Nova::Camera cam;
                if (Nova::BuildSceneCamera(scene, frameViewport.Aspect, cam)) {
                    const Nova::Vec3 worldPos =
                        WorldPositionFromMatrix(scene.GetWorldMatrix(selected));
                    const float axisLen =
                        std::max(0.25f, scene.GetTransform(selected).Scale.x * 0.75f);
                    DrawTranslationGizmo(ImGui::GetWindowDrawList(), frameViewport,
                                           cam.GetViewProjectionMatrix(), worldPos, axisLen);
                }
            }
        ImGui::End();

        if (ImGui::Shortcut(ImGuiKey_M, ImGuiInputFlags_RouteGlobal)) {
            viewportTool = ViewportTool::Move;
        }
        if (ImGui::Shortcut(ImGuiKey_R, ImGuiInputFlags_RouteGlobal)) {
            viewportTool = ViewportTool::Rotate;
        }
        if (ImGui::Shortcut(ImGuiKey_S, ImGuiInputFlags_RouteGlobal) &&
            !ImGui::GetIO().KeyCtrl) {
            viewportTool = ViewportTool::Scale;
        }

        Nova::Camera viewportCamera;
        const bool hasViewportCamera =
            Nova::BuildSceneCamera(scene, frameViewport.Aspect, viewportCamera);
        const float rotPerPixel =
            ViewportRotationRadiansPerPixel(std::max(frameViewport.CanvasSize.y, 64.0f));

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            Nova::Editor::ResetViewportRotateSession(rotateSession);
            scaleSession.Active = false;
            moveHistoryPushed = false;
        }

        if (viewportHovered) {
            const float dx = input.GetMouseDeltaX();
            const float dy = input.GetMouseDeltaY();
            if (ImGui::Shortcut(ImGuiKey_F, ImGuiInputFlags_RouteGlobal) && selected.IsValid() &&
                scene.IsAlive(selected)) {
                FocusOrbitOn(scene, selected, orbit);
                sceneDirty = true;
            }
            if (ImGui::Shortcut(ImGuiKey_Home, ImGuiInputFlags_RouteGlobal) ||
                ImGui::Shortcut(ImGuiKey_Keypad5, ImGuiInputFlags_RouteGlobal)) {
                orbit.YawRadians = 0.0f;
                orbit.PitchRadians = 0.25f;
                orbit.Distance = 3.3f;
                if (selected.IsValid() && scene.IsAlive(selected)) {
                    orbit.Target = WorldPositionFromMatrix(scene.GetWorldMatrix(selected));
                } else {
                    orbit.Target = {0.0f, 0.0f, 0.0f};
                }
                sceneDirty = true;
            }

            if (selected.IsValid() && scene.IsAlive(selected) && !scene.HasCamera(selected)) {
                Nova::Transform& xform = scene.GetTransform(selected);
                if (frameViewport.CanvasActive && input.IsMouseButtonDown(Nova::MouseButton::Left) &&
                    (dx != 0.0f || dy != 0.0f) && viewportTool == ViewportTool::Move) {
                        if (!moveHistoryPushed) {
                            history.Push(scene);
                            moveHistoryPushed = true;
                        }
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
                            if (hasViewportCamera) {
                                const Nova::Vec3 right = CameraRight(viewportCamera);
                                const Nova::Vec3 up = CameraUp(viewportCamera);
                                const float moveScale = s * 2.0f;
                                xform.Position =
                                    xform.Position + right * (dx * moveScale) - up * (dy * moveScale);
                            } else {
                                xform.Position.x += dx * s;
                                xform.Position.y -= dy * s;
                            }
                            break;
                        }
                        if (ImGui::GetIO().KeyShift) {
                            xform.Position = Nova::Editor::SnapPositionToGrid(xform.Position);
                        }
                        sceneDirty = true;
                    }
            }

            if (input.IsMouseButtonDown(Nova::MouseButton::Right) && (dx != 0.0f || dy != 0.0f)) {
                OrbitFromMouseDelta(orbit, dx, dy, rotPerPixel);
                sceneDirty = true;
            }
            if (hasViewportCamera &&
                input.IsMouseButtonDown(Nova::MouseButton::Middle) &&
                (dx != 0.0f || dy != 0.0f)) {
                PanOrbitTarget(orbit, dx, dy, viewportCamera, frameViewport.CanvasSize.y);
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
        Nova::RenderScene(activeScene, *renderer, renderAspect, project.Root, meshCache, textureCache);

        ImGui::Render();
        renderer->BeginDrawing();
        renderer->EndFrame();
    }

    ImGui_ImplMetal_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    renderer->Shutdown();
    Nova::Editor::DetachEditorLogSink();
    Nova::Log::Shutdown();
    return 0;
}
