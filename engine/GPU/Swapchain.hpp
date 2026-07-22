#pragma once

#include "engine/GPU/VulkanContext.hpp"
#include "engine/Platform/Window.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace GPU
{

class Swapchain
{
public:
    Swapchain(VulkanContext& ctx, Platform::Extent2D extent);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    // Caller must have waited for the device to go idle.
    void recreate(Platform::Extent2D extent);

    // Caller must have waited for the device to go idle,
    void setVsync(bool enabled);
    bool vsyncEnabled() const { return _presentMode == vk::PresentModeKHR::eFifo; }

    // nullopt = OUT_OF_DATE: no image acquired, semaphore untouched - recreate and
    // retry with the same semaphore. SUBOPTIMAL is a success: the frame must complete.
    std::optional<uint32_t> acquire(vk::Semaphore acquireSemaphore);

    // False when out of date or suboptimal - flag a resize for the next frame.
    bool present(uint32_t imageIndex);

    vk::Image image(uint32_t i) const { return _images[i]; }
    vk::ImageView imageView(uint32_t i) const { return _views[i]; }
    vk::Semaphore renderFinished(uint32_t i) const { return _renderFinished[i]; }
    vk::Format format() const { return _format; }
    vk::Extent2D extent() const { return _extent; }
    uint32_t imageCount() const { return static_cast<uint32_t>(_images.size()); }

private:
    void create(Platform::Extent2D extent, vk::SwapchainKHR oldSwapchain);
    void destroyImageResources();

    VulkanContext& _ctx;

    vk::SwapchainKHR _swapchain;
    vk::Format _format = vk::Format::eUndefined;
    vk::Extent2D _extent;
    vk::PresentModeKHR _presentMode = vk::PresentModeKHR::eFifo;

    std::vector<vk::Image> _images;
    std::vector<vk::ImageView> _views;
    std::vector<vk::Semaphore> _renderFinished;
};

} // namespace GPU
