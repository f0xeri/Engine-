#include "Application.hpp"
#include "engine/App/DebugOverlay.hpp"
#include "engine/GPU/Pipeline.hpp"
#include "engine/RenderGraph/RenderGraph.hpp"

#include <chrono>

namespace App
{
Application::Application(const Config& config)
    : _window({config.title})
    , _gpuContext(_window)
    , _swapchain(_gpuContext, _window.framebufferSize())
    , _frames(_gpuContext)
    , _bindless(_gpuContext)
    , _uniforms(_gpuContext, _bindless)
    , _pipelines(_gpuContext, _bindless.setLayout(), config.pipelineCache)
    , _uploader(_gpuContext)
    , _geometry(_gpuContext, _bindless, _uploader)
    , _assets(_gpuContext, _bindless, _uploader, _geometry)
    , _graph(_gpuContext, _bindless)
    , _imgui(_gpuContext, _window, _swapchain.format(), _swapchain.imageCount())
    , _registry(_gpuContext, _pipelines, config.shaderRoot)
{
}

void Application::run(const std::function<void(const FrameInfo&)>& tick)
{
    bool resizeRequested = false;
    bool vsyncEnabled = _swapchain.vsyncEnabled();
    auto last = std::chrono::steady_clock::now();

    while (!_quit && _window.pumpEvents([this](const SDL_Event& e) { _imgui.processEvent(e); }))
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
        _uniforms.beginFrame(frame.index); // safe to rewind: begin() waited this slot's fence

        if (resizeRequested)
        {
            resize();
            resizeRequested = false;
        }

        if (vsyncEnabled != _swapchain.vsyncEnabled())
        {
            _gpuContext.device.waitIdle();
            _swapchain.setVsync(vsyncEnabled);
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

        _imgui.newFrame();
        _debugOverlay.draw(
            _bindless, _geometry, _graph, vsyncEnabled, _debugTabName, _debugTabDraw);
        tick({_graph, _uniforms, backbuffer, {extent.width, extent.height}, _window.input(), dt});

        _graph.addPass("imgui-overlay",
                       {.color = {{backbuffer, Graph::LoadOp::Load}}},
                       [this](Graph::CmdRecorder& rec) { _imgui.render(rec.raw()); });

        _graph.execute(frame.cmd, frame.index);

        _frames.submit(_swapchain.renderFinished(*imageIndex));
        resizeRequested = !_swapchain.present(*imageIndex);
    }
}

void Application::setRelativeMouseMode(bool enabled)
{
    _window.setRelativeMouseMode(enabled);
}

void Application::setDebugTab(std::string name, std::function<void()> draw)
{
    _debugTabName = std::move(name);
    _debugTabDraw = std::move(draw);
}

Renderer::PipelineHandle Application::loadPipeline(std::string module)
{
    std::array colorFormats{_swapchain.format()};
    return _registry.load(std::move(module), colorFormats, vk::Format::eD32Sfloat);
}

Renderer::PipelineHandle Application::loadPipeline(std::string module,
                                                   std::span<const Graph::Format> colorFormats)
{
    std::vector<vk::Format> vkFormats(colorFormats.size());
    std::ranges::transform(
        colorFormats, vkFormats.begin(), [](Graph::Format f) { return Graph::toVk(f); });
    return _registry.load(std::move(module), vkFormats, vk::Format::eD32Sfloat);
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
