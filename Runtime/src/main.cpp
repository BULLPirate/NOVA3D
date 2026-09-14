#include <Nova/Core/Log.h>
#include <Nova/Core/Input.h>
#include <Nova/Platform/Window.h>
#include <Nova/Renderer/Renderer.h>

int main() {
    Nova::Log::Init();

    NOVA_LOG_INFO("NOVA3D Engine v0.1.0 — Metal renderer");

    {
        Nova::Window window({"NOVA3D — Metal", 1280, 720});
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

        // Bright cyan-blue: if Metal clear works, this cannot be mistaken for an empty window.
        renderer->SetClearColor(0.10f, 0.55f, 0.90f, 1.0f);

        NOVA_LOG_INFO("Entering main loop... Press Escape or close the window to exit.");
        bool firstFrameLogged = false;
        while (!window.ShouldClose()) {
            input.BeginFrame();
            window.PollEvents(input);

            if (input.IsKeyPressed(Nova::KeyCode::Escape)) {
                NOVA_LOG_INFO("Escape pressed — exiting.");
                break;
            }

            renderer->BeginFrame();
            renderer->EndFrame();

            if (!firstFrameLogged) {
                NOVA_LOG_INFO("First Metal frame presented — window should be visible.");
                firstFrameLogged = true;
            }
        }

        renderer->Shutdown();
        NOVA_LOG_INFO("Main loop exited.");
    }

    Nova::Log::Shutdown();
    return 0;
}
