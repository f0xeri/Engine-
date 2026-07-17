#pragma once

#include "engine/Platform/Extent2D.hpp"
#include "engine/Platform/Input.hpp"

#include <cstdint>
#include <span>

#include <vulkan/vulkan.hpp>

struct SDL_Window;
union SDL_Event;

namespace Platform
{

struct WindowDesc
{
    const char* title = "Engine";
    uint32_t width = 1280;
    uint32_t height = 720;
};

class Window
{
public:
    explicit Window(const WindowDesc& desc);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Drains the event queue into the input state. Returns false once the user asked to close.
    bool pumpEvents();

    const Input& input() const { return _input; }

    // if enabled, cursor is hidden, deltas are unlimited
    void setRelativeMouseMode(bool enabled);

    Extent2D framebufferSize() const;
    bool minimized() const;
    const char* getTitle() const;

    // ---vulkan related---
    static std::span<const char* const> requiredInstanceExtensions();
    vk::SurfaceKHR createSurface(vk::Instance instance) const;
    // --------------------

private:
    void applyInputEvent(const SDL_Event& event);

    SDL_Window* _window = nullptr;
    bool _shouldClose = false;
    Input _input;
};

} // namespace Platform
