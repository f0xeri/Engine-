#pragma once

#include "Config.hpp"

#include "engine/GPU/Pipeline.hpp"
#include "engine/Platform/Window.hpp"
#include "engine/GPU/VulkanContext.hpp"
#include "engine/GPU/Swapchain.hpp"
#include "engine/GPU/PipelineFactory.hpp"
#include "engine/GPU/FrameContext.hpp"
#include "engine/RenderGraph/RenderGraph.hpp"
#include "engine/Renderer/PipelineRegistry.hpp"

#include <functional>

namespace App
{
struct FrameInfo
{
    Graph::RenderGraph& graph;
    Graph::ResourceHandle backbuffer;
    Platform::Extent2D extent;
    const Platform::Input& input;
    float dt;
};

class Application
{
public:
    explicit Application(const Config& config);

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run(const std::function<void(const FrameInfo&)>& tick);

    // exits run() after the current frame
    void quit() { _quit = true; }

    Renderer::PipelineHandle loadPipeline(std::string module);
    GPU::Pipeline pipeline(Renderer::PipelineHandle handle) const;

private:
    void resize();

    Platform::Window _window;
    GPU::VulkanContext _gpuContext;
    GPU::Swapchain _swapchain;
    GPU::FrameContext _frames;
    GPU::PipelineFactory _pipelines;
    Graph::RenderGraph _graph; // destroyed after _registry's waitIdle
    Renderer::PipelineRegistry _registry;

    bool _quit = false;
};
} // namespace App
