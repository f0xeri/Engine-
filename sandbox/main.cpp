#include "engine/gpu/vulkan.hpp"
#include "engine/platform/window.hpp"

int main(int, char**)
{
    Platform::Window window({.title = "Engine sandbox"});
    GPU::VulkanContext vulkan(window);

    while (window.pumpEvents())
    {
    }

    return 0;
}
