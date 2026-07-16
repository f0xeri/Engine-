#pragma once

#include <vulkan/vulkan.hpp>

namespace GPU
{

struct Pipeline
{
    vk::Pipeline handle;
    vk::PipelineLayout layout;
};

} // namespace GPU
