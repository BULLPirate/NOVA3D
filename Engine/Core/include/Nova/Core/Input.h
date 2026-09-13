#pragma once

#include <cstdint>
#include <unordered_map>

namespace Nova {

/// Key codes aligned with SDL_Scancode values.
/// If you add keys, use the SDL_SCANCODE_* constant as the value.
enum class KeyCode : uint32_t {
    Unknown = 0,
    A = 4,  B = 5,  C = 6,  D = 7,  E = 8,  F = 9,  G = 10,
    H = 11, I = 12, J = 13, K = 14, L = 15, M = 16, N = 17,
    O = 18, P = 19, Q = 20, R = 21, S = 22, T = 23, U = 24,
    V = 25, W = 26, X = 27, Y = 28, Z = 29,
    Num1 = 30, Num2 = 31, Num3 = 32, Num4 = 33, Num5 = 34,
    Num6 = 35, Num7 = 36, Num8 = 37, Num9 = 38, Num0 = 39,
    Return = 40, Escape = 41, Backspace = 42, Tab = 43, Space = 44,
    Right = 79, Left = 80, Down = 81, Up = 82,
    F1 = 58, F2 = 59, F3 = 60, F4 = 61, F5 = 62, F6 = 63,
    F7 = 64, F8 = 65, F9 = 66, F10 = 67, F11 = 68, F12 = 69,
    LCtrl = 224, LShift = 225, LAlt = 226, RCtrl = 228, RShift = 229, RAlt = 230,
};

enum class MouseButton : uint8_t {
    Left = 0, Right = 1, Middle = 2,
};

/// Per-frame input state. Queried after event processing.
class Input {
public:
    Input() = default;

    void OnKeyDown(KeyCode key);
    void OnKeyUp(KeyCode key);
    void OnMouseMove(float x, float y, float dx, float dy);
    void OnMouseButtonDown(MouseButton btn);
    void OnMouseButtonUp(MouseButton btn);
    void OnMouseScroll(float scrollX, float scrollY);
    void BeginFrame();

    bool IsKeyDown(KeyCode key)    const;
    bool IsKeyUp(KeyCode key)      const { return !IsKeyDown(key); }
    bool IsKeyPressed(KeyCode key) const;
    bool IsKeyReleased(KeyCode key) const;

    float GetMouseX()       const { return m_MouseX; }
    float GetMouseY()       const { return m_MouseY; }
    float GetMouseDeltaX()  const { return m_MouseDeltaX; }
    float GetMouseDeltaY()  const { return m_MouseDeltaY; }
    float GetScrollX()      const { return m_ScrollX; }
    float GetScrollY()      const { return m_ScrollY; }
    bool  IsMouseButtonDown(MouseButton btn) const;
    bool  IsMouseButtonPressed(MouseButton btn) const;

private:
    struct KeyFlags {
        bool held     = false;
        bool pressed  = false;
        bool released = false;
    };

    std::unordered_map<uint32_t, KeyFlags> m_KeyMap;

    float m_MouseX = 0, m_MouseY = 0;
    float m_MouseDeltaX = 0, m_MouseDeltaY = 0;
    float m_ScrollX = 0, m_ScrollY = 0;
    bool  m_MouseButtonState[3]   = {};
    bool  m_MouseButtonPressed[3] = {};
};

} // namespace Nova
