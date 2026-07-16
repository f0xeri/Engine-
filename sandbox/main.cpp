#include "engine/App/Application.hpp"
#include "engine/Platform/Input.hpp"
#include "engine/RenderGraph/RenderGraph.hpp"

#include <cmath>

namespace
{

struct PushConstants
{
    float tint[4];
};

} // namespace

int main()
{
    App::Application app({.title = "Engine sandbox",
                          .shaderRoot = ENGINE_SHADER_DIR,
                          .pipelineCache = ENGINE_PIPELINE_CACHE});

    const auto triangle = app.loadPipeline("Triangle");

    float time = 0.0f;

    app.run(
        [&](const App::FrameInfo& frame)
        {
            if (frame.input.pressed(Platform::Key::Escape))
            {
                app.quit();
            }

            time += frame.dt;
            const float pulse = 0.75f + (0.25f * std::sin(time * 2.0f));
            const PushConstants push{{pulse, pulse, pulse, 1.0f}};

            const auto depth =
                frame.graph.createTexture({.format = Graph::Format::D32, .extent = frame.extent});

            const Graph::PassDesc passDesc{
                .color = {{frame.backbuffer, Graph::LoadOp::Clear, {0.05f, 0.05f, 0.08f, 1.0f}}},
                .depth = Graph::DepthAttachment{.texture = depth}};

            frame.graph.addPass("triangle",
                                passDesc,
                                [&app, triangle, push](Graph::CmdRecorder& rec)
                                {
                                    rec.bindPipeline(app.pipeline(triangle));
                                    rec.pushConstants(push);
                                    rec.draw(3);
                                });
        });

    return 0;
}
