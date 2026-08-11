#include "project/image_import.h"

#include "core/safe_math.h"

#include <ctype.h>
#include <math.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

#define AF_SVG_MAX_ELEMENTS 1024U
#define AF_SVG_MAX_POINTS 4096U
#define AF_SVG_MAX_RASTER_WORK (UINT64_C(512) * 1024U * 1024U)

typedef struct AfSvgPoint {
    float x;
    float y;
    bool move;
} AfSvgPoint;

typedef struct AfSvgView {
    float x;
    float y;
    float width;
    float height;
    uint32_t pixel_width;
    uint32_t pixel_height;
} AfSvgView;

static bool af_svg_contains_case_insensitive(const char *text, const char *needle) {
    const size_t needle_length = strlen(needle);
    if (needle_length == 0U) {
        return true;
    }
    for (size_t index = 0U; text[index] != '\0'; ++index) {
        size_t match = 0U;
        while (match < needle_length && text[index + match] != '\0' &&
               tolower((unsigned char)text[index + match]) ==
                   tolower((unsigned char)needle[match])) {
            ++match;
        }
        if (match == needle_length) {
            return true;
        }
    }
    return false;
}

static const char *af_svg_skip_space(const char *cursor) {
    while (*cursor != '\0' && (isspace((unsigned char)*cursor) || *cursor == ',')) {
        ++cursor;
    }
    return cursor;
}

static bool af_svg_number(const char **cursor, float *out_value) {
    char *end = NULL;
    double value = 0.0;
    const char *start = af_svg_skip_space(*cursor);
    if (*start == '\0') {
        return false;
    }
    value = strtod(start, &end);
    if (end == start || !isfinite(value) || value < -1000000.0 || value > 1000000.0) {
        return false;
    }
    *cursor = end;
    *out_value = (float)value;
    return true;
}

static bool af_svg_attribute(const char *tag, const char *end, const char *name, char *output,
                             size_t capacity) {
    const size_t name_length = strlen(name);
    const char *cursor = tag;
    while (cursor < end) {
        cursor = strstr(cursor, name);
        if (cursor == NULL || cursor >= end) {
            return false;
        }
        if ((cursor == tag || isspace((unsigned char)cursor[-1])) && cursor + name_length < end &&
            (isspace((unsigned char)cursor[name_length]) || cursor[name_length] == '=')) {
            const char *value = cursor + name_length;
            char quote = '\0';
            while (value < end && isspace((unsigned char)*value)) {
                ++value;
            }
            if (value >= end || *value++ != '=') {
                ++cursor;
                continue;
            }
            while (value < end && isspace((unsigned char)*value)) {
                ++value;
            }
            if (value >= end || (*value != '\'' && *value != '"')) {
                return false;
            }
            quote = *value++;
            {
                const char *finish = memchr(value, quote, (size_t)(end - value));
                const size_t length = finish == NULL ? 0U : (size_t)(finish - value);
                if (finish == NULL || length == 0U || length >= capacity) {
                    return false;
                }
                memcpy(output, value, length);
                output[length] = '\0';
                return true;
            }
        }
        ++cursor;
    }
    return false;
}

static bool af_svg_attribute_float(const char *tag, const char *end, const char *name,
                                   float *out_value) {
    char text[64];
    const char *cursor = text;
    if (!af_svg_attribute(tag, end, name, text, sizeof(text)) || strchr(text, '%') != NULL ||
        !af_svg_number(&cursor, out_value) || *af_svg_skip_space(cursor) != '\0') {
        return false;
    }
    return true;
}

static bool af_svg_hex_digit(char value, uint8_t *out) {
    if (value >= '0' && value <= '9') {
        *out = (uint8_t)(value - '0');
    } else if (value >= 'a' && value <= 'f') {
        *out = (uint8_t)(value - 'a' + 10);
    } else if (value >= 'A' && value <= 'F') {
        *out = (uint8_t)(value - 'A' + 10);
    } else {
        return false;
    }
    return true;
}

static bool af_svg_color(const char *tag, const char *end, uint8_t color[4]) {
    char fill[32] = "#000000";
    char opacity_text[32];
    float opacity = 1.0F;
    size_t length = 0U;
    (void)af_svg_attribute(tag, end, "fill", fill, sizeof(fill));
    if (strcmp(fill, "none") == 0) {
        color[3] = 0U;
        return true;
    }
    if (af_svg_attribute(tag, end, "fill-opacity", opacity_text, sizeof(opacity_text))) {
        const char *cursor = opacity_text;
        if (!af_svg_number(&cursor, &opacity) || *af_svg_skip_space(cursor) != '\0' ||
            opacity < 0.0F || opacity > 1.0F) {
            return false;
        }
    }
    length = strlen(fill);
    if (fill[0] != '#' || (length != 4U && length != 7U && length != 9U)) {
        return false;
    }
    if (length == 4U) {
        for (uint32_t index = 0U; index < 3U; ++index) {
            uint8_t digit = 0U;
            if (!af_svg_hex_digit(fill[index + 1U], &digit)) {
                return false;
            }
            color[index] = (uint8_t)(digit * 17U);
        }
        color[3] = 255U;
    } else {
        const uint32_t channels = length == 9U ? 4U : 3U;
        for (uint32_t index = 0U; index < channels; ++index) {
            uint8_t high = 0U;
            uint8_t low = 0U;
            if (!af_svg_hex_digit(fill[index * 2U + 1U], &high) ||
                !af_svg_hex_digit(fill[index * 2U + 2U], &low)) {
                return false;
            }
            color[index] = (uint8_t)(high * 16U + low);
        }
        color[3] = channels == 4U ? color[3] : 255U;
    }
    color[3] = (uint8_t)lroundf((float)color[3] * opacity);
    return true;
}

static void af_svg_blend(uint8_t *target, const uint8_t source[4]) {
    const uint32_t alpha = source[3];
    const uint32_t inverse = 255U - alpha;
    const uint32_t target_alpha = target[3];
    const uint32_t output_alpha = alpha + (target_alpha * inverse + 127U) / 255U;
    if (output_alpha == 0U) {
        memset(target, 0, 4U);
        return;
    }
    for (uint32_t channel = 0U; channel < 3U; ++channel) {
        const uint32_t premultiplied =
            source[channel] * alpha + (target[channel] * target_alpha * inverse + 127U) / 255U;
        target[channel] = (uint8_t)((premultiplied + output_alpha / 2U) / output_alpha);
    }
    target[3] = (uint8_t)output_alpha;
}

static float af_svg_pixel_x(const AfSvgView *view, uint32_t x) {
    return view->x + ((float)x + 0.5F) * view->width / (float)view->pixel_width;
}

static float af_svg_pixel_y(const AfSvgView *view, uint32_t y) {
    return view->y + ((float)y + 0.5F) * view->height / (float)view->pixel_height;
}

static void af_svg_raster_rect(uint8_t *pixels, const AfSvgView *view, float x, float y,
                               float width, float height, const uint8_t color[4]) {
    for (uint32_t py = 0U; py < view->pixel_height; ++py) {
        const float sample_y = af_svg_pixel_y(view, py);
        if (sample_y < y || sample_y >= y + height) {
            continue;
        }
        for (uint32_t px = 0U; px < view->pixel_width; ++px) {
            const float sample_x = af_svg_pixel_x(view, px);
            if (sample_x >= x && sample_x < x + width) {
                af_svg_blend(&pixels[((size_t)py * view->pixel_width + px) * 4U], color);
            }
        }
    }
}

static void af_svg_raster_ellipse(uint8_t *pixels, const AfSvgView *view, float cx, float cy,
                                  float rx, float ry, const uint8_t color[4]) {
    for (uint32_t py = 0U; py < view->pixel_height; ++py) {
        const float dy = (af_svg_pixel_y(view, py) - cy) / ry;
        for (uint32_t px = 0U; px < view->pixel_width; ++px) {
            const float dx = (af_svg_pixel_x(view, px) - cx) / rx;
            if (dx * dx + dy * dy <= 1.0F) {
                af_svg_blend(&pixels[((size_t)py * view->pixel_width + px) * 4U], color);
            }
        }
    }
}

static bool af_svg_path_points(const char *data, AfSvgPoint points[AF_SVG_MAX_POINTS],
                               uint32_t *out_count) {
    const char *cursor = data;
    char command = '\0';
    float x = 0.0F;
    float y = 0.0F;
    float start_x = 0.0F;
    float start_y = 0.0F;
    uint32_t count = 0U;
    while (*(cursor = af_svg_skip_space(cursor)) != '\0') {
        if (isalpha((unsigned char)*cursor)) {
            command = *cursor++;
            if (command == 'Z' || command == 'z') {
                x = start_x;
                y = start_y;
                continue;
            }
        }
        if (command == '\0' || count >= AF_SVG_MAX_POINTS) {
            return false;
        }
        if (command == 'M' || command == 'm' || command == 'L' || command == 'l') {
            float px = 0.0F;
            float py = 0.0F;
            const bool relative = command == 'm' || command == 'l';
            if (!af_svg_number(&cursor, &px) || !af_svg_number(&cursor, &py)) {
                return false;
            }
            if (relative) {
                px += x;
                py += y;
            }
            points[count++] =
                (AfSvgPoint){.x = px, .y = py, .move = command == 'M' || command == 'm'};
            x = px;
            y = py;
            if (command == 'M' || command == 'm') {
                start_x = x;
                start_y = y;
                command = command == 'M' ? 'L' : 'l';
            }
        } else if (command == 'H' || command == 'h' || command == 'V' || command == 'v') {
            float value = 0.0F;
            if (!af_svg_number(&cursor, &value)) {
                return false;
            }
            if (command == 'H' || command == 'h') {
                x = command == 'h' ? x + value : value;
            } else {
                y = command == 'v' ? y + value : value;
            }
            points[count++] = (AfSvgPoint){.x = x, .y = y};
        } else if (command == 'C' || command == 'c') {
            float values[6] = {0};
            const float origin_x = x;
            const float origin_y = y;
            const bool relative = command == 'c';
            for (uint32_t index = 0U; index < 6U; ++index) {
                if (!af_svg_number(&cursor, &values[index])) {
                    return false;
                }
            }
            if (relative) {
                values[0] += origin_x;
                values[1] += origin_y;
                values[2] += origin_x;
                values[3] += origin_y;
                values[4] += origin_x;
                values[5] += origin_y;
            }
            for (uint32_t step = 1U; step <= 16U; ++step) {
                const float t = (float)step / 16.0F;
                const float inverse = 1.0F - t;
                if (count >= AF_SVG_MAX_POINTS) {
                    return false;
                }
                points[count++] =
                    (AfSvgPoint){.x = inverse * inverse * inverse * origin_x +
                                      3.0F * inverse * inverse * t * values[0] +
                                      3.0F * inverse * t * t * values[2] + t * t * t * values[4],
                                 .y = inverse * inverse * inverse * origin_y +
                                      3.0F * inverse * inverse * t * values[1] +
                                      3.0F * inverse * t * t * values[3] + t * t * t * values[5]};
            }
            x = values[4];
            y = values[5];
        } else {
            return false;
        }
    }
    *out_count = count;
    return count >= 3U;
}

static bool af_svg_path_contains(const AfSvgPoint *points, uint32_t count, float x, float y) {
    bool inside = false;
    uint32_t contour_start = 0U;
    for (uint32_t index = 0U; index <= count; ++index) {
        if (index == count || (index > contour_start && points[index].move)) {
            uint32_t previous = index - 1U;
            for (uint32_t current = contour_start; current < index; ++current) {
                const AfSvgPoint a = points[current];
                const AfSvgPoint b = points[previous];
                if (((a.y > y) != (b.y > y)) && x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x) {
                    inside = !inside;
                }
                previous = current;
            }
            contour_start = index;
        }
    }
    return inside;
}

static void af_svg_raster_path(uint8_t *pixels, const AfSvgView *view, const AfSvgPoint *points,
                               uint32_t count, const uint8_t color[4]) {
    for (uint32_t py = 0U; py < view->pixel_height; ++py) {
        const float y = af_svg_pixel_y(view, py);
        for (uint32_t px = 0U; px < view->pixel_width; ++px) {
            if (af_svg_path_contains(points, count, af_svg_pixel_x(view, px), y)) {
                af_svg_blend(&pixels[((size_t)py * view->pixel_width + px) * 4U], color);
            }
        }
    }
}

static bool af_svg_reserve_raster_work(size_t pixel_count, uint32_t multiplier,
                                       uint64_t *raster_work) {
    const uint64_t work = (uint64_t)pixel_count * multiplier;
    if (work > AF_SVG_MAX_RASTER_WORK - *raster_work) {
        return false;
    }
    *raster_work += work;
    return true;
}

static bool af_svg_parse_view(const char *text, AfSvgView *view) {
    const char *tag = strstr(text, "<svg");
    const char *end = tag == NULL ? NULL : strchr(tag, '>');
    char view_box[160];
    float width = 0.0F;
    float height = 0.0F;
    if (tag == NULL || end == NULL) {
        return false;
    }
    if (af_svg_attribute(tag, end, "viewBox", view_box, sizeof(view_box))) {
        const char *cursor = view_box;
        if (!af_svg_number(&cursor, &view->x) || !af_svg_number(&cursor, &view->y) ||
            !af_svg_number(&cursor, &view->width) || !af_svg_number(&cursor, &view->height) ||
            *af_svg_skip_space(cursor) != '\0' || view->width <= 0.0F || view->height <= 0.0F) {
            return false;
        }
    }
    (void)af_svg_attribute_float(tag, end, "width", &width);
    (void)af_svg_attribute_float(tag, end, "height", &height);
    if (view->width == 0.0F) {
        view->width = width;
        view->height = height;
    }
    if (width <= 0.0F) {
        width = view->width;
    }
    if (height <= 0.0F) {
        height = view->height;
    }
    if (!isfinite(width) || !isfinite(height) || width <= 0.0F || height <= 0.0F ||
        width > (float)AF_MAX_IMAGE_DIMENSION || height > (float)AF_MAX_IMAGE_DIMENSION) {
        return false;
    }
    view->pixel_width = (uint32_t)ceilf(width);
    view->pixel_height = (uint32_t)ceilf(height);
    return view->pixel_width != 0U && view->pixel_height != 0U;
}

AfResult af_image_decode_svg(const void *data, size_t size, AfAllocator *allocator,
                             AfDecodedImage *out_image) {
    const char *cursor = NULL;
    char *text = NULL;
    uint8_t *pixels = NULL;
    AfSvgView view = {0};
    size_t pixel_count = 0U;
    size_t pixel_size = 0U;
    uint64_t raster_work = 0U;
    uint32_t element_count = 0U;
    AfResult result = AF_RESULT_INVALID_PACKAGE;
    if (data == NULL || size == 0U || allocator == NULL || out_image == NULL ||
        memchr(data, '\0', size) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    text = af_allocate(allocator, size + 1U, alignof(char));
    if (text == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memcpy(text, data, size);
    text[size] = '\0';
    if (af_svg_contains_case_insensitive(text, "<!entity") ||
        af_svg_contains_case_insensitive(text, "<!doctype") ||
        af_svg_contains_case_insensitive(text, "<script") ||
        af_svg_contains_case_insensitive(text, "<image") ||
        af_svg_contains_case_insensitive(text, "href=") ||
        af_svg_contains_case_insensitive(text, "url(") || !af_svg_parse_view(text, &view) ||
        af_size_mul_overflow(view.pixel_width, view.pixel_height, &pixel_count) ||
        pixel_count > AF_MAX_IMAGE_PIXELS || af_size_mul_overflow(pixel_count, 4U, &pixel_size)) {
        goto cleanup;
    }
    pixels = af_allocate(allocator, pixel_size, alignof(uint8_t));
    if (pixels == NULL) {
        result = AF_RESULT_OUT_OF_MEMORY;
        goto cleanup;
    }
    memset(pixels, 0, pixel_size);
    cursor = text;
    while ((cursor = strchr(cursor, '<')) != NULL) {
        const char *end = strchr(cursor, '>');
        uint8_t color[4] = {0U, 0U, 0U, 255U};
        if (end == NULL || ++element_count > AF_SVG_MAX_ELEMENTS) {
            goto cleanup;
        }
        if (strncmp(cursor, "<rect", 5U) == 0) {
            float x = 0.0F;
            float y = 0.0F;
            float width = 0.0F;
            float height = 0.0F;
            (void)af_svg_attribute_float(cursor, end, "x", &x);
            (void)af_svg_attribute_float(cursor, end, "y", &y);
            if (!af_svg_attribute_float(cursor, end, "width", &width) ||
                !af_svg_attribute_float(cursor, end, "height", &height) || width < 0.0F ||
                height < 0.0F || !af_svg_color(cursor, end, color) ||
                !af_svg_reserve_raster_work(pixel_count, 1U, &raster_work)) {
                goto cleanup;
            }
            af_svg_raster_rect(pixels, &view, x, y, width, height, color);
        } else if (strncmp(cursor, "<circle", 7U) == 0 || strncmp(cursor, "<ellipse", 8U) == 0) {
            const bool circle = cursor[1] == 'c';
            float cx = 0.0F;
            float cy = 0.0F;
            float rx = 0.0F;
            float ry = 0.0F;
            if (!af_svg_attribute_float(cursor, end, "cx", &cx) ||
                !af_svg_attribute_float(cursor, end, "cy", &cy) ||
                (circle && !af_svg_attribute_float(cursor, end, "r", &rx)) ||
                (!circle && (!af_svg_attribute_float(cursor, end, "rx", &rx) ||
                             !af_svg_attribute_float(cursor, end, "ry", &ry))) ||
                !af_svg_color(cursor, end, color) ||
                !af_svg_reserve_raster_work(pixel_count, 1U, &raster_work)) {
                goto cleanup;
            }
            ry = circle ? rx : ry;
            if (rx <= 0.0F || ry <= 0.0F) {
                goto cleanup;
            }
            af_svg_raster_ellipse(pixels, &view, cx, cy, rx, ry, color);
        } else if (strncmp(cursor, "<path", 5U) == 0) {
            char path_data[16384];
            AfSvgPoint points[AF_SVG_MAX_POINTS];
            uint32_t point_count = 0U;
            if (!af_svg_attribute(cursor, end, "d", path_data, sizeof(path_data)) ||
                !af_svg_color(cursor, end, color) ||
                !af_svg_path_points(path_data, points, &point_count) ||
                !af_svg_reserve_raster_work(pixel_count, point_count, &raster_work)) {
                goto cleanup;
            }
            af_svg_raster_path(pixels, &view, points, point_count, color);
        }
        cursor = end + 1U;
    }
    *out_image = (AfDecodedImage){.pixels = pixels,
                                  .width = view.pixel_width,
                                  .height = view.pixel_height,
                                  .format = AF_IMAGE_SOURCE_SVG,
                                  .color_space = AF_IMAGE_COLOR_SRGB,
                                  .allocator = allocator};
    pixels = NULL;
    result = AF_RESULT_SUCCESS;

cleanup:
    af_deallocate(allocator, pixels);
    af_deallocate(allocator, text);
    return result;
}
