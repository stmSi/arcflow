#include "project/interchange.h"

#include "af/core/png.h"
#include "af/core/safe_math.h"
#include "af/path/path.h"
#include "project/project.h"

#include <ctype.h>
#include <math.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct AfInterchangeWriter {
    char *output;
    size_t capacity;
    size_t size;
} AfInterchangeWriter;

static void af_interchange_report(AfInterchangeDiagnostics *diagnostics,
                                  AfInterchangeDiagnosticSeverity severity,
                                  AfInterchangeDiagnosticCode code, uint32_t source_index,
                                  uint64_t object_id, const char *message) {
    (void)af_interchange_diagnostics_push(diagnostics, severity, code, source_index, 0U, 0U,
                                          object_id, message);
}

void af_interchange_diagnostics_clear(AfInterchangeDiagnostics *diagnostics) {
    if (diagnostics != NULL) {
        *diagnostics = (AfInterchangeDiagnostics){0};
    }
}

bool af_interchange_diagnostics_push(AfInterchangeDiagnostics *diagnostics,
                                     AfInterchangeDiagnosticSeverity severity,
                                     AfInterchangeDiagnosticCode code, uint32_t source_index,
                                     uint32_t line, uint32_t column, uint64_t object_id,
                                     const char *message) {
    AfInterchangeDiagnostic *entry = NULL;
    if (diagnostics == NULL || (uint32_t)severity > (uint32_t)AF_INTERCHANGE_DIAGNOSTIC_ERROR ||
        (uint32_t)code > (uint32_t)AF_INTERCHANGE_DIAGNOSTIC_TRANSACTION_COMMIT ||
        message == NULL) {
        return false;
    }
    if (severity == AF_INTERCHANGE_DIAGNOSTIC_WARNING) {
        ++diagnostics->warning_count;
    } else if (severity == AF_INTERCHANGE_DIAGNOSTIC_ERROR) {
        ++diagnostics->error_count;
    }
    if (diagnostics->count >= AF_INTERCHANGE_MAX_DIAGNOSTICS) {
        ++diagnostics->dropped_count;
        return false;
    }
    entry = &diagnostics->items[diagnostics->count++];
    *entry = (AfInterchangeDiagnostic){.severity = severity,
                                       .code = code,
                                       .source_index = source_index,
                                       .line = line,
                                       .column = column,
                                       .object_id = object_id};
    (void)snprintf(entry->message, sizeof(entry->message), "%s", message);
    return true;
}

AfResult af_interchange_check_import_support(AfInterchangeFormat format,
                                             AfInterchangeDiagnostics *diagnostics) {
    if ((uint32_t)format > (uint32_t)AF_INTERCHANGE_FORMAT_LOTTIE_JSON) {
        af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_ERROR,
                              AF_INTERCHANGE_DIAGNOSTIC_UNSUPPORTED_FORMAT, 0U, 0U,
                              "Unknown interchange format");
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (format == AF_INTERCHANGE_FORMAT_LOTTIE_JSON) {
        af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_WARNING,
                              AF_INTERCHANGE_DIAGNOSTIC_UNSUPPORTED_FORMAT, 0U, 0U,
                              "Lottie JSON import is not supported");
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    return AF_RESULT_SUCCESS;
}

static bool af_interchange_easing_text(const char *text, size_t size, char output[]) {
    size_t first = 0U;
    size_t last = size;
    if (text == NULL || size == 0U || size >= AF_INTERCHANGE_MAX_EASING_TEXT || output == NULL ||
        memchr(text, '\0', size) != NULL) {
        return false;
    }
    while (first < last && isspace((unsigned char)text[first]) != 0) {
        ++first;
    }
    while (last > first && isspace((unsigned char)text[last - 1U]) != 0) {
        --last;
    }
    if (last == first || last - first >= AF_INTERCHANGE_MAX_EASING_TEXT) {
        return false;
    }
    for (size_t index = first; index < last; ++index) {
        const unsigned char byte = (unsigned char)text[index];
        if (byte < 32U || byte > 126U) {
            return false;
        }
        output[index - first] = (char)tolower(byte);
    }
    output[last - first] = '\0';
    return true;
}

static bool af_interchange_parse_css_bezier(const char *text, float points[4]) {
    static const char prefix[] = "cubic-bezier(";
    int consumed = 0;
    if (text == NULL || points == NULL || strncmp(text, prefix, sizeof(prefix) - 1U) != 0) {
        return false;
    }
    if (sscanf(text, "cubic-bezier( %f , %f , %f , %f ) %n", &points[0], &points[1], &points[2],
               &points[3], &consumed) != 4 ||
        consumed <= 0 || text[consumed] != '\0') {
        return false;
    }
    return isfinite(points[0]) && isfinite(points[1]) && isfinite(points[2]) &&
           isfinite(points[3]) && points[0] >= 0.0F && points[0] <= 1.0F && points[2] >= 0.0F &&
           points[2] <= 1.0F;
}

AfResult af_interchange_parse_easing(const char *text, size_t text_size,
                                     AfInterchangeEasing *out_easing,
                                     AfInterchangeDiagnostics *diagnostics) {
    char normalized[AF_INTERCHANGE_MAX_EASING_TEXT];
    AfInterchangeEasing easing = {0};
    if (out_easing == NULL || !af_interchange_easing_text(text, text_size, normalized)) {
        af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_ERROR,
                              AF_INTERCHANGE_DIAGNOSTIC_INVALID_SYNTAX, 0U, 0U,
                              "Easing text is empty, oversized, or contains invalid bytes");
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (strcmp(normalized, "linear") == 0) {
        easing = (AfInterchangeEasing){.kind = AF_INTERCHANGE_EASING_LINEAR,
                                       .control_points = {0.0F, 0.0F, 1.0F, 1.0F}};
    } else if (strcmp(normalized, "ease") == 0) {
        easing = (AfInterchangeEasing){.kind = AF_INTERCHANGE_EASING_CUBIC_BEZIER,
                                       .control_points = {0.25F, 0.1F, 0.25F, 1.0F}};
    } else if (strcmp(normalized, "ease-in") == 0) {
        easing = (AfInterchangeEasing){.kind = AF_INTERCHANGE_EASING_CUBIC_BEZIER,
                                       .control_points = {0.42F, 0.0F, 1.0F, 1.0F}};
    } else if (strcmp(normalized, "ease-out") == 0) {
        easing = (AfInterchangeEasing){.kind = AF_INTERCHANGE_EASING_CUBIC_BEZIER,
                                       .control_points = {0.0F, 0.0F, 0.58F, 1.0F}};
    } else if (strcmp(normalized, "ease-in-out") == 0) {
        easing = (AfInterchangeEasing){.kind = AF_INTERCHANGE_EASING_CUBIC_BEZIER,
                                       .control_points = {0.42F, 0.0F, 0.58F, 1.0F}};
    } else if (strcmp(normalized, "step-start") == 0) {
        easing.kind = AF_INTERCHANGE_EASING_STEP_START;
    } else if (strcmp(normalized, "step-end") == 0) {
        easing.kind = AF_INTERCHANGE_EASING_STEP_END;
    } else if (strncmp(normalized, "cubic-bezier(", strlen("cubic-bezier(")) == 0 &&
               af_interchange_parse_css_bezier(normalized, easing.control_points)) {
        easing.kind = AF_INTERCHANGE_EASING_CUBIC_BEZIER;
    } else {
        af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_ERROR,
                              AF_INTERCHANGE_DIAGNOSTIC_INVALID_SYNTAX, 0U, 0U,
                              "Unsupported or malformed CSS easing");
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_easing = easing;
    return AF_RESULT_SUCCESS;
}

static uint32_t af_interchange_next_power_of_two(uint32_t value) {
    --value;
    value |= value >> 1U;
    value |= value >> 2U;
    value |= value >> 4U;
    value |= value >> 8U;
    value |= value >> 16U;
    return value + 1U;
}

AfResult af_interchange_plan_sprite_sheet(const AfInterchangeRgbaFrame *frames,
                                          uint32_t frame_count, const AfSpriteSheetOptions *options,
                                          AfSpriteSheetPlan *out_plan,
                                          AfInterchangeDiagnostics *diagnostics) {
    AfSpriteSheetOptions settings = {0};
    uint32_t cell_width = 0U;
    uint32_t cell_height = 0U;
    uint32_t columns = 0U;
    uint32_t rows = 0U;
    uint64_t width = 0U;
    uint64_t height = 0U;
    if (frames == NULL || frame_count == 0U || frame_count > AF_MAX_IMAGE_SEQUENCE_FRAMES ||
        out_plan == NULL) {
        af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_ERROR,
                              AF_INTERCHANGE_DIAGNOSTIC_INVALID_SYNTAX, 0U, 0U,
                              "Sprite sheet requires a bounded non-empty frame list");
        return AF_RESULT_INVALID_ARGUMENT;
    }
    settings = options == NULL ? (AfSpriteSheetOptions){0} : *options;
    if (settings.padding > AF_INTERCHANGE_MAX_SPRITE_PADDING || settings.columns > frame_count) {
        af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_ERROR,
                              AF_INTERCHANGE_DIAGNOSTIC_COMPLEXITY_LIMIT, 0U, 0U,
                              "Sprite sheet padding or column count exceeds limits");
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    for (uint32_t index = 0U; index < frame_count; ++index) {
        const AfInterchangeRgbaFrame *frame = &frames[index];
        uint64_t minimum_stride = 0U;
        if (frame->pixels == NULL || frame->width == 0U || frame->height == 0U ||
            frame->width > AF_MAX_IMAGE_DIMENSION || frame->height > AF_MAX_IMAGE_DIMENSION) {
            af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_ERROR,
                                  AF_INTERCHANGE_DIAGNOSTIC_INVALID_SYNTAX, index, 0U,
                                  "Sprite frame has invalid dimensions or pixels");
            return AF_RESULT_INVALID_ARGUMENT;
        }
        minimum_stride = (uint64_t)frame->width * 4U;
        if ((uint64_t)frame->row_stride < minimum_stride) {
            af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_ERROR,
                                  AF_INTERCHANGE_DIAGNOSTIC_INVALID_SYNTAX, index, 0U,
                                  "Sprite frame row stride is too small");
            return AF_RESULT_INVALID_ARGUMENT;
        }
        cell_width = frame->width > cell_width ? frame->width : cell_width;
        cell_height = frame->height > cell_height ? frame->height : cell_height;
    }
    columns = settings.columns;
    if (columns == 0U) {
        columns = (uint32_t)ceil(sqrt((double)frame_count));
    }
    rows = (frame_count + columns - 1U) / columns;
    width =
        (uint64_t)settings.padding + (uint64_t)columns * ((uint64_t)cell_width + settings.padding);
    height =
        (uint64_t)settings.padding + (uint64_t)rows * ((uint64_t)cell_height + settings.padding);
    if (settings.power_of_two) {
        if (width > UINT32_MAX || height > UINT32_MAX) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        width = af_interchange_next_power_of_two((uint32_t)width);
        height = af_interchange_next_power_of_two((uint32_t)height);
    }
    if (width > AF_MAX_IMAGE_DIMENSION || height > AF_MAX_IMAGE_DIMENSION ||
        width * height > AF_MAX_IMAGE_PIXELS) {
        af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_ERROR,
                              AF_INTERCHANGE_DIAGNOSTIC_COMPLEXITY_LIMIT, 0U, 0U,
                              "Planned sprite sheet exceeds image complexity limits");
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    *out_plan = (AfSpriteSheetPlan){.width = (uint32_t)width,
                                    .height = (uint32_t)height,
                                    .cell_width = cell_width,
                                    .cell_height = cell_height,
                                    .columns = columns,
                                    .rows = rows,
                                    .padding = settings.padding,
                                    .frame_count = frame_count};
    return AF_RESULT_SUCCESS;
}

AfResult af_interchange_sprite_frame_position(const AfSpriteSheetPlan *plan, uint32_t frame_index,
                                              uint32_t *out_x, uint32_t *out_y) {
    if (plan == NULL || out_x == NULL || out_y == NULL || plan->columns == 0U ||
        frame_index >= plan->frame_count) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_x = plan->padding + (frame_index % plan->columns) * (plan->cell_width + plan->padding);
    *out_y = plan->padding + (frame_index / plan->columns) * (plan->cell_height + plan->padding);
    return AF_RESULT_SUCCESS;
}

AfResult af_interchange_export_sprite_sheet_png(const char *path,
                                                const AfInterchangeRgbaFrame *frames,
                                                uint32_t frame_count,
                                                const AfSpriteSheetOptions *options,
                                                AfAllocator *allocator, AfSpriteSheetPlan *out_plan,
                                                AfInterchangeDiagnostics *diagnostics) {
    AfSpriteSheetPlan plan = {0};
    uint8_t *pixels = NULL;
    size_t pixel_size = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (path == NULL || path[0] == '\0' || allocator == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_interchange_plan_sprite_sheet(frames, frame_count, options, &plan, diagnostics);
    if (result != AF_RESULT_SUCCESS ||
        af_size_mul_overflow((size_t)plan.width, (size_t)plan.height, &pixel_size) ||
        af_size_mul_overflow(pixel_size, 4U, &pixel_size)) {
        return result == AF_RESULT_SUCCESS ? AF_RESULT_LIMIT_EXCEEDED : result;
    }
    pixels = af_allocate(allocator, pixel_size, alignof(uint8_t));
    if (pixels == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memset(pixels, 0, pixel_size);
    for (uint32_t index = 0U; index < frame_count; ++index) {
        uint32_t target_x = 0U;
        uint32_t target_y = 0U;
        (void)af_interchange_sprite_frame_position(&plan, index, &target_x, &target_y);
        for (uint32_t row = 0U; row < frames[index].height; ++row) {
            memcpy(pixels + ((size_t)(target_y + row) * plan.width + target_x) * 4U,
                   frames[index].pixels + (size_t)row * frames[index].row_stride,
                   (size_t)frames[index].width * 4U);
        }
    }
    result = af_png_write_rgba8(path, pixels, plan.width, plan.height, plan.width * 4U,
                                AF_PIXEL_ORDER_RGBA);
    af_deallocate(allocator, pixels);
    if (result != AF_RESULT_SUCCESS) {
        af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_ERROR,
                              AF_INTERCHANGE_DIAGNOSTIC_IO, 0U, 0U,
                              "Could not write sprite sheet PNG");
        return result;
    }
    if (out_plan != NULL) {
        *out_plan = plan;
    }
    return AF_RESULT_SUCCESS;
}

static bool af_interchange_writer_append(AfInterchangeWriter *writer, const char *format, ...) {
    va_list arguments;
    int length = 0;
    size_t available = 0U;
    if (writer == NULL || format == NULL || writer->size >= writer->capacity) {
        return false;
    }
    available = writer->capacity - writer->size;
    va_start(arguments, format);
    length = vsnprintf(writer->output + writer->size, available, format, arguments);
    va_end(arguments);
    if (length < 0 || (size_t)length >= available) {
        return false;
    }
    writer->size += (size_t)length;
    return true;
}

static uint8_t af_interchange_color_byte(float component) {
    const float bounded = component < 0.0F ? 0.0F : (component > 1.0F ? 1.0F : component);
    return (uint8_t)lroundf(bounded * 255.0F);
}

static bool af_interchange_svg_style(AfInterchangeWriter *writer, const AfSceneNode *node,
                                     const AfStyle *style) {
    const AfColor fill = style == NULL ? node->fill : style->fill.solid;
    const float fill_opacity =
        fill.a * node->world_opacity * (style == NULL ? 1.0F : style->fill_opacity);
    if (!af_interchange_writer_append(writer, " fill=\"#%02X%02X%02X\" fill-opacity=\"%.9g\"",
                                      af_interchange_color_byte(fill.r),
                                      af_interchange_color_byte(fill.g),
                                      af_interchange_color_byte(fill.b), fill_opacity)) {
        return false;
    }
    if (style == NULL || !style->stroke.enabled) {
        return true;
    }
    if (!af_interchange_writer_append(
            writer,
            " stroke=\"#%02X%02X%02X\" stroke-opacity=\"%.9g\" stroke-width=\"%.9g\" "
            "stroke-linecap=\"%s\" stroke-linejoin=\"%s\" stroke-miterlimit=\"%.9g\"",
            af_interchange_color_byte(style->stroke_paint.solid.r),
            af_interchange_color_byte(style->stroke_paint.solid.g),
            af_interchange_color_byte(style->stroke_paint.solid.b),
            style->stroke_paint.solid.a * style->stroke_opacity * node->world_opacity,
            style->stroke.width,
            style->stroke.cap == AF_STROKE_CAP_ROUND
                ? "round"
                : (style->stroke.cap == AF_STROKE_CAP_SQUARE ? "square" : "butt"),
            style->stroke.join == AF_STROKE_JOIN_ROUND
                ? "round"
                : (style->stroke.join == AF_STROKE_JOIN_BEVEL ? "bevel" : "miter"),
            style->stroke.miter_limit)) {
        return false;
    }
    if (style->stroke.dash_count != 0U) {
        if (!af_interchange_writer_append(writer, " stroke-dasharray=\"")) {
            return false;
        }
        for (uint32_t index = 0U; index < style->stroke.dash_count; ++index) {
            if (!af_interchange_writer_append(writer, index == 0U ? "%.9g" : " %.9g",
                                              style->stroke.dashes[index])) {
                return false;
            }
        }
        if (!af_interchange_writer_append(writer, "\" stroke-dashoffset=\"%.9g\"",
                                          style->stroke.dash_offset)) {
            return false;
        }
    }
    return true;
}

static bool af_interchange_svg_transform(AfInterchangeWriter *writer, const AfSceneNode *node) {
    return af_interchange_writer_append(
        writer, " transform=\"matrix(%.9g %.9g %.9g %.9g %.9g %.9g)\"", node->world_transform.m00,
        node->world_transform.m10, node->world_transform.m01, node->world_transform.m11,
        node->world_transform.tx, node->world_transform.ty);
}

static bool af_interchange_svg_path(AfInterchangeWriter *writer, const AfScene *scene,
                                    const AfSceneNode *node, const AfStyle *style) {
    const AfPath *path = af_path_find_node_const(scene->paths, node->id);
    if (path == NULL || path->command_start > scene->paths->command_count ||
        path->command_count > scene->paths->command_count - path->command_start) {
        return false;
    }
    if (!af_interchange_writer_append(writer, "<path d=\"")) {
        return false;
    }
    for (uint32_t index = 0U; index < path->command_count; ++index) {
        const AfPathCommand *command = &scene->paths->commands[path->command_start + index];
        bool appended = false;
        if (command->type == AF_PATH_MOVE) {
            appended = af_interchange_writer_append(writer, "M %.9g %.9g ", command->point.x,
                                                    command->point.y);
        } else if (command->type == AF_PATH_LINE) {
            appended = af_interchange_writer_append(writer, "L %.9g %.9g ", command->point.x,
                                                    command->point.y);
        } else if (command->type == AF_PATH_CUBIC) {
            appended = af_interchange_writer_append(writer, "C %.9g %.9g %.9g %.9g %.9g %.9g ",
                                                    command->control1.x, command->control1.y,
                                                    command->control2.x, command->control2.y,
                                                    command->point.x, command->point.y);
        } else {
            appended = af_interchange_writer_append(writer, "Z ");
        }
        if (!appended) {
            return false;
        }
    }
    return af_interchange_writer_append(writer, "\" fill-rule=\"%s\"",
                                        path->fill_rule == AF_PATH_FILL_EVEN_ODD ? "evenodd"
                                                                                 : "nonzero") &&
           af_interchange_svg_style(writer, node, style) &&
           af_interchange_svg_transform(writer, node) &&
           af_interchange_writer_append(writer, "/>\n");
}

static AfResult af_interchange_svg_node_style(const AfScene *scene, const AfSceneNode *node,
                                              uint32_t source_index, AfStyle *out_style,
                                              const AfStyle **out_resolved,
                                              AfInterchangeDiagnostics *diagnostics) {
    const AfStyle *style =
        scene->styles == NULL ? NULL : af_style_find_node_const(scene->styles, node->id);
    *out_resolved = NULL;
    if (style == NULL) {
        return AF_RESULT_SUCCESS;
    }
    if (af_style_resolve(scene->styles, style, out_style) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (out_style->fill.type != AF_PAINT_SOLID ||
        (out_style->stroke.enabled &&
         (out_style->stroke_paint.type != AF_PAINT_SOLID ||
          fabsf(out_style->stroke.width - out_style->stroke.end_width) > 0.00001F))) {
        af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_WARNING,
                              AF_INTERCHANGE_DIAGNOSTIC_UNSUPPORTED_FEATURE, source_index, node->id,
                              "Gradient paint or variable-width stroke cannot be represented by "
                              "this static SVG exporter; object was omitted");
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    *out_resolved = out_style;
    return AF_RESULT_SUCCESS;
}

bool af_interchange_static_svg_node_supported(const AfScene *scene, const AfSceneNode *node) {
    if (!scene || !node || !node->world_visible || node->world_opacity <= 0 ||
        (node->type != AF_NODE_RECTANGLE && node->type != AF_NODE_ROUNDED_RECTANGLE &&
         node->type != AF_NODE_ELLIPSE && !(node->type == AF_NODE_PATH && scene->paths)))
        return false;
    AfStyle style;
    const AfStyle *resolved = NULL;
    return af_interchange_svg_node_style(scene, node, 0, &style, &resolved, NULL) ==
           AF_RESULT_SUCCESS;
}

AfResult af_interchange_export_static_svg(const AfScene *scene, AfVec2 canvas_size, char *output,
                                          size_t output_capacity, size_t *out_size,
                                          AfInterchangeDiagnostics *diagnostics) {
    AfInterchangeWriter writer = {.output = output, .capacity = output_capacity};
    uint32_t order[AF_MAX_NODES];
    if (output != NULL && output_capacity != 0U) {
        output[0] = '\0';
    }
    if (out_size != NULL) {
        *out_size = 0U;
    }
    if (scene == NULL || output == NULL || output_capacity == 0U || out_size == NULL ||
        !isfinite(canvas_size.x) || !isfinite(canvas_size.y) || canvas_size.x <= 0.0F ||
        canvas_size.y <= 0.0F || af_scene_validate(scene) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if ((scene->paths != NULL && af_path_data_validate(scene->paths) != AF_RESULT_SUCCESS) ||
        (scene->styles != NULL && af_style_data_validate(scene->styles) != AF_RESULT_SUCCESS)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < scene->count; ++index) {
        const AfSceneNode *node = &scene->nodes[index];
        uint32_t position = index;
        if (!isfinite(node->world_transform.m00) || !isfinite(node->world_transform.m01) ||
            !isfinite(node->world_transform.m10) || !isfinite(node->world_transform.m11) ||
            !isfinite(node->world_transform.tx) || !isfinite(node->world_transform.ty) ||
            !isfinite(node->world_opacity) || node->world_opacity < 0.0F ||
            node->world_opacity > 1.0F || !isfinite(node->resolved_bounds.x) ||
            !isfinite(node->resolved_bounds.y) || node->resolved_bounds.x < 0.0F ||
            node->resolved_bounds.y < 0.0F) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        while (position > 0U &&
               scene->nodes[order[position - 1U]].layer_order > node->layer_order) {
            order[position] = order[position - 1U];
            --position;
        }
        order[position] = index;
    }
    if (scene->particles != NULL && scene->particles->count != 0U) {
        af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_WARNING,
                              AF_INTERCHANGE_DIAGNOSTIC_UNSUPPORTED_FEATURE, 0U, 0U,
                              "Particle output is not representable in static SVG");
    }
    if (!af_interchange_writer_append(&writer,
                                      "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%.9g\" "
                                      "height=\"%.9g\" viewBox=\"0 0 %.9g %.9g\">\n",
                                      canvas_size.x, canvas_size.y, canvas_size.x, canvas_size.y)) {
        goto output_limit;
    }
    for (uint32_t index = 0U; index < scene->count; ++index) {
        const uint32_t node_index = order[index];
        const AfSceneNode *node = &scene->nodes[node_index];
        AfStyle resolved_style = {0};
        const AfStyle *style = NULL;
        bool appended = true;
        AfResult style_result = AF_RESULT_SUCCESS;
        if (!node->world_visible || node->world_opacity <= 0.0F || node->type == AF_NODE_GROUP) {
            continue;
        }
        style_result = af_interchange_svg_node_style(scene, node, node_index, &resolved_style,
                                                     &style, diagnostics);
        if (style_result == AF_RESULT_UNSUPPORTED_FEATURE) {
            continue;
        }
        if (style_result != AF_RESULT_SUCCESS) {
            output[0] = '\0';
            return style_result;
        }
        if (node->type == AF_NODE_RECTANGLE || node->type == AF_NODE_ROUNDED_RECTANGLE) {
            const float radius =
                node->type == AF_NODE_ROUNDED_RECTANGLE
                    ? fminf(node->corner_radius,
                            fminf(node->resolved_bounds.x, node->resolved_bounds.y) * 0.5F)
                    : 0.0F;
            appended = af_interchange_writer_append(
                           &writer, "<rect width=\"%.9g\" height=\"%.9g\" rx=\"%.9g\" ry=\"%.9g\"",
                           node->resolved_bounds.x, node->resolved_bounds.y, radius, radius) &&
                       af_interchange_svg_style(&writer, node, style) &&
                       af_interchange_svg_transform(&writer, node) &&
                       af_interchange_writer_append(&writer, "/>\n");
        } else if (node->type == AF_NODE_ELLIPSE) {
            const float radius_x = node->resolved_bounds.x * 0.5F;
            const float radius_y = node->resolved_bounds.y * 0.5F;
            appended = af_interchange_writer_append(
                           &writer, "<ellipse cx=\"%.9g\" cy=\"%.9g\" rx=\"%.9g\" ry=\"%.9g\"",
                           radius_x, radius_y, radius_x, radius_y) &&
                       af_interchange_svg_style(&writer, node, style) &&
                       af_interchange_svg_transform(&writer, node) &&
                       af_interchange_writer_append(&writer, "/>\n");
        } else if (node->type == AF_NODE_PATH && scene->paths != NULL) {
            appended = af_interchange_svg_path(&writer, scene, node, style);
        } else {
            af_interchange_report(
                diagnostics, AF_INTERCHANGE_DIAGNOSTIC_WARNING,
                AF_INTERCHANGE_DIAGNOSTIC_UNSUPPORTED_FEATURE, node_index, node->id,
                "Scene object cannot be represented by static SVG and was omitted");
            continue;
        }
        if (!appended) {
            goto output_limit;
        }
    }
    if (!af_interchange_writer_append(&writer, "</svg>\n")) {
        goto output_limit;
    }
    *out_size = writer.size;
    return AF_RESULT_SUCCESS;

output_limit:
    output[0] = '\0';
    af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_ERROR,
                          AF_INTERCHANGE_DIAGNOSTIC_OUTPUT_LIMIT, 0U, 0U,
                          "Static SVG output capacity is too small");
    return AF_RESULT_LIMIT_EXCEEDED;
}

#include "project/interchange_lottie.inc"

AfResult af_interchange_transaction_execute(AfAllocator *allocator, void *user_data,
                                            AfInterchangeStageCallback stage,
                                            AfInterchangeCommitCallback commit,
                                            AfInterchangeDiscardCallback discard,
                                            AfInterchangeDiagnostics *diagnostics) {
    void *staging = NULL;
    AfResult result = AF_RESULT_SUCCESS;
    if (allocator == NULL || stage == NULL || commit == NULL || discard == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = stage(user_data, allocator, &staging, diagnostics);
    if (result != AF_RESULT_SUCCESS || staging == NULL) {
        af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_ERROR,
                              AF_INTERCHANGE_DIAGNOSTIC_TRANSACTION_STAGE, 0U, 0U,
                              "Interchange staging failed; destination was not modified");
        if (staging != NULL) {
            discard(user_data, allocator, staging);
        }
        return result == AF_RESULT_SUCCESS ? AF_RESULT_INVALID_ARGUMENT : result;
    }
    result = commit(user_data, staging, diagnostics);
    if (result != AF_RESULT_SUCCESS) {
        af_interchange_report(diagnostics, AF_INTERCHANGE_DIAGNOSTIC_ERROR,
                              AF_INTERCHANGE_DIAGNOSTIC_TRANSACTION_COMMIT, 0U, 0U,
                              "Interchange commit failed");
    }
    discard(user_data, allocator, staging);
    return result;
}
