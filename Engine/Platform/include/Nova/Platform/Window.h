#pragma once

#include <string>
#include <cstdint>

struct SDL_Window;

namespace Nova {

class Input;  // forward declaration

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

    /// Poll events and forward to Input.
    void PollEvents(Input& input);
    bool ShouldClose() const { return m_ShouldClose; }
    bool IsValid()     const { return m_Window != nullptr; }

    uint32_t GetWidth()  const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }

    /// Retina framebuffer size in pixels (what Metal draws into).
    void GetFramebufferSize(uint32_t& width, uint32_t& height) const;
    /// pixels / points — 2.0 on typical Retina.
    float GetContentScale() const;

    SDL_Window* GetSDLWindow() const { return m_Window; }
    /// CAMetalLayer* as void*. Valid for the lifetime of this Window.
    void* GetNativeMetalLayer() const;

    /// Show, raise, and activate so the window is not stuck behind the IDE.
    void BringToFront();

private:
    SDL_Window* m_Window     = nullptr;
    void*       m_MetalView  = nullptr; // SDL_MetalView
    uint32_t    m_Width;
    uint32_t    m_Height;
    bool        m_ShouldClose = false;
};

} // namespace Nova
