#ifndef AF_PLATFORM_SDL_H
#define AF_PLATFORM_SDL_H

#include "arcflow/arcflow_result.h"

#include <SDL3/SDL.h>
#include <stdbool.h>

typedef struct AfPlatformWindow {
    SDL_Window *window;
    bool sdl_initialized;
} AfPlatformWindow;

AfResult af_platform_window_create(const char *title, int width, int height,
                                   AfPlatformWindow *out_window);
void af_platform_window_destroy(AfPlatformWindow *window);

#endif
