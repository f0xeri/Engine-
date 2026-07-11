#pragma once

#include <vulkan/vulkan.hpp>

namespace gpu {

// Loads vulkan-1.dll and initializes the Vulkan-Hpp global dispatcher with
// instance-independent entry points. Must be the first gpu call; returns the
// instance API version supported by the loader.
uint32_t initVulkanLoader();

} // namespace gpu
