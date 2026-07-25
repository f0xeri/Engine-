#pragma once

#include "engine/GPU/BindlessRegistry.hpp"
#include "engine/GPU/FrameContext.hpp"
#include "engine/GPU/VulkanContext.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
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

    // current frame's buffer, for referencing a pushed indirect-command array in a draw
    vk::Buffer currentBuffer() const { return _slots[_current].buffer; }

    template <typename T>
    Ref push(const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>, "shader constants must be POD");

        const Ref ref = allocate(sizeof(T));
        std::memcpy(_slots[_current].mapped + ref.offset, &value, sizeof(T));
        return ref;
    }

    // Contiguous array; element i lives at ref.offset + i * sizeof(T). Ref points at element 0 -
    // the shader indexes it by gl_DrawID (draw table) or material index.
    template <typename T>
    Ref pushArray(std::span<const T> values)
    {
        static_assert(std::is_trivially_copyable_v<T>, "shader constants must be POD");

        const auto bytes = static_cast<uint32_t>(sizeof(T) * values.size());
        const Ref ref = allocate(bytes);
        std::memcpy(_slots[_current].mapped + ref.offset, values.data(), bytes);
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
