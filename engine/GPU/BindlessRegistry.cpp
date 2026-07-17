#include "engine/GPU/BindlessRegistry.hpp"

#include "engine/Core/Log.hpp"
#include "vulkan/vulkan.hpp"

#include <cstdlib>

namespace GPU
{

namespace
{

// capacities
constexpr uint32_t MaxTextures = 16384;
constexpr uint32_t MaxBuffers = 16384;

constexpr uint32_t TextureBinding = 0;
constexpr uint32_t BufferBinding = 1;

} // namespace

BindlessRegistry::BindlessRegistry(VulkanContext& ctx)
    : _ctx(ctx)
{
    const vk::DescriptorSetLayoutBinding bindings[] = {
        {TextureBinding,
         vk::DescriptorType::eCombinedImageSampler,
         MaxTextures,
         vk::ShaderStageFlagBits::eAll},
        {BufferBinding,
         vk::DescriptorType::eStorageBuffer,
         MaxBuffers,
         vk::ShaderStageFlagBits::eAll},
    };

    const vk::DescriptorBindingFlags perBinding = vk::DescriptorBindingFlagBits::eUpdateAfterBind |
                                                  vk::DescriptorBindingFlagBits::ePartiallyBound;
    const vk::DescriptorBindingFlags bindingFlags[] = {perBinding, perBinding};
    const vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo(bindingFlags);

    vk::DescriptorSetLayoutCreateInfo layoutInfo(
        vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool, bindings);
    layoutInfo.pNext = &flagsInfo;
    _layout = _ctx.device.createDescriptorSetLayout(layoutInfo);

    const vk::DescriptorPoolSize sizes[] = {
        {vk::DescriptorType::eCombinedImageSampler, MaxTextures},
        {vk::DescriptorType::eStorageBuffer, MaxBuffers},
    };
    _pool = _ctx.device.createDescriptorPool(
        {vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind, 1, sizes});

    _set = _ctx.device.allocateDescriptorSets({_pool, 1, &_layout}).front();

    vk::SamplerCreateInfo samplerInfo;
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
    samplerInfo.anisotropyEnable = vk::True;
    samplerInfo.maxAnisotropy = 8.0f;
    samplerInfo.maxLod = vk::LodClampNone;
    _defaultSampler = _ctx.device.createSampler(samplerInfo);

    LOG_INFO(Vulkan, "bindless set created: {} textures, {} buffers", MaxTextures, MaxBuffers);
}

BindlessRegistry::~BindlessRegistry()
{
    _ctx.device.destroySampler(_defaultSampler);
    _ctx.device.destroyDescriptorPool(_pool); // frees the set with it
    _ctx.device.destroyDescriptorSetLayout(_layout);
}

uint32_t BindlessRegistry::registerTexture(vk::ImageView view)
{
    if (_nextTexture >= MaxTextures)
    {
        LOG_ERROR(Vulkan, "bindless texture slots exhausted ({})", MaxTextures);
        std::abort();
    }
    const uint32_t slot = _nextTexture++;

    const vk::DescriptorImageInfo info(
        _defaultSampler, view, vk::ImageLayout::eShaderReadOnlyOptimal);
    _ctx.device.updateDescriptorSets(
        vk::WriteDescriptorSet(
            _set, TextureBinding, slot, 1, vk::DescriptorType::eCombinedImageSampler, &info),
        {});
    return slot;
}

uint32_t BindlessRegistry::registerBuffer(vk::Buffer buffer)
{
    if (_nextBuffer >= MaxBuffers)
    {
        LOG_ERROR(Vulkan, "bindless buffer slots exhausted ({})", MaxBuffers);
        std::abort();
    }
    const uint32_t slot = _nextBuffer++;

    const vk::DescriptorBufferInfo info(buffer, 0, vk::WholeSize);
    _ctx.device.updateDescriptorSets(
        vk::WriteDescriptorSet(
            _set, BufferBinding, slot, 1, vk::DescriptorType::eStorageBuffer, nullptr, &info),
        {});
    return slot;
}
} // namespace GPU
