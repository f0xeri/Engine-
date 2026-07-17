#include "engine/Asset/GeometryPool.hpp"

#include "engine/Core/Log.hpp"

#include <cstdlib>

namespace Asset
{

namespace
{

constexpr uint32_t MaxVertices = (64u << 20) / sizeof(Vertex);  // 64 MiB
constexpr uint32_t MaxIndices = (32u << 20) / sizeof(uint32_t); // 32 MiB

vk::Buffer createDeviceLocal(GPU::VulkanContext& ctx,
                             vk::DeviceSize size,
                             vk::BufferUsageFlags usage,
                             VmaAllocation& outAlloc)
{
    const vk::BufferCreateInfo bufferInfo({}, size, usage | vk::BufferUsageFlagBits::eTransferDst);
    const VkBufferCreateInfo rawInfo = bufferInfo;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VkBuffer buffer = VK_NULL_HANDLE;
    if (vmaCreateBuffer(ctx.allocator, &rawInfo, &allocInfo, &buffer, &outAlloc, nullptr) !=
        VK_SUCCESS)
    {
        LOG_ERROR(Asset, "geometry pool buffer creation failed ({} bytes)", size);
        std::abort();
    }
    return buffer;
}

} // namespace

GeometryPool::GeometryPool(GPU::VulkanContext& ctx,
                           GPU::BindlessRegistry& bindless,
                           GPU::UploadContext& uploader)
    : _ctx(ctx)
    , _uploader(uploader)
{
    _vertexBuffer = createDeviceLocal(
        ctx, MaxVertices * sizeof(Vertex), vk::BufferUsageFlagBits::eStorageBuffer, _vertexAlloc);
    _indexBuffer = createDeviceLocal(
        ctx, MaxIndices * sizeof(uint32_t), vk::BufferUsageFlagBits::eIndexBuffer, _indexAlloc);

    _vertexSlot = bindless.registerBuffer(_vertexBuffer);

    LOG_INFO(Asset,
             "geometry pool: {} vertices / {} indices capacity, bindless slot {}",
             MaxVertices,
             MaxIndices,
             _vertexSlot);
}

GeometryPool::~GeometryPool()
{
    vmaDestroyBuffer(_ctx.allocator, _indexBuffer, _indexAlloc);
    vmaDestroyBuffer(_ctx.allocator, _vertexBuffer, _vertexAlloc);
}

MeshRange GeometryPool::add(std::span<const Vertex> vertices, std::span<const uint32_t> indices)
{
    if (_vertexCount + vertices.size() > MaxVertices || _indexCount + indices.size() > MaxIndices)
    {
        LOG_ERROR(Asset, "geometry pool exhausted");
        std::abort();
    }

    _uploader.upload(_vertexBuffer, _vertexCount * sizeof(Vertex), std::as_bytes(vertices));
    _uploader.upload(_indexBuffer, _indexCount * sizeof(uint32_t), std::as_bytes(indices));

    const MeshRange range{_indexCount, static_cast<uint32_t>(indices.size()), _vertexCount};
    _vertexCount += static_cast<uint32_t>(vertices.size());
    _indexCount += static_cast<uint32_t>(indices.size());
    return range;
}

} // namespace Asset
