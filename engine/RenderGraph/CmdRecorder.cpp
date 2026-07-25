#include "engine/RenderGraph/CmdRecorder.hpp"

namespace Graph
{

CmdRecorder::CmdRecorder(vk::CommandBuffer cmd)
    : _cmd(cmd)
{
}

void CmdRecorder::bindPipeline(const GPU::Pipeline& pipeline)
{
    _cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.handle);
    _layout = pipeline.layout;
    _cmd.setCullMode(vk::CullModeFlagBits::eNone); // dynamic defaults; a pass overrides
    _cmd.setDepthBiasEnable(vk::False);
}

void CmdRecorder::bindIndexBuffer(vk::Buffer buffer)
{
    _cmd.bindIndexBuffer(buffer, 0, vk::IndexType::eUint32);
}

void CmdRecorder::setCullMode(CullMode mode)
{
    switch (mode)
    {
        case CullMode::Back:
            _cmd.setCullMode(vk::CullModeFlagBits::eBack);
            return;
        case CullMode::Front:
            _cmd.setCullMode(vk::CullModeFlagBits::eFront);
            return;
        case CullMode::None:
            _cmd.setCullMode(vk::CullModeFlagBits::eNone);
            return;
    }
}

void CmdRecorder::setDepthBias(float constant, float slope)
{
    _cmd.setDepthBiasEnable(vk::True);
    _cmd.setDepthBias(constant, 0.0f, slope);
}

void CmdRecorder::drawIndexed(uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount)
{
    _cmd.drawIndexed(indexCount, instanceCount, firstIndex, 0, 0);
}

void CmdRecorder::drawIndexedIndirect(vk::Buffer buffer,
                                      vk::DeviceSize offset,
                                      uint32_t drawCount,
                                      uint32_t stride)
{
    _cmd.drawIndexedIndirect(buffer, offset, drawCount, stride);
}

void CmdRecorder::draw(uint32_t vertexCount,
                       uint32_t instanceCount,
                       uint32_t firstVertex,
                       uint32_t firstInstance)
{
    _cmd.draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

} // namespace Graph
