#pragma once

#include "Config.hpp"

#include "engine/Platform/Window.hpp"
#include "engine/GPU/VulkanContext.hpp"
#include "engine/GPU/Swapchain.hpp"
#include "engine/GPU/PipelineFactory.hpp"
#include "engine/GPU/FrameContext.hpp"
#include "engine/Renderer/PipelineRegistry.hpp"

#include <functional>

namespace App
{
// tick runs inside an active rendering scope; viewport/scissor already set
struct FrameInfo
{
    vk::CommandBuffer cmd;
    vk::Extent2D extent;
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
    vk::Pipeline pipeline(Renderer::PipelineHandle handle) const;

    GPU::PipelineFactory& pipelines() { return _pipelines; }

private:
    void resize();
    void recordFrame(uint32_t imageIndex,
                     const std::function<void(const FrameInfo&)>& tick,
                     const FrameInfo& frame);

    Platform::Window _window;
    GPU::VulkanContext _gpuContext;
    GPU::Swapchain _swapchain;
    GPU::FrameContext _frames;
    GPU::PipelineFactory _pipelines;
    Renderer::PipelineRegistry _registry;

    bool _quit = false;
};
} // namespace App
