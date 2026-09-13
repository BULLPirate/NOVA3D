#pragma once

#include <string>
#include <cstdint>

struct SDL_Window;

namespace Nova {

struct WindowProps {
    std::string Title  = "NOVA3D";
    uint32_t   Width   = 1280;
    uint32_t   Height  = 720;
};

/// Platform window backed by SDL3.
class Window {
public:
    explicit Window(const WindowProps& props = {});
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void PollEvents();
    bool ShouldClose() const { return m_ShouldClose; }

    uint32_t GetWidth()  const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }

    SDL_Window* GetSDLWindow() const { return m_Window; }

private:
    SDL_Window* m_Window     = nullptr;
    uint32_t    m_Width;
    uint32_t    m_Height;
    bool        m_ShouldClose = false;
};

} // namespace Nova
