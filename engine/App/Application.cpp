#include "Application.hpp"

#include <chrono>

namespace App
{
Application::Application(const Config& config)
    : _window({config.title})
    , _gpuContext(_window)
    , _swapchain(_gpuContext, _window.framebufferSize())
    , _frames(_gpuContext)
    , _pipelines(_gpuContext, config.pipelineCache)
    , _registry(_gpuContext, _pipelines, config.shaderRoot)
{
}

void Application::run(const std::function<void(const FrameInfo&)>& tick)
{
    bool resizeRequested = false;
    auto last = std::chrono::steady_clock::now();

    while (_window.pumpEvents())
    {
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        if (_window.minimized())
        {
            continue;
        }

        const GPU::Frame frame = _frames.begin();

        _registry.update(frame.index);

        if (resizeRequested)
        {
            resize();
            resizeRequested = false;
        }

        auto imageIndex = _swapchain.acquire(frame.acquireSemaphore);
        if (!imageIndex)
        {
            resize();
            imageIndex = _swapchain.acquire(frame.acquireSemaphore);
        }

        if (!imageIndex)
        {
            _frames.abandon();
            continue;
        }

        recordFrame(*imageIndex, tick, {frame.cmd, _swapchain.extent(), dt});

        _frames.submit(_swapchain.renderFinished(*imageIndex));
        resizeRequested = !_swapchain.present(*imageIndex);
    }
}

Renderer::PipelineHandle Application::loadPipeline(std::string module)
{
    return _registry.load(std::move(module), _swapchain.format());
}

vk::Pipeline Application::pipeline(Renderer::PipelineHandle handle) const
{
    return _registry.get(handle);
}

void Application::resize()
{
    _gpuContext.device.waitIdle();
    _swapchain.recreate(_window.framebufferSize());
}

void Application::recordFrame(uint32_t imageIndex,
                              const std::function<void(const FrameInfo&)>& tick,
                              const FrameInfo& frame)
{
    const vk::ImageSubresourceRange range(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

    // srcStage must intersect the acquire semaphore's wait stage, or the transition is not
    // ordered after the acquire.
    const vk::ImageMemoryBarrier2 toAttachment(vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                               vk::AccessFlagBits2::eNone,
                                               vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                               vk::AccessFlagBits2::eColorAttachmentWrite,
                                               vk::ImageLayout::eUndefined,
                                               vk::ImageLayout::eColorAttachmentOptimal,
                                               vk::QueueFamilyIgnored,
                                               vk::QueueFamilyIgnored,
                                               _swapchain.image(imageIndex),
                                               range);

    frame.cmd.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, toAttachment));

    const vk::ClearValue clear(vk::ClearColorValue(0.05f, 0.05f, 0.08f, 1.0f));
    const vk::RenderingAttachmentInfo color(_swapchain.imageView(imageIndex),
                                            vk::ImageLayout::eColorAttachmentOptimal,
                                            {},
                                            {},
                                            {},
                                            vk::AttachmentLoadOp::eClear,
                                            vk::AttachmentStoreOp::eStore,
                                            clear);

    frame.cmd.beginRendering(vk::RenderingInfo({}, {{}, frame.extent}, 1, 0, color));

    frame.cmd.setViewport(0,
                          vk::Viewport(0.0f,
                                       0.0f,
                                       static_cast<float>(frame.extent.width),
                                       static_cast<float>(frame.extent.height),
                                       0.0f,
                                       1.0f));
    frame.cmd.setScissor(0, vk::Rect2D({}, frame.extent));

    tick(frame);

    frame.cmd.endRendering();

    // dstStage NONE: the render-finished semaphore carries the dependency to the
    // presentation engine, so there is nothing in this queue left to order against.
    const vk::ImageMemoryBarrier2 toPresent(vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                            vk::AccessFlagBits2::eColorAttachmentWrite,
                                            vk::PipelineStageFlagBits2::eNone,
                                            vk::AccessFlagBits2::eNone,
                                            vk::ImageLayout::eColorAttachmentOptimal,
                                            vk::ImageLayout::ePresentSrcKHR,
                                            vk::QueueFamilyIgnored,
                                            vk::QueueFamilyIgnored,
                                            _swapchain.image(imageIndex),
                                            range);

    frame.cmd.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, toPresent));
}
} // namespace App
