#ifndef AF_EDITOR_INSPECTOR_H
#define AF_EDITOR_INSPECTOR_H

#include "core/math.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool af_editor_inspector_parse_number(const char *text, float *out_value);
bool af_editor_inspector_parse_integer(const char *text, int32_t *out_value);
bool af_editor_inspector_parse_hex_color(const char *text, AfColor *out_color);
void af_editor_inspector_format_hex_color(AfColor color, char *output, size_t capacity);

#endif
