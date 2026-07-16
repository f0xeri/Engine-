#include "engine/App/Application.hpp"
#include "engine/Core/Log.hpp"
#include "engine/Platform/Input.hpp"

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

            frame.cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, app.pipeline(triangle));
            frame.cmd.pushConstants<PushConstants>(
                app.pipelines().layout(), vk::ShaderStageFlagBits::eAll, 0, push);
            frame.cmd.draw(3, 1, 0, 0);
        });

    return 0;
}
