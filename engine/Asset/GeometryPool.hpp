#pragma once

#include "engine/Asset/Vertex.hpp"
#include "engine/GPU/BindlessRegistry.hpp"
#include "engine/GPU/UploadContext.hpp"
#include "engine/GPU/VulkanContext.hpp"

#include <cstdint>
#include <span>

namespace Asset
{

struct MeshRange
{
    uint32_t firstIndex;
    uint32_t indexCount;
    uint32_t baseVertex;
};

// All mesh geometry in two device-local buffers
class GeometryPool
{
public:
    GeometryPool(GPU::VulkanContext& ctx,
                 GPU::BindlessRegistry& bindless,
                 GPU::UploadContext& uploader);
    ~GeometryPool();

    GeometryPool(const GeometryPool&) = delete;
    GeometryPool& operator=(const GeometryPool&) = delete;

    // blocking (load time)
    MeshRange add(std::span<const Vertex> vertices, std::span<const uint32_t> indices);

    uint32_t vertexBufferSlot() const { return _vertexSlot; } // bindless SSBO slot
    vk::Buffer indexBuffer() const { return _indexBuffer; }

private:
    GPU::VulkanContext& _ctx;
    GPU::UploadContext& _uploader;

    vk::Buffer _vertexBuffer;
    VmaAllocation _vertexAlloc = nullptr;
    vk::Buffer _indexBuffer;
    VmaAllocation _indexAlloc = nullptr;

    uint32_t _vertexCount = 0; // bump cursors, in elements
    uint32_t _indexCount = 0;
    uint32_t _vertexSlot = 0;
};

} // namespace Asset
