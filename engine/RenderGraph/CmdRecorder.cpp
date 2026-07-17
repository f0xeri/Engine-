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
}

void CmdRecorder::bindIndexBuffer(vk::Buffer buffer)
{
    _cmd.bindIndexBuffer(buffer, 0, vk::IndexType::eUint32);
}

void CmdRecorder::drawIndexed(uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount)
{
    _cmd.drawIndexed(indexCount, instanceCount, firstIndex, 0, 0);
}

void CmdRecorder::draw(uint32_t vertexCount,
                       uint32_t instanceCount,
                       uint32_t firstVertex,
                       uint32_t firstInstance)
{
    _cmd.draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

} // namespace Graph
