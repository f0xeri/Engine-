#include "engine/App/Application.hpp"
#include "engine/App/FlyCamera.hpp"
#include "engine/Asset/Library.hpp"
#include "engine/Core/Math.hpp"
#include "engine/Platform/Input.hpp"
#include "engine/RenderGraph/RenderGraph.hpp"

#include <filesystem>

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

} // namespace

int main()
{
    App::Application app({.title = "Engine sandbox",
                          .shaderRoot = ENGINE_SHADER_DIR,
                          .pipelineCache = ENGINE_PIPELINE_CACHE});

    const auto meshPipeline = app.loadPipeline("Mesh");
    const Asset::Model& sponza = app.assets().load(
        std::filesystem::path(ENGINE_SAMPLE_ASSETS_DIR) / "2.0/Sponza/glTF/Sponza.gltf");

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

            const float aspect =
                static_cast<float>(frame.extent.width) / static_cast<float>(frame.extent.height);
            const glm::mat4 viewProj =
                Core::perspective(glm::radians(70.0f), aspect, 0.01f) * camera.view();

            const auto depth =
                frame.graph.createTexture({.format = Graph::Format::D32, .extent = frame.extent});

            frame.graph.addPass(
                "sponza",
                {.color = {{frame.backbuffer, Graph::LoadOp::Clear, {0.05f, 0.05f, 0.08f, 1.0f}}},
                 .depth = Graph::DepthAttachment{.texture = depth}},
                [&app, &sponza, meshPipeline, viewProj](Graph::CmdRecorder& rec)
                {
                    rec.bindPipeline(app.pipeline(meshPipeline));
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
        });

    return 0;
}
