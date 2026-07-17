#pragma once

#include "engine/GPU/VulkanContext.hpp"

#include <cstddef>
#include <span>

namespace GPU
{

// Blocking upload: returns once the copy in done on GPU
class UploadContext
{
public:
    explicit UploadContext(VulkanContext& ctx);
    ~UploadContext();

    UploadContext(const UploadContext&) = delete;
    UploadContext& operator=(const UploadContext&) = delete;

    void upload(vk::Buffer dst, vk::DeviceSize dstOffset, std::span<const std::byte> data);

    void uploadImage(vk::Image image,
                     uint32_t width,
                     uint32_t height,
                     uint32_t mipLevels,
                     std::span<const std::byte> rgba);

private:
    void ensureStaging(vk::DeviceSize size);
    void destroyStaging();
    void beginCmd();
    void submitAndWait();

    VulkanContext& _ctx;

    vk::CommandPool _cmdPool;
    vk::CommandBuffer _cmd;
    vk::Fence _fence;

    vk::Buffer _staging;
    VmaAllocation _stagingAlloc = nullptr;
    void* _mapped = nullptr;
    vk::DeviceSize _stagingSize = 0;
};

} // namespace GPU
