#pragma once

#include <cstdint>
#include <span>

#include <vulkan/vulkan.hpp>

struct SDL_Window;

namespace Platform
{

struct WindowDesc
{
    const char* title = "Engine";
    uint32_t width = 1280;
    uint32_t height = 720;
};

struct Extent2D
{
    uint32_t width = 0;
    uint32_t height = 0;
};

class Window
{
public:
    explicit Window(const WindowDesc& desc);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Drains the event queue. Returns false once the user asked to close.
    bool pumpEvents();

    Extent2D framebufferSize() const;
    bool minimized() const;
    const char* getTitle() const;

    // ---vulkan related---
    static std::span<const char* const> requiredInstanceExtensions();
    vk::SurfaceKHR createSurface(vk::Instance instance) const;
    // --------------------

private:
    SDL_Window* _window = nullptr;
    bool _shouldClose = false;
};

} // namespace Platform
