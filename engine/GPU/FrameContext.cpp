#include "engine/GPU/FrameContext.hpp"

#include "engine/Core/Log.hpp"

#include <cstdlib>

namespace GPU
{

FrameContext::FrameContext(VulkanContext& ctx) : _ctx(ctx)
{
    const vk::CommandPoolCreateInfo poolInfo(vk::CommandPoolCreateFlagBits::eTransient,
                                             _ctx.graphicsQueueFamily);

    for (Slot& slot : _slots)
    {
        slot.pool = _ctx.device.createCommandPool(poolInfo);

        const vk::CommandBufferAllocateInfo allocInfo(
            slot.pool, vk::CommandBufferLevel::ePrimary, 1);
        slot.cmd = _ctx.device.allocateCommandBuffers(allocInfo).front();

        slot.fence = _ctx.device.createFence({vk::FenceCreateFlagBits::eSignaled});
        slot.acquireSemaphore = _ctx.device.createSemaphore({});
    }
}

FrameContext::~FrameContext()
{
    _ctx.device.waitIdle();

    for (const Slot& slot : _slots)
    {
        _ctx.device.destroySemaphore(slot.acquireSemaphore);
        _ctx.device.destroyFence(slot.fence);
        _ctx.device.destroyCommandPool(slot.pool);
    }
}

Frame FrameContext::begin()
{
    Slot& slot = currentSlot();

    if (_ctx.device.waitForFences(slot.fence, vk::True, UINT64_MAX) != vk::Result::eSuccess)
    {
        LOG_ERROR(Vulkan, "frame fence wait timed out");
        std::abort();
    }

    _ctx.device.resetCommandPool(slot.pool);
    slot.cmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    return {.cmd = slot.cmd, .acquireSemaphore = slot.acquireSemaphore, .index = _frameIndex};
}

void FrameContext::submit(vk::Semaphore renderFinished)
{
    Slot& slot = currentSlot();

    slot.cmd.end();

    const vk::SemaphoreSubmitInfo wait(
        slot.acquireSemaphore, 0, vk::PipelineStageFlagBits2::eColorAttachmentOutput);
    const vk::SemaphoreSubmitInfo signal(
        renderFinished, 0, vk::PipelineStageFlagBits2::eAllCommands);
    const vk::CommandBufferSubmitInfo cmd(slot.cmd);
    const vk::SubmitInfo2 info({}, wait, cmd, signal);

    _ctx.device.resetFences(slot.fence);
    _ctx.graphicsQueue.submit2(info, slot.fence);

    ++_frameIndex;
}

void FrameContext::abandon()
{
    currentSlot().cmd.end();
    ++_frameIndex;
}

} // namespace GPU
