#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>                  // IWYU pragma: export
#include <glm/gtc/matrix_transform.hpp> // IWYU pragma: export
#include <glm/gtc/type_ptr.hpp>         // IWYU pragma: export

#include <cmath>

namespace Core
{
// vulkan-friendly perspective. reversed-Z, +Y up, -Z forward (m[1][1] flips clip Y)
inline glm::mat4 perspective(float fovY, float aspect, float nearPlane)
{
    const float f = 1.0f / std::tan(fovY * 0.5f);

    glm::mat4 m(0.0f);
    m[0][0] = f / aspect;
    m[1][1] = -f;
    m[2][3] = -1.0f;
    m[3][2] = nearPlane;
    return m;
}

// vulkan-friendly orthographic, centered. reversed-Z like perspective, so GREATER holds
inline glm::mat4 orthographic(float halfWidth, float halfHeight, float nearPlane, float farPlane)
{
    const float range = farPlane - nearPlane;

    glm::mat4 m(0.0f);
    m[0][0] = 1.0f / halfWidth;
    m[1][1] = -1.0f / halfHeight;
    m[2][2] = 1.0f / range;
    m[3][2] = farPlane / range;
    m[3][3] = 1.0f;
    return m;
}

} // namespace Core
