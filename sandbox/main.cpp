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
#include <span>
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
    glm::mat4 lightViewProj;

    glm::vec3 sunDir;
    float shadowBias;

    glm::vec3 sunRadiance;
    float shadowMapSize;

    glm::vec3 ambientRadiance;
    uint32_t albedoTexture;

    uint32_t normalTexture;
    uint32_t depthSlot;
    uint32_t shadowSlot;
    uint32_t pad;
};

struct LightingPush
{
    uint32_t paramsSlot;
    uint32_t paramsOffset;
};

constexpr uint32_t ShadowMapResolution = 2048;
constexpr glm::vec3 SceneCenter = {0.0f, 5.0f, 0.0f};

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

    const auto shadowPipeline = app.loadPipeline("Shadow", std::span<const Graph::Format>{});
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

    float shadowRadius = 18.0f;
    float shadowBias = 0.001f;

    app.setDebugTab("Sandbox",
                    [&]
                    {
                        ImGui::SliderFloat3("Sun direction", &sunDir.x, -1.0f, 1.0f);
                        ImGui::ColorEdit3("Sun color", &sunColor.x);
                        ImGui::SliderFloat("Sun intensity", &sunIntensity, 0.0f, 20.0f);
                        ImGui::ColorEdit3("Ambient color", &ambientColor.x);
                        ImGui::SliderFloat("Ambient intensity", &ambientIntensity, 0.0f, 2.0f);
                        ImGui::Separator();
                        ImGui::SliderFloat("Shadow radius", &shadowRadius, 5.0f, 60.0f);
                        ImGui::SliderFloat("Shadow bias", &shadowBias, 0.0f, 0.01f, "%.5f");
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
            const auto shadowMap = graph.createTexture(
                {Graph::Format::D32, {ShadowMapResolution, ShadowMapResolution}});

            const glm::vec3 sun = glm::normalize(sunDir);
            const glm::vec3 up =
                std::abs(sun.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
            const glm::mat4 lightViewProj =
                Core::orthographic(shadowRadius, shadowRadius, 0.05f, shadowRadius * 3.0f) *
                glm::lookAt(SceneCenter + (sun * shadowRadius * 1.5f), SceneCenter, up);

            graph.addPass(
                "shadow",
                {.depth = Graph::DepthAttachment{.texture = shadowMap}},
                [&app, &sponza, shadowPipeline, lightViewProj](Graph::CmdRecorder& rec)
                {
                    rec.bindPipeline(app.pipeline(shadowPipeline));
                    rec.bindIndexBuffer(app.geometry().indexBuffer());

                    for (const Asset::SubMesh& submesh : sponza.submeshes)
                    {
                        const Asset::Material& material = sponza.materials[submesh.materialIndex];
                        rec.pushConstants(PushConstants{
                            .mvp = lightViewProj * submesh.model,
                            .baseColorFactor = material.baseColorFactor,
                            .vertexBufferSlot = app.geometry().vertexBufferSlot(),
                            .baseVertex = submesh.range.baseVertex,
                            .albedoTexture = material.albedoTexture,
                        });
                        rec.drawIndexed(submesh.range.indexCount, submesh.range.firstIndex);
                    }
                });

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

            const GPU::FrameUniforms::Ref lightingParams = frame.uniforms.push(
                LightingParams{.invViewProj = glm::inverse(viewProj),
                               .lightViewProj = lightViewProj,
                               .sunDir = sunDir,
                               .shadowBias = shadowBias,
                               .sunRadiance = sunColor * sunIntensity,
                               .shadowMapSize = ShadowMapResolution,
                               .ambientRadiance = ambientColor * ambientIntensity,
                               .albedoTexture = graph.bindlessSlot(albedo),
                               .normalTexture = graph.bindlessSlot(normal),
                               .depthSlot = graph.bindlessSlot(depth),
                               .shadowSlot = graph.shadowSlot(shadowMap),
                               .pad = 0});

            graph.addPass("lighting",
                          {.input = {albedo, normal, depth, shadowMap},
                           .color = {{hdr, Graph::LoadOp::DontCare}}},
                          [&app, lightingPipeline, lightingParams](Graph::CmdRecorder& rec)
                          {
                              rec.bindPipeline(app.pipeline(lightingPipeline));
                              rec.pushConstants(
                                  LightingPush{.paramsSlot = lightingParams.slot,
                                               .paramsOffset = lightingParams.offset});
                              rec.draw(3);
                          });

            const TonemapParams tonemapParams{.hdrTexture = graph.bindlessSlot(hdr),
                                              .exposure = std::exp2(exposureEv)};

            graph.addPass("tonemap",
                          {.input = {hdr}, .color = {{frame.backbuffer, Graph::LoadOp::DontCare}}},
                          [&app, tonemapPipeline, tonemapParams](Graph::CmdRecorder& rec)
                          {
                              rec.bindPipeline(app.pipeline(tonemapPipeline));
                              rec.pushConstants(tonemapParams);
                              rec.draw(3);
                          });
        });

    return 0;
}
