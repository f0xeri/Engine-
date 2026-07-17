#pragma once

#include "engine/GPU/VulkanContext.hpp"

#include <cstdint>

namespace GPU
{

// The single engine-wide descriptor set
// unbounded texture/buffer arrays indexed from shaders
class BindlessRegistry
{
public:
    explicit BindlessRegistry(VulkanContext& ctx);
    ~BindlessRegistry();

    BindlessRegistry(const BindlessRegistry&) = delete;
    BindlessRegistry& operator=(const BindlessRegistry&) = delete;

    // main thread only, slot is written immediately and stays valid forever
    uint32_t registerTexture(vk::ImageView view); // sampled with the default sampler
    uint32_t registerBuffer(vk::Buffer buffer);

    vk::DescriptorSetLayout setLayout() const { return _layout; }
    vk::DescriptorSet set() const { return _set; }

private:
    VulkanContext& _ctx;

    vk::DescriptorSetLayout _layout;
    vk::DescriptorPool _pool;
    vk::DescriptorSet _set;
    vk::Sampler _defaultSampler;

    uint32_t _nextTexture = 0;
    uint32_t _nextBuffer = 0;
};

} // namespace GPU
