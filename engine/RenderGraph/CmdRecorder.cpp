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

void CmdRecorder::draw(uint32_t vertexCount,
                       uint32_t instanceCount,
                       uint32_t firstVertex,
                       uint32_t firstInstance)
{
    _cmd.draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

} // namespace Graph
