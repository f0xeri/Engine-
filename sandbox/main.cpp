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
#include <vector>
#include <imgui.h>

namespace
{

struct ViewBlock
{
    glm::mat4 viewProj;
    glm::mat4 lightViewProj;
};

// per-draw data
struct DrawBlock
{
    glm::mat4 model;
    glm::mat4 normalMatrix;
    uint32_t vertexBufferSlot;
    uint32_t baseVertex;
    uint32_t materialIndex;
    uint32_t _pad;
};
static_assert(sizeof(DrawBlock) == 144);

struct GeometryPush
{
    uint32_t bufferSlot;
    uint32_t viewOffset;
    uint32_t drawOffset;
    uint32_t materialOffset;
};

struct LightingBlock
{
    glm::mat4 invViewProj;
    glm::mat4 lightViewProj;

    glm::vec3 sunDir;
    float normalOffset; // normal shadow bias magnitude

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

    float depthBias; // bias for shadows
    glm::vec3 _pad;
};

struct LightingPush
{
    uint32_t paramsSlot;
    uint32_t paramsOffset;
};

constexpr uint32_t ShadowMapResolution = 2048;
constexpr glm::vec3 SceneCenter = {0.0f, 5.0f, 0.0f};

struct TonemapPush
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
    // both in shadow texels; scaled to world units by shadowTexelWorld
    float shadowNormalOffset = 2.0f; // grazing-surface term (walls), matters most under a low sun
    float shadowDepthBias = 1.2f;    // flat floor; covers light-facing surfaces (ground)

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
            ImGui::SliderFloat("Shadow normal offset", &shadowNormalOffset, 0.0f, 6.0f, "%.2f");
            ImGui::SliderFloat("Shadow depth bias", &shadowDepthBias, 0.0f, 6.0f, "%.2f");
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

            // world size of one shadow texel
            const float shadowTexelWorld =
                2.0f * shadowRadius / static_cast<float>(ShadowMapResolution);

            const GPU::FrameUniforms::Ref viewRef = frame.uniforms.push(
                ViewBlock{.viewProj = viewProj, .lightViewProj = lightViewProj});

            // draws[i]/commands[i] are 1:1 so gl_DrawID indexes both
            // backface culled first, double sided second, dynamic state change between
            const uint32_t vertexSlot = app.geometry().vertexBufferSlot();
            std::vector<DrawBlock> draws;
            std::vector<vk::DrawIndexedIndirectCommand> commands;
            draws.reserve(sponza.submeshes.size());
            commands.reserve(sponza.submeshes.size());

            const auto appendDraw = [&](const Asset::SubMesh& submesh)
            {
                draws.push_back({.model = submesh.model,
                                 .normalMatrix = submesh.normalMatrix,
                                 .vertexBufferSlot = vertexSlot,
                                 .baseVertex = submesh.range.baseVertex,
                                 .materialIndex = submesh.materialIndex,
                                 ._pad = 0});
                commands.push_back({submesh.range.indexCount,
                                    1,
                                    submesh.range.firstIndex,
                                    /*vertexOffset*/ 0,
                                    /*firstInstance*/ 0});
            };
            const auto isDoubleSided = [&](const Asset::SubMesh& submesh)
            { return sponza.materials[submesh.materialIndex].doubleSided != 0; };

            for (const Asset::SubMesh& submesh : sponza.submeshes)
            {
                if (!isDoubleSided(submesh))
                {
                    appendDraw(submesh);
                }
            }
            const auto culledCount = static_cast<uint32_t>(draws.size());
            for (const Asset::SubMesh& submesh : sponza.submeshes)
            {
                if (isDoubleSided(submesh))
                {
                    appendDraw(submesh);
                }
            }
            const auto doubleSidedCount = static_cast<uint32_t>(draws.size()) - culledCount;

            const GPU::FrameUniforms::Ref drawsRef =
                frame.uniforms.pushArray(std::span<const DrawBlock>(draws));
            const GPU::FrameUniforms::Ref materialsRef =
                frame.uniforms.pushArray(std::span<const Asset::Material>(sponza.materials));
            const GPU::FrameUniforms::Ref indirectRef =
                frame.uniforms.pushArray(std::span<const vk::DrawIndexedIndirectCommand>(commands));
            const vk::Buffer indirectBuffer = frame.uniforms.currentBuffer();
            const auto drawCount = static_cast<uint32_t>(commands.size());

            const GeometryPush basePush{.bufferSlot = viewRef.slot,
                                        .viewOffset = viewRef.offset,
                                        .drawOffset = drawsRef.offset,
                                        .materialOffset = materialsRef.offset};

            // depth-only pass; no culling needed, bindPipeline leaves cull at None
            graph.addPass("shadow",
                          {.depth = Graph::DepthAttachment{.texture = shadowMap}},
                          [&app, shadowPipeline, basePush, indirectBuffer, indirectRef, drawCount](
                              Graph::CmdRecorder& rec)
                          {
                              rec.bindPipeline(app.pipeline(shadowPipeline));
                              rec.bindIndexBuffer(app.geometry().indexBuffer());
                              rec.pushConstants(basePush);
                              rec.drawIndexedIndirect(indirectBuffer,
                                                      indirectRef.offset,
                                                      drawCount,
                                                      sizeof(vk::DrawIndexedIndirectCommand));
                          });

            graph.addPass("gbuffer",
                          {.color = {{albedo}, {normal}, {material}},
                           .depth = Graph::DepthAttachment{.texture = depth}},
                          [&app,
                           gbufferPipeline,
                           basePush,
                           indirectBuffer,
                           indirectRef,
                           culledCount,
                           doubleSidedCount](Graph::CmdRecorder& rec)
                          {
                              constexpr uint32_t cmdStride = sizeof(vk::DrawIndexedIndirectCommand);
                              constexpr uint32_t drawStride = sizeof(DrawBlock);
                              rec.bindPipeline(app.pipeline(gbufferPipeline));
                              rec.bindIndexBuffer(app.geometry().indexBuffer());

                              // single-sided: back-face culled
                              rec.setCullMode(Graph::CullMode::Back);
                              rec.pushConstants(basePush);
                              rec.drawIndexedIndirect(
                                  indirectBuffer, indirectRef.offset, culledCount, cmdStride);

                              // double-sided: no culling, its own DrawBlock/command range
                              if (doubleSidedCount > 0)
                              {
                                  GeometryPush doublePush = basePush;
                                  doublePush.drawOffset += culledCount * drawStride;
                                  rec.setCullMode(Graph::CullMode::None);
                                  rec.pushConstants(doublePush);
                                  rec.drawIndexedIndirect(indirectBuffer,
                                                          indirectRef.offset +
                                                              (culledCount * cmdStride),
                                                          doubleSidedCount,
                                                          cmdStride);
                              }
                          });

            const GPU::FrameUniforms::Ref lightingParams = frame.uniforms.push(
                LightingBlock{.invViewProj = glm::inverse(viewProj),
                              .lightViewProj = lightViewProj,
                              .sunDir = sunDir,
                              .normalOffset = shadowNormalOffset * shadowTexelWorld,
                              .sunRadiance = sunColor * sunIlluminance,
                              .shadowMapSize = ShadowMapResolution,
                              .ambientRadiance = skyColor * skyIlluminance,
                              .albedoTexture = graph.bindlessSlot(albedo),
                              .normalTexture = graph.bindlessSlot(normal),
                              .depthSlot = graph.bindlessSlot(depth),
                              .shadowSlot = graph.shadowSlot(shadowMap),
                              .materialSlot = graph.bindlessSlot(material),
                              .cameraPos = camera.position(),
                              .exposure = 1.0f / (1.2f * std::exp2(exposureEv100)),
                              .depthBias = shadowDepthBias * shadowTexelWorld,
                              ._pad = {}});

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

            const TonemapPush tonemapPush{.hdrTexture = graph.bindlessSlot(hdr)};

            graph.addPass("tonemap",
                          {.input = {hdr}, .color = {{frame.backbuffer, Graph::LoadOp::DontCare}}},
                          [&app, tonemapPipeline, tonemapPush](Graph::CmdRecorder& rec)
                          {
                              rec.bindPipeline(app.pipeline(tonemapPipeline));
                              rec.pushConstants(tonemapPush);
                              rec.draw(3);
                          });
        });

    return 0;
}
