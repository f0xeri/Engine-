#include "engine/App/DebugOverlay.hpp"

#include <imgui.h>

namespace App
{

void DebugOverlay::draw(const GPU::BindlessRegistry& bindless,
                        const Asset::GeometryPool& geometry,
                        const Renderer::PipelineRegistry& pipelines,
                        bool& vsyncEnabled,
                        const std::string& extraTabName,
                        const std::function<void()>& extraTabDraw)
{
    const ImGuiIO& io = ImGui::GetIO();
    _frameTimesMs[_frameTimeOffset] = io.DeltaTime * 1000.0f;
    _frameTimeOffset = (_frameTimeOffset + 1) % FrameTimeHistoryLength;

    if (!ImGui::Begin("Debug") || !ImGui::BeginTabBar("DebugTabs"))
    {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabItem("Engine"))
    {
        ImGui::Text("%.2f ms (%.0f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::PlotLines("##FrameTimes",
                         _frameTimesMs.data(),
                         FrameTimeHistoryLength,
                         _frameTimeOffset,
                         nullptr,
                         0.0f,
                         33.3f,
                         ImVec2(0, 40));

        if (ImGui::Button(vsyncEnabled ? "VSync: On" : "VSync: Off"))
        {
            vsyncEnabled = !vsyncEnabled;
        }

        ImGui::Separator();
        const GPU::BindlessRegistry::Stats bindlessStats = bindless.stats();
        ImGui::Text("Bindless textures: %u / %u",
                    bindlessStats.usedTextures,
                    GPU::BindlessRegistry::MaxTextures);
        ImGui::Text("Bindless buffers: %u / %u",
                    bindlessStats.usedBuffers,
                    GPU::BindlessRegistry::MaxBuffers);

        const Asset::GeometryPool::Stats geometryStats = geometry.stats();
        ImGui::Text(
            "Vertices: %u / %u", geometryStats.usedVertices, Asset::GeometryPool::MaxVertices);
        ImGui::Text("Indices: %u / %u", geometryStats.usedIndices, Asset::GeometryPool::MaxIndices);

        ImGui::Separator();
        ImGui::Text("Pipelines:");
        for (const std::string& module : pipelines.loadedModules())
        {
            ImGui::BulletText("%s", module.c_str());
        }

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

} // namespace App
