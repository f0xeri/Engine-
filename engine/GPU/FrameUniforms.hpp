#pragma once

#include "engine/GPU/BindlessRegistry.hpp"
#include "engine/GPU/FrameContext.hpp"
#include "engine/GPU/VulkanContext.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace GPU
{

// host-coherent buffer per frame slot, permanently mapped and permanently bindless-registered
// drops every frame
class FrameUniforms
{
public:
    // Where a pushed struct landed. Both halves go to the shader as push constants.
    struct Ref
    {
        uint32_t slot;   // bindless buffer slot
        uint32_t offset; // byte offset within that buffer
    };

    FrameUniforms(VulkanContext& ctx, BindlessRegistry& bindless);
    ~FrameUniforms();

    FrameUniforms(const FrameUniforms&) = delete;
    FrameUniforms& operator=(const FrameUniforms&) = delete;

    // drops cursor to 0
    void beginFrame(uint64_t frameIndex);

    template <typename T>
    Ref push(const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "shader constants must be POD");

        const Ref ref = allocate(sizeof(T));
        std::memcpy(_slots[_current].mapped + ref.offset, &value, sizeof(T));
        return ref;
    }

private:
    static constexpr uint32_t BufferBytes = 64 * 1024;
    static constexpr uint32_t Alignment = 16;

    Ref allocate(uint32_t bytes);

    struct Slot
    {
        vk::Buffer buffer;
        VmaAllocation allocation = nullptr;
        std::byte* mapped = nullptr;
        uint32_t bindlessSlot = InvalidBindlessSlot;
    };

    VulkanContext& _ctx;

    std::array<Slot, FramesInFlight> _slots;
    uint32_t _current = 0;
    uint32_t _cursor = 0;
};

} // namespace GPU
