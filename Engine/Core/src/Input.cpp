#include <Nova/Core/Input.h>

namespace Nova {

void Input::OnKeyDown(KeyCode key) {
    auto& flags = m_KeyMap[static_cast<uint32_t>(key)];
    if (!flags.held) {
        flags.pressed = true;
    }
    flags.held = true;
}

void Input::OnKeyUp(KeyCode key) {
    auto& flags = m_KeyMap[static_cast<uint32_t>(key)];
    flags.held = false;
    flags.released = true;
}

void Input::OnMouseMove(float x, float y, float dx, float dy) {
    m_MouseX = x;
    m_MouseY = y;
    m_MouseDeltaX += dx;
    m_MouseDeltaY += dy;
}

void Input::OnMouseButtonDown(MouseButton btn) {
    auto idx = static_cast<uint8_t>(btn);
    if (idx < 3 && !m_MouseButtonState[idx]) {
        m_MouseButtonPressed[idx] = true;
    }
    if (idx < 3) m_MouseButtonState[idx] = true;
}

void Input::OnMouseButtonUp(MouseButton btn) {
    auto idx = static_cast<uint8_t>(btn);
    if (idx < 3) m_MouseButtonState[idx] = false;
}

void Input::OnMouseScroll(float scrollX, float scrollY) {
    m_ScrollX += scrollX;
    m_ScrollY += scrollY;
}

void Input::BeginFrame() {
    for (auto& [key, flags] : m_KeyMap) {
        flags.pressed = false;
        flags.released = false;
    }
    m_MouseDeltaX = 0;
    m_MouseDeltaY = 0;
    m_ScrollX = 0;
    m_ScrollY = 0;
    m_MouseButtonPressed[0] = m_MouseButtonPressed[1] = m_MouseButtonPressed[2] = false;
}

bool Input::IsKeyDown(KeyCode key) const {
    auto it = m_KeyMap.find(static_cast<uint32_t>(key));
    return it != m_KeyMap.end() && it->second.held;
}

bool Input::IsKeyPressed(KeyCode key) const {
    auto it = m_KeyMap.find(static_cast<uint32_t>(key));
    return it != m_KeyMap.end() && it->second.pressed;
}

bool Input::IsKeyReleased(KeyCode key) const {
    auto it = m_KeyMap.find(static_cast<uint32_t>(key));
    return it != m_KeyMap.end() && it->second.released;
}

bool Input::IsMouseButtonDown(MouseButton btn) const {
    auto idx = static_cast<uint8_t>(btn);
    return idx < 3 && m_MouseButtonState[idx];
}

bool Input::IsMouseButtonPressed(MouseButton btn) const {
    auto idx = static_cast<uint8_t>(btn);
    return idx < 3 && m_MouseButtonPressed[idx];
}

} // namespace Nova
