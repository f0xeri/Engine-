#include "engine/RenderGraph/RenderGraph.hpp"

#include "engine/Core/Log.hpp"
#include "engine/GPU/BindlessRegistry.hpp"
#include "vulkan/vulkan.hpp"

#include <cassert>
#include <cstdlib>

namespace Graph
{

vk::Format toVk(Format format)
{
    switch (format)
    {
        case Format::D32:
            return vk::Format::eD32Sfloat;
        case Format::RGBA8_Srgb:
            return vk::Format::eR8G8B8A8Srgb;
        case Format::RG16F:
            return vk::Format::eR16G16Sfloat;
        case Format::RGBA16F:
            return vk::Format::eR16G16B16A16Sfloat;
    }
    return vk::Format::eUndefined;
}

namespace
{

vk::AttachmentLoadOp toVk(LoadOp op)
{
    switch (op)
    {
        case LoadOp::Load:
            return vk::AttachmentLoadOp::eLoad;
        case LoadOp::Clear:
            return vk::AttachmentLoadOp::eClear;
        case LoadOp::DontCare:
            return vk::AttachmentLoadOp::eDontCare;
    }
    return vk::AttachmentLoadOp::eDontCare;
}

vk::ImageUsageFlags usageFor(Format format)
{
    // every transient may end up as a pass input (sampled read), regardless of
    // whether this particular frame's declaration order uses it that way
    switch (format)
    {
        case Format::D32:
            return vk::ImageUsageFlagBits::eDepthStencilAttachment |
                   vk::ImageUsageFlagBits::eSampled;
        case Format::RGBA8_Srgb:
        case Format::RG16F:
        case Format::RGBA16F:
            return vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    }
    return {};
}

vk::ImageAspectFlags aspectFor(Format format)
{
    switch (format)
    {
        case Format::D32:
            return vk::ImageAspectFlagBits::eDepth;
        case Format::RGBA8_Srgb:
        case Format::RG16F:
        case Format::RGBA16F:
            return vk::ImageAspectFlagBits::eColor;
    }
    return vk::ImageAspectFlagBits::eColor;
}

} // namespace

RenderGraph::RenderGraph(GPU::VulkanContext& ctx, GPU::BindlessRegistry& bindless)
    : _ctx(ctx)
    , _bindless(bindless)
{
}

RenderGraph::~RenderGraph()
{
    trim();
}

ResourceHandle
RenderGraph::importBackbuffer(vk::Image image, vk::ImageView view, vk::Extent2D extent)
{
    // srcStage must intersect the acquire semaphore's wait stage, or the first
    // transition is not ordered after the acquire.
    _resources.push_back({.image = image,
                          .view = view,
                          .extent = extent,
                          .aspect = vk::ImageAspectFlagBits::eColor,
                          .sync = {vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                   vk::AccessFlagBits2::eNone,
                                   vk::ImageLayout::eUndefined},
                          .isBackbuffer = true});
    return {static_cast<uint32_t>(_resources.size() - 1)};
}

ResourceHandle RenderGraph::createTexture(const TextureDesc& desc)
{
    int32_t poolIndex = -1;
    for (size_t i = 0; i < _pool.size(); ++i)
    {
        const PoolEntry& entry = _pool[i];
        if (!entry.usedThisFrame && entry.desc.format == desc.format &&
            entry.desc.extent.width == desc.extent.width &&
            entry.desc.extent.height == desc.extent.height)
        {
            poolIndex = static_cast<int32_t>(i);
            break;
        }
    }

    if (poolIndex < 0)
    {
        const vk::Format format = toVk(desc.format);
        const vk::ImageCreateInfo imageInfo({},
                                            vk::ImageType::e2D,
                                            format,
                                            {desc.extent.width, desc.extent.height, 1},
                                            1,
                                            1,
                                            vk::SampleCountFlagBits::e1,
                                            vk::ImageTiling::eOptimal,
                                            usageFor(desc.format));
        const VkImageCreateInfo rawInfo = imageInfo;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = nullptr;
        const VkResult result =
            vmaCreateImage(_ctx.allocator, &rawInfo, &allocInfo, &image, &allocation, nullptr);
        if (result != VK_SUCCESS)
        {
            LOG_ERROR(
                Graph, "transient image creation failed: {}", vk::to_string(vk::Result(result)));
            std::abort();
        }

        const vk::ImageView view = _ctx.device.createImageView(
            {{}, image, vk::ImageViewType::e2D, format, {}, {aspectFor(desc.format), 0, 1, 0, 1}});

        _pool.push_back({.desc = desc, .image = image, .view = view, .allocation = allocation});
        poolIndex = static_cast<int32_t>(_pool.size() - 1);

        LOG_INFO(Graph, "transient created: {}x{}", desc.extent.width, desc.extent.height);
    }

    PoolEntry& entry = _pool[poolIndex];
    entry.usedThisFrame = true;

    _resources.push_back({.image = entry.image,
                          .view = entry.view,
                          .extent = {desc.extent.width, desc.extent.height},
                          .aspect = aspectFor(desc.format),
                          .sync = entry.sync,
                          .poolIndex = poolIndex});
    return {static_cast<uint32_t>(_resources.size() - 1)};
}

uint32_t RenderGraph::bindlessSlot(ResourceHandle handle)
{
    const int32_t poolIndex = _resources[handle.value].poolIndex;
    assert(poolIndex >= 0 && "cannot use imported resource as bindless slot");
    auto& entry = _pool[poolIndex];
    if (entry.bindlessSlot == GPU::InvalidBindlessSlot)
    {
        entry.bindlessSlot = _bindless.registerTexture(entry.view);
    }
    return entry.bindlessSlot;
}

void RenderGraph::addPass(std::string name, PassDesc desc, std::function<void(CmdRecorder&)> record)
{
    _passes.push_back({std::move(name), std::move(desc), std::move(record)});
}

vk::ImageMemoryBarrier2 RenderGraph::transition(Resource& res, const Sync& next, bool discard)
{
    const vk::ImageMemoryBarrier2 barrier(res.sync.stage,
                                          res.sync.access,
                                          next.stage,
                                          next.access,
                                          discard ? vk::ImageLayout::eUndefined : res.sync.layout,
                                          next.layout,
                                          vk::QueueFamilyIgnored,
                                          vk::QueueFamilyIgnored,
                                          res.image,
                                          {res.aspect, 0, 1, 0, 1});
    res.sync = next;
    return barrier;
}

void RenderGraph::execute(vk::CommandBuffer cmd)
{
    const bool debugLabels = VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdBeginDebugUtilsLabelEXT != nullptr;

    for (Pass& pass : _passes)
    {
        std::vector<vk::ImageMemoryBarrier2> barriers;
        vk::Extent2D area{};

        for (const ResourceHandle input : pass.desc.input)
        {
            Resource& res = _resources[input.value];
            const bool isDepth = res.aspect == vk::ImageAspectFlagBits::eDepth;

            barriers.push_back(transition(res,
                                          {vk::PipelineStageFlagBits2::eFragmentShader,
                                           vk::AccessFlagBits2::eShaderRead,
                                           isDepth ? vk::ImageLayout::eDepthReadOnlyOptimal
                                                   : vk::ImageLayout::eShaderReadOnlyOptimal},
                                          false));
        }

        std::vector<vk::RenderingAttachmentInfo> colorInfos;
        for (const ColorAttachment& attachment : pass.desc.color)
        {
            Resource& res = _resources[attachment.texture.value];
            barriers.push_back(transition(res,
                                          {vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                           vk::AccessFlagBits2::eColorAttachmentWrite,
                                           vk::ImageLayout::eColorAttachmentOptimal},
                                          attachment.loadOp != LoadOp::Load));
            area = res.extent;

            const auto& c = attachment.clear;
            colorInfos.emplace_back(res.view,
                                    vk::ImageLayout::eColorAttachmentOptimal,
                                    vk::ResolveModeFlagBits::eNone,
                                    nullptr,
                                    vk::ImageLayout::eUndefined,
                                    toVk(attachment.loadOp),
                                    vk::AttachmentStoreOp::eStore,
                                    vk::ClearColorValue(c[0], c[1], c[2], c[3]));
        }

        vk::RenderingAttachmentInfo depthInfo;
        if (pass.desc.depth)
        {
            const DepthAttachment& attachment = *pass.desc.depth;
            Resource& res = _resources[attachment.texture.value];
            barriers.push_back(transition(res,
                                          {vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                                               vk::PipelineStageFlagBits2::eLateFragmentTests,
                                           vk::AccessFlagBits2::eDepthStencilAttachmentRead |
                                               vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                                           vk::ImageLayout::eDepthAttachmentOptimal},
                                          attachment.loadOp != LoadOp::Load));
            area = res.extent;

            depthInfo = vk::RenderingAttachmentInfo(res.view,
                                                    vk::ImageLayout::eDepthAttachmentOptimal,
                                                    vk::ResolveModeFlagBits::eNone,
                                                    nullptr,
                                                    vk::ImageLayout::eUndefined,
                                                    toVk(attachment.loadOp),
                                                    vk::AttachmentStoreOp::eStore,
                                                    vk::ClearDepthStencilValue(attachment.clear));
        }

        if (!barriers.empty())
        {
            cmd.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, barriers));
        }

        if (debugLabels)
        {
            cmd.beginDebugUtilsLabelEXT(vk::DebugUtilsLabelEXT(pass.name.c_str()));
        }

        cmd.beginRendering(vk::RenderingInfo(
            {}, {{}, area}, 1, 0, colorInfos, pass.desc.depth ? &depthInfo : nullptr));

        cmd.setViewport(0,
                        vk::Viewport(0.0f,
                                     0.0f,
                                     static_cast<float>(area.width),
                                     static_cast<float>(area.height),
                                     0.0f,
                                     1.0f));
        cmd.setScissor(0, vk::Rect2D({}, area));

        CmdRecorder recorder(cmd);
        pass.record(recorder);

        cmd.endRendering();

        if (debugLabels)
        {
            cmd.endDebugUtilsLabelEXT();
        }
    }

    for (Resource& res : _resources)
    {
        if (res.isBackbuffer)
        {
            // dstStage NONE: the render-finished semaphore carries the dependency to the
            // presentation engine, so there is nothing in this queue left to order against.
            const auto toPresent = transition(res,
                                              {vk::PipelineStageFlagBits2::eNone,
                                               vk::AccessFlagBits2::eNone,
                                               vk::ImageLayout::ePresentSrcKHR},
                                              false);
            cmd.pipelineBarrier2(vk::DependencyInfo({}, {}, {}, toPresent));
        }
        else if (res.poolIndex >= 0)
        {
            _pool[res.poolIndex].sync = res.sync; // next frame's first-use barrier source
        }
    }

    for (PoolEntry& entry : _pool)
    {
        entry.usedThisFrame = false;
    }
    _resources.clear();
    _passes.clear();
}

void RenderGraph::trim()
{
    for (PoolEntry& entry : _pool)
    {
        _ctx.device.destroyImageView(entry.view);
        vmaDestroyImage(_ctx.allocator, entry.image, entry.allocation);
    }
    _pool.clear();
}

} // namespace Graph
