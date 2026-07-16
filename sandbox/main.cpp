#include "engine/App/Application.hpp"
#include "engine/App/FlyCamera.hpp"
#include "engine/Core/Math.hpp"
#include "engine/Platform/Input.hpp"
#include "engine/RenderGraph/RenderGraph.hpp"

#include <cmath>

namespace
{

struct PushConstants
{
    glm::mat4 mvp;
    float tint[4];
};

} // namespace

int main()
{
    App::Application app({.title = "Engine sandbox",
                          .shaderRoot = ENGINE_SHADER_DIR,
                          .pipelineCache = ENGINE_PIPELINE_CACHE});

    const auto cube = app.loadPipeline("Cube");

    App::FlyCamera camera;
    float time = 0.0f;

    app.run(
        [&](const App::FrameInfo& frame)
        {
            if (frame.input.pressed(Platform::Key::Escape))
            {
                app.quit();
            }

            camera.update(frame.input, frame.dt);
            app.setRelativeMouseMode(camera.looking());

            time += frame.dt;
            const float pulse = 0.75f + (0.25f * std::sin(time * 2.0f));
            const float aspect =
                static_cast<float>(frame.extent.width) / static_cast<float>(frame.extent.height);

            const glm::mat4 model =
                glm::rotate(glm::mat4(1.0f), time, glm::normalize(glm::vec3(0.5f, 1.0f, 0.0f)));

            const PushConstants push{
                .mvp =
                    Core::perspective(glm::radians(70.0f), aspect, 0.01f) * camera.view() * model,
                .tint = {pulse, pulse, pulse, 1.0f},
            };

            const auto depth =
                frame.graph.createTexture({.format = Graph::Format::D32, .extent = frame.extent});

            frame.graph.addPass(
                "cube",
                {.color = {{frame.backbuffer, Graph::LoadOp::Clear, {0.05f, 0.05f, 0.08f, 1.0f}}},
                 .depth = Graph::DepthAttachment{.texture = depth}},
                [&app, cube, push](Graph::CmdRecorder& rec)
                {
                    rec.bindPipeline(app.pipeline(cube));
                    rec.pushConstants(push);
                    rec.draw(36);
                });
        });

    return 0;
}
