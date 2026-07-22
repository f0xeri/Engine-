#include "engine/GPU/BindlessRegistry.hpp"

#include "engine/Core/Log.hpp"
#include "vulkan/vulkan.hpp"

#include <cstdlib>

namespace GPU
{

namespace
{

constexpr uint32_t TextureBinding = 0;
constexpr uint32_t BufferBinding = 1;
constexpr uint32_t ShadowBinding = 2;

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
        {ShadowBinding,
         vk::DescriptorType::eCombinedImageSampler,
         MaxShadowTextures,
         vk::ShaderStageFlagBits::eAll},
    };

    const vk::DescriptorBindingFlags perBinding = vk::DescriptorBindingFlagBits::eUpdateAfterBind |
                                                  vk::DescriptorBindingFlagBits::ePartiallyBound;
    const vk::DescriptorBindingFlags bindingFlags[] = {perBinding, perBinding, perBinding};
    const vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo(bindingFlags);

    vk::DescriptorSetLayoutCreateInfo layoutInfo(
        vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool, bindings);
    layoutInfo.pNext = &flagsInfo;
    _layout = _ctx.device.createDescriptorSetLayout(layoutInfo);

    const vk::DescriptorPoolSize sizes[] = {
        {vk::DescriptorType::eCombinedImageSampler, MaxTextures + MaxShadowTextures},
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

    // Linear filtering here interpolates *comparison results*, not depths - that is what
    // makes a single tap a 2x2 PCF. GREATER_OR_EQUAL because depth is reversed-Z, so the
    // reference passes when it is at least as close to the light as the stored occluder.
    vk::SamplerCreateInfo shadowInfo;
    shadowInfo.magFilter = vk::Filter::eLinear;
    shadowInfo.minFilter = vk::Filter::eLinear;
    shadowInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
    shadowInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    shadowInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    shadowInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    shadowInfo.compareEnable = vk::True;
    shadowInfo.compareOp = vk::CompareOp::eGreaterOrEqual;
    _shadowSampler = _ctx.device.createSampler(shadowInfo);

    LOG_INFO(Vulkan,
             "bindless set created: {} textures, {} buffers, {} shadow maps",
             MaxTextures,
             MaxBuffers,
             MaxShadowTextures);
}

BindlessRegistry::~BindlessRegistry()
{
    _ctx.device.destroySampler(_shadowSampler);
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

uint32_t BindlessRegistry::registerShadowTexture(vk::ImageView view)
{
    if (_nextShadowTexture >= MaxShadowTextures)
    {
        LOG_ERROR(Vulkan, "bindless shadow slots exhausted ({})", MaxShadowTextures);
        std::abort();
    }
    const uint32_t slot = _nextShadowTexture++;

    const vk::DescriptorImageInfo info(
        _shadowSampler, view, vk::ImageLayout::eDepthReadOnlyOptimal);
    _ctx.device.updateDescriptorSets(
        vk::WriteDescriptorSet(
            _set, ShadowBinding, slot, 1, vk::DescriptorType::eCombinedImageSampler, &info),
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
