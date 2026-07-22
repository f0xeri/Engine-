#include "engine/GPU/FrameUniforms.hpp"

#include "engine/Core/Log.hpp"

#include <cstdlib>

namespace GPU
{

FrameUniforms::FrameUniforms(VulkanContext& ctx, BindlessRegistry& bindless) : _ctx(ctx)
{
    const vk::BufferCreateInfo bufferInfo({}, BufferBytes, vk::BufferUsageFlagBits::eStorageBuffer);
    const VkBufferCreateInfo rawInfo = bufferInfo;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                      VMA_ALLOCATION_CREATE_MAPPED_BIT;

    for (Slot& slot : _slots)
    {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocationInfo mapping{};
        if (vmaCreateBuffer(
                _ctx.allocator, &rawInfo, &allocInfo, &buffer, &slot.allocation, &mapping) !=
            VK_SUCCESS)
        {
            LOG_ERROR(Vulkan, "frame uniform buffer creation failed ({} bytes)", BufferBytes);
            std::abort();
        }

        slot.buffer = buffer;
        slot.mapped = static_cast<std::byte*>(mapping.pMappedData);
        slot.bindlessSlot = bindless.registerBuffer(buffer);
    }

    LOG_INFO(Vulkan,
             "frame uniforms: {} x {} bytes, bindless slots {}..{}",
             FramesInFlight,
             BufferBytes,
             _slots.front().bindlessSlot,
             _slots.back().bindlessSlot);
}

FrameUniforms::~FrameUniforms()
{
    for (const Slot& slot : _slots)
    {
        vmaDestroyBuffer(_ctx.allocator, slot.buffer, slot.allocation);
    }
}

void FrameUniforms::beginFrame(uint64_t frameIndex)
{
    _current = static_cast<uint32_t>(frameIndex % FramesInFlight);
    _cursor = 0;
}

FrameUniforms::Ref FrameUniforms::allocate(uint32_t bytes)
{
    const uint32_t offset = _cursor;
    _cursor += (bytes + Alignment - 1) & ~(Alignment - 1);

    if (_cursor > BufferBytes)
    {
        LOG_ERROR(Vulkan, "frame uniform buffer overflow ({} bytes requested)", _cursor);
        std::abort();
    }

    return {_slots[_current].bindlessSlot, offset};
}

} // namespace GPU
