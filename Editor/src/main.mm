#include <Nova/Core/Log.h>
#include <Nova/Core/Input.h>
#include <Nova/Core/Time.h>
#include <Nova/Core/EngineSettings.h>
#include <Nova/Core/FileSystem.h>
#include <Nova/Audio/AudioEngine.h>
#include <Nova/Scene/Script.h>
#include <Editor/I18n.h>
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
#include <Nova/Scene/Gameplay.h>
#include <Nova/Scene/Placement.h>
#include <Nova/Scene/SaveGame.h>
#include <Nova/Project/Workspace.h>
#include <Nova/Project/ProjectTemplate.h>
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

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <optional>
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
    const Nova::Vec3 feet = WorldPositionFromMatrix(scene.GetWorldMatrix(entity));
    const Nova::Vec3 scale = scene.GetTransform(entity).Scale;
    const float lift = std::max(0.8f, scale.y * 0.55f);
    orbit.Target = {feet.x, feet.y + lift, feet.z};
    const float radius = std::max(std::max(scale.x, scale.y), scale.z);
    orbit.Distance = Nova::Clamp(std::max(4.5f, radius * 4.2f), 4.5f, 22.0f);
    orbit.PitchRadians = 0.42f;
}

const char* PlaceableI18nKey(Nova::PlaceableKind kind) {
    switch (kind) {
    case Nova::PlaceableKind::Cube:
        return "place_cube";
    case Nova::PlaceableKind::Sphere:
        return "place_sphere";
    case Nova::PlaceableKind::Plane:
        return "place_plane";
    case Nova::PlaceableKind::House:
        return "place_house";
    case Nova::PlaceableKind::Tree:
        return "place_tree";
    case Nova::PlaceableKind::Crate:
        return "place_crate";
    case Nova::PlaceableKind::Wall:
        return "place_wall";
    case Nova::PlaceableKind::Knight:
        return "place_knight";
    case Nova::PlaceableKind::Bandit:
        return "place_bandit";
    case Nova::PlaceableKind::Herb:
        return "place_herb";
    case Nova::PlaceableKind::Road:
        return "place_road";
    case Nova::PlaceableKind::Fence:
        return "place_fence";
    case Nova::PlaceableKind::Well:
        return "place_well";
    }
    return "place_cube";
}

void FrameOrbitOnScene(const Nova::Scene& scene, OrbitCamera& orbit) {
    bool any = false;
    Nova::Vec3 mn{0.0f, 0.0f, 0.0f};
    Nova::Vec3 mx{0.0f, 0.0f, 0.0f};
    scene.ForEachEntity([&](Nova::Entity entity) {
        if (!scene.HasMeshRenderer(entity) || scene.HasCamera(entity)) {
            return;
        }
        const Nova::Vec3 p = WorldPositionFromMatrix(scene.GetWorldMatrix(entity));
        const Nova::Vec3 h = scene.GetTransform(entity).Scale * 0.5f;
        if (!any) {
            mn = p - h;
            mx = p + h;
            any = true;
            return;
        }
        mn.x = std::min(mn.x, p.x - h.x);
        mn.y = std::min(mn.y, p.y - h.y);
        mn.z = std::min(mn.z, p.z - h.z);
        mx.x = std::max(mx.x, p.x + h.x);
        mx.y = std::max(mx.y, p.y + h.y);
        mx.z = std::max(mx.z, p.z + h.z);
    });
    if (!any) {
        Nova::Editor::FrameOrbitOnBounds(orbit.YawRadians, orbit.PitchRadians, orbit.Distance,
                                         orbit.Target, {0.0f, 1.0f, 0.0f}, 8.0f);
        return;
    }
    const Nova::Vec3 center{(mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f + 0.4f,
                            (mn.z + mx.z) * 0.5f};
    const float radius = std::max(mx.x - mn.x, std::max(mx.y - mn.y, mx.z - mn.z)) * 0.5f;
    Nova::Editor::FrameOrbitOnBounds(orbit.YawRadians, orbit.PitchRadians, orbit.Distance,
                                     orbit.Target, center, radius);
}

const std::vector<Nova::Editor::FileFilter> kSceneFileFilters = {
    {"NOVA Scene", {"json"}},
};
const std::vector<Nova::Editor::FileFilter> kPrefabFileFilters = {
    {"NOVA Prefab", {"json"}},
};
const std::vector<Nova::Editor::FileFilter> kMeshFileFilters = {
    {"Blender / glTF / OBJ", {"gltf", "glb", "obj"}},
};
const std::vector<Nova::Editor::FileFilter> kAudioFileFilters = {
    {"WAV", {"wav"}},
};

enum class WorkspaceTab { Projects, Scene, Content, Engine, Audio, Settings };

void ApplyNovaEditorStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 8.0f;
    style.ScrollbarRounding = 10.0f;
    style.WindowPadding = ImVec2(14.0f, 12.0f);
    style.FramePadding = ImVec2(11.0f, 7.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.IndentSpacing = 16.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = ImVec4(0.93f, 0.94f, 0.96f, 1.0f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.58f, 0.64f, 1.0f);
    c[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.10f, 0.13f, 0.99f);
    c[ImGuiCol_ChildBg] = ImVec4(0.11f, 0.12f, 0.16f, 0.96f);
    c[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.11f, 0.15f, 0.98f);
    c[ImGuiCol_Border] = ImVec4(0.20f, 0.24f, 0.30f, 0.40f);
    c[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.16f, 0.21f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.24f, 0.34f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.34f, 0.48f, 1.0f);
    c[ImGuiCol_TitleBg] = ImVec4(0.07f, 0.08f, 0.10f, 1.0f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.13f, 0.18f, 1.0f);
    c[ImGuiCol_Button] = ImVec4(0.18f, 0.36f, 0.52f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.48f, 0.68f, 1.0f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.58f, 0.78f, 1.0f);
    c[ImGuiCol_Header] = ImVec4(0.16f, 0.28f, 0.40f, 1.0f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.40f, 0.56f, 1.0f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.48f, 0.66f, 1.0f);
    c[ImGuiCol_Tab] = ImVec4(0.12f, 0.14f, 0.18f, 1.0f);
    c[ImGuiCol_TabHovered] = ImVec4(0.24f, 0.42f, 0.60f, 1.0f);
    c[ImGuiCol_TabSelected] = ImVec4(0.20f, 0.38f, 0.56f, 1.0f);
    c[ImGuiCol_Separator] = ImVec4(0.22f, 0.26f, 0.32f, 0.45f);
    c[ImGuiCol_CheckMark] = ImVec4(0.55f, 0.86f, 1.0f, 1.0f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.42f, 0.76f, 0.96f, 1.0f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.55f, 0.86f, 1.0f, 1.0f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.09f, 0.11f, 1.0f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.28f, 0.34f, 1.0f);
}

void LoadNovaUiFont(ImGuiIO& io) {
    const char* candidates[] = {
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial Unicode.ttf",
        "/System/Library/Fonts/Supplemental/Times New Roman.ttf",
        "/System/Library/Fonts/AppleSDGothicNeo.ttc",
    };
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = true;
    const ImWchar* ranges = io.Fonts->GetGlyphRangesCyrillic();
    for (const char* path : candidates) {
        if (!std::filesystem::exists(path)) {
            continue;
        }
        if (io.Fonts->AddFontFromFileTTF(path, 17.0f, &cfg, ranges)) {
            io.FontDefault = io.Fonts->Fonts.back();
            return;
        }
    }
}

std::optional<std::filesystem::path> ImportFileIntoProject(
    const Nova::ProjectDescriptor& project, const std::filesystem::path& source,
    const std::filesystem::path& destDirRel) {
    std::error_code ec;
    const std::filesystem::path destDir = project.Root / destDirRel;
    std::filesystem::create_directories(destDir, ec);
    const std::filesystem::path dest = destDir / source.filename();
    std::filesystem::copy_file(source, dest, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        NOVA_LOG_ERROR("Import failed: {}", ec.message());
        return std::nullopt;
    }
    const std::filesystem::path parent = source.parent_path();
    const std::string stem = source.stem().string();
    const char* sidecars[] = {".mtl", ".png", ".jpg", ".jpeg", ".tga"};
    for (const char* ext : sidecars) {
        const std::filesystem::path side = parent / (stem + ext);
        if (std::filesystem::is_regular_file(side, ec)) {
            std::filesystem::copy_file(side, destDir / side.filename(),
                                       std::filesystem::copy_options::overwrite_existing, ec);
        }
    }
    const std::filesystem::path texDir = parent / "textures";
    if (std::filesystem::is_directory(texDir, ec)) {
        std::filesystem::copy(texDir, destDir / "textures",
                              std::filesystem::copy_options::overwrite_existing |
                                  std::filesystem::copy_options::recursive,
                              ec);
    }
    return Nova::MakeProjectRelativePath(project, dest);
}

void DrawKeyBindingCombo(const char* label, Nova::EngineSettings& settings, const std::string& action) {
    const Nova::KeyCode current = settings.Binding(action);
    const std::string preview = Nova::KeyCodeName(current);
    if (ImGui::BeginCombo(label, preview.c_str())) {
        for (const std::string& name : Nova::AllBindableKeyNames()) {
            Nova::KeyCode code = Nova::KeyCode::Unknown;
            if (!Nova::KeyCodeFromName(name, code)) {
                continue;
            }
            const bool selected = code == current;
            if (ImGui::Selectable(name.c_str(), selected)) {
                settings.Bindings[action] = code;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

enum class ViewportTool { Move, Rotate, Scale, Place };

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
                          const Nova::Mat4& viewProj,
                          const Nova::Vec3& center,
                          float halfExtent,
                          float step) {
    const int kHalfLines = static_cast<int>(std::ceil(halfExtent / std::max(step, 0.25f)));
    const ImU32 major = IM_COL32(80, 85, 95, 140);
    const ImU32 minor = IM_COL32(55, 58, 68, 90);
    const float ox = std::round(center.x / step) * step;
    const float oz = std::round(center.z / step) * step;

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
        const float x = ox + static_cast<float>(i) * step;
        const float z0 = oz - static_cast<float>(kHalfLines) * step;
        const float z1 = oz + static_cast<float>(kHalfLines) * step;
        const ImU32 color = (i % 4 == 0) ? major : minor;
        drawSegment({x, 0.0f, z0}, {x, 0.0f, z1}, color);
    }
    for (int i = -kHalfLines; i <= kHalfLines; ++i) {
        const float z = oz + static_cast<float>(i) * step;
        const float x0 = ox - static_cast<float>(kHalfLines) * step;
        const float x1 = ox + static_cast<float>(kHalfLines) * step;
        const ImU32 color = (i % 4 == 0) ? major : minor;
        drawSegment({x0, 0.0f, z}, {x1, 0.0f, z}, color);
    }
}

void DrawPlacementGhost(ImDrawList* drawList, const FrameViewport& vp, const Nova::Mat4& viewProj,
                        const Nova::PlaceablePreview& ghost) {
    const Nova::Vec3 c = ghost.Center;
    const Nova::Vec3 h = ghost.HalfExtents;
    const Nova::Vec3 corners[8] = {
        {c.x - h.x, c.y - h.y, c.z - h.z}, {c.x + h.x, c.y - h.y, c.z - h.z},
        {c.x + h.x, c.y - h.y, c.z + h.z}, {c.x - h.x, c.y - h.y, c.z + h.z},
        {c.x - h.x, c.y + h.y, c.z - h.z}, {c.x + h.x, c.y + h.y, c.z - h.z},
        {c.x + h.x, c.y + h.y, c.z + h.z}, {c.x - h.x, c.y + h.y, c.z + h.z},
    };
    const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                              {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    const ImU32 color = IM_COL32(static_cast<int>(ghost.Color.x * 255.0f),
                                 static_cast<int>(ghost.Color.y * 255.0f),
                                 static_cast<int>(ghost.Color.z * 255.0f), 220);
    for (const auto& edge : edges) {
        float ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f;
        if (!Nova::ProjectWorldToViewport(viewProj, corners[edge[0]], vp.CanvasSize.x,
                                          vp.CanvasSize.y, ax, ay)) {
            continue;
        }
        if (!Nova::ProjectWorldToViewport(viewProj, corners[edge[1]], vp.CanvasSize.x,
                                          vp.CanvasSize.y, bx, by)) {
            continue;
        }
        drawList->AddLine(ImVec2(vp.CanvasMin.x + ax, vp.CanvasMin.y + ay),
                          ImVec2(vp.CanvasMin.x + bx, vp.CanvasMin.y + by), color, 2.0f);
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

void DrawRotationGizmo(ImDrawList* drawList,
                       const FrameViewport& vp,
                       const Nova::Mat4& viewProj,
                       const Nova::Vec3& origin,
                       float radius) {
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

    const Nova::Vec3 us[3] = {{0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
    const Nova::Vec3 vs[3] = {{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}};
    const ImU32 colors[3] = {IM_COL32(230, 80, 80, 255), IM_COL32(90, 220, 100, 255),
                             IM_COL32(90, 140, 255, 255)};
    constexpr int kSegs = 48;
    for (int a = 0; a < 3; ++a) {
        ImVec2 prev;
        bool havePrev = false;
        for (int i = 0; i <= kSegs; ++i) {
            const float t = (2.0f * 3.14159265f * static_cast<float>(i)) / kSegs;
            const Nova::Vec3 p =
                origin + (us[a] * std::cos(t) + vs[a] * std::sin(t)) * radius;
            ImVec2 screen;
            if (!toScreen(p, screen)) {
                havePrev = false;
                continue;
            }
            if (havePrev) {
                drawList->AddLine(prev, screen, colors[a], 2.5f);
            }
            prev = screen;
            havePrev = true;
        }
    }
}

void DrawScaleGizmo(ImDrawList* drawList,
                    const FrameViewport& vp,
                    const Nova::Mat4& viewProj,
                    const Nova::Vec3& origin,
                    float axisLength) {
    DrawTranslationGizmo(drawList, vp, viewProj, origin, axisLength);
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
    NOVA_LOG_INFO("NOVA3D");
    Nova::EnsureMacOSApplicationReady();

    Nova::Window window({"NOVA3D", 1440, 900});
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
    LoadNovaUiFont(io);
    ImGui::StyleColorsDark();
    ApplyNovaEditorStyle();
    ImGui_ImplSDL3_InitForMetal(window.GetSDLWindow());
    ImGui_ImplMetal_Init((__bridge id<MTLDevice>)renderer->GetNativeDevice());

    Nova::ProjectDescriptor project;
    Nova::UserWorkspace userWorkspace;
    Nova::LoadUserWorkspace(userWorkspace);
    Nova::EngineSettings engineSettings = Nova::EngineSettings::Defaults();
    Nova::AudioEngine::Get().Init();
    Nova::AudioEngine::Get().SetEnabled(false);
    Nova::AudioEngine::Get().SetMasterVolume(0.0f);
    engineSettings.EnableAudio = false;
    engineSettings.MasterVolume = 0.0f;

    Nova::Scene scene = Nova::Scene::CreateEmptyLevel();
    std::filesystem::path scenePath;
    bool sceneDirty = false;
    bool projectOpen = false;
    OrbitCamera orbit;
    InitOrbitFromScene(scene, orbit);
    FrameOrbitOnScene(scene, orbit);
    Nova::Entity selected{Nova::Entity::kInvalidEntity};
    bool broughtWindowForward = false;
    char newProjectName[128] = "New Game";
    char newProjectPath[1024] = {};
    int newProjectTemplate = 0;
    std::snprintf(newProjectPath, sizeof(newProjectPath), "%s",
                  (Nova::DefaultProjectsDirectory() / "NewGame").generic_string().c_str());

    bool viewportHovered = false;
    FrameViewport frameViewport;
    ViewportTool viewportTool = ViewportTool::Place;
    Nova::PlaceableKind placeKind = Nova::PlaceableKind::Cube;
    float placeYaw = 0.0f;
    Nova::Vec3 lastPlaceStamp{};
    bool lastPlaceValid = false;
    MoveAxis moveAxis = MoveAxis::Free;
    char renameBuffer[128] = {};
    Nova::Editor::ViewportGizmoSession gizmoSession;
    bool moveHistoryPushed = false;
    Nova::Editor::EditorHistory history;
    std::vector<std::filesystem::path> projectAssetsCache;
    bool projectAssetsStale = true;
    bool isPlaying = false;
    bool isPaused = false;
    bool playJustStarted = false;
    float editorPreviewOpacity = 1.0f;
    WorkspaceTab workspaceTab = WorkspaceTab::Projects;
    Nova::Scene playScene;
    Nova::Clock clock;
    Nova::PluginRegistry plugins;
    Nova::ServiceHub services;
    Nova::RegisterBuiltinPlugins(plugins, services,
                                 Nova::UserWorkspaceFilePath().parent_path() / "storage");
    NOVA_LOG_INFO("Plugins loaded: {}", plugins.Count());
    Nova::MeshAssetCache meshCache;
    Nova::TextureAssetCache textureCache;
    auto startPlay = [&]() {
        if (!projectOpen) {
            return;
        }
        playScene = Nova::CloneScene(scene);
        clock.Reset();
        playJustStarted = true;
        Nova::AudioEngine::Get().SetEnabled(false);
        Nova::AudioEngine::Get().SetMasterVolume(0.0f);
        engineSettings.MouseLook = true;
        window.SetCursorCaptured(true);
        isPlaying = true;
        isPaused = false;
    };
    auto stopPlay = [&]() {
        window.SetCursorCaptured(false);
        isPlaying = false;
        isPaused = false;
        engineSettings.MouseLook = true;
        FrameOrbitOnScene(scene, orbit);
    };
    auto invalidateProjectCaches = [&]() {
        meshCache.Clear();
        textureCache.Clear();
        projectAssetsStale = true;
    };
    auto refreshProjectAssets = [&]() {
        if (projectAssetsStale && projectOpen) {
            projectAssetsCache = Nova::ListProjectAssets(project);
            projectAssetsStale = false;
        }
    };
    auto applyOpenProject = [&](const Nova::ProjectDescriptor& opened) {
        project = opened;
        projectOpen = true;
        Nova::LoadEngineSettings(project.Root, engineSettings);
        engineSettings.EnableAudio = false;
        engineSettings.MasterVolume = 0.0f;
        Nova::AudioEngine::Get().RefreshUserSounds(project.Root);
        invalidateProjectCaches();
        scenePath = project.LastOpenedSceneAbsolute();
        if (!std::filesystem::exists(scenePath)) {
            scenePath = project.StartupSceneAbsolute();
        }
        if (!LoadSceneFromDisk(scenePath, scene, project, selected, orbit, sceneDirty)) {
            scene = Nova::Scene::CreateEmptyLevel();
            sceneDirty = true;
            if (!scenePath.empty()) {
                SaveSceneToDisk(scene, scenePath, project, sceneDirty);
            }
            InitOrbitFromScene(scene, orbit);
        }
        FrameOrbitOnScene(scene, orbit);
        selected = Nova::Entity{};
        workspaceTab = WorkspaceTab::Scene;
        Nova::RememberRecentProject(userWorkspace, project.Root);
        window.SetTitle("NOVA3D — " + project.Name);
    };
    auto closeProject = [&]() {
        if (isPlaying) {
            stopPlay();
        }
        projectOpen = false;
        project = Nova::ProjectDescriptor{};
        scene = Nova::Scene::CreateEmptyLevel();
        scenePath.clear();
        sceneDirty = false;
        selected = Nova::Entity{};
        InitOrbitFromScene(scene, orbit);
        FrameOrbitOnScene(scene, orbit);
        workspaceTab = WorkspaceTab::Projects;
        window.SetTitle("NOVA3D");
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
            }
        }
        if (isPlaying && input.IsKeyPressed(Nova::KeyCode::Tab)) {
            const bool capture = !window.IsCursorCaptured();
            window.SetCursorCaptured(capture);
            engineSettings.MouseLook = capture;
        }
        if (isPlaying && input.IsKeyPressed(Nova::KeyCode::F6)) {
            isPaused = !isPaused;
            window.SetCursorCaptured(!isPaused);
            engineSettings.MouseLook = !isPaused;
        }
        if (projectOpen && input.IsKeyPressed(Nova::KeyCode::F8)) {
            Nova::SaveGameSlot(project, "quick", isPlaying ? playScene : scene);
        }
        if (projectOpen && input.IsKeyPressed(Nova::KeyCode::F9)) {
            if (Nova::LoadGameSlot(project, "quick", isPlaying ? playScene : scene).Ok &&
                !isPlaying) {
                sceneDirty = true;
                FrameOrbitOnScene(scene, orbit);
            }
        }

        ImGui_ImplSDL3_NewFrame();
        renderer->BeginFrame();
        ImGui::NewFrame();

        auto tr = [&](const char* key) { return Nova::Editor::Tr(engineSettings.Language, key); };

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
                    Nova::Editor::ResetViewportGizmoSession(gizmoSession);
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
                            ImGuiInputFlags_RouteGlobal) &&
            projectOpen) {
            if (sceneDirty) {
                SaveSceneToDisk(scene, scenePath, project, sceneDirty);
            }
            Nova::Editor::LaunchGame(project, scenePath);
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D, ImGuiInputFlags_RouteGlobal) && !isPlaying) {
            if (selected.IsValid() && scene.IsAlive(selected)) {
                history.Push(scene);
                Nova::Entity dup = scene.DuplicateEntity(selected);
                scene.SetName(dup, scene.MakeUniqueName(scene.GetName(selected)));
                scene.GetTransform(dup).Position.x += 0.75f;
                Nova::PlantOnGround(scene, dup, scene.GetTransform(dup).Position);
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
                        applyOpenProject(opened);
                    }
                }
                break;
            }
            case PendingNavigation::NewProject: {
                if (auto folder = Nova::Editor::ShowOpenFolderDialog("Create Project Folder")) {
                    Nova::ProjectTemplateKind kind = Nova::ProjectTemplateKind::Empty;
                    if (newProjectTemplate == 1) {
                        kind = Nova::ProjectTemplateKind::ThirdPerson;
                    } else if (newProjectTemplate == 2) {
                        kind = Nova::ProjectTemplateKind::KnightBandits;
                    }
                    if (Nova::CreateGameProject(*folder, folder->filename().string(), kind,
                                                NOVA_SOURCE_DIR)
                            .Ok) {
                        Nova::ProjectDescriptor opened;
                        if (Nova::LoadProject(*folder, opened).Ok) {
                            applyOpenProject(opened);
                        }
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
            ImGui::TextUnformatted(tr("unsaved"));
            if (ImGui::Button(tr("save"), ImVec2(120, 0))) {
                SaveSceneToDisk(scene, scenePath, project, sceneDirty);
                showUnsavedPrompt = false;
                runPendingNavigation();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(tr("discard"), ImVec2(120, 0))) {
                sceneDirty = false;
                showUnsavedPrompt = false;
                runPendingNavigation();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(tr("cancel"), ImVec2(120, 0))) {
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
            if (ImGui::BeginMenu(tr("file"))) {
                if (ImGui::MenuItem(tr("open_project"))) {
                    requestNavigation(PendingNavigation::OpenProject);
                }
                if (ImGui::MenuItem(tr("new_project"))) {
                    requestNavigation(PendingNavigation::NewProject);
                }
                if (ImGui::MenuItem(tr("close_project"), nullptr, false, projectOpen)) {
                    closeProject();
                }
                ImGui::Separator();
                if (ImGui::MenuItem(tr("open_scene"))) {
                    requestNavigation(PendingNavigation::OpenScene);
                }
                if (ImGui::BeginMenu(tr("scenes_in_project"))) {
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
                if (ImGui::MenuItem(tr("save_scene"), "Cmd+S")) {
                    SaveSceneToDisk(scene, scenePath, project, sceneDirty);
                }
                if (ImGui::MenuItem(tr("save_scene_as"))) {
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
                if (ImGui::MenuItem(tr("reload"))) {
                    LoadSceneFromDisk(scenePath, scene, project, selected, orbit, sceneDirty);
                }
                if (ImGui::MenuItem(tr("new_empty_scene"))) {
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
                if (ImGui::MenuItem(tr("save_prefab"), nullptr,
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
                if (ImGui::BeginMenu(tr("prefabs_in_project"))) {
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
                if (ImGui::MenuItem(tr("instantiate_prefab"))) {
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
            if (ImGui::BeginMenu(tr("game"))) {
                if (ImGui::MenuItem(tr("new_playable"))) {
                    history.Push(scene);
                    scene = Nova::Scene::CreateSandboxLevel();
                    selected = scene.FindEntityByName("Player");
                    sceneDirty = true;
                    InitOrbitFromScene(scene, orbit);
                    if (selected.IsValid()) {
                        FocusOrbitOn(scene, selected, orbit);
                        orbit.Distance = 7.0f;
                    }
                }
                if (ImGui::MenuItem(tr("sample_village"))) {
                    history.Push(scene);
                    selected = Nova::SpawnSimpleMap(scene);
                    sceneDirty = true;
                    FrameOrbitOnScene(scene, orbit);
                }
                if (ImGui::MenuItem(tr("create_player"))) {
                    history.Push(scene);
                    selected = Nova::SpawnPlayer(scene);
                    Nova::PlaceOnGround(scene, selected);
                    FocusOrbitOn(scene, selected, orbit);
                    orbit.Distance = 6.5f;
                    sceneDirty = true;
                }
                if (ImGui::MenuItem(tr("import_character"))) {
                    if (auto file = Nova::Editor::ShowOpenFileDialog(tr("import_character"),
                                                                     kMeshFileFilters)) {
                        if (auto rel = ImportFileIntoProject(project, *file, "Assets/Characters")) {
                            history.Push(scene);
                            selected = Nova::SpawnPlayer(scene, rel->generic_string());
                            Nova::PlaceOnGround(scene, selected);
                            FocusOrbitOn(scene, selected, orbit);
                            orbit.Distance = 6.5f;
                            sceneDirty = true;
                            invalidateProjectCaches();
                        }
                    }
                }
                if (ImGui::MenuItem(tr("import_item"))) {
                    if (auto file = Nova::Editor::ShowOpenFileDialog(tr("import_item"),
                                                                     kMeshFileFilters)) {
                        if (auto rel = ImportFileIntoProject(project, *file, "Assets/Items")) {
                            history.Push(scene);
                            selected = Nova::SpawnImportedMesh(scene,
                                UniqueEntityName(scene, "Item"), rel->generic_string());
                            Nova::PlaceOnGround(scene, selected);
                            FocusOrbitOn(scene, selected, orbit);
                            sceneDirty = true;
                            invalidateProjectCaches();
                        }
                    }
                }
                if (ImGui::MenuItem(tr("import_map"))) {
                    if (auto file = Nova::Editor::ShowOpenFileDialog(tr("import_map"),
                                                                     kMeshFileFilters)) {
                        if (auto rel =
                                ImportFileIntoProject(project, *file, "Assets/Environment")) {
                            history.Push(scene);
                            selected = Nova::SpawnImportedMesh(scene,
                                UniqueEntityName(scene, "Map"), rel->generic_string());
                            Nova::PlaceOnGround(scene, selected);
                            FocusOrbitOn(scene, selected, orbit);
                            orbit.Distance = 12.0f;
                            sceneDirty = true;
                            invalidateProjectCaches();
                        }
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(tr("play_menu"))) {
                if (!isPlaying && ImGui::MenuItem(tr("play_scene"), "F5", false, projectOpen)) {
                    startPlay();
                }
                if (isPlaying && ImGui::MenuItem(tr("pause"), "F6")) {
                    isPaused = !isPaused;
                    window.SetCursorCaptured(!isPaused);
                    engineSettings.MouseLook = !isPaused;
                }
                if (isPlaying && ImGui::MenuItem(tr("stop"), "Esc")) {
                    stopPlay();
                }
                ImGui::Separator();
                if (ImGui::MenuItem(tr("run_standalone"), "Cmd+Shift+G", false, projectOpen)) {
                    if (sceneDirty) {
                        SaveSceneToDisk(scene, scenePath, project, sceneDirty);
                    }
                    Nova::Editor::LaunchGame(project, scenePath);
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (isPlaying) {
                if (ImGui::Button(tr("stop"))) {
                    stopPlay();
                }
            } else {
                ImGui::BeginDisabled(!projectOpen);
                if (ImGui::Button(tr("play"))) {
                    startPlay();
                }
                ImGui::EndDisabled();
            }
            ImGui::Separator();
            ImGui::Text(" | %s", project.Name.c_str());
            if (isPlaying) {
                ImGui::Text(" | PLAY  WASD jump Space  mouse look");
            } else if (sceneDirty) {
                ImGui::Text(" *");
            }
            ImGui::EndMainMenuBar();
        }

        Nova::Scene& activeScene = isPlaying ? playScene : scene;

        if (projectOpen &&
            ImGui::Shortcut(ImGuiKey_F5, ImGuiInputFlags_RouteGlobal | ImGuiInputFlags_RouteOverFocused)) {
            startPlay();
        }

        if (isPlaying) {
            if (!isPaused) {
                Nova::TickScene(playScene, clock.DeltaSeconds(), &input, &engineSettings, project.Root,
                                playJustStarted);
                playJustStarted = false;
            }
            if (!Nova::SceneHasFollowCamera(playScene)) {
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
            }
            const Nova::GameplayHudSnapshot hud = Nova::QueryGameplayHud(playScene);
            ImGui::SetNextWindowPos(ImVec2(18.0f, 18.0f), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.72f);
            ImGui::Begin("##game_hud", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse);
            ImGui::TextUnformatted(project.Name.empty() ? "NOVA3D" : project.Name.c_str());
            if (hud.PlayerMaxHealth > 0.0f) {
                const float pmax = hud.PlayerMaxHealth > 1.0f ? hud.PlayerMaxHealth : 1.0f;
                ImGui::ProgressBar(hud.PlayerHealth / pmax, ImVec2(260.0f, 18.0f));
                ImGui::Text("HP  %.0f / %.0f", hud.PlayerHealth, hud.PlayerMaxHealth);
            }
            if (hud.ShowCombatHud) {
                ImGui::Text("Волна %d   Очки %d   Бандиты: %d", hud.Wave, hud.Score, hud.AliveEnemies);
                if (hud.TargetMaxHealth > 0.0f && hud.Phase == Nova::GameplayPhase::Combat) {
                    ImGui::Text("Цель HP  %.0f / %.0f", hud.TargetHealth, hud.TargetMaxHealth);
                }
            }
            ImGui::Separator();
            if (ImGui::Button(isPaused ? tr("play") : tr("pause"))) {
                isPaused = !isPaused;
                window.SetCursorCaptured(!isPaused);
                engineSettings.MouseLook = !isPaused;
            }
            if (isPaused) {
                ImGui::TextUnformatted(tr("paused"));
            }
            ImGui::TextUnformatted(Nova::GameplayHudStatusLine(hud));
            ImGui::TextUnformatted("F8 — сохранить   F9 — загрузить   F6 — пауза");
            if (!window.IsCursorCaptured()) {
                ImGui::TextUnformatted("Курсор свободен. Tab — снова крутить камеру.");
            }
            if (hud.Phase == Nova::GameplayPhase::Victory &&
                input.IsKeyPressed(Nova::KeyCode::R)) {
                startPlay();
            }
            ImGui::End();
            Nova::RenderViewport fullVp;
            fullVp.Active = false;
            renderer->SetRenderViewport(fullVp);
            uint32_t fbW = 0, fbH = 0;
            window.GetFramebufferSize(fbW, fbH);
            const float aspect = fbH > 0 ? static_cast<float>(fbW) / static_cast<float>(fbH)
                                         : 16.0f / 9.0f;
            Nova::RenderScene(activeScene, *renderer, aspect, project.Root, meshCache, textureCache,
                              1.0f);
            ImGui::Render();
            renderer->BeginDrawing();
            renderer->EndFrame();
            continue;
        }

        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        const ImVec2 fullPos = mainViewport->WorkPos;
        const ImVec2 fullSize = mainViewport->WorkSize;
        constexpr float kChromeH = 56.0f;

        ImGui::SetNextWindowPos(fullPos);
        ImGui::SetNextWindowSize(ImVec2(fullSize.x, kChromeH));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.08f, 0.11f, 1.0f));
        ImGui::Begin("##nova_chrome", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoBringToFrontOnFocus);
        {
            const ImVec2 emblem = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(emblem, ImVec2(emblem.x + 38.0f, emblem.y + 34.0f),
                              IM_COL32(36, 132, 214, 255), 9.0f);
            dl->AddCircleFilled(ImVec2(emblem.x + 19.0f, emblem.y + 17.0f), 7.5f,
                                IM_COL32(250, 252, 255, 255));
            dl->AddCircle(ImVec2(emblem.x + 19.0f, emblem.y + 17.0f), 11.0f,
                          IM_COL32(180, 220, 255, 180), 0, 1.6f);
            ImGui::Dummy(ImVec2(46.0f, 34.0f));
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::Dummy(ImVec2(0.0f, 2.0f));
            ImGui::TextUnformatted("NOVA3D");
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.68f, 0.76f, 1.0f));
            ImGui::TextUnformatted(tr("brand_sub"));
            ImGui::PopStyleColor();
            ImGui::EndGroup();
            ImGui::SameLine(0.0f, 28.0f);
            auto workspaceButton = [&](const char* key, WorkspaceTab tab) {
                const bool on = workspaceTab == tab;
                if (on) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.48f, 0.68f, 1.0f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.15f, 0.20f, 1.0f));
                }
                if (ImGui::Button(tr(key), ImVec2(96.0f, 34.0f))) {
                    workspaceTab = tab;
                }
                ImGui::PopStyleColor();
            };
            workspaceButton("tab_projects", WorkspaceTab::Projects);
            ImGui::SameLine();
            if (projectOpen) {
                workspaceButton("tab_scene", WorkspaceTab::Scene);
                ImGui::SameLine();
                workspaceButton("tab_editor", WorkspaceTab::Content);
                ImGui::SameLine();
                workspaceButton("tab_engine", WorkspaceTab::Engine);
                ImGui::SameLine();
                workspaceButton("tab_audio", WorkspaceTab::Audio);
                ImGui::SameLine();
            }
            workspaceButton("tab_settings", WorkspaceTab::Settings);
            ImGui::SameLine(0.0f, 18.0f);
            if (isPlaying) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.62f, 0.22f, 0.20f, 1.0f));
                if (ImGui::Button(tr("stop_go"), ImVec2(118.0f, 34.0f))) {
                    stopPlay();
                }
                ImGui::PopStyleColor();
            } else {
                ImGui::BeginDisabled(!projectOpen);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.52f, 0.28f, 1.0f));
                if (ImGui::Button(tr("play_go"), ImVec2(118.0f, 34.0f))) {
                    startPlay();
                }
                ImGui::PopStyleColor();
                ImGui::EndDisabled();
            }
        }
        ImGui::End();
        ImGui::PopStyleColor();

        const ImVec2 workPos = ImVec2(fullPos.x, fullPos.y + kChromeH);
        const ImVec2 workSize = ImVec2(fullSize.x, std::max(80.0f, fullSize.y - kChromeH));
        constexpr float kSidePanelWidth = 280.0f;
        constexpr float kBottomPanelHeight = 168.0f;
        const float upperH = std::max(120.0f, workSize.y - kBottomPanelHeight);

        if (!projectOpen && workspaceTab != WorkspaceTab::Settings &&
            workspaceTab != WorkspaceTab::Projects) {
            workspaceTab = WorkspaceTab::Projects;
        }

        if (workspaceTab == WorkspaceTab::Projects ||
            (!projectOpen && workspaceTab != WorkspaceTab::Settings)) {
            ImGui::SetNextWindowPos(workPos);
            ImGui::SetNextWindowSize(workSize);
            ImGui::Begin(tr("tab_projects"), nullptr,
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse);
            ImGui::TextUnformatted(tr("projects_intro"));
            ImGui::Separator();
            ImGui::TextUnformatted(tr("profile"));
            char profileBuf[64];
            std::snprintf(profileBuf, sizeof(profileBuf), "%s", userWorkspace.ProfileName.c_str());
            if (ImGui::InputText("##profile", profileBuf, sizeof(profileBuf))) {
                userWorkspace.ProfileName = profileBuf;
                Nova::SaveUserWorkspace(userWorkspace);
            }
            ImGui::Separator();
            ImGui::TextUnformatted(tr("new_project"));
            ImGui::InputText(tr("project_name"), newProjectName, sizeof(newProjectName));
            ImGui::InputText(tr("project_path"), newProjectPath, sizeof(newProjectPath));
            ImGui::SameLine();
            if (ImGui::Button(tr("choose_folder"))) {
                if (auto folder = Nova::Editor::ShowOpenFolderDialog(tr("choose_folder"))) {
                    std::snprintf(newProjectPath, sizeof(newProjectPath), "%s",
                                  folder->generic_string().c_str());
                }
            }
            ImGui::RadioButton(tr("template_empty"), &newProjectTemplate, 0);
            ImGui::SameLine();
            ImGui::RadioButton(tr("template_tpp"), &newProjectTemplate, 1);
            ImGui::SameLine();
            ImGui::RadioButton(tr("template_knight"), &newProjectTemplate, 2);
            if (ImGui::Button(tr("create_project"), ImVec2(220.0f, 36.0f))) {
                const std::filesystem::path root(newProjectPath);
                Nova::ProjectTemplateKind kind = Nova::ProjectTemplateKind::Empty;
                if (newProjectTemplate == 1) {
                    kind = Nova::ProjectTemplateKind::ThirdPerson;
                } else if (newProjectTemplate == 2) {
                    kind = Nova::ProjectTemplateKind::KnightBandits;
                }
                if (Nova::CreateGameProject(root, newProjectName, kind, NOVA_SOURCE_DIR).Ok) {
                    Nova::ProjectDescriptor opened;
                    if (Nova::LoadProject(root, opened).Ok) {
                        applyOpenProject(opened);
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(tr("open_project"), ImVec2(180.0f, 36.0f))) {
                requestNavigation(PendingNavigation::OpenProject);
            }
            ImGui::Separator();
            ImGui::TextUnformatted(tr("recent_projects"));
            if (userWorkspace.RecentProjects.empty()) {
                ImGui::TextDisabled("%s", tr("no_recent_projects"));
            }
            for (const std::filesystem::path& recent : userWorkspace.RecentProjects) {
                const bool exists = Nova::IsNovaProjectRoot(recent);
                ImGui::BeginDisabled(!exists);
                if (ImGui::Selectable(recent.generic_string().c_str())) {
                    Nova::ProjectDescriptor opened;
                    if (Nova::LoadProject(recent, opened).Ok) {
                        applyOpenProject(opened);
                    }
                }
                ImGui::EndDisabled();
            }
            if (projectOpen) {
                ImGui::Separator();
                ImGui::Text("%s: %s", tr("current_project"), project.Name.c_str());
                if (ImGui::Button(tr("close_project"))) {
                    closeProject();
                }
                ImGui::SameLine();
                if (ImGui::Button(tr("save_game_slot"))) {
                    Nova::SaveGameSlot(project, "quick", isPlaying ? playScene : scene);
                }
                const auto slots = Nova::ListSaveSlots(project);
                for (const std::string& slot : slots) {
                    ImGui::BulletText("%s", slot.c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton((tr("load_game_slot") + std::string("##") + slot).c_str())) {
                        history.Push(scene);
                        if (Nova::LoadGameSlot(project, slot, scene).Ok) {
                            sceneDirty = true;
                            FrameOrbitOnScene(scene, orbit);
                        }
                    }
                }
            }
            ImGui::End();
            frameViewport.Gpu.Active = false;
            viewportHovered = false;
        } else if (projectOpen && workspaceTab == WorkspaceTab::Scene) {
        ImGui::SetNextWindowPos(workPos);
        ImGui::SetNextWindowSize(ImVec2(kSidePanelWidth, upperH));
        ImGui::Begin(tr("hierarchy"), nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::TextDisabled("%s", tr("axes"));
            static bool hierarchyRootsOnly = true;
            ImGui::SameLine();
            ImGui::Checkbox(tr("roots_only"), &hierarchyRootsOnly);
            if (ImGui::Button(tr("create_player"))) {
                history.Push(scene);
                if (scene.FindEntityByName("Player").IsValid()) {
                    selected = Nova::SpawnEnemy(scene, {2.4f, 0.0f, 2.4f});
                } else {
                    selected = Nova::SpawnPlayer(scene);
                }
                Nova::PlaceOnGround(scene, selected);
                FocusOrbitOn(scene, selected, orbit);
                orbit.Distance = 6.5f;
                sceneDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button(tr("place_plane"))) {
                history.Push(scene);
                selected = Nova::SpawnGround(scene);
                FocusOrbitOn(scene, selected, orbit);
                sceneDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button(tr("sample_village"))) {
                history.Push(scene);
                selected = Nova::SpawnSimpleMap(scene);
                FocusOrbitOn(scene, selected, orbit);
                orbit.Distance = 14.0f;
                sceneDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button(tr("template_empty"))) {
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
                if (hierarchyRootsOnly && scene.GetParent(entity).IsValid()) {
                    return;
                }
                const bool isSelected = selected.IsValid() && entity.Id == selected.Id;
                const int depth = EntityHierarchyDepth(scene, entity);
                ImGui::PushID(static_cast<int>(entity.Id));
                bool showAxes = scene.GetShowAxes(entity);
                if (ImGui::Checkbox("##axes", &showAxes)) {
                    scene.SetShowAxes(entity, showAxes);
                    sceneDirty = true;
                }
                ImGui::SameLine();
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
                if (scene.HasPlayerController(entity)) {
                    label += " [Player]";
                }
                if (scene.HasRotator(entity)) {
                    label += " [Rotator]";
                }
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selected = entity;
                    renameBuffer[0] = '\0';
                }
                ImGui::PopID();
            });
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(workPos.x + workSize.x - kSidePanelWidth, workPos.y));
        ImGui::SetNextWindowSize(ImVec2(kSidePanelWidth, upperH));
        ImGui::Begin(tr("inspector"), nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::TextUnformatted("Scene Environment");
            if (ImGui::ColorEdit3("Clear Color", &scene.Settings().ClearColor.x)) {
                sceneDirty = true;
            }
            if (ImGui::Checkbox(tr("enable_waves"), &scene.Settings().EnableWaves)) {
                sceneDirty = true;
            }
            ImGui::Separator();
            char projectNameBuf[128];
            std::snprintf(projectNameBuf, sizeof(projectNameBuf), "%s", project.Name.c_str());
            ImGui::TextUnformatted(tr("project_name"));
            if (ImGui::InputText("##project_display_name", projectNameBuf, sizeof(projectNameBuf))) {
                project.Name = projectNameBuf;
                Nova::SaveProject(project);
                window.SetTitle("NOVA3D — " + project.Name);
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
                bool showAxes = scene.GetShowAxes(selected);
                if (ImGui::Checkbox(tr("axes"), &showAxes)) {
                    scene.SetShowAxes(selected, showAxes);
                    sceneDirty = true;
                }
                ImGui::Separator();
                ImGui::TextUnformatted("Transform");
                Nova::Transform& xform = scene.GetTransform(selected);
                if (ImGui::DragFloat3("Position", &xform.Position.x, 0.02f)) {
                    sceneDirty = true;
                }
                if (gizmoSession.Active && viewportTool == ViewportTool::Rotate) {
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
                    if (ImGui::SliderFloat(tr("opacity"), &mesh.Opacity, 0.05f, 1.0f)) {
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
                    if (ImGui::Checkbox("Receive Shadows", &mesh.ReceiveShadows)) {
                        sceneDirty = true;
                    }
                } else if (ImGui::Button("Add Mesh Renderer")) {
                    scene.AddMeshRenderer(selected);
                    sceneDirty = true;
                }
                if (scene.HasCharacterController(selected)) {
                    Nova::CharacterControllerComponent& character =
                        scene.GetCharacterController(selected);
                    ImGui::TextUnformatted("Character Controller");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove##char")) {
                        scene.RemoveCharacterController(selected);
                        sceneDirty = true;
                    }
                    if (ImGui::DragFloat("Move Speed", &character.MoveSpeed, 0.05f, 0.5f, 20.0f)) {
                        sceneDirty = true;
                    }
                    if (ImGui::DragFloat("Jump", &character.JumpSpeed, 0.05f, 0.0f, 20.0f)) {
                        sceneDirty = true;
                    }
                    if (ImGui::DragFloat("Health", &character.Health, 1.0f, 0.0f, 400.0f)) {
                        sceneDirty = true;
                    }
                    if (ImGui::DragFloat("Max HP", &character.MaxHealth, 1.0f, 1.0f, 400.0f)) {
                        sceneDirty = true;
                    }
                    if (ImGui::DragFloat("Damage", &character.AttackDamage, 0.5f, 0.0f, 200.0f)) {
                        sceneDirty = true;
                    }
                    if (ImGui::DragInt("Team", &character.Team, 1, 0, 1)) {
                        sceneDirty = true;
                    }
                } else if (ImGui::Button("Add Character Controller")) {
                    scene.AddCharacterController(selected);
                    sceneDirty = true;
                }
                if (scene.HasPlayerController(selected)) {
                    Nova::PlayerControllerComponent& player = scene.GetPlayerController(selected);
                    ImGui::TextUnformatted("Player (WASD in Play)");
                    if (ImGui::Checkbox("Enabled", &player.Enabled)) {
                        sceneDirty = true;
                    }
                    if (ImGui::SmallButton("Remove Player")) {
                        scene.RemovePlayerController(selected);
                        sceneDirty = true;
                    }
                } else if (ImGui::Button("Make Playable Character")) {
                    if (!scene.HasCharacterController(selected)) {
                        scene.AddCharacterController(selected);
                    }
                    scene.AddPlayerController(selected);
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
                } else if (ImGui::Button(tr("add_camera"))) {
                    scene.AddCamera(selected);
                    scene.SetPrimaryCamera(selected);
                    sceneDirty = true;
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
                } else if (ImGui::Button(tr("add_light"))) {
                    scene.AddDirectionalLight(selected);
                    sceneDirty = true;
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
                if (scene.HasScript(selected)) {
                    Nova::ScriptComponent& script = scene.GetScript(selected);
                    ImGui::TextUnformatted(tr("script"));
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove##script")) {
                        scene.RemoveScript(selected);
                        sceneDirty = true;
                    }
                    char pathBuf[256] = {};
                    std::snprintf(pathBuf, sizeof(pathBuf), "%s", script.AssetPath.c_str());
                    if (ImGui::InputText("Script file", pathBuf, sizeof(pathBuf))) {
                        script.AssetPath = pathBuf;
                        sceneDirty = true;
                    }
                    ImGui::TextWrapped("%s", tr("logic_help"));
                } else if (ImGui::Button("Add NovaScript")) {
                    Nova::ScriptComponent script;
                    script.Source = Nova::DefaultPlayerScript();
                    scene.AddScript(selected, script);
                    sceneDirty = true;
                }
                if (scene.HasAudioSource(selected)) {
                    Nova::AudioSourceComponent& audio = scene.GetAudioSource(selected);
                    ImGui::TextUnformatted("Audio Source");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove##asrc")) {
                        scene.RemoveAudioSource(selected);
                        sceneDirty = true;
                    }
                    char soundBuf[128] = {};
                    std::snprintf(soundBuf, sizeof(soundBuf), "%s", audio.SoundId.c_str());
                    if (ImGui::InputText("Sound Id", soundBuf, sizeof(soundBuf))) {
                        audio.SoundId = soundBuf;
                        sceneDirty = true;
                    }
                    if (ImGui::Checkbox("Play On Start", &audio.PlayOnStart)) {
                        sceneDirty = true;
                    }
                    if (ImGui::SmallButton(tr("preview"))) {
                        Nova::AudioEngine::Get().Play(audio.SoundId);
                    }
                } else if (ImGui::Button("Add Audio Source")) {
                    scene.AddAudioSource(selected);
                    sceneDirty = true;
                }
                if (scene.HasCollider(selected)) {
                    Nova::ColliderComponent& collider = scene.GetCollider(selected);
                    ImGui::TextUnformatted("Collider");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove##col")) {
                        scene.RemoveCollider(selected);
                        sceneDirty = true;
                    }
                    if (ImGui::Checkbox("Solid", &collider.Solid)) {
                        sceneDirty = true;
                    }
                    if (ImGui::DragFloat3("Size", &collider.Size.x, 0.05f)) {
                        sceneDirty = true;
                    }
                } else if (ImGui::Button("Add Collider")) {
                    scene.AddCollider(selected);
                    sceneDirty = true;
                }
                if (scene.HasPickup(selected)) {
                    Nova::PickupComponent& pickup = scene.GetPickup(selected);
                    ImGui::TextUnformatted("Pickup");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove##pick")) {
                        scene.RemovePickup(selected);
                        sceneDirty = true;
                    }
                    if (ImGui::DragFloat("Heal", &pickup.Heal, 1.0f, 1.0f, 200.0f)) {
                        sceneDirty = true;
                    }
                    if (ImGui::Checkbox("Taken", &pickup.Taken)) {
                        sceneDirty = true;
                    }
                } else if (ImGui::Button("Add Pickup")) {
                    scene.AddPickup(selected);
                    sceneDirty = true;
                }
            } else {
                ImGui::TextUnformatted("Select an entity in Hierarchy.");
            }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(workPos.x, workPos.y + upperH));
        ImGui::SetNextWindowSize(ImVec2(workSize.x, kBottomPanelHeight));
        ImGui::Begin("Project", nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            if (ImGui::BeginTabBar("##project_tabs")) {
                if (ImGui::BeginTabItem("Assets")) {
                    ImGui::TextUnformatted(tr("place_in_world"));
                    refreshProjectAssets();
                    for (const std::filesystem::path& rel : projectAssetsCache) {
                        const std::string path = rel.generic_string();
                        if (ImGui::Selectable(path.c_str())) {
                            const std::string ext = rel.extension().string();
                            if (ext == ".wav") {
                                Nova::AudioEngine::Get().SetEnabled(false);
                                if (selected.IsValid() && scene.IsAlive(selected)) {
                                    if (!scene.HasAudioSource(selected)) {
                                        scene.AddAudioSource(selected);
                                    }
                                    scene.GetAudioSource(selected).SoundId = rel.stem().string();
                                    sceneDirty = true;
                                }
                            } else if (ext == ".ns") {
                                if (selected.IsValid() && scene.IsAlive(selected)) {
                                    if (!scene.HasScript(selected)) {
                                        scene.AddScript(selected);
                                    }
                                    scene.GetScript(selected).AssetPath = path;
                                    const Nova::FileIOResult read =
                                        Nova::ReadTextFile(project.Root / rel);
                                    if (read.Ok) {
                                        scene.GetScript(selected).Source = read.Text;
                                    }
                                    sceneDirty = true;
                                }
                            } else if (ext == ".png") {
                                if (selected.IsValid() && scene.IsAlive(selected) &&
                                    scene.HasMeshRenderer(selected)) {
                                    Nova::MeshRendererComponent& mesh =
                                        scene.GetMeshRenderer(selected);
                                    mesh.AlbedoTexturePath = path;
                                    mesh.UseAlbedoTexture = true;
                                    sceneDirty = true;
                                }
                            } else if (ext == ".obj" || ext == ".gltf" || ext == ".glb") {
                                history.Push(scene);
                                const bool asCharacter =
                                    path.find("Characters") != std::string::npos ||
                                    path.find("knight") != std::string::npos;
                                if (asCharacter) {
                                    selected = Nova::SpawnPlayer(scene, path);
                                } else {
                                    const char* base =
                                        path.find("Environment") != std::string::npos ? "Map"
                                                                                      : "Item";
                                    selected = Nova::SpawnImportedMesh(
                                        scene, UniqueEntityName(scene, base), path);
                                }
                                Nova::PlaceOnGround(scene, selected);
                                FocusOrbitOn(scene, selected, orbit);
                                sceneDirty = true;
                            }
                        }
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Console")) {
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
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        ImGui::End();

        const float centerX = workPos.x + kSidePanelWidth;
        const float centerW = workSize.x - kSidePanelWidth * 2.0f;
        ImGui::SetNextWindowPos(ImVec2(centerX, workPos.y));
        ImGui::SetNextWindowSize(ImVec2(centerW, upperH));
        ImGui::Begin(tr("viewport"), nullptr,
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
            ImGui::Text("FPS: %.0f", ImGui::GetIO().Framerate);
            if (ImGui::RadioButton(tr("tool_move"), viewportTool == ViewportTool::Move)) {
                viewportTool = ViewportTool::Move;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton(tr("tool_rotate"), viewportTool == ViewportTool::Rotate)) {
                viewportTool = ViewportTool::Rotate;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton(tr("tool_scale"), viewportTool == ViewportTool::Scale)) {
                viewportTool = ViewportTool::Scale;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton(tr("tool_place"), viewportTool == ViewportTool::Place)) {
                viewportTool = ViewportTool::Place;
            }
            const Nova::PlaceableKind kinds[] = {
                Nova::PlaceableKind::Cube,   Nova::PlaceableKind::Sphere,
                Nova::PlaceableKind::House, Nova::PlaceableKind::Tree,
                Nova::PlaceableKind::Crate,  Nova::PlaceableKind::Wall,  Nova::PlaceableKind::Road,
                Nova::PlaceableKind::Fence,  Nova::PlaceableKind::Well,  Nova::PlaceableKind::Plane,
                Nova::PlaceableKind::Knight, Nova::PlaceableKind::Bandit, Nova::PlaceableKind::Herb};
            int shown = 0;
            for (Nova::PlaceableKind kind : kinds) {
                if (shown > 0 && shown % 5 != 0) {
                    ImGui::SameLine();
                }
                if (ImGui::RadioButton(tr(PlaceableI18nKey(kind)), placeKind == kind)) {
                    placeKind = kind;
                    viewportTool = ViewportTool::Place;
                }
                ++shown;
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
            ImGui::TextUnformatted(tr("viewport_hint"));
            if (ImGui::Button(tr("import_blender"))) {
                if (auto file = Nova::Editor::ShowOpenFileDialog(tr("import_blender"),
                                                                 kMeshFileFilters)) {
                    if (auto rel = ImportFileIntoProject(project, *file, "Assets/Models")) {
                        history.Push(scene);
                        selected = Nova::SpawnImportedMesh(
                            scene, UniqueEntityName(scene, file->stem().string().c_str()),
                            rel->generic_string());
                        Nova::PlaceOnGround(scene, selected);
                        FocusOrbitOn(scene, selected, orbit);
                        sceneDirty = true;
                        invalidateProjectCaches();
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(tr("sample_village"))) {
                history.Push(scene);
                selected = Nova::SpawnSimpleMap(scene);
                FocusOrbitOn(scene, selected, orbit);
                orbit.Distance = 14.0f;
                sceneDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button(tr("create_player"))) {
                history.Push(scene);
                if (scene.FindEntityByName("Player").IsValid()) {
                    selected = Nova::SpawnEnemy(scene, {2.4f, 0.0f, 2.4f});
                } else {
                    selected = Nova::SpawnPlayer(scene);
                }
                Nova::PlaceOnGround(scene, selected);
                FocusOrbitOn(scene, selected, orbit);
                orbit.Distance = 6.5f;
                sceneDirty = true;
            }
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
            renderer->SetRenderViewport(frameViewport.Gpu);
            if (void* sceneTex = renderer->GetEditorSceneTexture()) {
                ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(sceneTex),
                                                     canvasMin, canvasMax);
            }

            frameViewport.CanvasActive = ImGui::IsItemActive();

            Nova::Camera viewportCameraInPanel;
            const bool hasCamInPanel =
                Nova::BuildSceneCamera(scene, frameViewport.Aspect, viewportCameraInPanel);
            const ImVec2 mousePos = ImGui::GetIO().MousePos;
            const float mouseLocalX = mousePos.x - frameViewport.CanvasMin.x;
            const float mouseLocalY = mousePos.y - frameViewport.CanvasMin.y;

            if (hasCamInPanel) {
                DrawViewportGroundGrid(ImGui::GetWindowDrawList(), frameViewport,
                                       viewportCameraInPanel.GetViewProjectionMatrix(),
                                       orbit.Target, 18.0f, 1.0f);
                if (viewportTool == ViewportTool::Place) {
                    Nova::Ray ghostRay;
                    if (Nova::ViewportPointToRay(viewportCameraInPanel, mouseLocalX, mouseLocalY,
                                                 frameViewport.CanvasSize.x,
                                                 frameViewport.CanvasSize.y, ghostRay)) {
                        Nova::Vec3 ghostHit;
                        if (Nova::RayHitGround(scene, ghostRay, ghostHit)) {
                            const Nova::PlaceablePreview ghost =
                                Nova::QueryPlaceablePreview(placeKind, ghostHit, placeYaw);
                            DrawPlacementGhost(ImGui::GetWindowDrawList(), frameViewport,
                                               viewportCameraInPanel.GetViewProjectionMatrix(),
                                               ghost);
                        }
                    }
                }
            }

            Nova::Vec3 gizmoOrigin{};
            float gizmoSize = 0.75f;
            if (selected.IsValid() && scene.IsAlive(selected)) {
                gizmoOrigin = WorldPositionFromMatrix(scene.GetWorldMatrix(selected));
                gizmoSize = std::max(0.35f, scene.GetTransform(selected).Scale.x * 0.75f);
            }

            const bool showAxes = selected.IsValid() && scene.IsAlive(selected) &&
                                  scene.GetShowAxes(selected) && !scene.HasCamera(selected);

            if (hasCamInPanel && ImGui::IsItemActivated() &&
                ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                const Nova::Mat4 vp = viewportCameraInPanel.GetViewProjectionMatrix();
                Nova::Editor::GizmoAxis picked = Nova::Editor::GizmoAxis::None;
                if (showAxes) {
                    if (viewportTool == ViewportTool::Rotate) {
                        picked = Nova::Editor::PickRotationAxis(
                            vp, gizmoOrigin, gizmoSize, mouseLocalX, mouseLocalY,
                            frameViewport.CanvasSize.x, frameViewport.CanvasSize.y, 22.0f);
                    } else if (viewportTool == ViewportTool::Move ||
                               viewportTool == ViewportTool::Scale) {
                        picked = Nova::Editor::PickMoveAxis(
                            vp, gizmoOrigin, gizmoSize, mouseLocalX, mouseLocalY,
                            frameViewport.CanvasSize.x, frameViewport.CanvasSize.y);
                        if (picked == Nova::Editor::GizmoAxis::None &&
                            viewportTool == ViewportTool::Scale) {
                            picked = Nova::Editor::GizmoAxis::Screen;
                        }
                    }
                }
                if (picked != Nova::Editor::GizmoAxis::None) {
                    history.Push(scene);
                    Nova::Editor::BeginViewportGizmoSession(
                        gizmoSession, picked, frameViewport.CanvasSize.y);
                } else if (viewportTool != ViewportTool::Place && !ImGui::GetIO().KeyAlt) {
                    Nova::Ray pickRay;
                    if (Nova::ViewportPointToRay(viewportCameraInPanel, mouseLocalX, mouseLocalY,
                                                 frameViewport.CanvasSize.x,
                                                 frameViewport.CanvasSize.y, pickRay)) {
                        const Nova::Entity hit = Nova::PickSceneMesh(scene, pickRay);
                        if (hit.IsValid()) {
                            selected = hit;
                            renameBuffer[0] = '\0';
                        } else {
                            selected = Nova::Entity{};
                        }
                    }
                }
            }

            if (showAxes && hasCamInPanel && gizmoSession.Active && frameViewport.CanvasActive) {
                Nova::Transform& xform = scene.GetTransform(selected);
                if (viewportTool == ViewportTool::Rotate) {
                    const Nova::Quat before = xform.Rotation;
                    Nova::Editor::ApplyViewportRotateAxis(
                        gizmoSession, xform, ImGui::GetIO().MouseDelta.x,
                        ImGui::GetIO().MouseDelta.y, viewportCameraInPanel);
                    if (before.Dot(xform.Rotation) < 0.99999f) {
                        sceneDirty = true;
                    }
                } else if (viewportTool == ViewportTool::Move) {
                    Nova::Editor::ApplyViewportMoveAxis(
                        gizmoSession, xform, ImGui::GetIO().MouseDelta.x,
                        ImGui::GetIO().MouseDelta.y, viewportCameraInPanel);
                    if (ImGui::GetIO().KeyShift) {
                        xform.Position = Nova::Editor::SnapPositionToGrid(xform.Position);
                    }
                    sceneDirty = true;
                } else if (viewportTool == ViewportTool::Scale) {
                    Nova::Editor::ApplyViewportScaleAxis(
                        gizmoSession, xform, ImGui::GetIO().MouseDelta.x,
                        ImGui::GetIO().MouseDelta.y, viewportCameraInPanel);
                    sceneDirty = true;
                }
            }

            if (showAxes && hasCamInPanel) {
                const Nova::Mat4 vp = viewportCameraInPanel.GetViewProjectionMatrix();
                if (viewportTool == ViewportTool::Move) {
                    DrawTranslationGizmo(ImGui::GetWindowDrawList(), frameViewport, vp, gizmoOrigin,
                                         gizmoSize);
                } else if (viewportTool == ViewportTool::Rotate) {
                    DrawRotationGizmo(ImGui::GetWindowDrawList(), frameViewport, vp, gizmoOrigin,
                                      gizmoSize);
                } else {
                    DrawScaleGizmo(ImGui::GetWindowDrawList(), frameViewport, vp, gizmoOrigin,
                                   gizmoSize);
                }
            }
            if (hasCamInPanel && selected.IsValid() && scene.IsAlive(selected) &&
                scene.HasMeshRenderer(selected) && !scene.HasCamera(selected)) {
                Nova::PlaceablePreview box;
                box.Center = gizmoOrigin;
                const Nova::Vec3 scale = scene.GetTransform(selected).Scale;
                box.HalfExtents = {std::max(0.2f, scale.x * 0.52f), std::max(0.2f, scale.y * 0.52f),
                                   std::max(0.2f, scale.z * 0.52f)};
                box.Color = {0.25f, 0.85f, 1.0f};
                DrawPlacementGhost(ImGui::GetWindowDrawList(), frameViewport,
                                   viewportCameraInPanel.GetViewProjectionMatrix(), box);
            }
        ImGui::End();

        if (!ImGui::GetIO().WantTextInput) {
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
        if (ImGui::Shortcut(ImGuiKey_P, ImGuiInputFlags_RouteGlobal)) {
            viewportTool = ViewportTool::Place;
        }
        if (viewportTool == ViewportTool::Place) {
            if (ImGui::Shortcut(ImGuiKey_LeftBracket, ImGuiInputFlags_RouteGlobal)) {
                placeYaw += 0.2617994f;
            }
            if (ImGui::Shortcut(ImGuiKey_RightBracket, ImGuiInputFlags_RouteGlobal)) {
                placeYaw -= 0.2617994f;
            }
        }
        }

        Nova::Camera viewportCamera;
        const bool hasViewportCamera =
            Nova::BuildSceneCamera(scene, frameViewport.Aspect, viewportCamera);
        const float rotPerPixel =
            ViewportRotationRadiansPerPixel(std::max(frameViewport.CanvasSize.y, 64.0f));

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            Nova::Editor::ResetViewportGizmoSession(gizmoSession);
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
                FrameOrbitOnScene(scene, orbit);
                sceneDirty = true;
            }

            if (!input.IsMouseButtonDown(Nova::MouseButton::Left)) {
                lastPlaceValid = false;
            }
            if (viewportTool == ViewportTool::Place && hasViewportCamera &&
                !ImGui::GetIO().KeyAlt && !gizmoSession.Active &&
                input.IsMouseButtonDown(Nova::MouseButton::Left)) {
                const float mx = ImGui::GetIO().MousePos.x - frameViewport.CanvasMin.x;
                const float my = ImGui::GetIO().MousePos.y - frameViewport.CanvasMin.y;
                Nova::Ray placeRay;
                if (Nova::ViewportPointToRay(viewportCamera, mx, my, frameViewport.CanvasSize.x,
                                             frameViewport.CanvasSize.y, placeRay)) {
                    Nova::Vec3 hit;
                    if (Nova::RayHitGround(scene, placeRay, hit)) {
                        const bool first = input.IsMouseButtonPressed(Nova::MouseButton::Left);
                        const bool stroke = Nova::PlaceableAllowsStroke(placeKind) && lastPlaceValid &&
                                            Nova::ShouldStampAlongStroke(
                                                lastPlaceStamp, hit, Nova::PlaceableSpacing(placeKind));
                        if (first || stroke) {
                            history.Push(scene);
                            selected = Nova::SpawnPlaceable(scene, placeKind, hit, placeYaw);
                            lastPlaceStamp = scene.GetTransform(selected).Position;
                            lastPlaceValid = true;
                            sceneDirty = true;
                        }
                    }
                }
            }

            if (viewportTool != ViewportTool::Place && selected.IsValid() &&
                scene.IsAlive(selected) && !scene.HasCamera(selected) &&
                !gizmoSession.Active && !scene.GetShowAxes(selected) &&
                scene.HasMeshRenderer(selected) && !ImGui::GetIO().KeyAlt) {
                if (frameViewport.CanvasActive && input.IsMouseButtonDown(Nova::MouseButton::Left) &&
                    (dx != 0.0f || dy != 0.0f)) {
                    if (!moveHistoryPushed) {
                        history.Push(scene);
                        moveHistoryPushed = true;
                    }
                    if (hasViewportCamera) {
                        const float mx = ImGui::GetIO().MousePos.x - frameViewport.CanvasMin.x;
                        const float my = ImGui::GetIO().MousePos.y - frameViewport.CanvasMin.y;
                        Nova::Ray placeRay;
                        if (Nova::ViewportPointToRay(viewportCamera, mx, my,
                                                     frameViewport.CanvasSize.x,
                                                     frameViewport.CanvasSize.y, placeRay)) {
                            Nova::Vec3 hit;
                            if (Nova::RayHitYPlane(placeRay, 0.0f, hit)) {
                                if (scene.HasCharacterController(selected) ||
                                    scene.HasPickup(selected)) {
                                    Nova::PlantOnGround(scene, selected, hit, 0.0f);
                                } else {
                                    Nova::Transform& xform = scene.GetTransform(selected);
                                    xform.Position.x = hit.x;
                                    xform.Position.z = hit.z;
                                }
                            }
                        }
                    }
                    sceneDirty = true;
                }
            }

            const bool orbitLook = input.IsMouseButtonDown(Nova::MouseButton::Right) ||
                                   (ImGui::GetIO().KeyAlt &&
                                    input.IsMouseButtonDown(Nova::MouseButton::Left) &&
                                    !gizmoSession.Active);
            if (orbitLook) {
                Nova::Editor::ApplyOrbitLook(orbit.YawRadians, orbit.PitchRadians, dx, dy,
                                             rotPerPixel);
                float wishRight = 0.0f;
                float wishUp = 0.0f;
                float wishForward = 0.0f;
                if (!ImGui::GetIO().WantTextInput) {
                    if (input.IsKeyDown(Nova::KeyCode::D)) {
                        wishRight += 1.0f;
                    }
                    if (input.IsKeyDown(Nova::KeyCode::A)) {
                        wishRight -= 1.0f;
                    }
                    if (input.IsKeyDown(Nova::KeyCode::W)) {
                        wishForward += 1.0f;
                    }
                    if (input.IsKeyDown(Nova::KeyCode::S)) {
                        wishForward -= 1.0f;
                    }
                    if (input.IsKeyDown(Nova::KeyCode::E)) {
                        wishUp += 1.0f;
                    }
                    if (input.IsKeyDown(Nova::KeyCode::Q)) {
                        wishUp -= 1.0f;
                    }
                }
                Nova::Editor::FlyEditCamera(orbit.YawRadians, orbit.PitchRadians, orbit.Distance,
                                            orbit.Target, 0.0f, 0.0f, rotPerPixel, wishRight,
                                            wishUp, wishForward, 10.0f, clock.DeltaSeconds());
                sceneDirty = true;
            }
            if (hasViewportCamera &&
                input.IsMouseButtonDown(Nova::MouseButton::Middle) &&
                (dx != 0.0f || dy != 0.0f)) {
                PanOrbitTarget(orbit, dx, dy, viewportCamera, frameViewport.CanvasSize.y);
                sceneDirty = true;
            }
            if (input.GetScrollY() != 0.0f) {
                Nova::Editor::ApplyOrbitDolly(orbit.Distance, input.GetScrollY() * 0.85f, 1.2f,
                                              80.0f);
                sceneDirty = true;
            }
        }

        } else if (projectOpen && workspaceTab == WorkspaceTab::Content) {
            ImGui::SetNextWindowPos(workPos);
            ImGui::SetNextWindowSize(workSize);
            ImGui::Begin(tr("tab_editor"), nullptr,
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse);
            ImGui::TextWrapped("%s", tr("content_help"));
            static char worldBuf[16384];
            static bool worldLoaded = false;
            const std::filesystem::path worldPath =
                project.Root / "Assets" / "Scripts" / "world.ns";
            if (!worldLoaded) {
                const Nova::FileIOResult read = Nova::ReadTextFile(worldPath);
                const std::string text = read.Ok ? read.Text : Nova::DefaultContentScript();
                std::snprintf(worldBuf, sizeof(worldBuf), "%s", text.c_str());
                worldLoaded = true;
            }
            ImGui::InputTextMultiline("##world", worldBuf, sizeof(worldBuf), ImVec2(-1.0f, -72.0f),
                                      ImGuiInputTextFlags_AllowTabInput);
            if (ImGui::Button(tr("apply"))) {
                history.Push(scene);
                Nova::Entity spawned{};
                std::string error;
                if (Nova::RunContentScript(scene, worldBuf, error, spawned)) {
                    Nova::WriteTextFile(worldPath, worldBuf);
                    if (spawned.IsValid()) {
                        selected = spawned;
                    }
                    sceneDirty = true;
                } else {
                    ImGui::OpenPopup("content_err");
                }
                if (!error.empty()) {
                    NOVA_LOG_ERROR("{}", error);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(tr("save"))) {
                Nova::WriteTextFile(worldPath, worldBuf);
            }
            if (ImGui::BeginPopup("content_err")) {
                ImGui::TextUnformatted("Check the script.");
                ImGui::EndPopup();
            }
            ImGui::End();
            frameViewport.Gpu.Active = false;
            viewportHovered = false;
        } else if (projectOpen && workspaceTab == WorkspaceTab::Engine) {
            ImGui::SetNextWindowPos(workPos);
            ImGui::SetNextWindowSize(workSize);
            ImGui::Begin(tr("tab_engine"), nullptr,
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse);
            ImGui::TextWrapped("%s", tr("engine_help"));
            static char gameBuf[16384];
            static bool gameLoaded = false;
            const std::filesystem::path gamePath = project.Root / "Assets" / "Scripts" / "game.ns";
            if (!gameLoaded) {
                const Nova::FileIOResult read = Nova::ReadTextFile(gamePath);
                const std::string text = read.Ok ? read.Text : Nova::DefaultGameScript();
                std::snprintf(gameBuf, sizeof(gameBuf), "%s", text.c_str());
                gameLoaded = true;
            }
            ImGui::InputTextMultiline("##game", gameBuf, sizeof(gameBuf), ImVec2(-1.0f, -96.0f),
                                      ImGuiInputTextFlags_AllowTabInput);
            Nova::CompiledScript compiled;
            std::string error;
            const bool ok = Nova::CompileNovaScript(gameBuf, compiled, error);
            if (!ok) {
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f), "%s", error.c_str());
            } else {
                ImGui::TextDisabled("OK");
            }
            if (ImGui::Button(tr("save"))) {
                Nova::WriteTextFile(gamePath, gameBuf);
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(tr("controls"));
            DrawKeyBindingCombo(tr("forward"), engineSettings, "forward");
            DrawKeyBindingCombo(tr("back"), engineSettings, "back");
            DrawKeyBindingCombo(tr("left"), engineSettings, "left");
            DrawKeyBindingCombo(tr("right"), engineSettings, "right");
            DrawKeyBindingCombo(tr("jump"), engineSettings, "jump");
            ImGui::End();
            frameViewport.Gpu.Active = false;
            viewportHovered = false;
        } else if (projectOpen && workspaceTab == WorkspaceTab::Audio) {
            ImGui::SetNextWindowPos(workPos);
            ImGui::SetNextWindowSize(workSize);
            ImGui::Begin(tr("tab_audio"), nullptr,
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse);
            if (ImGui::Button(tr("import_wav"))) {
                if (auto file = Nova::Editor::ShowOpenFileDialog(tr("import_wav"), kAudioFileFilters)) {
                    if (ImportFileIntoProject(project, *file, "Assets/Audio")) {
                        Nova::AudioEngine::Get().RefreshUserSounds(project.Root);
                        invalidateProjectCaches();
                    }
                }
            }
            ImGui::SameLine();
            ImGui::SliderFloat(tr("master_volume"), &engineSettings.MasterVolume, 0.0f, 1.0f);
            Nova::AudioEngine::Get().SetEnabled(false);
            Nova::AudioEngine::Get().SetMasterVolume(0.0f);
            engineSettings.EnableAudio = false;
            ImGui::Separator();
            ImGui::TextUnformatted(tr("builtin_sounds"));
            Nova::AudioEngine::Get().RefreshUserSounds(project.Root);
            for (const Nova::SoundInfo& sound : Nova::AudioEngine::Get().ListSounds()) {
                ImGui::PushID(sound.Id.c_str());
                ImGui::Text("%s%s", sound.DisplayName.c_str(), sound.Builtin ? "" : "  (project)");
                ImGui::SameLine();
                if (ImGui::SmallButton(tr("preview"))) {
                    Nova::AudioEngine::Get().Play(sound.Id);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Apply") && selected.IsValid() && scene.IsAlive(selected)) {
                    if (!scene.HasAudioSource(selected)) {
                        scene.AddAudioSource(selected);
                    }
                    scene.GetAudioSource(selected).SoundId = sound.Id;
                    sceneDirty = true;
                }
                ImGui::PopID();
            }
            ImGui::End();
            frameViewport.Gpu.Active = false;
            viewportHovered = false;
        } else {
            ImGui::SetNextWindowPos(workPos);
            ImGui::SetNextWindowSize(workSize);
            ImGui::Begin(tr("tab_settings"), nullptr,
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse);
            ImGui::TextUnformatted(tr("language"));
            int lang = engineSettings.Language == Nova::UiLanguage::English ? 0 : 1;
            const char* langs[] = {tr("english"), tr("russian")};
            if (ImGui::Combo("##lang", &lang, langs, 2)) {
                engineSettings.Language =
                    lang == 0 ? Nova::UiLanguage::English : Nova::UiLanguage::Russian;
            }
            ImGui::SliderFloat(tr("master_volume"), &engineSettings.MasterVolume, 0.0f, 1.0f);
            Nova::AudioEngine::Get().SetEnabled(false);
            Nova::AudioEngine::Get().SetMasterVolume(0.0f);
            engineSettings.EnableAudio = false;
            ImGui::SliderFloat(tr("editor_opacity"), &editorPreviewOpacity, 0.15f, 1.0f);
            ImGui::TextWrapped("%s", tr("editor_opacity_help"));
            ImGui::Separator();
            ImGui::TextUnformatted(tr("controls"));
            DrawKeyBindingCombo(tr("forward"), engineSettings, "forward");
            DrawKeyBindingCombo(tr("back"), engineSettings, "back");
            DrawKeyBindingCombo(tr("left"), engineSettings, "left");
            DrawKeyBindingCombo(tr("right"), engineSettings, "right");
            DrawKeyBindingCombo(tr("jump"), engineSettings, "jump");
            DrawKeyBindingCombo(tr("sprint"), engineSettings, "sprint");
            DrawKeyBindingCombo(tr("crouch"), engineSettings, "crouch");
            DrawKeyBindingCombo(tr("attack"), engineSettings, "attack");
            DrawKeyBindingCombo(tr("dodge"), engineSettings, "dodge");
            DrawKeyBindingCombo(tr("interact"), engineSettings, "interact");
            ImGui::SliderFloat(tr("mouse_sens"), &engineSettings.MouseSensitivity, 0.2f, 2.5f);
            ImGui::Checkbox(tr("invert_y"), &engineSettings.InvertY);
            if (ImGui::Button(tr("save_settings"))) {
                Nova::SaveEngineSettings(project.Root, engineSettings);
            }
            ImGui::End();
            frameViewport.Gpu.Active = false;
            viewportHovered = false;
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
        Nova::RenderScene(activeScene, *renderer, renderAspect, project.Root, meshCache, textureCache,
                          isPlaying ? 1.0f : editorPreviewOpacity);

        ImGui::Render();
        renderer->BeginDrawing();
        renderer->EndFrame();
    }

    ImGui_ImplMetal_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    renderer->Shutdown();
    Nova::AudioEngine::Get().Shutdown();
    Nova::Editor::DetachEditorLogSink();
    Nova::Log::Shutdown();
    return 0;
}
