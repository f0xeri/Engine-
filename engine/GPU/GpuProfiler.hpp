#pragma once

#include "engine/GPU/FrameContext.hpp"
#include "engine/GPU/VulkanContext.hpp"

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace GPU
{

class GpuProfiler
{
public:
    struct Scope
    {
        std::string name;
        float ms; // EMA-smoothed
    };

    explicit GpuProfiler(VulkanContext& ctx);
    ~GpuProfiler();

    GpuProfiler(const GpuProfiler&) = delete;
    GpuProfiler& operator=(const GpuProfiler&) = delete;

    // Resolves this slot's previous results and resets its queries. Must be recorded
    // outside a render pass.
    void beginFrame(vk::CommandBuffer cmd, uint64_t frameIndex);

    void beginScope(vk::CommandBuffer cmd, std::string_view name);
    void endScope(vk::CommandBuffer cmd);

    std::span<const Scope> scopes() const { return _scopes; }

private:
    static constexpr uint32_t MaxScopes = 32;
    static constexpr uint32_t QueriesPerFrame = MaxScopes * 2;

    void resolve();

    VulkanContext& _ctx;

    vk::QueryPool _pool;
    float _period = 0.0f;

    std::array<std::vector<std::string>, FramesInFlight> _names;
    uint32_t _slot = 0;
    uint32_t _scopeCount = 0;

    std::vector<Scope> _scopes;
};

} // namespace GPU
