#pragma once

#include "engine/Core/Math.hpp"

#include <bitset>
#include <cstdint>

namespace Platform
{

// matches SDL_SCANCODE_*, whitch contains more than 255 scancodes, but we don't use whem all.
// NOLINTNEXTLINE(performance-enum-size)
enum class Key : uint16_t
{
    A = 4,
    B = 5,
    C = 6,
    D = 7,
    E = 8,
    F = 9,
    G = 10,
    H = 11,
    I = 12,
    J = 13,
    K = 14,
    L = 15,
    M = 16,
    N = 17,
    O = 18,
    P = 19,
    Q = 20,
    R = 21,
    S = 22,
    T = 23,
    U = 24,
    V = 25,
    W = 26,
    X = 27,
    Y = 28,
    Z = 29,

    Num1 = 30,
    Num2 = 31,
    Num3 = 32,
    Num4 = 33,
    Num5 = 34,
    Num6 = 35,
    Num7 = 36,
    Num8 = 37,
    Num9 = 38,
    Num0 = 39,

    Enter = 40,
    Escape = 41,
    Backspace = 42,
    Tab = 43,
    Space = 44,
    Minus = 45,
    Equals = 46,
    LeftBracket = 47,
    RightBracket = 48,
    Backslash = 49,
    Semicolon = 51,
    Apostrophe = 52,
    Grave = 53,
    Comma = 54,
    Period = 55,
    Slash = 56,
    CapsLock = 57,

    F1 = 58,
    F2 = 59,
    F3 = 60,
    F4 = 61,
    F5 = 62,
    F6 = 63,
    F7 = 64,
    F8 = 65,
    F9 = 66,
    F10 = 67,
    F11 = 68,
    F12 = 69,

    PrintScreen = 70,
    ScrollLock = 71,
    Pause = 72,
    Insert = 73,
    Home = 74,
    PageUp = 75,
    Delete = 76,
    End = 77,
    PageDown = 78,
    Right = 79,
    Left = 80,
    Down = 81,
    Up = 82,

    LCtrl = 224,
    LShift = 225,
    LAlt = 226,
    LGui = 227,
    RCtrl = 228,
    RShift = 229,
    RAlt = 230,
    RGui = 231,
};

enum class MouseButton : uint8_t
{
    Left = 1,   // SDL_BUTTON_LEFT
    Middle = 2, // SDL_BUTTON_MIDDLE
    Right = 3,  // SDL_BUTTON_RIGHT
    X1 = 4,     // SDL_BUTTON_X1
    X2 = 5,     // SDL_BUTTON_X2
};

class Input
{
public:
    bool down(Key key) const;
    bool pressed(Key key) const;
    bool released(Key key) const;

    bool down(MouseButton button) const;
    bool pressed(MouseButton button) const;
    bool released(MouseButton button) const;

    glm::vec2 mousePosition() const;
    glm::vec2 mouseDelta() const;
    float wheelDelta() const;

private:
    friend class Window;

    void beginFrame();

    static size_t index(Key key);
    static size_t index(MouseButton button);

    std::bitset<256> _keys;
    std::bitset<256> _prevKeys;
    std::bitset<8> _buttons;
    std::bitset<8> _prevButtons;

    glm::vec2 _mousePosition{};
    glm::vec2 _mouseDelta{};
    float _wheelDelta = 0.0f;
};

} // namespace Platform
