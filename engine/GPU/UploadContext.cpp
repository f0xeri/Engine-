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

void UploadContext::beginCmd()
{
    _ctx.device.resetCommandPool(_cmdPool);
    _cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
}

void UploadContext::submitAndWait()
{
    _cmd.end();
    _ctx.graphicsQueue.submit(vk::SubmitInfo({}, {}, _cmd), _fence);
    if (_ctx.device.waitForFences(_fence, vk::True, UINT64_MAX) != vk::Result::eSuccess)
    {
        LOG_ERROR(Vulkan, "upload fence wait failed");
        std::abort();
    }
    _ctx.device.resetFences(_fence);
}

void UploadContext::uploadImage(vk::Image image,
                                uint32_t width,
                                uint32_t height,
                                uint32_t mipLevels,
                                std::span<const std::byte> rgba)
{
    using enum vk::ImageLayout;

    const auto mipRange = [&](uint32_t base, uint32_t count)
    {
        return vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, base, count, 0, 1);
    };

    const auto transition = [&](vk::ImageSubresourceRange range,
                                vk::PipelineStageFlags2 srcStage,
                                vk::AccessFlags2 srcAccess,
                                vk::ImageLayout oldLayout,
                                vk::PipelineStageFlags2 dstStage,
                                vk::AccessFlags2 dstAccess,
                                vk::ImageLayout newLayout)
    {
        const vk::ImageMemoryBarrier2 barrier(srcStage,
                                              srcAccess,
                                              dstStage,
                                              dstAccess,
                                              oldLayout,
                                              newLayout,
                                              vk::QueueFamilyIgnored,
                                              vk::QueueFamilyIgnored,
                                              image,
                                              range);
        _cmd.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, barrier));
    };

    ensureStaging(rgba.size());
    std::memcpy(_mapped, rgba.data(), rgba.size());
    vmaFlushAllocation(_ctx.allocator, _stagingAlloc, 0, rgba.size());

    beginCmd();

    const auto transfer = vk::PipelineStageFlagBits2::eTransfer;

    transition(mipRange(0, mipLevels),
               vk::PipelineStageFlagBits2::eNone,
               vk::AccessFlagBits2::eNone,
               eUndefined,
               transfer,
               vk::AccessFlagBits2::eTransferWrite,
               eTransferDstOptimal);

    const vk::BufferImageCopy copy(
        0, 0, 0, {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, {}, {width, height, 1});
    _cmd.copyBufferToImage(_staging, image, eTransferDstOptimal, copy);

    uint32_t srcW = width;
    uint32_t srcH = height;
    for (uint32_t mip = 1; mip < mipLevels; ++mip)
    {
        transition(mipRange(mip - 1, 1),
                   transfer,
                   vk::AccessFlagBits2::eTransferWrite,
                   eTransferDstOptimal,
                   transfer,
                   vk::AccessFlagBits2::eTransferRead,
                   eTransferSrcOptimal);

        const uint32_t dstW = std::max(srcW / 2, 1u);
        const uint32_t dstH = std::max(srcH / 2, 1u);
        const vk::ImageBlit2 region(
            {vk::ImageAspectFlagBits::eColor, mip - 1, 0, 1},
            {vk::Offset3D{}, {static_cast<int32_t>(srcW), static_cast<int32_t>(srcH), 1}},
            {vk::ImageAspectFlagBits::eColor, mip, 0, 1},
            {vk::Offset3D{}, {static_cast<int32_t>(dstW), static_cast<int32_t>(dstH), 1}});
        _cmd.blitImage2(
            {image, eTransferSrcOptimal, image, eTransferDstOptimal, region, vk::Filter::eLinear});

        srcW = dstW;
        srcH = dstH;
    }

    // after the loop: mips [0..n-2] are TRANSFER_SRC, the last one TRANSFER_DST
    if (mipLevels > 1)
    {
        transition(mipRange(0, mipLevels - 1),
                   transfer,
                   vk::AccessFlagBits2::eTransferRead,
                   eTransferSrcOptimal,
                   vk::PipelineStageFlagBits2::eFragmentShader,
                   vk::AccessFlagBits2::eShaderRead,
                   eShaderReadOnlyOptimal);
    }
    transition(mipRange(mipLevels - 1, 1),
               transfer,
               vk::AccessFlagBits2::eTransferWrite,
               eTransferDstOptimal,
               vk::PipelineStageFlagBits2::eFragmentShader,
               vk::AccessFlagBits2::eShaderRead,
               eShaderReadOnlyOptimal);

    submitAndWait();
}

void UploadContext::upload(vk::Buffer dst,
                           vk::DeviceSize dstOffset,
                           std::span<const std::byte> data)
{
    ensureStaging(data.size());

    std::memcpy(_mapped, data.data(), data.size());
    vmaFlushAllocation(_ctx.allocator, _stagingAlloc, 0, data.size()); // no-op if coherent

    beginCmd();
    _cmd.copyBuffer(_staging, dst, vk::BufferCopy(0, dstOffset, data.size()));

    // fence guarantees execution, not visibility to future submits - flush here
    const vk::MemoryBarrier2 barrier(vk::PipelineStageFlagBits2::eTransfer,
                                     vk::AccessFlagBits2::eTransferWrite,
                                     vk::PipelineStageFlagBits2::eAllCommands,
                                     vk::AccessFlagBits2::eMemoryRead);
    _cmd.pipelineBarrier2(vk::DependencyInfo({}, barrier, {}, {}));

    submitAndWait();
}

} // namespace GPU
