#pragma once

#include "engine/Asset/GeometryPool.hpp"
#include "engine/GPU/BindlessRegistry.hpp"
#include "engine/RenderGraph/RenderGraph.hpp"

#include <array>
#include <functional>
#include <string>

struct ImGuiIO;

namespace App
{
class DebugOverlay
{
public:
    void draw(const GPU::BindlessRegistry& bindless,
              const Asset::GeometryPool& geometry,
              const Graph::RenderGraph& graph,
              bool& vsyncEnabled,
              const std::string& extraTabName,
              const std::function<void()>& extraTabDraw);

private:
    void drawFrameTime(const ImGuiIO& io, bool& vsyncEnabled);
    static void drawPassTimings(const Graph::RenderGraph& graph);
    static void drawPools(const GPU::BindlessRegistry& bindless,
                          const Asset::GeometryPool& geometry);

    static constexpr int FrameTimeHistoryLength = 120;
    std::array<float, FrameTimeHistoryLength> _frameTimesMs{};
    int _frameTimeOffset = 0;
};

} // namespace App
