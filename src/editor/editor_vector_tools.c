#include "editor/editor_vector_tools.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define AF_EDITOR_TOOL_PI 3.14159265358979323846F

uint32_t af_editor_vector_primitive_path(AfNodeType type, AfVec2 size, float radius,
                                         AfPathCommand *out_commands, uint32_t capacity) {
    AfPathCommand commands[10] = {0};
    uint32_t count = 0U;
    const float w = size.x;
    const float h = size.y;
    const float k = 0.5522847498307936F;
    if (out_commands == NULL || !isfinite(w) || !isfinite(h) || w <= 0.0F || h <= 0.0F ||
        !isfinite(radius)) {
        return 0U;
    }
    if (type == AF_NODE_ELLIPSE) {
        const float x = w * 0.5F, y = h * 0.5F;
        commands[count++] = (AfPathCommand){.type = AF_PATH_MOVE, .point = {x, 0.0F}};
        commands[count++] = (AfPathCommand){.type = AF_PATH_CUBIC,
                                            .point = {w, y},
                                            .control1 = {x + k * x, 0.0F},
                                            .control2 = {w, y - k * y}};
        commands[count++] = (AfPathCommand){.type = AF_PATH_CUBIC,
                                            .point = {x, h},
                                            .control1 = {w, y + k * y},
                                            .control2 = {x + k * x, h}};
        commands[count++] = (AfPathCommand){.type = AF_PATH_CUBIC,
                                            .point = {0.0F, y},
                                            .control1 = {x - k * x, h},
                                            .control2 = {0.0F, y + k * y}};
        commands[count++] = (AfPathCommand){.type = AF_PATH_CUBIC,
                                            .point = {x, 0.0F},
                                            .control1 = {0.0F, y - k * y},
                                            .control2 = {x - k * x, 0.0F}};
    } else if (type == AF_NODE_RECTANGLE || type == AF_NODE_ROUNDED_RECTANGLE) {
        const float r =
            type == AF_NODE_RECTANGLE ? 0.0F : fminf(fmaxf(radius, 0.0F), fminf(w, h) * 0.5F);
        if (r == 0.0F) {
            commands[count++] = (AfPathCommand){.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}};
            commands[count++] = (AfPathCommand){.type = AF_PATH_LINE, .point = {w, 0.0F}};
            commands[count++] = (AfPathCommand){.type = AF_PATH_LINE, .point = {w, h}};
            commands[count++] = (AfPathCommand){.type = AF_PATH_LINE, .point = {0.0F, h}};
        } else {
            commands[count++] = (AfPathCommand){.type = AF_PATH_MOVE, .point = {r, 0.0F}};
            commands[count++] = (AfPathCommand){.type = AF_PATH_LINE, .point = {w - r, 0.0F}};
            commands[count++] = (AfPathCommand){.type = AF_PATH_CUBIC,
                                                .point = {w, r},
                                                .control1 = {w - r + k * r, 0.0F},
                                                .control2 = {w, r - k * r}};
            commands[count++] = (AfPathCommand){.type = AF_PATH_LINE, .point = {w, h - r}};
            commands[count++] = (AfPathCommand){.type = AF_PATH_CUBIC,
                                                .point = {w - r, h},
                                                .control1 = {w, h - r + k * r},
                                                .control2 = {w - r + k * r, h}};
            commands[count++] = (AfPathCommand){.type = AF_PATH_LINE, .point = {r, h}};
            commands[count++] = (AfPathCommand){.type = AF_PATH_CUBIC,
                                                .point = {0.0F, h - r},
                                                .control1 = {r - k * r, h},
                                                .control2 = {0.0F, h - r + k * r}};
            commands[count++] = (AfPathCommand){.type = AF_PATH_LINE, .point = {0.0F, r}};
            commands[count++] = (AfPathCommand){.type = AF_PATH_CUBIC,
                                                .point = {r, 0.0F},
                                                .control1 = {0.0F, r - k * r},
                                                .control2 = {r - k * r, 0.0F}};
        }
    } else {
        return 0U;
    }
    commands[count++] = (AfPathCommand){.type = AF_PATH_CLOSE};
    if (count > capacity) {
        return 0U;
    }
    memcpy(out_commands, commands, (size_t)count * sizeof(*commands));
    return count;
}

const char *af_editor_vector_tool_name(AfEditorVectorTool tool) {
    static const char *const names[AF_EDITOR_TOOL_COUNT] = {"Pick",
                                                            "Node",
                                                            "Scissors",
                                                            "Segment Delete",
                                                            "Knife",
                                                            "Rectangle",
                                                            "Rounded Rectangle",
                                                            "Ellipse",
                                                            "Line",
                                                            "Pen",
                                                            "Pencil",
                                                            "Polygon",
                                                            "Star",
                                                            "Arc",
                                                            "Spiral",
                                                            "Text",
                                                            "Gradient"};
    return tool < AF_EDITOR_TOOL_COUNT ? names[tool] : "Unknown";
}

const char *af_editor_vector_tool_label(AfEditorVectorTool tool) {
    static const char *const labels[AF_EDITOR_TOOL_COUNT] = {
        "PICK", "NODE",   "CUT",  "VDEL", "KNIFE", "RECT",   "ROUND", "OVAL", "LINE",
        "PEN",  "PENCIL", "POLY", "STAR", "ARC",   "SPIRAL", "TEXT",  "GRAD"};
    return tool < AF_EDITOR_TOOL_COUNT ? labels[tool] : "?";
}

bool af_editor_vector_tool_is_construction(AfEditorVectorTool tool) {
    return tool >= AF_EDITOR_TOOL_RECTANGLE && tool <= AF_EDITOR_TOOL_TEXT;
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

static AfVec2 af_editor_tool_lerp(AfVec2 first, AfVec2 second, float amount) {
    return (AfVec2){first.x + (second.x - first.x) * amount,
                    first.y + (second.y - first.y) * amount};
}

static AfVec2 af_editor_tool_curve_at(AfVec2 start, const AfPathCommand *command, float amount) {
    const AfVec2 first = af_editor_tool_lerp(start, command->control1, amount);
    const AfVec2 middle = af_editor_tool_lerp(command->control1, command->control2, amount);
    const AfVec2 last = af_editor_tool_lerp(command->control2, command->point, amount);
    return af_editor_tool_lerp(af_editor_tool_lerp(first, middle, amount),
                               af_editor_tool_lerp(middle, last, amount), amount);
}

static double af_editor_tool_cross(AfVec2 first, AfVec2 second) {
    return (double)first.x * second.y - (double)first.y * second.x;
}

static bool af_editor_tool_intersection(AfVec2 first_start, AfVec2 first_end, AfVec2 second_start,
                                        AfVec2 second_end, float *out_amount) {
    const AfVec2 first = {first_end.x - first_start.x, first_end.y - first_start.y};
    const AfVec2 second = {second_end.x - second_start.x, second_end.y - second_start.y};
    const AfVec2 offset = {second_start.x - first_start.x, second_start.y - first_start.y};
    const double denominator = af_editor_tool_cross(first, second);
    if (fabs(denominator) <= 1.0e-7) {
        return false;
    }
    const double amount = af_editor_tool_cross(offset, second) / denominator;
    const double knife_amount = af_editor_tool_cross(offset, first) / denominator;
    if (amount < 0.0 || amount > 1.0 || knife_amount < 0.0 || knife_amount > 1.0) {
        return false;
    }
    *out_amount = (float)amount;
    return true;
}

static float af_editor_tool_cut_amount(AfVec2 previous, const AfPathCommand *command, AfVec2 start,
                                       AfVec2 end) {
    const uint32_t samples = command->type == AF_PATH_CUBIC ? 32U : 1U;
    AfVec2 sample_start = previous;
    for (uint32_t sample = 1U; sample <= samples; ++sample) {
        const float t = (float)sample / (float)samples;
        const AfVec2 sample_end = command->type == AF_PATH_CUBIC
                                      ? af_editor_tool_curve_at(previous, command, t)
                                      : command->point;
        float local_amount = 0.0F;
        if (af_editor_tool_intersection(sample_start, sample_end, start, end, &local_amount)) {
            const float amount = ((float)(sample - 1U) + local_amount) / (float)samples;
            /* Avoid duplicate anchors when the gesture passes through an existing endpoint. */
            if (amount > 0.015F && amount < 0.985F) {
                return amount;
            }
        }
        sample_start = sample_end;
    }
    return -1.0F;
}

static void af_editor_tool_split_segment(AfVec2 start, AfPathCommand command, float amount,
                                         AfPathCommand *left, AfPathCommand *right) {
    *left = command;
    *right = command;
    if (command.type == AF_PATH_CUBIC) {
        const AfVec2 first = af_editor_tool_lerp(start, command.control1, amount);
        const AfVec2 middle = af_editor_tool_lerp(command.control1, command.control2, amount);
        const AfVec2 last = af_editor_tool_lerp(command.control2, command.point, amount);
        left->control1 = first;
        left->control2 = af_editor_tool_lerp(first, middle, amount);
        right->control1 = af_editor_tool_lerp(middle, last, amount);
        right->control2 = last;
        left->point = af_editor_tool_lerp(left->control2, right->control1, amount);
    } else {
        left->point = af_editor_tool_lerp(start, command.point, amount);
    }
}

static AfPathCommand af_editor_tool_explicit_segment(const AfPathCommand *commands,
                                                     uint32_t contour, uint32_t index) {
    return commands[index].type == AF_PATH_CLOSE
               ? (AfPathCommand){.type = AF_PATH_LINE, .point = commands[contour].point}
               : commands[index];
}

AfResult af_editor_vector_tool_knife(const AfPathCommand *commands, uint32_t command_count,
                                     AfVec2 start, AfVec2 end, AfPathCommand *out_commands,
                                     uint32_t capacity, AfEditorKnifeResult *out_result) {
    AfPathCommand cut[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY];
    float amounts[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY];
    AfEditorKnifeResult result = {.first_cut_command = UINT32_MAX};
    AfPath path = {
        .id = 1U, .node_id = 1U, .command_count = command_count, .geometry_generation = 1U};
    AfPathData data = {.paths = &path,
                       .path_count = 1U,
                       .path_capacity = 1U,
                       .commands = cut,
                       .command_count = command_count,
                       .command_capacity = AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY};
    uint32_t required = command_count;
    if (commands == NULL || out_commands == NULL || out_result == NULL || command_count == 0U ||
        !isfinite(start.x) || !isfinite(start.y) || !isfinite(end.x) || !isfinite(end.y) ||
        fabsf(start.x) > 1000000.0F || fabsf(start.y) > 1000000.0F || fabsf(end.x) > 1000000.0F ||
        fabsf(end.y) > 1000000.0F || hypotf(end.x - start.x, end.y - start.y) < 0.1F) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (command_count > AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memcpy(cut, commands, (size_t)command_count * sizeof(*commands));
    if (af_path_data_validate(&data) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t contour = 0U; contour < command_count;) {
        uint32_t limit = contour + 1U;
        uint32_t contour_cuts = 0U;
        while (limit < command_count && commands[limit].type != AF_PATH_MOVE) {
            ++limit;
        }
        for (uint32_t index = contour + 1U; index < limit; ++index) {
            const AfPathCommand segment = af_editor_tool_explicit_segment(commands, contour, index);
            amounts[index] =
                af_editor_tool_cut_amount(commands[index - 1U].point, &segment, start, end);
            if (amounts[index] >= 0.0F) {
                const uint32_t cut_index = result.cut_count + contour_cuts;
                result.source_commands[cut_index] = index;
                result.cut_points[cut_index] =
                    segment.type == AF_PATH_CUBIC
                        ? af_editor_tool_curve_at(commands[index - 1U].point, &segment,
                                                  amounts[index])
                        : af_editor_tool_lerp(commands[index - 1U].point, segment.point,
                                              amounts[index]);
            }
            contour_cuts += amounts[index] >= 0.0F;
        }
        required += contour_cuts * 2U;
        if (contour_cuts != 0U && commands[limit - 1U].type == AF_PATH_CLOSE) {
            /* Rotating a closed contour to its first cut eliminates one extra MOVE. */
            --required;
            if (commands[limit - 2U].point.x == commands[contour].point.x &&
                commands[limit - 2U].point.y == commands[contour].point.y) {
                --required; /* The closing edge is already explicit. */
            }
        }
        result.cut_count += contour_cuts;
        contour = limit;
    }
    if (result.cut_count == 0U) {
        return AF_RESULT_NOT_FOUND;
    }
    if (required > capacity || required > AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    for (uint32_t contour = 0U; contour < command_count;) {
        uint32_t limit = contour + 1U;
        uint32_t first_cut = UINT32_MAX;
        while (limit < command_count && commands[limit].type != AF_PATH_MOVE) {
            if (first_cut == UINT32_MAX && amounts[limit] >= 0.0F) {
                first_cut = limit;
            }
            ++limit;
        }
        if (first_cut == UINT32_MAX) {
            memcpy(&cut[result.command_count], &commands[contour],
                   (size_t)(limit - contour) * sizeof(*cut));
            result.command_count += limit - contour;
        } else {
            const bool closed = commands[limit - 1U].type == AF_PATH_CLOSE;
            uint32_t index = closed ? first_cut : contour + 1U;
            AfPathCommand closing_left = {0};
            if (closed) {
                AfPathCommand right = {0};
                af_editor_tool_split_segment(
                    commands[first_cut - 1U].point,
                    af_editor_tool_explicit_segment(commands, contour, first_cut),
                    amounts[first_cut], &closing_left, &right);
                if (result.first_cut_command == UINT32_MAX) {
                    result.first_cut_command = result.command_count;
                }
                cut[result.command_count++] =
                    (AfPathCommand){.type = AF_PATH_MOVE, .point = closing_left.point};
                cut[result.command_count++] = right;
                index = first_cut + 1U == limit ? contour + 1U : first_cut + 1U;
            } else {
                cut[result.command_count++] = commands[contour];
            }
            /* Walk each original segment once, wrapping at the old start for closed contours. */
            while (closed ? index != first_cut : index < limit) {
                const AfPathCommand segment =
                    af_editor_tool_explicit_segment(commands, contour, index);
                const bool empty_close = commands[index].type == AF_PATH_CLOSE &&
                                         commands[index - 1U].point.x == segment.point.x &&
                                         commands[index - 1U].point.y == segment.point.y;
                if (amounts[index] >= 0.0F) {
                    AfPathCommand left = {0};
                    AfPathCommand right = {0};
                    af_editor_tool_split_segment(commands[index - 1U].point, segment,
                                                 amounts[index], &left, &right);
                    if (result.first_cut_command == UINT32_MAX) {
                        result.first_cut_command = result.command_count;
                    }
                    cut[result.command_count++] = left;
                    cut[result.command_count++] =
                        (AfPathCommand){.type = AF_PATH_MOVE, .point = left.point};
                    cut[result.command_count++] = right;
                } else if (!empty_close) {
                    cut[result.command_count++] = segment;
                }
                ++index;
                if (closed && index == limit) {
                    index = contour + 1U;
                }
            }
            if (closed) {
                cut[result.command_count++] = closing_left;
            }
        }
        contour = limit;
    }
    memcpy(out_commands, cut, (size_t)result.command_count * sizeof(*cut));
    *out_result = result;
    return AF_RESULT_SUCCESS;
}

void af_editor_vector_reverse_open(const AfPathCommand *commands, uint32_t count,
                                   AfPathCommand *output) {
    output[0] = (AfPathCommand){.type = AF_PATH_MOVE,
                                .point = commands[count - 1U].point,
                                .handle_mode = commands[count - 1U].handle_mode};
    for (uint32_t i = 1U; i < count; ++i) {
        AfPathCommand segment = commands[count - i];
        segment.point = commands[count - i - 1U].point;
        segment.handle_mode = commands[count - i - 1U].handle_mode;
        if (segment.type == AF_PATH_CUBIC) {
            const AfVec2 control = segment.control1;
            segment.control1 = segment.control2;
            segment.control2 = control;
        }
        output[i] = segment;
    }
}

static bool af_vector_open_endpoint(const AfPathCommand *commands, uint32_t count,
                                    uint32_t endpoint, uint32_t *start, uint32_t *end) {
    if (endpoint >= count)
        return false;
    *start = endpoint;
    while (*start > 0U && commands[*start].type != AF_PATH_MOVE)
        --*start;
    *end = *start + 1U;
    while (*end < count && commands[*end].type != AF_PATH_MOVE)
        ++*end;
    return commands[*start].type == AF_PATH_MOVE && *end > *start + 1U &&
           commands[*end - 1U].type != AF_PATH_CLOSE &&
           (endpoint == *start || endpoint == *end - 1U);
}

AfResult af_editor_vector_join(const AfPathCommand *commands, uint32_t count, uint32_t first,
                               uint32_t second, AfPathCommand *output, uint32_t capacity,
                               uint32_t *out_count) {
    uint32_t a, b, c, d;
    AfPathCommand joined[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 1U];
    AfPathCommand tail[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY];
    if (!commands || !output || !out_count || first == second ||
        count > AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY ||
        !af_vector_open_endpoint(commands, count, first, &a, &b) ||
        !af_vector_open_endpoint(commands, count, second, &c, &d))
        return AF_RESULT_INVALID_ARGUMENT;
    uint32_t write = 0U;
    if (a == c) {
        memcpy(joined, commands, (size_t)b * sizeof(*joined));
        joined[b] = (AfPathCommand){.type = AF_PATH_CLOSE};
        memcpy(joined + b + 1U, commands + b, (size_t)(count - b) * sizeof(*joined));
        write = count + 1U;
    } else {
        /* Emit the joined contour first. The order of all untouched contours is preserved. */
        if (first == a)
            af_editor_vector_reverse_open(commands + a, b - a, joined);
        else
            memcpy(joined, commands + a, (size_t)(b - a) * sizeof(*joined));
        write = b - a;
        if (second != c)
            af_editor_vector_reverse_open(commands + c, d - c, tail);
        else
            memcpy(tail, commands + c, (size_t)(d - c) * sizeof(*tail));
        if (joined[write - 1U].point.x != tail[0].point.x ||
            joined[write - 1U].point.y != tail[0].point.y) {
            joined[write++] = (AfPathCommand){.type = AF_PATH_LINE, .point = tail[0].point};
        }
        memcpy(joined + write, tail + 1U, (size_t)(d - c - 1U) * sizeof(*joined));
        write += d - c - 1U;
        for (uint32_t i = 0; i < count; ++i)
            if ((i < a || i >= b) && (i < c || i >= d))
                joined[write++] = commands[i];
    }
    if (write > capacity)
        return AF_RESULT_LIMIT_EXCEEDED;
    memcpy(output, joined, (size_t)write * sizeof(*output));
    *out_count = write;
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_vector_combine(const AfPathCommand *commands, uint32_t count, uint32_t first,
                                  uint32_t second, bool merge, AfPathCommand *output,
                                  uint32_t capacity, uint32_t *out_count, uint32_t *out_selected) {
    if (!commands || !output || !out_count || !out_selected || first == second || first >= count ||
        second >= count || count > AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY ||
        commands[first].type == AF_PATH_CLOSE || commands[second].type == AF_PATH_CLOSE)
        return AF_RESULT_INVALID_ARGUMENT;
    AfPathCommand result[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 2U];
    AfPathCommand source[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY];
    memcpy(source, commands, (size_t)count * sizeof(*source));
    uint32_t a = 0, b = 0, c = 0, d = 0, write = count, selected = 0;
    const bool endpoints = af_vector_open_endpoint(source, count, first, &a, &b) &&
                           af_vector_open_endpoint(source, count, second, &c, &d);
    if (merge) {
        const AfVec2 point = {(source[first].point.x + source[second].point.x) * .5F,
                              (source[first].point.y + source[second].point.y) * .5F};
        const uint32_t indices[] = {first, second};
        for (uint32_t i = 0; i < 2; ++i) {
            const uint32_t index = indices[i];
            const AfVec2 delta = {point.x - source[index].point.x, point.y - source[index].point.y};
            source[index].point = point;
            source[index].control2.x += delta.x;
            source[index].control2.y += delta.y;
            if (index + 1 < count && source[index + 1].type == AF_PATH_CUBIC) {
                source[index + 1].control1.x += delta.x;
                source[index + 1].control1.y += delta.y;
            }
        }
        if (endpoints && a != c) {
            const AfResult joined =
                af_editor_vector_join(source, count, first, second, result,
                                      AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 2U, &write);
            if (joined != AF_RESULT_SUCCESS)
                return joined;
            selected = b - a - 1U;
        } else {
            const uint32_t keep = first < second ? first : second;
            const uint32_t remove = first < second ? second : first;
            if (source[remove].type == AF_PATH_MOVE)
                return AF_RESULT_INVALID_ARGUMENT;
            memcpy(result, source, (size_t)remove * sizeof(*result));
            memcpy(result + remove, source + remove + 1,
                   (size_t)(count - remove - 1U) * sizeof(*result));
            write = count - 1U;
            selected = keep;
            if (endpoints && a == c) {
                memmove(result + b, result + b - 1U, (size_t)(write - b + 1U) * sizeof(*result));
                result[b - 1U] = (AfPathCommand){.type = AF_PATH_CLOSE};
                ++write;
            }
        }
    } else if (endpoints) {
        const AfResult joined =
            af_editor_vector_join(source, count, first, second, result,
                                  AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 2U, &write);
        if (joined != AF_RESULT_SUCCESS)
            return joined;
        if (a == c) {
            memmove(result + b + 1U, result + b, (size_t)(write - b) * sizeof(*result));
            result[b] = (AfPathCommand){.type = AF_PATH_LINE, .point = source[a].point};
            ++write;
            selected = b;
        } else
            selected = b - a;
    } else if ((first + 1U == second || second + 1U == first) &&
               source[first > second ? first : second].type != AF_PATH_MOVE) {
        memcpy(result, source, (size_t)count * sizeof(*result));
        selected = first > second ? first : second;
        result[selected].type = AF_PATH_LINE;
    } else {
        memcpy(result, source, (size_t)count * sizeof(*result));
        result[write++] = (AfPathCommand){.type = AF_PATH_MOVE, .point = source[first].point};
        selected = write;
        result[write++] = (AfPathCommand){.type = AF_PATH_LINE, .point = source[second].point};
    }
    if (write > capacity)
        return AF_RESULT_LIMIT_EXCEEDED;
    AfPath path = {.id = 1, .node_id = 1, .command_count = write, .geometry_generation = 1};
    AfPathData data = {.paths = &path,
                       .path_count = 1,
                       .path_capacity = 1,
                       .commands = result,
                       .command_count = write,
                       .command_capacity = write};
    if (af_path_data_validate(&data) != AF_RESULT_SUCCESS)
        return AF_RESULT_INVALID_ARGUMENT;
    memcpy(output, result, (size_t)write * sizeof(*output));
    *out_count = write;
    *out_selected = selected;
    return AF_RESULT_SUCCESS;
}
