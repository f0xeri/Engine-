#include "engine/Platform/Window.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_vulkan.h>

#include "engine/Core/Log.hpp"

namespace Platform
{

static_assert(static_cast<int>(Key::A) == SDL_SCANCODE_A);
static_assert(static_cast<int>(Key::Z) == SDL_SCANCODE_Z);
static_assert(static_cast<int>(Key::Num1) == SDL_SCANCODE_1);
static_assert(static_cast<int>(Key::Num0) == SDL_SCANCODE_0);
static_assert(static_cast<int>(Key::Enter) == SDL_SCANCODE_RETURN);
static_assert(static_cast<int>(Key::Escape) == SDL_SCANCODE_ESCAPE);
static_assert(static_cast<int>(Key::Space) == SDL_SCANCODE_SPACE);
static_assert(static_cast<int>(Key::Slash) == SDL_SCANCODE_SLASH);
static_assert(static_cast<int>(Key::F1) == SDL_SCANCODE_F1);
static_assert(static_cast<int>(Key::F12) == SDL_SCANCODE_F12);
static_assert(static_cast<int>(Key::PrintScreen) == SDL_SCANCODE_PRINTSCREEN);
static_assert(static_cast<int>(Key::Up) == SDL_SCANCODE_UP);
static_assert(static_cast<int>(Key::LCtrl) == SDL_SCANCODE_LCTRL);
static_assert(static_cast<int>(Key::RGui) == SDL_SCANCODE_RGUI);

static_assert(static_cast<int>(MouseButton::Left) == SDL_BUTTON_LEFT);
static_assert(static_cast<int>(MouseButton::Middle) == SDL_BUTTON_MIDDLE);
static_assert(static_cast<int>(MouseButton::Right) == SDL_BUTTON_RIGHT);
static_assert(static_cast<int>(MouseButton::X1) == SDL_BUTTON_X1);
static_assert(static_cast<int>(MouseButton::X2) == SDL_BUTTON_X2);

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

bool Window::pumpEvents(const std::function<void(const SDL_Event&)>& onEvent)
{
    _input.beginFrame();

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (onEvent)
        {
            onEvent(event);
        }
        if (event.type == SDL_EVENT_QUIT)
        {
            _shouldClose = true;
            continue;
        }
        applyInputEvent(event);
    }
    return !_shouldClose;
}

void Window::applyInputEvent(const SDL_Event& event)
{
    switch (event.type)
    {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            if (event.key.scancode < _input._keys.size())
            {
                _input._keys[event.key.scancode] = event.key.down;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button < _input._buttons.size())
            {
                _input._buttons[event.button.button] = event.button.down;
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            _input._mousePosition = {event.motion.x, event.motion.y};
            _input._mouseDelta += glm::vec2(event.motion.xrel, event.motion.yrel);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            _input._wheelDelta += event.wheel.y;
            break;
        default:
            break;
    }
}

void Window::setRelativeMouseMode(bool enabled)
{
    SDL_SetWindowRelativeMouseMode(_window, enabled);
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
