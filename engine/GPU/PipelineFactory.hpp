#pragma once

#include "engine/GPU/VulkanContext.hpp"

#include <filesystem>
#include <span>

namespace GPU
{

// Owns the single engine-wide pipeline layout and a disk-persisted VkPipelineCache
class PipelineFactory
{
public:
    PipelineFactory(VulkanContext& ctx,
                    vk::DescriptorSetLayout bindlessLayout,
                    std::filesystem::path cacheFile);
    ~PipelineFactory();

    PipelineFactory(const PipelineFactory&) = delete;
    PipelineFactory& operator=(const PipelineFactory&) = delete;

    // depthFormat eUndefined = no depth attachment, depth test off
    vk::Pipeline createGraphics(std::span<const uint32_t> vertexSpirv,
                                std::span<const uint32_t> fragmentSpirv,
                                vk::Format colorFormat,
                                vk::Format depthFormat) const;

    vk::PipelineLayout layout() const { return _layout; }

private:
    VulkanContext& _ctx;
    std::filesystem::path _cacheFile;

    vk::PipelineCache _cache;
    vk::PipelineLayout _layout;
};

} // namespace GPU
