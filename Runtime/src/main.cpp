#include <Nova/Core/Log.h>
#include <Nova/Core/Input.h>
#include <Nova/Platform/Window.h>
#include <Nova/Renderer/Renderer.h>
#include <Nova/Renderer/Camera.h>

#include <SDL3/SDL.h>

int main() {
    Nova::Log::Init();

    NOVA_LOG_INFO("NOVA3D Engine v0.1.0 — camera");

    {
        Nova::Window window({"NOVA3D — Camera", 1280, 720});
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

        NOVA_LOG_INFO("Camera orbits the triangle — Escape to exit.");
        while (!window.ShouldClose()) {
            input.BeginFrame();
            window.PollEvents(input);

            if (input.IsKeyPressed(Nova::KeyCode::Escape)) {
                NOVA_LOG_INFO("Escape pressed — exiting.");
                break;
            }

            uint32_t fbW = 0, fbH = 0;
            window.GetFramebufferSize(fbW, fbH);
            const float aspect = fbH > 0 ? static_cast<float>(fbW) / static_cast<float>(fbH) : 16.0f / 9.0f;

            const float t = static_cast<float>(SDL_GetTicks()) * 0.001f;
            Nova::Camera camera;
            camera.Aspect = aspect;
            camera.SetOrbit(t * 0.6f, 2.2f, 0.15f);
            renderer->SetCamera(camera);

            renderer->BeginFrame();
            renderer->EndFrame();
        }

        renderer->Shutdown();
        NOVA_LOG_INFO("Main loop exited.");
    }

    Nova::Log::Shutdown();
    return 0;
}
