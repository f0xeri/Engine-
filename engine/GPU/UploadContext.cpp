#include "engine/GPU/UploadContext.hpp"

#include "engine/Core/Log.hpp"

#include <cstdlib>
#include <cstring>

namespace GPU
{

namespace
{
constexpr vk::DeviceSize InitialStagingBytes = 16ull << 20;
} // namespace

UploadContext::UploadContext(VulkanContext& ctx)
    : _ctx(ctx)
{
    _cmdPool = _ctx.device.createCommandPool({{}, _ctx.graphicsQueueFamily});
    _cmd =
        _ctx.device.allocateCommandBuffers({_cmdPool, vk::CommandBufferLevel::ePrimary, 1}).front();
    _fence = _ctx.device.createFence({});

    ensureStaging(InitialStagingBytes);
}

UploadContext::~UploadContext()
{
    destroyStaging();
    _ctx.device.destroyFence(_fence);
    _ctx.device.destroyCommandPool(_cmdPool);
}

void UploadContext::ensureStaging(vk::DeviceSize size)
{
    if (size <= _stagingSize)
    {
        return;
    }
    destroyStaging();

    const vk::BufferCreateInfo bufferInfo({}, size, vk::BufferUsageFlagBits::eTransferSrc);
    const VkBufferCreateInfo rawInfo = bufferInfo;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocationInfo out{};
    if (vmaCreateBuffer(_ctx.allocator, &rawInfo, &allocInfo, &buffer, &_stagingAlloc, &out) !=
        VK_SUCCESS)
    {
        LOG_ERROR(Vulkan, "staging buffer creation failed ({} bytes)", size);
        std::abort();
    }

    _staging = buffer;
    _mapped = out.pMappedData;
    _stagingSize = size;
}

void UploadContext::destroyStaging()
{
    if (_staging)
    {
        vmaDestroyBuffer(_ctx.allocator, _staging, _stagingAlloc);
        _staging = nullptr;
        _stagingAlloc = nullptr;
        _mapped = nullptr;
        _stagingSize = 0;
    }
}

void UploadContext::upload(vk::Buffer dst,
                           vk::DeviceSize dstOffset,
                           std::span<const std::byte> data)
{
    ensureStaging(data.size());

    std::memcpy(_mapped, data.data(), data.size());
    vmaFlushAllocation(_ctx.allocator, _stagingAlloc, 0, data.size()); // no-op if coherent

    _ctx.device.resetCommandPool(_cmdPool);
    _cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    _cmd.copyBuffer(_staging, dst, vk::BufferCopy(0, dstOffset, data.size()));

    // fence guarantees execution, not visibility to future submits - flush here
    const vk::MemoryBarrier2 barrier(vk::PipelineStageFlagBits2::eTransfer,
                                     vk::AccessFlagBits2::eTransferWrite,
                                     vk::PipelineStageFlagBits2::eAllCommands,
                                     vk::AccessFlagBits2::eMemoryRead);
    _cmd.pipelineBarrier2(vk::DependencyInfo({}, barrier, {}, {}));
    _cmd.end();

    _ctx.graphicsQueue.submit(vk::SubmitInfo({}, {}, _cmd), _fence);
    if (_ctx.device.waitForFences(_fence, vk::True, UINT64_MAX) != vk::Result::eSuccess)
    {
        LOG_ERROR(Vulkan, "upload fence wait failed");
        std::abort();
    }
    _ctx.device.resetFences(_fence);
}

} // namespace GPU
