#pragma once

#include "engine/GPU/VulkanContext.hpp"
#include "engine/Platform/Window.hpp"

#include <cstdint>

union SDL_Event;

namespace App
{

class ImGuiLayer
{
public:
    ImGuiLayer(GPU::VulkanContext& ctx,
               Platform::Window& window,
               vk::Format colorFormat,
               uint32_t imageCount);
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    void processEvent(const SDL_Event& event);

    // call before tick() - user code may freely call ImGui:: widget functions inside tick
    void newFrame();

    // call as the final pass over the swapchain image (LoadOp::Load)
    void render(vk::CommandBuffer cmd);
};

} // namespace App
