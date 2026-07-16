#pragma once

#include "engine/Core/Math.hpp"

namespace Platform
{
class Input;
} // namespace Platform

namespace App
{

class FlyCamera
{
public:
    void update(const Platform::Input& input, float dt);

    glm::mat4 view() const;
    glm::vec3 position() const { return _position; }

    // if right mouse button down we should hide cursor
    bool looking() const { return _looking; }

    float moveSpeed = 5.0f; // m/s
    float fastMultiplier = 4.0f;
    float sensitivity = 0.0025f; // rad/pixel

private:
    glm::vec3 forward() const;

    glm::vec3 _position{0.0f, 0.0f, 3.0f};
    float _yaw = 0.0f; // rad, 0 = looking down -Z
    float _pitch = 0.0f;
    bool _looking = false;
};

} // namespace App
