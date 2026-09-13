#pragma once

#include <string>
#include <cstdint>

namespace Nova {

struct WindowProps {
    std::string Title  = "NOVA3D";
    uint32_t   Width   = 1280;
    uint32_t   Height  = 720;
};

/// Platform window — SDL3-backed in commit 3.
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

private:
    uint32_t m_Width;
    uint32_t m_Height;
    bool     m_ShouldClose = false;
};

} // namespace Nova
