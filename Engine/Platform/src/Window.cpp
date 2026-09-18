#include <Nova/Platform/Window.h>
#include <Nova/Platform/MacApp.h>
#include <Nova/Core/Log.h>
#include <Nova/Core/Input.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>

namespace Nova {

void ForceOrderFrontCocoaWindow(void* nsWindow);

Window::Window(const WindowProps& props)
    : m_Width(props.Width), m_Height(props.Height)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        NOVA_LOG_FATAL("SDL_Init failed: {}", SDL_GetError());
        return;
    }

    SDL_PropertiesID createProps = SDL_CreateProperties();
    SDL_SetStringProperty(createProps, SDL_PROP_WINDOW_CREATE_TITLE_STRING, props.Title.c_str());
    SDL_SetNumberProperty(createProps, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, m_Width);
    SDL_SetNumberProperty(createProps, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, m_Height);
    SDL_SetNumberProperty(createProps, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(createProps, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetBooleanProperty(createProps, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
    SDL_SetBooleanProperty(createProps, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
    SDL_SetBooleanProperty(createProps, SDL_PROP_WINDOW_CREATE_METAL_BOOLEAN, true);
    SDL_SetBooleanProperty(createProps, SDL_PROP_WINDOW_CREATE_FOCUSABLE_BOOLEAN, true);
    SDL_SetBooleanProperty(createProps, SDL_PROP_WINDOW_CREATE_ALWAYS_ON_TOP_BOOLEAN, true);

    m_Window = SDL_CreateWindowWithProperties(createProps);
    SDL_DestroyProperties(createProps);

    if (!m_Window) {
        NOVA_LOG_FATAL("SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        return;
    }

    m_MetalView = SDL_Metal_CreateView(m_Window);
    if (!m_MetalView) {
        NOVA_LOG_FATAL("SDL_Metal_CreateView failed: {}", SDL_GetError());
        SDL_DestroyWindow(m_Window);
        m_Window = nullptr;
        SDL_Quit();
        return;
    }

    SDL_ShowWindow(m_Window);
    BringToFront();

    NOVA_LOG_INFO("Window created: {} ({}x{}, scale {:.1f})",
                  props.Title, m_Width, m_Height, GetContentScale());
}

void Window::BringToFront() {
    EnsureMacOSApplicationReady();
    if (!m_Window) return;

    SDL_SetWindowAlwaysOnTop(m_Window, true);
    SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(m_Window);

    SDL_PropertiesID winProps = SDL_GetWindowProperties(m_Window);
    void* nsWindow = SDL_GetPointerProperty(winProps, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    ForceOrderFrontCocoaWindow(nsWindow);

    SDL_RaiseWindow(m_Window);
    SDL_PumpEvents();
    SDL_SetWindowAlwaysOnTop(m_Window, false);

    int x = 0, y = 0, w = 0, h = 0;
    SDL_GetWindowPosition(m_Window, &x, &y);
    SDL_GetWindowSize(m_Window, &w, &h);
    const bool hidden = (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_HIDDEN) != 0;
    NOVA_LOG_INFO("Window geometry: {}x{} at ({},{}), {}", w, h, x, y, hidden ? "HIDDEN" : "visible");
}

Window::~Window() {
    if (m_MetalView) {
        SDL_Metal_DestroyView(m_MetalView);
        m_MetalView = nullptr;
    }
    if (m_Window) {
        SDL_DestroyWindow(m_Window);
        m_Window = nullptr;
    }
    SDL_Quit();
    NOVA_LOG_INFO("Window destroyed");
}

void Window::GetFramebufferSize(uint32_t& width, uint32_t& height) const {
    int w = 0, h = 0;
    if (m_Window) {
        SDL_GetWindowSizeInPixels(m_Window, &w, &h);
    }
    width  = static_cast<uint32_t>(w);
    height = static_cast<uint32_t>(h);
}

float Window::GetContentScale() const {
    if (!m_Window) return 1.0f;
    const float density = SDL_GetWindowPixelDensity(m_Window);
    return density > 0.0f ? density : 1.0f;
}

void* Window::GetNativeMetalLayer() const {
    if (!m_MetalView) return nullptr;
    return SDL_Metal_GetLayer(m_MetalView);
}

void Window::SetCursorCaptured(bool captured) {
    m_CursorCaptured = captured;
    if (!m_Window) {
        return;
    }
    SDL_SetWindowRelativeMouseMode(m_Window, captured);
}

void Window::PollEvents(Input& input, const EventHook& hook) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (hook) {
            hook(event);
        }
        switch (event.type) {
            case SDL_EVENT_QUIT:
                m_ShouldClose = true;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                m_Width  = static_cast<uint32_t>(event.window.data1);
                m_Height = static_cast<uint32_t>(event.window.data2);
                NOVA_LOG_DEBUG("Window resized to {}x{}", m_Width, m_Height);
                break;

            // ── Keyboard (use scancode — stable, hardware-oriented) ────────
            case SDL_EVENT_KEY_DOWN:
                input.OnKeyDown(static_cast<KeyCode>(event.key.scancode));
                break;
            case SDL_EVENT_KEY_UP:
                input.OnKeyUp(static_cast<KeyCode>(event.key.scancode));
                break;

            // ── Mouse ────────────────────────────────────────────────────────
            case SDL_EVENT_MOUSE_MOTION:
                input.OnMouseMove(
                    event.motion.x, event.motion.y,
                    event.motion.xrel, event.motion.yrel
                );
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                input.OnMouseButtonDown(static_cast<MouseButton>(event.button.button - 1));
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                input.OnMouseButtonUp(static_cast<MouseButton>(event.button.button - 1));
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                input.OnMouseScroll(event.wheel.x, event.wheel.y);
                break;

            default:
                break;
        }
    }
}

} // namespace Nova
