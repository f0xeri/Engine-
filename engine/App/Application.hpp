#pragma once

#include "Config.hpp"

#include "engine/GPU/Pipeline.hpp"
#include "engine/Platform/Window.hpp"
#include "engine/GPU/VulkanContext.hpp"
#include "engine/GPU/Swapchain.hpp"
#include "engine/GPU/PipelineFactory.hpp"
#include "engine/Asset/GeometryPool.hpp"
#include "engine/Asset/Library.hpp"
#include "engine/GPU/BindlessRegistry.hpp"
#include "engine/GPU/FrameContext.hpp"
#include "engine/GPU/UploadContext.hpp"
#include "engine/RenderGraph/RenderGraph.hpp"
#include "engine/Renderer/PipelineRegistry.hpp"
#include "engine/App/ImGuiLayer.hpp"
#include "engine/App/DebugOverlay.hpp"

#include <functional>
#include <span>
#include <string>

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

    void setRelativeMouseMode(bool enabled);

    // adds one extra tab to the shared "Debug" ImGui window
    void setDebugTab(std::string name, std::function<void()> draw);

    Renderer::PipelineHandle loadPipeline(std::string module);
    Renderer::PipelineHandle loadPipeline(std::string module,
                                          std::span<const Graph::Format> colorFormats);
    GPU::Pipeline pipeline(Renderer::PipelineHandle handle) const;

    Asset::GeometryPool& geometry() { return _geometry; }
    Asset::Library& assets() { return _assets; }

private:
    void resize();

    Platform::Window _window;
    GPU::VulkanContext _gpuContext;
    GPU::Swapchain _swapchain;
    GPU::FrameContext _frames;
    GPU::BindlessRegistry _bindless;
    GPU::PipelineFactory _pipelines;
    GPU::UploadContext _uploader;
    Asset::GeometryPool _geometry;
    Asset::Library _assets;
    Graph::RenderGraph _graph; // destroyed after _registry's waitIdle
    App::ImGuiLayer _imgui;    // destroyed after _registry's waitIdle
    Renderer::PipelineRegistry _registry;

    App::DebugOverlay _debugOverlay;

    bool _quit = false;
    std::string _debugTabName;
    std::function<void()> _debugTabDraw;
};
} // namespace App
