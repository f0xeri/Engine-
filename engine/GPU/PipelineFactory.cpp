#include "engine/GPU/PipelineFactory.hpp"

#include "engine/Core/Log.hpp"

#include <cstdlib>
#include <fstream>
#include <vector>

namespace GPU
{

namespace
{

// Guaranteed minimum for maxPushConstantsSize; enough for indices + a few scalars.
constexpr uint32_t PushConstantBytes = 128;

std::vector<char> readFileIfExists(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        return {};
    }

    std::vector<char> data(static_cast<size_t>(file.tellg()));
    file.seekg(0);
    file.read(data.data(), static_cast<std::streamsize>(data.size()));
    return data;
}

} // namespace

PipelineFactory::PipelineFactory(VulkanContext& ctx,
                                 vk::DescriptorSetLayout bindlessLayout,
                                 std::filesystem::path cacheFile)
    : _ctx(ctx)
    , _cacheFile(std::move(cacheFile))
{
    const std::vector<char> cached = readFileIfExists(_cacheFile);

    _cache = _ctx.device.createPipelineCache({{}, cached.size(), cached.data()});

    const vk::PushConstantRange pushRange(vk::ShaderStageFlagBits::eAll, 0, PushConstantBytes);
    _layout = _ctx.device.createPipelineLayout({{}, bindlessLayout, pushRange});
}

PipelineFactory::~PipelineFactory()
{
    const std::vector<uint8_t> data = _ctx.device.getPipelineCacheData(_cache);

    std::ofstream file(_cacheFile, std::ios::binary | std::ios::trunc);
    if (file)
    {
        file.write(reinterpret_cast<const char*>(data.data()),
                   static_cast<std::streamsize>(data.size()));
    }
    else
    {
        LOG_WARN(Vulkan, "cannot write pipeline cache to {}", _cacheFile.string());
    }

    _ctx.device.destroyPipelineLayout(_layout);
    _ctx.device.destroyPipelineCache(_cache);
}

vk::Pipeline PipelineFactory::createGraphics(std::span<const uint32_t> vertexSpirv,
                                             std::span<const uint32_t> fragmentSpirv,
                                             std::span<const vk::Format> colorFormats,
                                             vk::Format depthFormat) const
{
    const vk::ShaderModuleCreateInfo vsInfo({}, vertexSpirv.size_bytes(), vertexSpirv.data());
    const vk::ShaderModuleCreateInfo fsInfo({}, fragmentSpirv.size_bytes(), fragmentSpirv.data());
    const vk::ShaderModule vs = _ctx.device.createShaderModule(vsInfo);
    const vk::ShaderModule fs = _ctx.device.createShaderModule(fsInfo);

    // Slang renames every entry point to "main" when emitting per-entry-point SPIR-V
    const vk::PipelineShaderStageCreateInfo stages[] = {
        {{}, vk::ShaderStageFlagBits::eVertex, vs, "main"},
        {{}, vk::ShaderStageFlagBits::eFragment, fs, "main"},
    };

    // Vertex pulling: no vertex input state
    const vk::PipelineVertexInputStateCreateInfo vertexInput;
    const vk::PipelineInputAssemblyStateCreateInfo inputAssembly(
        {}, vk::PrimitiveTopology::eTriangleList);
    const vk::PipelineViewportStateCreateInfo viewport({}, 1, nullptr, 1, nullptr);

    vk::PipelineRasterizationStateCreateInfo raster;
    raster.cullMode = vk::CullModeFlagBits::eNone;
    raster.frontFace = vk::FrontFace::eCounterClockwise;
    raster.lineWidth = 1.0f;

    const vk::PipelineMultisampleStateCreateInfo multisample;

    vk::PipelineColorBlendAttachmentState blendAttachment;
    blendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

    const std::vector<vk::PipelineColorBlendAttachmentState> blendAttachments(colorFormats.size(),
                                                                              blendAttachment);
    const vk::PipelineColorBlendStateCreateInfo blend(
        {}, vk::False, vk::LogicOp::eCopy, blendAttachments);

    const vk::DynamicState dynamicStates[] = {vk::DynamicState::eViewport,
                                              vk::DynamicState::eScissor};
    const vk::PipelineDynamicStateCreateInfo dynamic({}, dynamicStates);

    vk::PipelineDepthStencilStateCreateInfo depthStencil;
    if (depthFormat != vk::Format::eUndefined)
    {
        depthStencil.depthTestEnable = vk::True;
        depthStencil.depthWriteEnable = vk::True;
        depthStencil.depthCompareOp = vk::CompareOp::eGreater; // reversed-Z
    }

    const vk::PipelineRenderingCreateInfo rendering(0, colorFormats, depthFormat);

    vk::GraphicsPipelineCreateInfo info;
    info.pNext = &rendering;
    info.stageCount = 2;
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &inputAssembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &multisample;
    info.pDepthStencilState = &depthStencil;
    info.pColorBlendState = &blend;
    info.pDynamicState = &dynamic;
    info.layout = _layout;

    const auto [result, pipeline] = _ctx.device.createGraphicsPipeline(_cache, info);
    _ctx.device.destroyShaderModule(vs);
    _ctx.device.destroyShaderModule(fs);

    if (result != vk::Result::eSuccess)
    {
        LOG_ERROR(Vulkan, "pipeline creation failed: {}", vk::to_string(result));
        std::abort();
    }

    return pipeline;
}

} // namespace GPU
