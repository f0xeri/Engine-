#pragma once

#include "engine/Core/Math.hpp"

namespace Asset
{

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

static_assert(sizeof(Vertex) == 32);

} // namespace Asset
