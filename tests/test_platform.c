#include "platform/platform_sdl.h"

#include <math.h>
#include <stdio.h>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            (void)fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #condition);    \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    float x = 0.0F;
    float y = 0.0F;
    char clipboard[8];
    size_t clipboard_size = 0U;
    CHECK(af_platform_scale_from_sizes(1920, 1080, 3840, 2160, &x, &y) == AF_RESULT_SUCCESS);
    CHECK(fabsf(x - 2.0F) < 0.0001F && fabsf(y - 2.0F) < 0.0001F);
    CHECK(af_platform_scale_from_sizes(1280, 720, 1920, 1080, &x, &y) == AF_RESULT_SUCCESS);
    CHECK(fabsf(x - 1.5F) < 0.0001F && fabsf(y - 1.5F) < 0.0001F);
    CHECK(af_platform_scale_from_sizes(0, 720, 1920, 1080, &x, &y) == AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_platform_window_metrics(NULL, NULL) == AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_platform_clipboard_get_text(clipboard, 0U, &clipboard_size) ==
          AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_platform_window_set_fullscreen(NULL, false) == AF_RESULT_INVALID_ARGUMENT);
    return 0;
}
