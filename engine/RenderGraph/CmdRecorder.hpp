#pragma once

#include "engine/GPU/Pipeline.hpp"

#include <cassert>

#include <vulkan/vulkan.hpp>

namespace Graph
{

enum class CullMode : uint8_t
{
    None,
    Back,
    Front
};

// minimal CommandBuffer interface
class CmdRecorder
{
public:
    explicit CmdRecorder(vk::CommandBuffer cmd);

    void bindPipeline(const GPU::Pipeline& pipeline);
    void bindIndexBuffer(vk::Buffer buffer);
    // dynamic state; bindPipeline resets it to None, passes override per draw batch
    void setCullMode(CullMode mode);
    // dynamic slope-scaled depth bias; bindPipeline disables it, the shadow pass enables + sets it
    void setDepthBias(float constant, float slope);
    void draw(uint32_t vertexCount,
              uint32_t instanceCount = 1,
              uint32_t firstVertex = 0,
              uint32_t firstInstance = 0);
    void drawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, uint32_t instanceCount = 1);
    void drawIndexedIndirect(vk::Buffer buffer,
                             vk::DeviceSize offset,
                             uint32_t drawCount,
                             uint32_t stride);

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
