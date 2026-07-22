#pragma once

#include "engine/Asset/GeometryPool.hpp"
#include "engine/GPU/BindlessRegistry.hpp"
#include "engine/RenderGraph/RenderGraph.hpp"

#include <array>
#include <functional>
#include <string>

namespace App
{
class DebugOverlay
{
public:
    void draw(const GPU::BindlessRegistry& bindless,
              const Asset::GeometryPool& geometry,
              const Graph::RenderGraph& graph,
              bool& vsyncEnabled, // toggled by the button; caller applies it next frame
              const std::string& extraTabName,
              const std::function<void()>& extraTabDraw);

private:
    static constexpr int FrameTimeHistoryLength = 120;
    std::array<float, FrameTimeHistoryLength> _frameTimesMs{};
    int _frameTimeOffset = 0;
};

} // namespace App
