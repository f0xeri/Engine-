#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>                  // IWYU pragma: export
#include <glm/gtc/matrix_transform.hpp> // IWYU pragma: export

#include <cmath>

namespace Core
{
// vulkan-friendly perspective. reversed-Z, +Y up, -Z forward (Y flip in viewport)
inline glm::mat4 perspective(float fovY, float aspect, float nearPlane)
{
    const float f = 1.0f / std::tan(fovY * 0.5f);

    glm::mat4 m(0.0f);
    m[0][0] = f / aspect;
    m[1][1] = f;
    m[2][3] = -1.0f;
    m[3][2] = nearPlane;
    return m;
}

} // namespace Core
