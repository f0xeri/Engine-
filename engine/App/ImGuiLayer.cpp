#include "engine/App/ImGuiLayer.hpp"

#include "engine/Core/Log.hpp"

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

namespace App
{

namespace
{

constexpr ImVec4 Bg{0.07f, 0.08f, 0.09f, 0.94f};
constexpr ImVec4 Panel{0.11f, 0.12f, 0.14f, 1.00f};
constexpr ImVec4 PanelHover{0.16f, 0.18f, 0.21f, 1.00f};
constexpr ImVec4 PanelActive{0.20f, 0.23f, 0.27f, 1.00f};
constexpr ImVec4 Border{0.22f, 0.24f, 0.28f, 1.00f};
constexpr ImVec4 Text{0.86f, 0.88f, 0.90f, 1.00f};
constexpr ImVec4 TextDim{0.48f, 0.52f, 0.57f, 1.00f};
constexpr ImVec4 Accent{0.20f, 0.68f, 0.78f, 1.00f};
constexpr ImVec4 AccentHover{0.28f, 0.80f, 0.90f, 1.00f};
constexpr ImVec4 AccentDim{0.20f, 0.68f, 0.78f, 0.35f};
constexpr ImVec4 Transparent{0.00f, 0.00f, 0.00f, 0.00f};

void loadFont()
{
    constexpr const char* Candidates[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    };

    ImGuiIO& io = ImGui::GetIO();
    for (const char* path : Candidates)
    {
        if (io.Fonts->AddFontFromFileTTF(path, 16.0f) != nullptr)
        {
            return;
        }
    }

    LOG_WARN(::Core::LogModule::App, "no system font found, falling back to the built-in font");
    io.Fonts->AddFontDefault();
}

void applyTheme()
{
    loadFont();

    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowPadding = {12.0f, 10.0f};
    style.FramePadding = {8.0f, 4.0f};
    style.CellPadding = {6.0f, 3.0f};
    style.ItemSpacing = {8.0f, 6.0f};
    style.ItemInnerSpacing = {6.0f, 4.0f};
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;

    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;

    style.WindowRounding = 6.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 5.0f;
    style.ScrollbarRounding = 8.0f;

    style.WindowTitleAlign = {0.0f, 0.5f};
    style.SeparatorTextBorderSize = 1.0f;
    style.SeparatorTextPadding = {0.0f, 6.0f};

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = Text;
    c[ImGuiCol_TextDisabled] = TextDim;
    c[ImGuiCol_WindowBg] = Bg;
    c[ImGuiCol_ChildBg] = Transparent;
    c[ImGuiCol_PopupBg] = Bg;
    c[ImGuiCol_Border] = Border;
    c[ImGuiCol_BorderShadow] = Transparent;

    c[ImGuiCol_FrameBg] = Panel;
    c[ImGuiCol_FrameBgHovered] = PanelHover;
    c[ImGuiCol_FrameBgActive] = PanelActive;

    c[ImGuiCol_TitleBg] = Panel;
    c[ImGuiCol_TitleBgActive] = Panel;
    c[ImGuiCol_TitleBgCollapsed] = Panel;
    c[ImGuiCol_MenuBarBg] = Panel;

    c[ImGuiCol_ScrollbarBg] = Transparent;
    c[ImGuiCol_ScrollbarGrab] = PanelActive;
    c[ImGuiCol_ScrollbarGrabHovered] = Border;
    c[ImGuiCol_ScrollbarGrabActive] = Accent;

    c[ImGuiCol_CheckMark] = Accent;
    c[ImGuiCol_SliderGrab] = Accent;
    c[ImGuiCol_SliderGrabActive] = AccentHover;

    c[ImGuiCol_Button] = Panel;
    c[ImGuiCol_ButtonHovered] = PanelHover;
    c[ImGuiCol_ButtonActive] = PanelActive;

    c[ImGuiCol_Header] = AccentDim;
    c[ImGuiCol_HeaderHovered] = PanelHover;
    c[ImGuiCol_HeaderActive] = PanelActive;

    c[ImGuiCol_Separator] = Border;
    c[ImGuiCol_SeparatorHovered] = Accent;
    c[ImGuiCol_SeparatorActive] = AccentHover;

    c[ImGuiCol_ResizeGrip] = Transparent;
    c[ImGuiCol_ResizeGripHovered] = AccentDim;
    c[ImGuiCol_ResizeGripActive] = Accent;

    c[ImGuiCol_Tab] = Transparent;
    c[ImGuiCol_TabHovered] = PanelHover;
    c[ImGuiCol_TabSelected] = Panel;
    c[ImGuiCol_TabSelectedOverline] = Accent;
    c[ImGuiCol_TabDimmed] = Transparent;
    c[ImGuiCol_TabDimmedSelected] = Panel;

    c[ImGuiCol_PlotLines] = Accent;
    c[ImGuiCol_PlotLinesHovered] = AccentHover;
    c[ImGuiCol_PlotHistogram] = Accent;
    c[ImGuiCol_PlotHistogramHovered] = AccentHover;

    c[ImGuiCol_TableHeaderBg] = Panel;
    c[ImGuiCol_TableBorderStrong] = Border;
    c[ImGuiCol_TableBorderLight] = Panel;
    c[ImGuiCol_TableRowBg] = Transparent;
    c[ImGuiCol_TableRowBgAlt] = {1.0f, 1.0f, 1.0f, 0.02f};

    c[ImGuiCol_TextSelectedBg] = AccentDim;
    c[ImGuiCol_NavCursor] = Accent;
}

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
    applyTheme();

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
