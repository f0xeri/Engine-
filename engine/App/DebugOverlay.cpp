#include "engine/App/DebugOverlay.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <imgui.h>

namespace App
{

namespace
{

constexpr ImVec4 Accent{0.20f, 0.68f, 0.78f, 1.00f};
constexpr ImVec4 Dim{0.48f, 0.52f, 0.57f, 1.00f};
constexpr ImVec4 Good{0.38f, 0.78f, 0.45f, 1.00f};
constexpr ImVec4 Warn{0.90f, 0.70f, 0.30f, 1.00f};
constexpr ImVec4 Bad{0.90f, 0.36f, 0.36f, 1.00f};

constexpr float BarHeight = 3.0f;

ImVec4 frameTimeColor(float ms)
{
    if (ms <= 8.0f)
    {
        return Good;
    }
    return ms <= 16.7f ? Warn : Bad;
}

// continues the current line, pushing the text flush against the right edge
void rightAligned(const ImVec4& color, const char* text)
{
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x -
                         ImGui::CalcTextSize(text).x);
    ImGui::TextColored(color, "%s", text);
}

void slimBar(float fraction, const ImVec4& color)
{
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    ImGui::ProgressBar(fraction, ImVec2(-FLT_MIN, BarHeight), "");
    ImGui::PopStyleColor();
}

void poolBar(const char* label, uint32_t used, uint32_t max)
{
    const float fraction = static_cast<float>(used) / static_cast<float>(max);

    std::array<char, 32> text{};
    snprintf(text.data(), text.size(), "%u / %u", used, max);

    ImGui::TextUnformatted(label);
    rightAligned(Dim, text.data());
    slimBar(fraction, fraction > 0.9f ? Bad : Accent);
}

} // namespace

void DebugOverlay::draw(const GPU::BindlessRegistry& bindless,
                        const Asset::GeometryPool& geometry,
                        const Graph::RenderGraph& graph,
                        bool& vsyncEnabled,
                        const std::string& extraTabName,
                        const std::function<void()>& extraTabDraw)
{
    const ImGuiIO& io = ImGui::GetIO();
    _frameTimesMs[_frameTimeOffset] = io.DeltaTime * 1000.0f;
    _frameTimeOffset = (_frameTimeOffset + 1) % FrameTimeHistoryLength;

    ImGui::SetNextWindowPos({16.0f, 16.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({340.0f, 0.0f}, ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Debug") || !ImGui::BeginTabBar("DebugTabs"))
    {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabItem("Engine"))
    {
        drawFrameTime(io, vsyncEnabled);
        drawPassTimings(graph);
        drawPools(bindless, geometry);

        ImGui::EndTabItem();
    }

    if (extraTabDraw && ImGui::BeginTabItem(extraTabName.c_str()))
    {
        extraTabDraw();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
    ImGui::End();
}

void DebugOverlay::drawFrameTime(const ImGuiIO& io, bool& vsyncEnabled)
{
    const float frameMs = 1000.0f / io.Framerate;

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(frameTimeColor(frameMs), "%.2f ms", frameMs);
    ImGui::SameLine();
    ImGui::TextColored(Dim, "%.0f FPS", io.Framerate);

    constexpr float ButtonWidth = 76.0f;
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ButtonWidth);
    ImGui::PushStyleColor(ImGuiCol_Text, vsyncEnabled ? Accent : Dim);
    if (ImGui::Button(vsyncEnabled ? "VSync on" : "VSync off", {ButtonWidth, 0.0f}))
    {
        vsyncEnabled = !vsyncEnabled;
    }
    ImGui::PopStyleColor();

    // autoscale to spike, with floor so idle graph isnt amplified noise
    const float peak = *std::ranges::max_element(_frameTimesMs);
    ImGui::PlotLines("##FrameTimes",
                     _frameTimesMs.data(),
                     FrameTimeHistoryLength,
                     _frameTimeOffset,
                     nullptr,
                     0.0f,
                     std::max(peak * 1.25f, 4.0f),
                     ImVec2(-FLT_MIN, 44.0f));
}

void DebugOverlay::drawPassTimings(const Graph::RenderGraph& graph)
{
    const std::span<const GPU::GpuProfiler::Scope> timings = graph.timings();
    if (timings.empty())
    {
        return;
    }

    ImGui::SeparatorText("GPU");

    float total = 0.0f;
    float peak = 0.0f;
    for (const GPU::GpuProfiler::Scope& scope : timings)
    {
        total += scope.ms;
        peak = std::max(peak, scope.ms);
    }

    std::array<char, 32> text{};
    for (const GPU::GpuProfiler::Scope& scope : timings)
    {
        snprintf(text.data(), text.size(), "%.3f ms", scope.ms);

        ImGui::TextUnformatted(scope.name.c_str());
        rightAligned(ImGui::GetStyleColorVec4(ImGuiCol_Text), text.data());

        // scale against heaviest pass, not total time
        slimBar(peak > 0.0f ? scope.ms / peak : 0.0f, Accent);
    }

    snprintf(text.data(), text.size(), "%.3f ms", total);
    ImGui::TextColored(Dim, "total");
    rightAligned(Dim, text.data());
}

void DebugOverlay::drawPools(const GPU::BindlessRegistry& bindless,
                             const Asset::GeometryPool& geometry)
{
    ImGui::SeparatorText("Pools");

    const GPU::BindlessRegistry::Stats bindlessStats = bindless.stats();
    poolBar("Textures", bindlessStats.usedTextures, GPU::BindlessRegistry::MaxTextures);
    poolBar("Buffers", bindlessStats.usedBuffers, GPU::BindlessRegistry::MaxBuffers);
    poolBar("Shadow maps",
            bindlessStats.usedShadowTextures,
            GPU::BindlessRegistry::MaxShadowTextures);

    const Asset::GeometryPool::Stats geometryStats = geometry.stats();
    poolBar("Vertices", geometryStats.usedVertices, Asset::GeometryPool::MaxVertices);
    poolBar("Indices", geometryStats.usedIndices, Asset::GeometryPool::MaxIndices);
}

} // namespace App
