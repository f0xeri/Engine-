#include "engine/GPU/GpuProfiler.hpp"

#include "engine/Core/Log.hpp"

namespace GPU
{

namespace
{
constexpr float Smoothing = 0.1f; // EMA weight of the newest sample
} // namespace

GpuProfiler::GpuProfiler(VulkanContext& ctx)
    : _ctx(ctx)
{
    const auto queueProps = _ctx.physicalDevice.getQueueFamilyProperties();
    if (queueProps[_ctx.graphicsQueueFamily].timestampValidBits == 0)
    {
        LOG_WARN(Vulkan, "graphics queue does not support timestamps, GPU profiling disabled");
        return;
    }

    _period = _ctx.deviceProps.limits.timestampPeriod;
    _pool = _ctx.device.createQueryPool(
        {{}, vk::QueryType::eTimestamp, QueriesPerFrame * FramesInFlight});
}

GpuProfiler::~GpuProfiler()
{
    if (_pool)
    {
        _ctx.device.destroyQueryPool(_pool);
    }
}

void GpuProfiler::beginFrame(vk::CommandBuffer cmd, uint64_t frameIndex)
{
    if (!_pool)
    {
        return;
    }

    _slot = static_cast<uint32_t>(frameIndex % FramesInFlight);
    resolve();

    cmd.resetQueryPool(_pool, _slot * QueriesPerFrame, QueriesPerFrame);
    _names[_slot].clear();
    _scopeCount = 0;
}

void GpuProfiler::beginScope(vk::CommandBuffer cmd, std::string_view name)
{
    if (!_pool || _scopeCount >= MaxScopes)
    {
        return;
    }

    _names[_slot].emplace_back(name);
    cmd.writeTimestamp2(vk::PipelineStageFlagBits2::eAllCommands,
                        _pool,
                        (_slot * QueriesPerFrame) + (_scopeCount * 2));
}

void GpuProfiler::endScope(vk::CommandBuffer cmd)
{
    if (!_pool || _scopeCount >= MaxScopes)
    {
        return;
    }

    cmd.writeTimestamp2(vk::PipelineStageFlagBits2::eAllCommands,
                        _pool,
                        (_slot * QueriesPerFrame) + (_scopeCount * 2) + 1);
    ++_scopeCount;
}

void GpuProfiler::resolve()
{
    const std::vector<std::string>& names = _names[_slot];
    if (names.empty())
    {
        return;
    }

    std::array<uint64_t, QueriesPerFrame> ticks{};
    const uint32_t count = static_cast<uint32_t>(names.size()) * 2;
    if (_ctx.device.getQueryPoolResults(_pool,
                                        _slot * QueriesPerFrame,
                                        count,
                                        count * sizeof(uint64_t),
                                        ticks.data(),
                                        sizeof(uint64_t),
                                        vk::QueryResultFlagBits::e64) != vk::Result::eSuccess)
    {
        return;
    }

    if (_scopes.size() != names.size())
    {
        _scopes.assign(names.size(), {});
    }

    for (size_t i = 0; i < names.size(); ++i)
    {
        const float ms = static_cast<float>(ticks[(i * 2) + 1] - ticks[i * 2]) * _period * 1e-6f;

        Scope& scope = _scopes[i];
        scope.ms = scope.name == names[i] ? scope.ms + (ms - scope.ms) * Smoothing : ms;
        scope.name = names[i];
    }
}

} // namespace GPU
