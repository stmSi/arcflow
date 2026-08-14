#include "platform/platform_sdl.h"

#include "core/log.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

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

AfResult af_platform_scale_from_sizes(int logical_width, int logical_height, int pixel_width,
                                      int pixel_height, float *out_scale_x, float *out_scale_y) {
    if (logical_width <= 0 || logical_height <= 0 || pixel_width <= 0 || pixel_height <= 0 ||
        out_scale_x == NULL || out_scale_y == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_scale_x = (float)pixel_width / (float)logical_width;
    *out_scale_y = (float)pixel_height / (float)logical_height;
    return isfinite(*out_scale_x) && isfinite(*out_scale_y) && *out_scale_x > 0.0F &&
                   *out_scale_y > 0.0F
               ? AF_RESULT_SUCCESS
               : AF_RESULT_INVALID_ARGUMENT;
}

AfResult af_platform_window_metrics(const AfPlatformWindow *window,
                                    AfPlatformWindowMetrics *out_metrics) {
    AfPlatformWindowMetrics metrics = {0};
    if (window == NULL || window->window == NULL || out_metrics == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (!SDL_GetWindowSize(window->window, &metrics.logical_width, &metrics.logical_height) ||
        !SDL_GetWindowSizeInPixels(window->window, &metrics.pixel_width, &metrics.pixel_height) ||
        af_platform_scale_from_sizes(metrics.logical_width, metrics.logical_height,
                                     metrics.pixel_width, metrics.pixel_height,
                                     &metrics.pixel_scale_x,
                                     &metrics.pixel_scale_y) != AF_RESULT_SUCCESS) {
        return AF_RESULT_IO_ERROR;
    }
    metrics.display_scale = SDL_GetWindowDisplayScale(window->window);
    metrics.display_id = SDL_GetDisplayForWindow(window->window);
    metrics.fullscreen = (SDL_GetWindowFlags(window->window) & SDL_WINDOW_FULLSCREEN) != 0U;
    *out_metrics = metrics;
    return AF_RESULT_SUCCESS;
}

AfResult af_platform_query_capabilities(const AfPlatformWindow *window,
                                        AfPlatformCapabilities *out_capabilities) {
    AfPlatformCapabilities capabilities = {0};
    SDL_DisplayID *displays = NULL;
    SDL_KeyboardID *keyboards = NULL;
    SDL_TouchID *touch_devices = NULL;
    const char *platform = NULL;
    const char *driver = NULL;
    int display_count = 0;
    int keyboard_count = 0;
    int touch_count = 0;
    if (window == NULL || window->window == NULL || out_capabilities == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    platform = SDL_GetPlatform();
    driver = SDL_GetCurrentVideoDriver();
    (void)snprintf(capabilities.operating_system, sizeof(capabilities.operating_system), "%s",
                   platform != NULL ? platform : "Unknown");
    (void)snprintf(capabilities.video_driver, sizeof(capabilities.video_driver), "%s",
                   driver != NULL ? driver : "Unknown");
    displays = SDL_GetDisplays(&display_count);
    if (displays == NULL && display_count != 0) {
        return AF_RESULT_IO_ERROR;
    }
    capabilities.display_count = display_count < 0 ? 0U
                                 : (uint32_t)display_count > AF_PLATFORM_MAX_DISPLAYS
                                     ? AF_PLATFORM_MAX_DISPLAYS
                                     : (uint32_t)display_count;
    capabilities.display_list_truncated =
        display_count > 0 && (uint32_t)display_count > AF_PLATFORM_MAX_DISPLAYS;
    for (uint32_t index = 0U; index < capabilities.display_count; ++index) {
        AfPlatformDisplay *display = &capabilities.displays[index];
        const char *name = SDL_GetDisplayName(displays[index]);
        const SDL_PropertiesID properties = SDL_GetDisplayProperties(displays[index]);
        display->id = displays[index];
        (void)snprintf(display->name, sizeof(display->name), "%s",
                       name != NULL ? name : "Unnamed display");
        (void)SDL_GetDisplayBounds(displays[index], &display->bounds);
        (void)SDL_GetDisplayUsableBounds(displays[index], &display->usable_bounds);
        display->content_scale = SDL_GetDisplayContentScale(displays[index]);
        display->hdr_enabled =
            properties != 0U &&
            SDL_GetBooleanProperty(properties, SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN, false);
    }
    SDL_free(displays);
    keyboards = SDL_GetKeyboards(&keyboard_count);
    touch_devices = SDL_GetTouchDevices(&touch_count);
    capabilities.keyboard_count = keyboard_count > 0 ? (uint32_t)keyboard_count : 0U;
    capabilities.touch_device_count = touch_count > 0 ? (uint32_t)touch_count : 0U;
    capabilities.clipboard_supported = true;
    capabilities.text_input_supported = true;
    capabilities.native_file_dialogs_supported = true;
    capabilities.pen_events_supported = true;
    SDL_free(touch_devices);
    SDL_free(keyboards);
    *out_capabilities = capabilities;
    return AF_RESULT_SUCCESS;
}

AfResult af_platform_window_point_to_pixels(const AfPlatformWindow *window, float *x, float *y) {
    AfPlatformWindowMetrics metrics = {0};
    if (x == NULL || y == NULL ||
        af_platform_window_metrics(window, &metrics) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *x *= metrics.pixel_scale_x;
    *y *= metrics.pixel_scale_y;
    return AF_RESULT_SUCCESS;
}

AfResult af_platform_window_set_fullscreen(AfPlatformWindow *window, bool fullscreen) {
    if (window == NULL || window->window == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return SDL_SetWindowFullscreen(window->window, fullscreen) ? AF_RESULT_SUCCESS
                                                               : AF_RESULT_IO_ERROR;
}

AfResult af_platform_window_toggle_fullscreen(AfPlatformWindow *window, bool *out_fullscreen) {
    bool fullscreen = false;
    AfResult result = AF_RESULT_SUCCESS;
    if (window == NULL || window->window == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    fullscreen = (SDL_GetWindowFlags(window->window) & SDL_WINDOW_FULLSCREEN) != 0U;
    result = af_platform_window_set_fullscreen(window, !fullscreen);
    if (result == AF_RESULT_SUCCESS && out_fullscreen != NULL) {
        *out_fullscreen = !fullscreen;
    }
    return result;
}

AfResult af_platform_clipboard_set_text(const char *text) {
    if (text == NULL || strlen(text) >= AF_PLATFORM_CLIPBOARD_CAPACITY) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return SDL_SetClipboardText(text) ? AF_RESULT_SUCCESS : AF_RESULT_IO_ERROR;
}

AfResult af_platform_clipboard_get_text(char *output, size_t capacity, size_t *out_size) {
    char *text = NULL;
    size_t length = 0U;
    if (output == NULL || capacity == 0U || out_size == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_size = 0U;
    output[0] = '\0';
    text = SDL_GetClipboardText();
    if (text == NULL) {
        return AF_RESULT_IO_ERROR;
    }
    length = strlen(text);
    if (length >= capacity || length >= AF_PLATFORM_CLIPBOARD_CAPACITY) {
        SDL_free(text);
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memcpy(output, text, length + 1U);
    *out_size = length;
    SDL_free(text);
    return AF_RESULT_SUCCESS;
}

AfResult af_platform_text_input_set(AfPlatformWindow *window, bool enabled) {
    if (window == NULL || window->window == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return (enabled ? SDL_StartTextInput(window->window) : SDL_StopTextInput(window->window))
               ? AF_RESULT_SUCCESS
               : AF_RESULT_IO_ERROR;
}

AfResult af_platform_resource_path(const char *relative_path, char *output, size_t capacity) {
    const char *base_path = SDL_GetBasePath();
    int written = 0;
    if (relative_path == NULL || relative_path[0] == '\0' || output == NULL || capacity == 0U ||
        base_path == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (relative_path[0] == '/' || relative_path[0] == '\\' ||
        strstr(relative_path, "..") != NULL || strchr(relative_path, ':') != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    written = snprintf(output, capacity, "%s%s", base_path, relative_path);
    return written >= 0 && (size_t)written < capacity ? AF_RESULT_SUCCESS
                                                      : AF_RESULT_LIMIT_EXCEEDED;
}

AfResult af_platform_show_open_file_dialog(AfPlatformWindow *window,
                                           SDL_DialogFileCallback callback, void *userdata,
                                           const SDL_DialogFileFilter *filters, int filter_count,
                                           const char *default_location, bool allow_many) {
    if (window == NULL || window->window == NULL || callback == NULL || filter_count < 0 ||
        (filter_count != 0 && filters == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    SDL_ShowOpenFileDialog(callback, userdata, window->window, filters, filter_count,
                           default_location, allow_many);
    return AF_RESULT_SUCCESS;
}

AfResult af_platform_show_save_file_dialog(AfPlatformWindow *window,
                                           SDL_DialogFileCallback callback, void *userdata,
                                           const SDL_DialogFileFilter *filters, int filter_count,
                                           const char *default_location) {
    if (window == NULL || window->window == NULL || callback == NULL || filter_count < 0 ||
        (filter_count != 0 && filters == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    SDL_ShowSaveFileDialog(callback, userdata, window->window, filters, filter_count,
                           default_location);
    return AF_RESULT_SUCCESS;
}
