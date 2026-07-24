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

struct ViewConstants
{
    glm::mat4 viewProj;
    glm::mat4 lightViewProj;
};

struct PushConstants
{
    glm::mat4 model;
    glm::vec4 baseColorFactor;
    uint32_t vertexBufferSlot;
    uint32_t baseVertex;
    uint32_t albedoTexture;
    uint32_t metallicRoughnessTexture;
    uint32_t normalTexture;
    float metallicFactor;
    float roughnessFactor;
    uint32_t viewSlot;
    uint32_t viewOffset;
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
    uint32_t materialSlot;

    glm::vec3 cameraPos;
    float exposure;
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
};

} // namespace

int main()
{
    App::Application app({.title = "Engine sandbox",
                          .shaderRoot = ENGINE_SHADER_DIR,
                          .pipelineCache = ENGINE_PIPELINE_CACHE});

    const auto shadowPipeline = app.loadPipeline("Shadow", std::span<const Graph::Format>{});
    const auto gbufferPipeline = app.loadPipeline(
        "GBuffer",
        std::array{Graph::Format::RGBA8_Srgb, Graph::Format::RGBA16F, Graph::Format::RGBA8_Unorm});
    const auto lightingPipeline = app.loadPipeline("Lighting", std::array{Graph::Format::RGBA16F});
    const auto tonemapPipeline = app.loadPipeline("Tonemap");
    const Asset::Model& sponza = app.assets().load(std::filesystem::path(ENGINE_SAMPLE_ASSETS_DIR) /
                                                   "2.0/Sponza/glTF/Sponza.gltf");

    App::FlyCamera camera;

    glm::vec3 sunDir = {0.4f, 1.0f, 0.3f};
    glm::vec3 sunColor = {1.0f, 0.95f, 0.85f};
    float sunIlluminance = 100000.0f; // lux
    glm::vec3 skyColor = {0.5f, 0.6f, 0.8f};
    float skyIlluminance = 20000.0f; // lux, blue sky ambient

    // physical camera: Exposure = 1 / (1.2 * 2^EV100); sunny-16 is EV100 ~15
    float exposureEv100 = 15.0f;

    float shadowRadius = 18.0f;
    float shadowBias = 0.001f;

    app.setDebugTab(
        "Sandbox",
        [&]
        {
            ImGui::SliderFloat3("Sun direction", &sunDir.x, -1.0f, 1.0f);
            ImGui::ColorEdit3("Sun color", &sunColor.x);
            ImGui::SliderFloat("Sun illuminance (lux)", &sunIlluminance, 0.0f, 150000.0f, "%.0f");
            ImGui::ColorEdit3("Sky color", &skyColor.x);
            ImGui::SliderFloat("Sky illuminance (lux)", &skyIlluminance, 0.0f, 50000.0f, "%.0f");
            ImGui::Separator();
            ImGui::SliderFloat("Shadow radius", &shadowRadius, 5.0f, 60.0f);
            ImGui::SliderFloat("Shadow bias", &shadowBias, 0.0f, 0.01f, "%.5f");
            ImGui::Separator();
            ImGui::SliderFloat("Exposure (EV100)", &exposureEv100, 0.0f, 20.0f, "%.1f");
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
            const auto material = graph.createTexture({Graph::Format::RGBA8_Unorm, frame.extent});
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

            const GPU::FrameUniforms::Ref viewRef = frame.uniforms.push(
                ViewConstants{.viewProj = viewProj, .lightViewProj = lightViewProj});

            graph.addPass(
                "shadow",
                {.depth = Graph::DepthAttachment{.texture = shadowMap}},
                [&app, &sponza, shadowPipeline, viewRef](Graph::CmdRecorder& rec)
                {
                    rec.bindPipeline(app.pipeline(shadowPipeline));
                    rec.bindIndexBuffer(app.geometry().indexBuffer());
                    for (const Asset::SubMesh& submesh : sponza.submeshes)
                    {
                        const Asset::Material& material = sponza.materials[submesh.materialIndex];
                        rec.pushConstants(PushConstants{
                            .model = submesh.model,
                            .baseColorFactor = material.baseColorFactor,
                            .vertexBufferSlot = app.geometry().vertexBufferSlot(),
                            .baseVertex = submesh.range.baseVertex,
                            .albedoTexture = material.albedoTexture,
                            .metallicRoughnessTexture = material.metallicRoughnessTexture,
                            .normalTexture = material.normalTexture,
                            .metallicFactor = material.metallicFactor,
                            .roughnessFactor = material.roughnessFactor,
                            .viewSlot = viewRef.slot,
                            .viewOffset = viewRef.offset,
                        });
                        rec.drawIndexed(submesh.range.indexCount, submesh.range.firstIndex);
                    }
                });

            graph.addPass(
                "gbuffer",
                {.color = {{albedo}, {normal}, {material}},
                 .depth = Graph::DepthAttachment{.texture = depth}},
                [&app, &sponza, gbufferPipeline, viewRef](Graph::CmdRecorder& rec)
                {
                    rec.bindPipeline(app.pipeline(gbufferPipeline));
                    rec.bindIndexBuffer(app.geometry().indexBuffer());

                    for (const Asset::SubMesh& submesh : sponza.submeshes)
                    {
                        const Asset::Material& material = sponza.materials[submesh.materialIndex];
                        rec.pushConstants(PushConstants{
                            .model = submesh.model,
                            .baseColorFactor = material.baseColorFactor,
                            .vertexBufferSlot = app.geometry().vertexBufferSlot(),
                            .baseVertex = submesh.range.baseVertex,
                            .albedoTexture = material.albedoTexture,
                            .metallicRoughnessTexture = material.metallicRoughnessTexture,
                            .normalTexture = material.normalTexture,
                            .metallicFactor = material.metallicFactor,
                            .roughnessFactor = material.roughnessFactor,
                            .viewSlot = viewRef.slot,
                            .viewOffset = viewRef.offset,
                        });
                        rec.drawIndexed(submesh.range.indexCount, submesh.range.firstIndex);
                    }
                });

            const GPU::FrameUniforms::Ref lightingParams = frame.uniforms.push(
                LightingParams{.invViewProj = glm::inverse(viewProj),
                               .lightViewProj = lightViewProj,
                               .sunDir = sunDir,
                               .shadowBias = shadowBias,
                               .sunRadiance = sunColor * sunIlluminance,
                               .shadowMapSize = ShadowMapResolution,
                               .ambientRadiance = skyColor * skyIlluminance,
                               .albedoTexture = graph.bindlessSlot(albedo),
                               .normalTexture = graph.bindlessSlot(normal),
                               .depthSlot = graph.bindlessSlot(depth),
                               .shadowSlot = graph.shadowSlot(shadowMap),
                               .materialSlot = graph.bindlessSlot(material),
                               .cameraPos = camera.position(),
                               .exposure = 1.0f / (1.2f * std::exp2(exposureEv100))});

            graph.addPass("lighting",
                          {.input = {albedo, normal, material, depth, shadowMap},
                           .color = {{hdr, Graph::LoadOp::DontCare}}},
                          [&app, lightingPipeline, lightingParams](Graph::CmdRecorder& rec)
                          {
                              rec.bindPipeline(app.pipeline(lightingPipeline));
                              rec.pushConstants(
                                  LightingPush{.paramsSlot = lightingParams.slot,
                                               .paramsOffset = lightingParams.offset});
                              rec.draw(3);
                          });

            const TonemapParams tonemapParams{.hdrTexture = graph.bindlessSlot(hdr)};

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
