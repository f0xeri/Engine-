#pragma once

#include "engine/GPU/PipelineFactory.hpp"
#include "engine/GPU/VulkanContext.hpp"
#include "engine/Shader/ShaderSystem.hpp"

#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace Renderer
{

struct PipelineHandle
{
    uint32_t value;
};

// The one place that combines SPIR-V with the GPU layer. Owns every pipeline;
// hot reload swaps table entries, so handles stay valid forever.
class PipelineRegistry
{
public:
    PipelineRegistry(GPU::VulkanContext& ctx,
                     GPU::PipelineFactory& factory,
                     std::filesystem::path shaderRoot);
    ~PipelineRegistry();

    PipelineRegistry(const PipelineRegistry&) = delete;
    PipelineRegistry& operator=(const PipelineRegistry&) = delete;

    // blocks until the pipeline is built (startup only)
    PipelineHandle load(std::string module, vk::Format colorFormat);

    vk::Pipeline get(PipelineHandle handle) const { return _table[handle.value]; }

    // swaps rebuilt pipelines in and destroys retired ones proven complete by
    // begin(frameIndex)'s fence wait; call once per frame after begin()
    void update(uint64_t frameIndex);

private:
    GPU::VulkanContext& _ctx;
    GPU::PipelineFactory& _factory;

    std::vector<vk::Pipeline> _table; // main thread only

    std::mutex _mutex;
    std::vector<std::pair<uint32_t, vk::Pipeline>> _rebuilt; // worker -> main

    std::deque<std::pair<uint64_t, vk::Pipeline>> _retired; // main thread only

    std::unique_ptr<Shader::ShaderSystem> _shaders; // joined first in the dtor
};

} // namespace Renderer
