#pragma once

#include "engine/GPU/VulkanContext.hpp"

#include <cstdint>

namespace GPU
{
constexpr uint32_t InvalidBindlessSlot = -1;
// The single engine-wide descriptor set
// unbounded texture/buffer arrays indexed from shaders
class BindlessRegistry
{
public:
    static constexpr uint32_t MaxTextures = 16384;
    static constexpr uint32_t MaxBuffers = 16384;
    static constexpr uint32_t MaxShadowTextures = 64;

    struct Stats
    {
        uint32_t usedTextures;
        uint32_t usedBuffers;
        uint32_t usedShadowTextures;
    };

    explicit BindlessRegistry(VulkanContext& ctx);
    ~BindlessRegistry();

    BindlessRegistry(const BindlessRegistry&) = delete;
    BindlessRegistry& operator=(const BindlessRegistry&) = delete;

    // main thread only, slot is written immediately and stays valid forever
    uint32_t registerTexture(vk::ImageView view);       // sampled with the default sampler
    uint32_t registerShadowTexture(vk::ImageView view); // sampled with shadow sampler
    uint32_t registerBuffer(vk::Buffer buffer);

    vk::DescriptorSetLayout setLayout() const { return _layout; }
    vk::DescriptorSet set() const { return _set; }
    Stats stats() const { return {_nextTexture, _nextBuffer, _nextShadowTexture}; }

private:
    VulkanContext& _ctx;

    vk::DescriptorSetLayout _layout;
    vk::DescriptorPool _pool;
    vk::DescriptorSet _set;
    vk::Sampler _defaultSampler;
    vk::Sampler _shadowSampler;

    uint32_t _nextTexture = 0;
    uint32_t _nextBuffer = 0;
    uint32_t _nextShadowTexture = 0;
};

} // namespace GPU
