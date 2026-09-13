#include <Nova/Platform/Window.h>
#include <Nova/Core/Log.h>

#include <SDL3/SDL.h>

namespace Nova {

Window::Window(const WindowProps& props)
    : m_Width(props.Width), m_Height(props.Height)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        NOVA_LOG_FATAL("SDL_Init failed: {}", SDL_GetError());
        return;
    }

    m_Window = SDL_CreateWindow(
        props.Title.c_str(),
        static_cast<int>(m_Width),
        static_cast<int>(m_Height),
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );

    if (!m_Window) {
        NOVA_LOG_FATAL("SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        return;
    }

    NOVA_LOG_INFO("Window created: {} ({}x{})", props.Title, m_Width, m_Height);
}

Window::~Window() {
    if (m_Window) {
        SDL_DestroyWindow(m_Window);
    }
    SDL_Quit();
    NOVA_LOG_INFO("Window destroyed");
}

void Window::PollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                m_ShouldClose = true;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                m_Width  = static_cast<uint32_t>(event.window.data1);
                m_Height = static_cast<uint32_t>(event.window.data2);
                NOVA_LOG_DEBUG("Window resized to {}x{}", m_Width, m_Height);
                break;
            case SDL_EVENT_KEY_DOWN:
                NOVA_LOG_DEBUG("Key pressed: {}", SDL_GetKeyName(event.key.key));
                break;
            default:
                break;
        }
    }
}

} // namespace Nova
