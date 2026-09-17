#include <Nova/Core/Log.h>
#include <Nova/Core/Input.h>
#include <Nova/Core/Time.h>
#include <Nova/Platform/Window.h>
#include <Nova/Scene/Scene.h>
#include <Nova/Project/Project.h>
#include <Nova/Scene/SceneSerialization.h>
#include <Nova/Renderer/Renderer.h>
#include <Nova/Plugins/BuiltinPlugins.h>
#include <Nova/Plugins/PluginRegistry.h>
#include <Nova/Plugins/ServiceHub.h>

#include <Nova/Assets/MeshCache.h>
#include <Nova/Assets/TextureCache.h>
#include <Nova/Scene/SceneRendererBridge.h>
#include <Nova/Scene/SceneRuntime.h>

#include <cstring>
#include <filesystem>

namespace {

std::filesystem::path ResolveRuntimeScenePath(int argc, char** argv) {
    std::filesystem::path sceneArg;
    std::filesystem::path projectArg;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            sceneArg = argv[++i];
        } else if (std::strcmp(argv[i], "--project") == 0 && i + 1 < argc) {
            projectArg = argv[++i];
        }
    }

    const std::filesystem::path devRoot = NOVA_SOURCE_DIR;
    Nova::ProjectDescriptor project;
    if (!projectArg.empty()) {
        if (Nova::LoadProject(projectArg, project).Ok) {
            if (!sceneArg.empty()) {
                return sceneArg.is_absolute() ? sceneArg : project.Root / sceneArg;
            }
            return project.LastOpenedSceneAbsolute();
        }
        NOVA_LOG_WARN("Failed to load project '{}'", projectArg.string());
    }

    if (Nova::LoadProject(devRoot, project).Ok) {
        if (!sceneArg.empty()) {
            return sceneArg.is_absolute() ? sceneArg : project.Root / sceneArg;
        }
        return project.LastOpenedSceneAbsolute();
    }

    if (!sceneArg.empty()) {
        return sceneArg;
    }
    return devRoot / "Assets/Scenes/demo.scene.json";
}

} // namespace

int main(int argc, char** argv) {
    Nova::Log::Init();

    NOVA_LOG_INFO("NOVA3D Engine v0.1.0 — scene runtime");

    {
        Nova::Window window({"NOVA3D", 1280, 720});
        if (!window.IsValid()) {
            NOVA_LOG_FATAL("Failed to create window");
            Nova::Log::Shutdown();
            return 1;
        }

        Nova::Input input;
        auto renderer = Nova::CreateRenderer();
        if (!renderer->Init(window)) {
            NOVA_LOG_FATAL("Failed to initialize renderer");
            Nova::Log::Shutdown();
            return 1;
        }
        renderer->SetClearColor(0.08f, 0.09f, 0.12f, 1.0f);

        const std::filesystem::path scenePath = ResolveRuntimeScenePath(argc, argv);

        Nova::Scene scene;
        const Nova::SceneIOResult sceneLoad = Nova::LoadSceneFromFile(scenePath, scene);
        if (!sceneLoad.Ok) {
            NOVA_LOG_WARN("Failed to load scene '{}': {} — using built-in demo",
                          scenePath.string(), sceneLoad.Error);
            scene = Nova::Scene::CreateDemoLevel();
        } else {
            NOVA_LOG_INFO("Scene loaded from {}", scenePath.string());
        }
        NOVA_LOG_INFO("Scene entities: {}", scene.EntityCount());

        Nova::ProjectDescriptor project;
        std::filesystem::path projectRoot = NOVA_SOURCE_DIR;
        if (Nova::LoadProject(NOVA_SOURCE_DIR, project).Ok) {
            projectRoot = project.Root;
        }

        Nova::MeshAssetCache meshCache;
        Nova::TextureAssetCache textureCache;
        Nova::Clock clock;
        Nova::PluginRegistry plugins;
        Nova::ServiceHub services;
        Nova::RegisterBuiltinPlugins(plugins, services, projectRoot / ".nova" / "storage");
        NOVA_LOG_INFO("Plugins loaded: {}", plugins.Count());
        if (services.AI) {
            NOVA_LOG_INFO("AI provider: {} available={}", services.AI->Id(),
                          services.AI->IsAvailable());
        }

        while (!window.ShouldClose()) {
            input.BeginFrame();
            window.PollEvents(input);
            clock.Tick();

            if (input.IsKeyPressed(Nova::KeyCode::Escape)) {
                break;
            }

            uint32_t fbW = 0, fbH = 0;
            window.GetFramebufferSize(fbW, fbH);
            const float aspect = fbH > 0 ? static_cast<float>(fbW) / static_cast<float>(fbH)
                                         : 16.0f / 9.0f;

            Nova::TickScene(scene, clock.DeltaSeconds());
            Nova::RenderScene(scene, *renderer, aspect, projectRoot, meshCache, textureCache);

            renderer->BeginFrame();
            renderer->BeginDrawing();
            renderer->EndFrame();
        }

        renderer->Shutdown();
    }

    Nova::Log::Shutdown();
    return 0;
}
