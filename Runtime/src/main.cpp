#include <Nova/Core/Log.h>
#include <Nova/Core/Input.h>
#include <Nova/Core/Time.h>
#include <Nova/Core/EngineSettings.h>
#include <Nova/Audio/AudioEngine.h>
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
#include <Nova/Scene/Gameplay.h>

#include <Nova/Project/ProjectTemplate.h>
#include <Nova/Project/Workspace.h>
#include <Nova/Scene/GameSession.h>
#include <Nova/Scene/SaveGame.h>

#include <NovaRuntimeHud.h>
#include <SDL3/SDL_events.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace {

bool ParseFlag(int argc, char** argv, const char* name, std::filesystem::path& out) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0 && i + 1 < argc) {
            out = argv[++i];
            return true;
        }
    }
    return false;
}

bool HasArg(int argc, char** argv, const char* name) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0) {
            return true;
        }
    }
    return false;
}

std::string ParseString(int argc, char** argv, const char* name, const std::string& fallback) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0 && i + 1 < argc) {
            return argv[++i];
        }
    }
    return fallback;
}

} // namespace

#ifndef NOVA_SOURCE_DIR
#define NOVA_SOURCE_DIR ""
#endif

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
        renderer->SetRenderPassReadyCallback(Nova::RuntimeHud::PassReady, nullptr);
        renderer->SetFrameOverlayCallback(Nova::RuntimeHud::Overlay, nullptr);
        Nova::RuntimeHud::Init(window.GetSDLWindow(), renderer->GetNativeDevice());

        std::filesystem::path ensureRoot;
        ParseFlag(argc, argv, "--ensure-project", ensureRoot);
        const std::string ensureName = ParseString(argc, argv, "--name", "Game");
        const std::string templateName = ParseString(argc, argv, "--template", "empty");
        if (!ensureRoot.empty()) {
            Nova::ProjectTemplateKind kind = Nova::ProjectTemplateKind::Empty;
            if (templateName == "combat" || templateName == "knight" || templateName == "bandits") {
                kind = Nova::ProjectTemplateKind::KnightBandits;
            } else if (templateName == "tpp" || templateName == "third" ||
                       templateName == "sandbox") {
                kind = Nova::ProjectTemplateKind::ThirdPerson;
            }
            if (Nova::ProjectIOResult created =
                    Nova::EnsureGameProject(ensureRoot, ensureName, kind, NOVA_SOURCE_DIR);
                !created.Ok) {
                NOVA_LOG_FATAL("Failed to create project '{}': {}", ensureRoot.string(), created.Error);
                Nova::Log::Shutdown();
                return 1;
            }
        }

        std::filesystem::path projectArg;
        ParseFlag(argc, argv, "--project", projectArg);
        if (projectArg.empty()) {
            projectArg = ensureRoot;
        }
        if (projectArg.empty()) {
            const std::filesystem::path desktopGame =
                std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : "") /
                "Desktop" / "KnightBandits";
            const std::filesystem::path documentsGame = Nova::DefaultProjectsDirectory() / "KnightBandits";
            if (Nova::IsNovaProjectRoot(desktopGame)) {
                projectArg = desktopGame;
            } else if (Nova::IsNovaProjectRoot(documentsGame)) {
                projectArg = documentsGame;
            } else {
                projectArg = documentsGame;
                ensureRoot = documentsGame;
                if (Nova::ProjectIOResult created = Nova::EnsureGameProject(
                        documentsGame, "Knight Bandits", Nova::ProjectTemplateKind::KnightBandits,
                        NOVA_SOURCE_DIR);
                    !created.Ok) {
                    NOVA_LOG_FATAL("Failed to create default game: {}", created.Error);
                    Nova::Log::Shutdown();
                    return 1;
                }
            }
        }
        if (projectArg.empty()) {
            NOVA_LOG_FATAL("NOVA3D runtime needs --project PATH. The engine repo is not a game.");
            Nova::Log::Shutdown();
            return 1;
        }

        Nova::ProjectDescriptor project;
        if (!Nova::LoadProject(projectArg, project).Ok) {
            NOVA_LOG_FATAL("Failed to load project '{}'", projectArg.string());
            Nova::Log::Shutdown();
            return 1;
        }
        std::filesystem::path projectRoot = project.Root;
        std::filesystem::path sceneArg;
        ParseFlag(argc, argv, "--scene", sceneArg);
        std::filesystem::path scenePath =
            sceneArg.empty() ? project.StartupSceneAbsolute()
                             : (sceneArg.is_absolute() ? sceneArg : project.Root / sceneArg);
        if (!std::filesystem::exists(scenePath)) {
            scenePath = project.LastOpenedSceneAbsolute();
        }

        Nova::Scene scene;
        const Nova::SceneIOResult sceneLoad = Nova::LoadSceneFromFile(scenePath, scene);
        if (!sceneLoad.Ok) {
            NOVA_LOG_WARN("Failed to load scene '{}': {} — empty level", scenePath.string(),
                          sceneLoad.Error);
            scene = Nova::Scene::CreateEmptyLevel();
        } else {
            NOVA_LOG_INFO("Scene loaded from {}", scenePath.string());
        }
        if (!scene.FindEntityByName("Ground").IsValid() || !scene.FindEntityByName("Player").IsValid() ||
            scene.FindEntityByName("Player Body").IsValid()) {
            NOVA_LOG_INFO("Scene has no playable map — building village, hero and bandits");
            scene = Nova::Scene::CreatePlayableLevel();
            Nova::SaveSceneToFile(scene, scenePath);
        }
        NOVA_LOG_INFO("Scene entities: {}", scene.EntityCount());

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

        Nova::EngineSettings engineSettings = Nova::EngineSettings::Defaults();
        Nova::LoadEngineSettings(projectRoot, engineSettings);
        Nova::AudioEngine::Get().Init();
        Nova::AudioEngine::Get().SetEnabled(false);
        Nova::AudioEngine::Get().SetMasterVolume(0.0f);
        engineSettings.EnableAudio = false;
        engineSettings.MasterVolume = 0.0f;
        engineSettings.MouseLook = false;
        bool playJustStarted = true;
        bool wantQuit = false;
        Nova::GameSession session;
        if (HasArg(argc, argv, "--play")) {
            session.Current = Nova::GameSession::Phase::Playing;
            session.PhaseTime = 0.0f;
        }
        window.SetCursorCaptured(false);

        while (!window.ShouldClose() && !wantQuit) {
            input.BeginFrame();
            window.PollEvents(input, [](const SDL_Event& event) {
                Nova::RuntimeHud::ProcessEvent(&event);
            });
            clock.Tick();
            session.Tick(clock.DeltaSeconds());
            if (session.Current == Nova::GameSession::Phase::Loading) {
                playJustStarted = true;
            }

            if (input.IsKeyPressed(Nova::KeyCode::Escape)) {
                if (session.Current == Nova::GameSession::Phase::Playing) {
                    session.TogglePause();
                } else if (session.Current == Nova::GameSession::Phase::Paused ||
                           session.Current == Nova::GameSession::Phase::Settings) {
                    session.QuitToTitle();
                } else if (session.Current == Nova::GameSession::Phase::Title) {
                    wantQuit = true;
                }
            }
            if (input.IsKeyPressed(Nova::KeyCode::Tab) &&
                session.Current == Nova::GameSession::Phase::Playing) {
                const bool capture = !window.IsCursorCaptured();
                window.SetCursorCaptured(capture);
                engineSettings.MouseLook = capture;
            }
            if (session.Current == Nova::GameSession::Phase::Playing) {
                if (!window.IsCursorCaptured()) {
                    window.SetCursorCaptured(true);
                    engineSettings.MouseLook = true;
                }
            } else {
                window.SetCursorCaptured(false);
                engineSettings.MouseLook = false;
            }
            if (session.Current == Nova::GameSession::Phase::Playing) {
                if (input.IsKeyPressed(Nova::KeyCode::F8)) {
                    Nova::SaveGameSlot(project, "quick", scene);
                }
                if (input.IsKeyPressed(Nova::KeyCode::F9)) {
                    Nova::LoadGameSlot(project, "quick", scene);
                }
            }
            if (input.IsKeyPressed(Nova::KeyCode::F6) &&
                (session.Current == Nova::GameSession::Phase::Playing ||
                 session.Current == Nova::GameSession::Phase::Paused)) {
                session.TogglePause();
            }

            uint32_t fbW = 0, fbH = 0;
            window.GetFramebufferSize(fbW, fbH);
            const float aspect = fbH > 0 ? static_cast<float>(fbW) / static_cast<float>(fbH)
                                         : 16.0f / 9.0f;

            renderer->BeginFrame();
            Nova::RuntimeHud::NewFrame();
            if (!session.BlocksWorldTick()) {
                Nova::TickScene(scene, clock.DeltaSeconds(), &input, &engineSettings, projectRoot,
                                playJustStarted);
                playJustStarted = false;
            }
            Nova::RenderScene(scene, *renderer, aspect, projectRoot, meshCache, textureCache);
            const Nova::GameplayHudSnapshot hud = Nova::QueryGameplayHud(scene);
            window.SetTitle(Nova::FormatGameplayHudTitle(hud));
            if (hud.Phase == Nova::GameplayPhase::Victory &&
                input.IsKeyPressed(Nova::KeyCode::R) &&
                session.Current == Nova::GameSession::Phase::Playing) {
                if (Nova::LoadSceneFromFile(scenePath, scene).Ok) {
                    playJustStarted = true;
                }
            }
            Nova::RuntimeHud::Draw(hud, project.Name.c_str(), session, engineSettings);
            Nova::RuntimeHud::EndFrameWidgets();

            renderer->BeginDrawing();
            renderer->EndFrame();
        }

        window.SetCursorCaptured(false);
        Nova::RuntimeHud::Shutdown();

        renderer->Shutdown();
        Nova::AudioEngine::Get().Shutdown();
    }

    Nova::Log::Shutdown();
    return 0;
}
