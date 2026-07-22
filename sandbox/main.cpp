#include "engine/App/Application.hpp"
#include "engine/App/FlyCamera.hpp"
#include "engine/Asset/Library.hpp"
#include "engine/Core/Math.hpp"
#include "engine/Platform/Input.hpp"
#include "engine/RenderGraph/RenderGraph.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <glm/matrix.hpp>
#include <imgui.h>

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

    glm::vec3 sunDir;
    uint32_t albedoTexture;

    glm::vec3 sunColor;
    float sunIntensity;

    glm::vec3 ambientColor;
    float ambientIntensity;

    uint32_t normalTexture;
    uint32_t depthSlot;
};

struct TonemapParams
{
    uint32_t hdrTexture;
    float exposure;
};

} // namespace

int main()
{
    App::Application app({.title = "Engine sandbox",
                          .shaderRoot = ENGINE_SHADER_DIR,
                          .pipelineCache = ENGINE_PIPELINE_CACHE});

    const auto gbufferPipeline =
        app.loadPipeline("GBuffer", std::array{Graph::Format::RGBA8_Srgb, Graph::Format::RGBA16F});
    const auto lightingPipeline = app.loadPipeline("Lighting", std::array{Graph::Format::RGBA16F});
    const auto tonemapPipeline = app.loadPipeline("Tonemap");
    const Asset::Model& sponza = app.assets().load(std::filesystem::path(ENGINE_SAMPLE_ASSETS_DIR) /
                                                   "2.0/Sponza/glTF/Sponza.gltf");

    App::FlyCamera camera;

    glm::vec3 sunDir = {0.4f, 1.0f, 0.3f};
    glm::vec3 sunColor = {1.0f, 0.95f, 0.85f};
    float sunIntensity = 5.0f;
    glm::vec3 ambientColor = {0.5f, 0.6f, 0.8f};
    float ambientIntensity = 0.5f;

    // в шейдер уходит 2^exposureEv, что даёт более точную конфигурацию в начале слайдера
    float exposureEv = -2.0f;

    app.setDebugTab("Sandbox",
                    [&]
                    {
                        ImGui::SliderFloat3("Sun direction", &sunDir.x, -1.0f, 1.0f);
                        ImGui::ColorEdit3("Sun color", &sunColor.x);
                        ImGui::SliderFloat("Sun intensity", &sunIntensity, 0.0f, 20.0f);
                        ImGui::ColorEdit3("Ambient color", &ambientColor.x);
                        ImGui::SliderFloat("Ambient intensity", &ambientIntensity, 0.0f, 2.0f);
                        ImGui::Separator();
                        ImGui::SliderFloat("Exposure (EV)", &exposureEv, -8.0f, 8.0f);
                    });

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
            const auto hdr = graph.createTexture({Graph::Format::RGBA16F, frame.extent});

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
                {.input = {albedo, normal, depth}, .color = {{hdr, Graph::LoadOp::DontCare}}},
                [&, albedo, normal, depth](Graph::CmdRecorder& rec)
                {
                    rec.bindPipeline(app.pipeline(lightingPipeline));
                    rec.pushConstants(LightingParams{.invViewProj = glm::inverse(viewProj),
                                                     .sunDir = sunDir,
                                                     .albedoTexture = graph.bindlessSlot(albedo),
                                                     .sunColor = sunColor,
                                                     .sunIntensity = sunIntensity,
                                                     .ambientColor = ambientColor,
                                                     .ambientIntensity = ambientIntensity,
                                                     .normalTexture = graph.bindlessSlot(normal),
                                                     .depthSlot = graph.bindlessSlot(depth)});
                    rec.draw(3);
                });

            graph.addPass("tonemap",
                          {.input = {hdr}, .color = {{frame.backbuffer, Graph::LoadOp::DontCare}}},
                          [&, hdr](Graph::CmdRecorder& rec)
                          {
                              rec.bindPipeline(app.pipeline(tonemapPipeline));
                              rec.pushConstants(TonemapParams{.hdrTexture = graph.bindlessSlot(hdr),
                                                              .exposure = std::exp2(exposureEv)});
                              rec.draw(3);
                          });
        });

    return 0;
}
