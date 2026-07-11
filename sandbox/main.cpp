#include <SDL3/SDL.h>

#include "engine/core/log.hpp"
#include "engine/gpu/vulkan.hpp"

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("sandbox", "SDL_Init failed: {}", SDL_GetError());
        return 1;
    }

    const uint32_t apiVersion = gpu::initVulkanLoader();
    LOG_INFO("sandbox", "Vulkan loader: instance API {}.{}.{}",
                  VK_API_VERSION_MAJOR(apiVersion),
                  VK_API_VERSION_MINOR(apiVersion),
                  VK_API_VERSION_PATCH(apiVersion));

    SDL_Window* window =
        SDL_CreateWindow("Engine sandbox", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        LOG_ERROR("sandbox", "SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
                running = false;
            }
        }
        SDL_Delay(16);
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
