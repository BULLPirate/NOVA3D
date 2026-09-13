#include <Nova/Core/Log.h>
#include <Nova/Platform/Window.h>
#include <Nova/Math/Math.h>

int main() {
    Nova::Log::Init();

    NOVA_LOG_INFO("NOVA3D Engine v0.1.0 — Foundation");

    {
        Nova::Window window({"NOVA3D", 1280, 720});

        // Smoke test: math
        Nova::Vec3 a{1, 0, 0};
        Nova::Vec3 b{0, 1, 0};
        auto c = a.Cross(b);
        NOVA_LOG_INFO("Cross(1,0,0 x 0,1,0) = ({}, {}, {})", c.x, c.y, c.z);

        auto proj = Nova::Mat4::Perspective(Nova::Radians(60.0f), 16.0f/9.0f, 0.1f, 100.0f);
        NOVA_LOG_INFO("Perspective matrix created OK (m[0][0]={})", proj.m[0][0]);

        // Main loop
        NOVA_LOG_INFO("Entering main loop...");
        while (!window.ShouldClose()) {
            window.PollEvents();
            // TODO: update, render
        }
        NOVA_LOG_INFO("Main loop exited.");
    }

    Nova::Log::Shutdown();
    return 0;
}
