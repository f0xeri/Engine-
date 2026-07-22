add_library(engine STATIC
    Core/Env.cpp
    Core/FileWatcher/FileWatcher.cpp
    Core/FileWatcher/FileWatcherWindows.cpp
    Core/Log.cpp
    Platform/Window.cpp
    Platform/Input.cpp
    GPU/VulkanContext.cpp
    GPU/Swapchain.cpp
    GPU/FrameContext.cpp
    GPU/BindlessRegistry.cpp
    GPU/PipelineFactory.cpp
    GPU/UploadContext.cpp
    GPU/FrameUniforms.cpp
    GPU/GpuProfiler.cpp
    Asset/GeometryPool.cpp
    Asset/Library.cpp
    Shader/ShaderSystem.cpp
    RenderGraph/RenderGraph.cpp
    RenderGraph/CmdRecorder.cpp
    Renderer/PipelineRegistry.cpp
    App/Application.cpp
    App/FlyCamera.cpp
    App/ImGuiLayer.cpp
    App/DebugOverlay.cpp
)

target_include_directories(engine PUBLIC ${CMAKE_SOURCE_DIR})

target_compile_definitions(engine PUBLIC
    VK_NO_PROTOTYPES
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)

target_link_libraries(engine
    PUBLIC
        Vulkan::Headers
        glm::glm
        GPUOpen::VulkanMemoryAllocator
        SDL3::SDL3
    PRIVATE
        slang::slang
        single_headers
        engine_warnings
        imgui::imgui
)
