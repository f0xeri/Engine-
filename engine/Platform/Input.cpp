#include "engine/Platform/Input.hpp"

namespace Platform
{

bool Input::down(Key key) const
{
    return _keys[index(key)];
}

bool Input::pressed(Key key) const
{
    return _keys[index(key)] && !_prevKeys[index(key)];
}

bool Input::released(Key key) const
{
    return !_keys[index(key)] && _prevKeys[index(key)];
}

bool Input::down(MouseButton button) const
{
    return _buttons[index(button)];
}

bool Input::pressed(MouseButton button) const
{
    return _buttons[index(button)] && !_prevButtons[index(button)];
}

bool Input::released(MouseButton button) const
{
    return !_buttons[index(button)] && _prevButtons[index(button)];
}

glm::vec2 Input::mousePosition() const
{
    return _mousePosition;
}

glm::vec2 Input::mouseDelta() const
{
    return _mouseDelta;
}

float Input::wheelDelta() const
{
    return _wheelDelta;
}

void Input::beginFrame()
{
    _prevKeys = _keys;
    _prevButtons = _buttons;
    _mouseDelta = {};
    _wheelDelta = 0.0f;
}

size_t Input::index(Key key)
{
    return static_cast<size_t>(key);
}

size_t Input::index(MouseButton button)
{
    return static_cast<size_t>(button);
}

} // namespace Platform
