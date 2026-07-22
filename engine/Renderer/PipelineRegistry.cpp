#include "engine/Renderer/PipelineRegistry.hpp"

#include <future>

namespace Renderer
{

PipelineRegistry::PipelineRegistry(GPU::VulkanContext& ctx,
                                   GPU::PipelineFactory& factory,
                                   std::filesystem::path shaderRoot)
    : _ctx(ctx)
    , _factory(factory)
    , _shaders(std::make_unique<Shader::ShaderSystem>(std::move(shaderRoot)))
{
}

PipelineRegistry::~PipelineRegistry()
{
    _shaders.reset(); // join the worker before touching anything it could still deliver to

    _ctx.device.waitIdle();
    for (const vk::Pipeline pipeline : _table)
    {
        _ctx.device.destroyPipeline(pipeline);
    }
    for (const auto& [frameIndex, pipeline] : _retired)
    {
        _ctx.device.destroyPipeline(pipeline);
    }
    for (const auto& [index, pipeline] : _rebuilt)
    {
        _ctx.device.destroyPipeline(pipeline);
    }
}

PipelineHandle PipelineRegistry::load(std::string module,
                                      std::span<vk::Format> colorFormats,
                                      vk::Format depthFormat)
{
    const auto index = static_cast<uint32_t>(_table.size());
    _names.push_back(module);

    auto initial = std::make_shared<std::promise<vk::Pipeline>>();
    std::future<vk::Pipeline> future = initial->get_future();

    // handler state is worker-private: ShaderSystem invokes handlers serially
    _shaders->load(
        std::move(module),
        // onCompiled
        [this, colorFormats, depthFormat, initial = std::move(initial)](
            const Shader::SpirvBlobs& spirv) mutable
        {
            const auto pipeline =
                _factory.createGraphics(spirv.vertex, spirv.fragment, colorFormats, depthFormat);

            initial->set_value(pipeline);
            initial.reset();
        },
        // onRecompiled (hot reload)
        [this, index, colorFormats, depthFormat](const Shader::SpirvBlobs& spirv)
        {
            const auto pipeline =
                _factory.createGraphics(spirv.vertex, spirv.fragment, colorFormats, depthFormat);

            std::lock_guard lock(_mutex);
            _rebuilt.emplace_back(index, pipeline);
        });

    _table.push_back(future.get());
    return {index};
}

void PipelineRegistry::update(uint64_t frameIndex)
{
    _shaders->pump();

    while (!_retired.empty() && _retired.front().first <= frameIndex)
    {
        _ctx.device.destroyPipeline(_retired.front().second);
        _retired.pop_front();
    }

    std::vector<std::pair<uint32_t, vk::Pipeline>> rebuilt;
    {
        std::lock_guard lock(_mutex);
        rebuilt = std::exchange(_rebuilt, {});
    }

    for (const auto& [index, pipeline] : rebuilt)
    {
        _retired.emplace_back(frameIndex + 1, _table[index]);
        _table[index] = pipeline;
    }
}

} // namespace Renderer
