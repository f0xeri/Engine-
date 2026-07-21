#include "engine/App/Application.hpp"
#include "engine/App/FlyCamera.hpp"
#include "engine/Asset/Library.hpp"
#include "engine/Core/Math.hpp"
#include "engine/Platform/Input.hpp"
#include "engine/RenderGraph/RenderGraph.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <glm/matrix.hpp>

namespace
{

struct PushConstants
{
    glm::mat4 mvp;
    glm::vec4 baseColorFactor;
    uint32_t vertexBufferSlot;
    uint32_t baseVertex;
    uint32_t albedoTexture;
};

struct LightingParams
{
    glm::mat4 invViewProj;
    glm::vec3 sunDir = {0.4, 1.0, 0.3};
    uint32_t albedoTexture;
    uint32_t normalTexture;
    uint32_t depthSlot;
};

} // namespace

int main()
{
    App::Application app({.title = "Engine sandbox",
                          .shaderRoot = ENGINE_SHADER_DIR,
                          .pipelineCache = ENGINE_PIPELINE_CACHE});

    const auto gbufferPipeline =
        app.loadPipeline("GBuffer", std::array{Graph::Format::RGBA8_Srgb, Graph::Format::RGBA16F});
    const auto lightingPipeline = app.loadPipeline("Lighting");
    const Asset::Model& sponza = app.assets().load(std::filesystem::path(ENGINE_SAMPLE_ASSETS_DIR) /
                                                   "2.0/Sponza/glTF/Sponza.gltf");

    App::FlyCamera camera;

    app.run(
        [&](const App::FrameInfo& frame)
        {
            if (frame.input.pressed(Platform::Key::Escape))
            {
                app.quit();
            }

            camera.update(frame.input, frame.dt);
            app.setRelativeMouseMode(camera.looking());

            auto& graph = frame.graph;

            const float aspect =
                static_cast<float>(frame.extent.width) / static_cast<float>(frame.extent.height);
            const glm::mat4 viewProj =
                Core::perspective(glm::radians(70.0f), aspect, 0.01f) * camera.view();

            const auto albedo = graph.createTexture({Graph::Format::RGBA8_Srgb, frame.extent});
            const auto normal = graph.createTexture({Graph::Format::RGBA16F, frame.extent});
            const auto depth = graph.createTexture({Graph::Format::D32, frame.extent});

            graph.addPass(
                "gbuffer",
                {.color = {{albedo}, {normal}}, .depth = Graph::DepthAttachment{.texture = depth}},
                [&app, &sponza, gbufferPipeline, viewProj](Graph::CmdRecorder& rec)
                {
                    rec.bindPipeline(app.pipeline(gbufferPipeline));
                    rec.bindIndexBuffer(app.geometry().indexBuffer());

                    for (const Asset::SubMesh& submesh : sponza.submeshes)
                    {
                        const Asset::Material& material = sponza.materials[submesh.materialIndex];
                        rec.pushConstants(PushConstants{
                            .mvp = viewProj * submesh.model,
                            .baseColorFactor = material.baseColorFactor,
                            .vertexBufferSlot = app.geometry().vertexBufferSlot(),
                            .baseVertex = submesh.range.baseVertex,
                            .albedoTexture = material.albedoTexture,
                        });
                        rec.drawIndexed(submesh.range.indexCount, submesh.range.firstIndex);
                    }
                });

            graph.addPass(
                "lighting",
                {.input = {albedo, normal, depth},
                 .color = {{frame.backbuffer, Graph::LoadOp::Clear, {0.05f, 0.05f, 0.08f, 1.0f}}}},
                [&, albedo, normal, depth](Graph::CmdRecorder& rec)
                {
                    rec.bindPipeline(app.pipeline(lightingPipeline));
                    rec.pushConstants(LightingParams{.invViewProj = glm::inverse(viewProj),
                                                     .albedoTexture = graph.bindlessSlot(albedo),
                                                     .normalTexture = graph.bindlessSlot(normal),
                                                     .depthSlot = graph.bindlessSlot(depth)});
                    rec.draw(3);
                });
        });

    return 0;
}
