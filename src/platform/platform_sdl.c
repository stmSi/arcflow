#include "platform/platform_sdl.h"

#include "core/log.h"

AfResult af_platform_window_create(const char *title, int width, int height,
                                   AfPlatformWindow *out_window) {
    if (title == NULL || width <= 0 || height <= 0 || out_window == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_window = (AfPlatformWindow){0};
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        af_log(AF_LOG_ERROR, "SDL initialization failed: %s", SDL_GetError());
        return AF_RESULT_IO_ERROR;
    }
    out_window->sdl_initialized = true;
    out_window->window =
        SDL_CreateWindow(title, width, height,
                         SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (out_window->window == NULL) {
        af_log(AF_LOG_ERROR, "SDL window creation failed: %s", SDL_GetError());
        af_platform_window_destroy(out_window);
        return AF_RESULT_IO_ERROR;
    }
    return AF_RESULT_SUCCESS;
}

void af_platform_window_destroy(AfPlatformWindow *window) {
    if (window == NULL) {
        return;
    }
    if (window->window != NULL) {
        SDL_DestroyWindow(window->window);
    }
    if (window->sdl_initialized) {
        SDL_Quit();
    }
    *window = (AfPlatformWindow){0};
}
