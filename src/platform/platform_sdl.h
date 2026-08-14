#ifndef AF_PLATFORM_SDL_H
#define AF_PLATFORM_SDL_H

#include "arcflow/arcflow_result.h"

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AF_PLATFORM_MAX_DISPLAYS 16U
#define AF_PLATFORM_NAME_CAPACITY 96U
#define AF_PLATFORM_CLIPBOARD_CAPACITY (1024U * 1024U)

typedef struct AfPlatformDisplay {
    uint32_t id;
    char name[AF_PLATFORM_NAME_CAPACITY];
    SDL_Rect bounds;
    SDL_Rect usable_bounds;
    float content_scale;
    bool hdr_enabled;
} AfPlatformDisplay;

typedef struct AfPlatformCapabilities {
    char operating_system[AF_PLATFORM_NAME_CAPACITY];
    char video_driver[AF_PLATFORM_NAME_CAPACITY];
    AfPlatformDisplay displays[AF_PLATFORM_MAX_DISPLAYS];
    uint32_t display_count;
    uint32_t keyboard_count;
    uint32_t touch_device_count;
    bool display_list_truncated;
    bool clipboard_supported;
    bool text_input_supported;
    bool native_file_dialogs_supported;
    bool pen_events_supported;
} AfPlatformCapabilities;

typedef struct AfPlatformWindowMetrics {
    int logical_width;
    int logical_height;
    int pixel_width;
    int pixel_height;
    float pixel_scale_x;
    float pixel_scale_y;
    float display_scale;
    uint32_t display_id;
    bool fullscreen;
} AfPlatformWindowMetrics;

typedef struct AfPlatformWindow {
    SDL_Window *window;
    bool sdl_initialized;
} AfPlatformWindow;

AfResult af_platform_window_create(const char *title, int width, int height,
                                   AfPlatformWindow *out_window);
void af_platform_window_destroy(AfPlatformWindow *window);
AfResult af_platform_window_metrics(const AfPlatformWindow *window,
                                    AfPlatformWindowMetrics *out_metrics);
AfResult af_platform_query_capabilities(const AfPlatformWindow *window,
                                        AfPlatformCapabilities *out_capabilities);
AfResult af_platform_scale_from_sizes(int logical_width, int logical_height, int pixel_width,
                                      int pixel_height, float *out_scale_x, float *out_scale_y);
AfResult af_platform_window_point_to_pixels(const AfPlatformWindow *window, float *x, float *y);
AfResult af_platform_window_set_fullscreen(AfPlatformWindow *window, bool fullscreen);
AfResult af_platform_window_toggle_fullscreen(AfPlatformWindow *window, bool *out_fullscreen);
AfResult af_platform_clipboard_set_text(const char *text);
AfResult af_platform_clipboard_get_text(char *output, size_t capacity, size_t *out_size);
AfResult af_platform_text_input_set(AfPlatformWindow *window, bool enabled);
AfResult af_platform_resource_path(const char *relative_path, char *output, size_t capacity);
AfResult af_platform_show_open_file_dialog(AfPlatformWindow *window,
                                           SDL_DialogFileCallback callback, void *userdata,
                                           const SDL_DialogFileFilter *filters, int filter_count,
                                           const char *default_location, bool allow_many);
AfResult af_platform_show_save_file_dialog(AfPlatformWindow *window,
                                           SDL_DialogFileCallback callback, void *userdata,
                                           const SDL_DialogFileFilter *filters, int filter_count,
                                           const char *default_location);

#endif
