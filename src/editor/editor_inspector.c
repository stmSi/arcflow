#include "editor/editor_inspector.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static const char *af_editor_inspector_skip_space(const char *text) {
    while (text != NULL && isspace((unsigned char)*text) != 0) {
        ++text;
    }
    return text;
}

bool af_editor_inspector_parse_number(const char *text, float *out_value) {
    char *end = NULL;
    float value = 0.0F;
    if (text == NULL || out_value == NULL) {
        return false;
    }
    text = af_editor_inspector_skip_space(text);
    errno = 0;
    value = strtof(text, &end);
    if (end == text || errno == ERANGE || !isfinite(value)) {
        return false;
    }
    end = (char *)af_editor_inspector_skip_space(end);
    if (*end != '\0') {
        return false;
    }
    *out_value = value;
    return true;
}

bool af_editor_inspector_parse_integer(const char *text, int32_t *out_value) {
    char *end = NULL;
    long value = 0L;
    if (text == NULL || out_value == NULL) {
        return false;
    }
    text = af_editor_inspector_skip_space(text);
    errno = 0;
    value = strtol(text, &end, 10);
    if (end == text || errno == ERANGE || value < INT32_MIN || value > INT32_MAX) {
        return false;
    }
    end = (char *)af_editor_inspector_skip_space(end);
    if (*end != '\0') {
        return false;
    }
    *out_value = (int32_t)value;
    return true;
}

static int af_editor_inspector_hex_digit(char digit) {
    if (digit >= '0' && digit <= '9') {
        return digit - '0';
    }
    if (digit >= 'a' && digit <= 'f') {
        return digit - 'a' + 10;
    }
    if (digit >= 'A' && digit <= 'F') {
        return digit - 'A' + 10;
    }
    return -1;
}

bool af_editor_inspector_parse_hex_color(const char *text, AfColor *out_color) {
    uint8_t channels[4] = {0U, 0U, 0U, 255U};
    size_t length = 0U;
    if (text == NULL || out_color == NULL) {
        return false;
    }
    text = af_editor_inspector_skip_space(text);
    if (*text == '#') {
        ++text;
    }
    while (isxdigit((unsigned char)text[length]) != 0) {
        ++length;
    }
    if (af_editor_inspector_skip_space(text + length)[0] != '\0' ||
        (length != 3U && length != 4U && length != 6U && length != 8U)) {
        return false;
    }
    if (length == 3U || length == 4U) {
        for (size_t index = 0U; index < length; ++index) {
            const int digit = af_editor_inspector_hex_digit(text[index]);
            if (digit < 0) {
                return false;
            }
            channels[index] = (uint8_t)(digit * 17);
        }
    } else {
        for (size_t index = 0U; index < length / 2U; ++index) {
            const int high = af_editor_inspector_hex_digit(text[index * 2U]);
            const int low = af_editor_inspector_hex_digit(text[index * 2U + 1U]);
            if (high < 0 || low < 0) {
                return false;
            }
            channels[index] = (uint8_t)((high << 4) | low);
        }
    }
    *out_color = (AfColor){(float)channels[0] / 255.0F, (float)channels[1] / 255.0F,
                           (float)channels[2] / 255.0F, (float)channels[3] / 255.0F};
    return true;
}

void af_editor_inspector_format_hex_color(AfColor color, char *output, size_t capacity) {
    const uint32_t red = (uint32_t)lroundf(fminf(fmaxf(color.r, 0.0F), 1.0F) * 255.0F);
    const uint32_t green = (uint32_t)lroundf(fminf(fmaxf(color.g, 0.0F), 1.0F) * 255.0F);
    const uint32_t blue = (uint32_t)lroundf(fminf(fmaxf(color.b, 0.0F), 1.0F) * 255.0F);
    const uint32_t alpha = (uint32_t)lroundf(fminf(fmaxf(color.a, 0.0F), 1.0F) * 255.0F);
    if (output != NULL && capacity != 0U) {
        (void)snprintf(output, capacity, "#%02X%02X%02X%02X", red, green, blue, alpha);
    }
}
