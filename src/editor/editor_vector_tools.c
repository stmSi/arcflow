#include "editor/editor_vector_tools.h"

#include <math.h>
#include <stddef.h>

#define AF_EDITOR_TOOL_PI 3.14159265358979323846F

const char *af_editor_vector_tool_name(AfEditorVectorTool tool) {
    static const char *const names[AF_EDITOR_TOOL_COUNT] = {
        "Pick",    "Node",  "Scissors", "Segment Delete", "Rectangle", "Rounded Rectangle",
        "Ellipse", "Line",  "Pen",      "Pencil",         "Polygon",   "Star",
        "Arc",     "Spiral"};
    return tool < AF_EDITOR_TOOL_COUNT ? names[tool] : "Unknown";
}

const char *af_editor_vector_tool_label(AfEditorVectorTool tool) {
    static const char *const labels[AF_EDITOR_TOOL_COUNT] = {
        "PICK", "NODE", "CUT",    "VDEL", "RECT", "ROUND", "OVAL",
        "LINE", "PEN",  "PENCIL", "POLY", "STAR", "ARC",   "SPIRAL"};
    return tool < AF_EDITOR_TOOL_COUNT ? labels[tool] : "?";
}

bool af_editor_vector_tool_is_construction(AfEditorVectorTool tool) {
    return tool >= AF_EDITOR_TOOL_RECTANGLE && tool <= AF_EDITOR_TOOL_SPIRAL;
}

bool af_editor_vector_tool_rect(AfVec2 start, AfVec2 current, bool from_center,
                                bool constrain_ratio, AfEditorToolRect *out_rect) {
    float dx = current.x - start.x;
    float dy = current.y - start.y;
    if (out_rect == NULL || !isfinite(start.x) || !isfinite(start.y) || !isfinite(current.x) ||
        !isfinite(current.y)) {
        return false;
    }
    if (constrain_ratio) {
        const float extent = fmaxf(fabsf(dx), fabsf(dy));
        dx = copysignf(extent, fabsf(dx) > 0.0001F ? dx : 1.0F);
        dy = copysignf(extent, fabsf(dy) > 0.0001F ? dy : 1.0F);
    }
    if (from_center) {
        out_rect->position = (AfVec2){start.x - fabsf(dx), start.y - fabsf(dy)};
        out_rect->size = (AfVec2){fabsf(dx) * 2.0F, fabsf(dy) * 2.0F};
    } else {
        out_rect->position = (AfVec2){fminf(start.x, start.x + dx), fminf(start.y, start.y + dy)};
        out_rect->size = (AfVec2){fabsf(dx), fabsf(dy)};
    }
    return out_rect->size.x > 0.0001F && out_rect->size.y > 0.0001F;
}

uint32_t af_editor_vector_tool_polygon(AfVec2 size, uint32_t sides, float inset,
                                       AfPathCommand *out_commands, uint32_t capacity) {
    uint32_t point_count = sides;
    if (out_commands == NULL || !isfinite(size.x) || !isfinite(size.y) || size.x <= 0.0F ||
        size.y <= 0.0F || sides < 3U || sides > 64U || !isfinite(inset) || inset < 0.0F ||
        inset >= 1.0F) {
        return 0U;
    }
    if (inset > 0.0F) {
        point_count *= 2U;
    }
    if (point_count + 1U > capacity) {
        return 0U;
    }
    for (uint32_t index = 0U; index < point_count; ++index) {
        const float angle = -AF_EDITOR_TOOL_PI * 0.5F +
                            2.0F * AF_EDITOR_TOOL_PI * (float)index / (float)point_count;
        const float radius = inset > 0.0F && (index & 1U) != 0U ? inset : 1.0F;
        const AfVec2 point = {size.x * 0.5F + cosf(angle) * size.x * 0.5F * radius,
                              size.y * 0.5F + sinf(angle) * size.y * 0.5F * radius};
        out_commands[index] =
            (AfPathCommand){.type = index == 0U ? AF_PATH_MOVE : AF_PATH_LINE, .point = point};
    }
    out_commands[point_count] = (AfPathCommand){.type = AF_PATH_CLOSE};
    return point_count + 1U;
}

uint32_t af_editor_vector_tool_arc(AfVec2 size, AfPathCommand *out_commands, uint32_t capacity) {
    const float kappa = 0.552284749831F;
    const float rx = size.x * 0.5F;
    const float ry = size.y * 0.5F;
    if (out_commands == NULL || capacity < 4U || !isfinite(size.x) || !isfinite(size.y) ||
        size.x <= 0.0F || size.y <= 0.0F) {
        return 0U;
    }
    out_commands[0] = (AfPathCommand){.type = AF_PATH_MOVE, .point = {size.x, ry}};
    out_commands[1] = (AfPathCommand){.type = AF_PATH_CUBIC,
                                      .point = {rx, size.y},
                                      .control1 = {size.x, ry + kappa * ry},
                                      .control2 = {rx + kappa * rx, size.y}};
    out_commands[2] = (AfPathCommand){.type = AF_PATH_CUBIC,
                                      .point = {0.0F, ry},
                                      .control1 = {rx - kappa * rx, size.y},
                                      .control2 = {0.0F, ry + kappa * ry}};
    out_commands[3] = (AfPathCommand){.type = AF_PATH_CUBIC,
                                      .point = {rx, 0.0F},
                                      .control1 = {0.0F, ry - kappa * ry},
                                      .control2 = {rx - kappa * rx, 0.0F}};
    return 4U;
}

uint32_t af_editor_vector_tool_spiral(AfVec2 size, uint32_t turns, AfPathCommand *out_commands,
                                      uint32_t capacity) {
    uint32_t point_count = 0U;
    if (out_commands == NULL || !isfinite(size.x) || !isfinite(size.y) || size.x <= 0.0F ||
        size.y <= 0.0F || turns == 0U || turns > 12U) {
        return 0U;
    }
    point_count = turns * 20U + 1U;
    if (point_count > capacity) {
        return 0U;
    }
    for (uint32_t index = 0U; index < point_count; ++index) {
        const float progress = (float)index / (float)(point_count - 1U);
        const float angle =
            -AF_EDITOR_TOOL_PI * 0.5F + progress * (float)turns * 2.0F * AF_EDITOR_TOOL_PI;
        const float radius = 0.03F + progress * 0.97F;
        const AfVec2 point = {size.x * 0.5F + cosf(angle) * size.x * 0.5F * radius,
                              size.y * 0.5F + sinf(angle) * size.y * 0.5F * radius};
        out_commands[index] =
            (AfPathCommand){.type = index == 0U ? AF_PATH_MOVE : AF_PATH_LINE, .point = point};
    }
    return point_count;
}

static float af_editor_tool_distance_squared(AfVec2 point, AfVec2 first, AfVec2 second) {
    const float dx = second.x - first.x;
    const float dy = second.y - first.y;
    const float length_squared = dx * dx + dy * dy;
    float parameter = 0.0F;
    if (length_squared > 1.0e-12F) {
        parameter = ((point.x - first.x) * dx + (point.y - first.y) * dy) / length_squared;
    }
    parameter = fmaxf(0.0F, fminf(1.0F, parameter));
    {
        const float x = first.x + parameter * dx;
        const float y = first.y + parameter * dy;
        const float px = point.x - x;
        const float py = point.y - y;
        return px * px + py * py;
    }
}

static void af_editor_tool_simplify_range(const AfVec2 *points, uint32_t first, uint32_t last,
                                          float tolerance_squared, bool *keep) {
    uint32_t farthest = first;
    float maximum = 0.0F;
    for (uint32_t index = first + 1U; index < last; ++index) {
        const float distance =
            af_editor_tool_distance_squared(points[index], points[first], points[last]);
        if (distance > maximum) {
            maximum = distance;
            farthest = index;
        }
    }
    if (maximum > tolerance_squared && farthest > first && farthest < last) {
        keep[farthest] = true;
        af_editor_tool_simplify_range(points, first, farthest, tolerance_squared, keep);
        af_editor_tool_simplify_range(points, farthest, last, tolerance_squared, keep);
    }
}

uint32_t af_editor_vector_tool_simplify(const AfVec2 *points, uint32_t point_count, float tolerance,
                                        AfVec2 *out_points, uint32_t capacity) {
    bool keep[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY] = {0};
    uint32_t needed = 0U;
    uint32_t write = 0U;
    if (points == NULL || out_points == NULL || point_count < 2U ||
        point_count > AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY || capacity < 2U ||
        !isfinite(tolerance) || tolerance < 0.0F) {
        return 0U;
    }
    keep[0] = true;
    keep[point_count - 1U] = true;
    af_editor_tool_simplify_range(points, 0U, point_count - 1U, tolerance * tolerance, keep);
    for (uint32_t index = 0U; index < point_count; ++index) {
        needed += keep[index] ? 1U : 0U;
    }
    if (needed > capacity) {
        return 0U;
    }
    for (uint32_t index = 0U; index < point_count; ++index) {
        if (keep[index]) {
            out_points[write++] = points[index];
        }
    }
    return write;
}
