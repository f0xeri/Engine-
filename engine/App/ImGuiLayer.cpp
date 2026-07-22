#include "engine/App/ImGuiLayer.hpp"

#include "engine/Core/Log.hpp"

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

namespace App
{

namespace
{

PFN_vkVoidFunction loadVulkanFunction(const char* name, void* userData)
{
    const auto* instance = static_cast<const vk::Instance*>(userData);
    return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(*instance, name);
}

void checkVkResult(VkResult err)
{
    if (err != VK_SUCCESS)
    {
        LOG_ERROR(Vulkan, "ImGui Vulkan backend error: {}", vk::to_string(vk::Result(err)));
    }
}

} // namespace

ImGuiLayer::ImGuiLayer(GPU::VulkanContext& ctx,
                       Platform::Window& window,
                       vk::Format colorFormat,
                       uint32_t imageCount)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForVulkan(window.nativeHandle());

    ImGui_ImplVulkan_LoadFunctions(
        VK_API_VERSION_1_3, loadVulkanFunction, static_cast<void*>(&ctx.instance));

    const vk::PipelineRenderingCreateInfo renderingInfo(0, colorFormat);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = ctx.instance;
    initInfo.PhysicalDevice = ctx.physicalDevice;
    initInfo.Device = ctx.device;
    initInfo.QueueFamily = ctx.graphicsQueueFamily;
    initInfo.Queue = ctx.graphicsQueue;
    initInfo.DescriptorPoolSize = 16;
    initInfo.MinImageCount = imageCount;
    initInfo.ImageCount = imageCount;
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo =
        static_cast<VkPipelineRenderingCreateInfo>(renderingInfo);
    initInfo.CheckVkResultFn = checkVkResult;

    ImGui_ImplVulkan_Init(&initInfo);
}

ImGuiLayer::~ImGuiLayer()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiLayer::processEvent(const SDL_Event& event)
{
    ImGui_ImplSDL3_ProcessEvent(&event);
}

void ImGuiLayer::newFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::render(vk::CommandBuffer cmd)
{
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

} // namespace App
