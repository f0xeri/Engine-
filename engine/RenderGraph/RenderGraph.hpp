#pragma once

#include "engine/GPU/BindlessRegistry.hpp"
#include "engine/GPU/GpuProfiler.hpp"
#include "engine/GPU/VulkanContext.hpp"
#include "engine/Platform/Extent2D.hpp"
#include "engine/RenderGraph/CmdRecorder.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Graph
{

enum class Format : uint8_t
{
    D32,
    RGBA8_Srgb,
    RG16F,
    RGBA16F
};

vk::Format toVk(Format format);

enum class LoadOp : uint8_t
{
    Load,
    Clear,
    DontCare,
};

// frame-local: valid until execute() of the frame that declared it
struct ResourceHandle
{
    uint32_t value;
};

struct TextureDesc
{
    Format format;
    Platform::Extent2D extent;
};

struct ColorAttachment
{
    ResourceHandle texture;
    LoadOp loadOp = LoadOp::Clear;
    std::array<float, 4> clear{};
};

struct DepthAttachment
{
    ResourceHandle texture;
    LoadOp loadOp = LoadOp::Clear;
    float clear = 0.0f; // reversed-Z: 0 is the far plane
};

struct PassDesc
{
    std::vector<ResourceHandle> input = {};
    std::vector<ColorAttachment> color = {};
    std::optional<DepthAttachment> depth = {};
};

// Not a real graph yet - decl order == exec order.
// Derives every sync2 barrier from declared usage. owns transient textures, pooled across frames
class RenderGraph
{
public:
    RenderGraph(GPU::VulkanContext& ctx, GPU::BindlessRegistry& bindless);
    ~RenderGraph();

    RenderGraph(const RenderGraph&) = delete;
    RenderGraph& operator=(const RenderGraph&) = delete;

    ResourceHandle importBackbuffer(vk::Image image, vk::ImageView view, vk::Extent2D extent);
    ResourceHandle createTexture(const TextureDesc& desc);

    // register bindless slot or return existing
    uint32_t bindlessSlot(ResourceHandle handle);

    void addPass(std::string name, PassDesc desc, std::function<void(CmdRecorder&)> record);
    void execute(vk::CommandBuffer cmd, uint64_t frameIndex);

    std::span<const GPU::GpuProfiler::Scope> timings() const { return _profiler.scopes(); }

    // frees pooled textures. caller must have waited for device idle
    void trim();

private:
    // what the next barrier on a resource must wait on
    struct Sync
    {
        vk::PipelineStageFlags2 stage;
        vk::AccessFlags2 access;
        vk::ImageLayout layout = vk::ImageLayout::eUndefined;
    };

    struct PoolEntry
    {
        TextureDesc desc;
        vk::Image image;
        vk::ImageView view;
        VmaAllocation allocation = nullptr;
        Sync sync{}; // survives in _pool across frames: source scope of the first-use barrier
        bool usedThisFrame = false;
        uint32_t bindlessSlot = GPU::InvalidBindlessSlot;
    };

    struct Resource
    {
        vk::Image image;
        vk::ImageView view;
        vk::Extent2D extent;
        vk::ImageAspectFlags aspect;
        Sync sync;
        int32_t poolIndex = -1; // -1: imported
        bool isBackbuffer = false;
    };

    struct Pass
    {
        std::string name;
        PassDesc desc;
        std::function<void(CmdRecorder&)> record;
    };

    // emits the barrier moving the resource into `next`; discard drops old contents
    // (UNDEFINED waives content preservation only, never execution ordering)
    vk::ImageMemoryBarrier2 transition(Resource& res, const Sync& next, bool discard);

    GPU::VulkanContext& _ctx;
    GPU::BindlessRegistry& _bindless;
    GPU::GpuProfiler _profiler;

    std::vector<PoolEntry> _pool;
    std::vector<Resource> _resources; // frame-local
    std::vector<Pass> _passes;        // frame-local
};

} // namespace Graph
