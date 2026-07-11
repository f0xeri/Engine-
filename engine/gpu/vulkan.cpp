#include "engine/gpu/vulkan.hpp"

#include "engine/core/log.hpp"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace gpu {

uint32_t initVulkanLoader() {
    VULKAN_HPP_DEFAULT_DISPATCHER.init();
    return vk::enumerateInstanceVersion();
}

} // namespace gpu
