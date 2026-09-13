#include <Nova/Platform/Window.h>
#include <Nova/Core/Log.h>

namespace Nova {

Window::Window(const WindowProps& props)
    : m_Width(props.Width), m_Height(props.Height)
{
    NOVA_LOG_INFO("Window created: %s (%ux%u)", props.Title.c_str(), m_Width, m_Height);
}

Window::~Window() {
    NOVA_LOG_INFO("Window destroyed");
}

void Window::PollEvents() {
    // Stub — SDL3 event loop in commit 3
}

} // namespace Nova
