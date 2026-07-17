#include "engine/GPU/Swapchain.hpp"

#include "engine/Core/Log.hpp"

#include <algorithm>

namespace GPU
{

namespace
{

constexpr vk::Format PreferredFormat = vk::Format::eB8G8R8A8Srgb;
constexpr vk::ColorSpaceKHR PreferredColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
constexpr vk::PresentModeKHR PresentMode = vk::PresentModeKHR::eFifo;

vk::SurfaceFormatKHR pickFormat(const std::vector<vk::SurfaceFormatKHR>& available)
{
    for (const auto& f : available)
    {
        if (f.format == PreferredFormat && f.colorSpace == PreferredColorSpace)
        {
            return f;
        }
    }

    LOG_WARN(Vulkan, "B8G8R8A8_SRGB unavailable, falling back to the first surface format");
    return available.front();
}

vk::Extent2D pickExtent(const vk::SurfaceCapabilitiesKHR& caps, Platform::Extent2D requested)
{
    // A driver-dictated currentExtent is binding; UINT32_MAX means we choose.
    if (caps.currentExtent.width != UINT32_MAX)
    {
        return caps.currentExtent;
    }

    return {std::clamp(requested.width, caps.minImageExtent.width, caps.maxImageExtent.width),
            std::clamp(requested.height, caps.minImageExtent.height, caps.maxImageExtent.height)};
}

uint32_t pickImageCount(const vk::SurfaceCapabilitiesKHR& caps)
{
    uint32_t count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0)
    {
        count = std::min(count, caps.maxImageCount);
    }
    return count;
}

} // namespace

Swapchain::Swapchain(VulkanContext& ctx, Platform::Extent2D extent)
    : _ctx(ctx)
{
    create(extent, nullptr);
}

Swapchain::~Swapchain()
{
    destroyImageResources();

    if (_swapchain)
    {
        _ctx.device.destroySwapchainKHR(_swapchain);
    }
}

void Swapchain::recreate(Platform::Extent2D extent)
{
    const vk::SwapchainKHR old = _swapchain;

    destroyImageResources();
    create(extent, old);

    if (old)
    {
        _ctx.device.destroySwapchainKHR(old);
    }
}

void Swapchain::create(Platform::Extent2D extent, vk::SwapchainKHR oldSwapchain)
{
    const auto caps = _ctx.physicalDevice.getSurfaceCapabilitiesKHR(_ctx.surface);
    const auto surfaceFormat = pickFormat(_ctx.physicalDevice.getSurfaceFormatsKHR(_ctx.surface));

    _format = surfaceFormat.format;
    _extent = pickExtent(caps, extent);

    if (_extent.width == 0 || _extent.height == 0)
    {
        LOG_ERROR(Vulkan, "swapchain requested with zero extent (minimized window?)");
        std::abort();
    }

    const vk::SwapchainCreateInfoKHR info({},
                                          _ctx.surface,
                                          pickImageCount(caps),
                                          _format,
                                          surfaceFormat.colorSpace,
                                          _extent,
                                          1,
                                          vk::ImageUsageFlagBits::eColorAttachment,
                                          vk::SharingMode::eExclusive,
                                          {},
                                          caps.currentTransform,
                                          vk::CompositeAlphaFlagBitsKHR::eOpaque,
                                          PresentMode,
                                          vk::True,
                                          oldSwapchain);

    _swapchain = _ctx.device.createSwapchainKHR(info);

    // driver may hand back more images than requested
    _images = _ctx.device.getSwapchainImagesKHR(_swapchain);

    _views.reserve(_images.size());
    _renderFinished.reserve(_images.size());

    for (const vk::Image image : _images)
    {
        const vk::ImageViewCreateInfo viewInfo(
            {},
            image,
            vk::ImageViewType::e2D,
            _format,
            {},
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

        _views.push_back(_ctx.device.createImageView(viewInfo));
        _renderFinished.push_back(_ctx.device.createSemaphore({}));
    }

    LOG_INFO(Vulkan, "swapchain: {}x{}, {} images", _extent.width, _extent.height, _images.size());
}

void Swapchain::destroyImageResources()
{
    for (const vk::ImageView view : _views)
    {
        _ctx.device.destroyImageView(view);
    }
    for (const vk::Semaphore semaphore : _renderFinished)
    {
        _ctx.device.destroySemaphore(semaphore);
    }

    _views.clear();
    _renderFinished.clear();
    _images.clear();
}

std::optional<uint32_t> Swapchain::acquire(vk::Semaphore acquireSemaphore)
{
    try
    {
        const auto [result, imageIndex] =
            _ctx.device.acquireNextImageKHR(_swapchain, UINT64_MAX, acquireSemaphore);
        return imageIndex;
    }
    catch (const vk::OutOfDateKHRError&)
    {
        return std::nullopt;
    }
}

bool Swapchain::present(uint32_t imageIndex)
{
    try
    {
        const vk::PresentInfoKHR info(_renderFinished[imageIndex], _swapchain, imageIndex);
        return _ctx.graphicsQueue.presentKHR(info) != vk::Result::eSuboptimalKHR;
    }
    catch (const vk::OutOfDateKHRError&)
    {
        return false;
    }
}

} // namespace GPU
