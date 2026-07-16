#include "engine/App/FlyCamera.hpp"

#include "engine/Platform/Input.hpp"

#include <cmath>

namespace App
{

namespace
{
constexpr float MaxPitch = glm::radians(89.0f);
} // namespace

void FlyCamera::update(const Platform::Input& input, float dt)
{
    using namespace Platform;

    _looking = input.down(MouseButton::Right);
    if (_looking)
    {
        const glm::vec2 delta = input.mouseDelta();
        _yaw -= delta.x * sensitivity;
        _pitch = glm::clamp(_pitch - (delta.y * sensitivity), -MaxPitch, MaxPitch);
    }

    const glm::vec3 fwd = forward();
    const glm::vec3 right = glm::normalize(glm::cross(fwd, {0.0f, 1.0f, 0.0f}));

    glm::vec3 move{};
    move += fwd * (float(input.down(Key::W)) - float(input.down(Key::S)));
    move += right * (float(input.down(Key::D)) - float(input.down(Key::A)));
    move.y += float(input.down(Key::Q)) - float(input.down(Key::Z));

    if (move != glm::vec3())
    {
        const float speed = moveSpeed * (input.down(Key::LShift) ? fastMultiplier : 1.0f);
        _position += glm::normalize(move) * speed * dt;
    }
}

glm::vec3 FlyCamera::forward() const
{
    const float cosPitch = std::cos(_pitch);
    return {-std::sin(_yaw) * cosPitch, std::sin(_pitch), -std::cos(_yaw) * cosPitch};
}

glm::mat4 FlyCamera::view() const
{
    return glm::lookAt(_position, _position + forward(), {0.0f, 1.0f, 0.0f});
}

} // namespace App
