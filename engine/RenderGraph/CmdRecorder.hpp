#pragma once

#include "engine/GPU/Pipeline.hpp"

#include <cassert>

#include <vulkan/vulkan.hpp>

namespace Graph
{

// minimal CommandBuffer interface
class CmdRecorder
{
public:
    explicit CmdRecorder(vk::CommandBuffer cmd);

    void bindPipeline(const GPU::Pipeline& pipeline);
    void draw(uint32_t vertexCount,
              uint32_t instanceCount = 1,
              uint32_t firstVertex = 0,
              uint32_t firstInstance = 0);

    template <typename T>
    void pushConstants(const T& data)
    {
        assert(_layout && "pushConstants requires a bound pipeline");
        _cmd.pushConstants(_layout, vk::ShaderStageFlagBits::eAll, 0, sizeof(T), &data);
    }

    vk::CommandBuffer raw() const { return _cmd; }

private:
    vk::CommandBuffer _cmd;
    vk::PipelineLayout _layout; // of the currently bound pipeline
};

} // namespace Graph
