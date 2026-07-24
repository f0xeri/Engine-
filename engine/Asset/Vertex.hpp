#pragma once

#include "engine/Core/Math.hpp"

namespace Asset
{

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 tangent; // xyz tangent, w = bitangent handedness (glTF convention)
};

static_assert(sizeof(Vertex) == 48);

} // namespace Asset
