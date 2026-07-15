#pragma once

#include "engine/GPU/VulkanContext.hpp"

#include <array>
#include <cstdint>

namespace GPU
{

constexpr uint32_t FramesInFlight = 2;

// view for internal Slot struct
struct Frame
{
    vk::CommandBuffer cmd;
    vk::Semaphore acquireSemaphore;
    uint64_t index;
};

class FrameContext
{
public:
    explicit FrameContext(VulkanContext& ctx);
    ~FrameContext();

    FrameContext(const FrameContext&) = delete;
    FrameContext& operator=(const FrameContext&) = delete;

    // Waits this slot's fence (the CPU throttle), resets its command pool and opens the command
    // buffer for recording.
    Frame begin();

    // Ends recording, resets the fence and submits: wait acquire semaphore at
    // COLOR_ATTACHMENT_OUTPUT, signal renderFinished + the slot fence.
    void submit(vk::Semaphore renderFinished);

    // Drops the frame without submitting. Only legal when nothing was acquired: the fence is still
    // signaled and the acquire semaphore was never signaled, so both are safe to reuse. An acquired
    // mage must always go through submit().
    void abandon();

    uint64_t frameIndex() const { return _frameIndex; }

private:
    struct Slot
    {
        vk::CommandPool pool;
        vk::CommandBuffer cmd;
        vk::Fence fence;
        vk::Semaphore acquireSemaphore;
    };
    Slot& currentSlot() { return _slots[_frameIndex % FramesInFlight]; }

    VulkanContext& _ctx;

    std::array<Slot, FramesInFlight> _slots;
    uint64_t _frameIndex = 0;
};

} // namespace GPU
