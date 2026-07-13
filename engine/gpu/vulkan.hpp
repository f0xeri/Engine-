#pragma once

#include <vulkan/vulkan.hpp>

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

namespace Platform
{
class Window;
} // namespace Platform

namespace GPU
{
class VulkanContext
{
public:
    explicit VulkanContext(const Platform::Window& window);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

private:
    void createInstance(const Platform::Window& window, bool validationEnabled);
    void createDebugMessenger();
    void createSurface(const Platform::Window& window);
    void pickPhysicalDevice();
    void createDevice();
    void createAllocator();

public:
    vk::Instance instance;
    vk::DebugUtilsMessengerEXT debugMessenger;
    vk::SurfaceKHR surface;
    vk::PhysicalDevice physicalDevice;
    vk::PhysicalDeviceProperties deviceProps;
    vk::Device device;

    vk::Queue graphicsQueue;
    uint32_t graphicsQueueFamily = VK_QUEUE_FAMILY_IGNORED;

    VmaAllocator allocator = nullptr;
};

} // namespace GPU
