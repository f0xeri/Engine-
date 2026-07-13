add_library(engine STATIC
    core/env.cpp
    core/log.cpp
    platform/window.cpp
    gpu/vulkan.cpp
    gpu/swapchain.cpp
    gpu/frame.cpp
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
        engine_warnings
)
