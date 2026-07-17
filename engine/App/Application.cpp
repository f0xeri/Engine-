#include "Application.hpp"
#include "engine/GPU/Pipeline.hpp"

#include <chrono>

namespace App
{
Application::Application(const Config& config)
    : _window({config.title})
    , _gpuContext(_window)
    , _swapchain(_gpuContext, _window.framebufferSize())
    , _frames(_gpuContext)
    , _bindless(_gpuContext)
    , _pipelines(_gpuContext, _bindless.setLayout(), config.pipelineCache)
    , _graph(_gpuContext)
    , _registry(_gpuContext, _pipelines, config.shaderRoot)
{
}

void Application::run(const std::function<void(const FrameInfo&)>& tick)
{
    bool resizeRequested = false;
    auto last = std::chrono::steady_clock::now();

    while (!_quit && _window.pumpEvents())
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

        // the only descriptor bind in frame
        frame.cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics, _pipelines.layout(), 0, _bindless.set(), {});

        const vk::Extent2D extent = _swapchain.extent();
        const Graph::ResourceHandle backbuffer = _graph.importBackbuffer(
            _swapchain.image(*imageIndex), _swapchain.imageView(*imageIndex), extent);

        tick({_graph, backbuffer, {extent.width, extent.height}, _window.input(), dt});
        _graph.execute(frame.cmd);

        _frames.submit(_swapchain.renderFinished(*imageIndex));
        resizeRequested = !_swapchain.present(*imageIndex);
    }
}

void Application::setRelativeMouseMode(bool enabled)
{
    _window.setRelativeMouseMode(enabled);
}

Renderer::PipelineHandle Application::loadPipeline(std::string module)
{
    return _registry.load(std::move(module), _swapchain.format(), vk::Format::eD32Sfloat);
}

GPU::Pipeline Application::pipeline(Renderer::PipelineHandle handle) const
{
    return _registry.get(handle);
}

void Application::resize()
{
    _gpuContext.device.waitIdle();
    _graph.trim();
    _swapchain.recreate(_window.framebufferSize());
}
} // namespace App
