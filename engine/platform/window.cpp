#include "engine/platform/window.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "engine/core/log.hpp"

namespace Platform
{

Window::Window(const WindowDesc& desc)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        LOG_ERROR(Platform, "SDL_Init failed: {}", SDL_GetError());
        std::abort();
    }

    _window = SDL_CreateWindow(desc.title,
                               static_cast<int>(desc.width),
                               static_cast<int>(desc.height),
                               SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!_window)
    {
        LOG_ERROR(Platform, "SDL_CreateWindow failed: {}", SDL_GetError());
        std::abort();
    }

    LOG_INFO(Platform, "window created: {}x{}", desc.width, desc.height);
}

Window::~Window()
{
    SDL_DestroyWindow(_window);
    SDL_Quit();
}

bool Window::pumpEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_EVENT_QUIT:
                _shouldClose = true;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE)
                {
                    _shouldClose = true;
                }
                break;
            default:
                break;
        }
    }
    return !_shouldClose;
}

Extent2D Window::framebufferSize() const
{
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(_window, &width, &height);
    return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

bool Window::minimized() const
{
    return (SDL_GetWindowFlags(_window) & SDL_WINDOW_MINIMIZED) != 0;
}

const char* Window::getTitle() const
{
    return SDL_GetWindowTitle(_window);
}

std::span<const char* const> Window::requiredInstanceExtensions()
{
    uint32_t count = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&count);
    if (!extensions)
    {
        LOG_ERROR(Platform, "SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
        std::abort();
    }
    return {extensions, count};
}

vk::SurfaceKHR Window::createSurface(vk::Instance instance) const
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(_window, instance, nullptr, &surface))
    {
        LOG_ERROR(Platform, "SDL_Vulkan_CreateSurface failed: {}", SDL_GetError());
        std::abort();
    }
    return surface;
}

} // namespace Platform
