#include "style/style.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define AF_STYLE_EPSILON 1.0e-6F
#define AF_STYLE_MAX_WIDTH 100000.0F
#define AF_STYLE_PI 3.14159265358979323846F

static bool af_style_finite_color(AfColor color) {
    return isfinite(color.r) && isfinite(color.g) && isfinite(color.b) && isfinite(color.a) &&
           color.r >= 0.0F && color.r <= 1.0F && color.g >= 0.0F && color.g <= 1.0F &&
           color.b >= 0.0F && color.b <= 1.0F && color.a >= 0.0F && color.a <= 1.0F;
}

static bool af_style_finite_affine(AfAffine2 value) {
    return isfinite(value.m00) && isfinite(value.m01) && isfinite(value.m10) &&
           isfinite(value.m11) && isfinite(value.tx) && isfinite(value.ty);
}

AfPaint af_paint_solid(AfColor color) {
    return (AfPaint){.type = AF_PAINT_SOLID, .solid = color, .transform = af_affine_identity()};
}

AfStyle af_style_default(uint64_t id, uint64_t node_id, AfColor fill) {
    return (AfStyle){.id = id,
                     .node_id = node_id,
                     .fill = af_paint_solid(fill),
                     .stroke_paint = af_paint_solid((AfColor){0.0F, 0.0F, 0.0F, 1.0F}),
                     .stroke = {.width = 1.0F,
                                .end_width = 1.0F,
                                .cap = AF_STROKE_CAP_BUTT,
                                .join = AF_STROKE_JOIN_MITER,
                                .miter_limit = 4.0F},
                     .fill_opacity = 1.0F,
                     .stroke_opacity = 1.0F,
                     .paint_generation = 1U,
                     .stroke_geometry_generation = 1U};
}

const AfStyle *af_style_find_const(const AfStyleData *data, uint64_t style_id) {
    if (data == NULL || data->styles == NULL || style_id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < data->count; ++index) {
        if (data->styles[index].id == style_id) {
            return &data->styles[index];
        }
    }
    return NULL;
}

const AfStyle *af_style_find_node_const(const AfStyleData *data, uint64_t node_id) {
    if (data == NULL || data->styles == NULL || node_id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < data->count; ++index) {
        if (data->styles[index].node_id == node_id) {
            return &data->styles[index];
        }
    }
    return NULL;
}

static bool af_paint_valid(const AfPaint *paint) {
    float previous = -1.0F;
    if (paint == NULL || paint->type > AF_PAINT_CONIC_GRADIENT ||
        !af_style_finite_color(paint->solid) || !isfinite(paint->start.x) ||
        !isfinite(paint->start.y) || !isfinite(paint->end.x) || !isfinite(paint->end.y) ||
        !isfinite(paint->start_radius) || !isfinite(paint->end_radius) || !isfinite(paint->angle) ||
        !af_style_finite_affine(paint->transform) || paint->start_radius < 0.0F ||
        paint->end_radius < 0.0F || paint->start_radius > AF_STYLE_MAX_WIDTH ||
        paint->end_radius > AF_STYLE_MAX_WIDTH) {
        return false;
    }
    if (paint->type == AF_PAINT_SOLID) {
        return paint->stop_count == 0U;
    }
    if (paint->stop_count < 2U || paint->stop_count > AF_MAX_GRADIENT_STOPS) {
        return false;
    }
    for (uint32_t index = 0U; index < paint->stop_count; ++index) {
        const AfGradientStop *stop = &paint->stops[index];
        if (!isfinite(stop->offset) || stop->offset < 0.0F || stop->offset > 1.0F ||
            stop->offset < previous || !af_style_finite_color(stop->color)) {
            return false;
        }
        previous = stop->offset;
    }
    return true;
}

static bool af_stroke_valid(const AfStroke *stroke) {
    float dash_total = 0.0F;
    if (stroke == NULL || !isfinite(stroke->width) || !isfinite(stroke->end_width) ||
        stroke->width < 0.0F || stroke->end_width < 0.0F || stroke->width > AF_STYLE_MAX_WIDTH ||
        stroke->end_width > AF_STYLE_MAX_WIDTH || stroke->cap > AF_STROKE_CAP_SQUARE ||
        stroke->join > AF_STROKE_JOIN_BEVEL || !isfinite(stroke->miter_limit) ||
        stroke->miter_limit < 1.0F || stroke->miter_limit > 1000.0F ||
        stroke->dash_count > AF_MAX_STROKE_DASHES || !isfinite(stroke->dash_offset)) {
        return false;
    }
    for (uint32_t index = 0U; index < stroke->dash_count; ++index) {
        if (!isfinite(stroke->dashes[index]) || stroke->dashes[index] <= 0.0F ||
            stroke->dashes[index] > AF_STYLE_MAX_WIDTH) {
            return false;
        }
        dash_total += stroke->dashes[index];
    }
    return stroke->dash_count == 0U || (isfinite(dash_total) && dash_total > AF_STYLE_EPSILON);
}

AfResult af_style_validate(const AfStyle *style) {
    if (style == NULL || style->id == 0U || style->id == UINT64_MAX ||
        style->node_id == UINT64_MAX ||
        (style->inherit_flags &
         (uint32_t)~(AF_STYLE_INHERIT_FILL | AF_STYLE_INHERIT_STROKE_PAINT |
                     AF_STYLE_INHERIT_STROKE_GEOMETRY | AF_STYLE_INHERIT_OPACITY)) != 0U ||
        !af_paint_valid(&style->fill) || !af_paint_valid(&style->stroke_paint) ||
        !af_stroke_valid(&style->stroke) || !isfinite(style->fill_opacity) ||
        !isfinite(style->stroke_opacity) || style->fill_opacity < 0.0F ||
        style->fill_opacity > 1.0F || style->stroke_opacity < 0.0F ||
        style->stroke_opacity > 1.0F || style->paint_generation == 0U ||
        style->stroke_geometry_generation == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_style_data_validate(const AfStyleData *data) {
    if (data == NULL || (data->capacity != 0U && data->styles == NULL) ||
        data->count > data->capacity || data->capacity > AF_MAX_STYLES) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < data->count; ++index) {
        const AfStyle *style = &data->styles[index];
        if ((style->node_id == 0U && index >= AF_MAX_STYLE_PRESETS) ||
            af_style_validate(style) != AF_RESULT_SUCCESS) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t other = index + 1U; other < data->count; ++other) {
            if (style->id == data->styles[other].id ||
                (style->node_id != 0U && style->node_id == data->styles[other].node_id)) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        if (style->parent_style_id != 0U &&
            af_style_find_const(data, style->parent_style_id) == NULL) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (uint32_t index = 0U; index < data->count; ++index) {
        uint64_t parent = data->styles[index].parent_style_id;
        for (uint32_t depth = 0U; parent != 0U; ++depth) {
            const AfStyle *candidate = af_style_find_const(data, parent);
            if (candidate == NULL || candidate->id == data->styles[index].id ||
                depth >= AF_MAX_STYLE_PRESETS) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            parent = candidate->parent_style_id;
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_style_resolve(const AfStyleData *data, const AfStyle *style, AfStyle *out_style) {
    AfStyle chain[AF_MAX_STYLE_PRESETS + 1U];
    uint32_t count = 0U;
    if (data == NULL || style == NULL || out_style == NULL || data->count > data->capacity ||
        data->capacity > AF_MAX_STYLES || (data->capacity != 0U && data->styles == NULL) ||
        af_style_validate(style) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    chain[count++] = *style;
    while (chain[count - 1U].parent_style_id != 0U && count < AF_MAX_STYLE_PRESETS + 1U) {
        const AfStyle *parent = af_style_find_const(data, chain[count - 1U].parent_style_id);
        if (parent == NULL) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        chain[count++] = *parent;
    }
    *out_style = chain[count - 1U];
    while (count-- > 1U) {
        const AfStyle *child = &chain[count - 1U];
        const uint32_t flags = child->inherit_flags;
        const uint64_t id = child->id;
        const uint64_t node_id = child->node_id;
        if ((flags & AF_STYLE_INHERIT_FILL) == 0U) {
            out_style->fill = child->fill;
        }
        if ((flags & AF_STYLE_INHERIT_STROKE_PAINT) == 0U) {
            out_style->stroke_paint = child->stroke_paint;
        }
        if ((flags & AF_STYLE_INHERIT_STROKE_GEOMETRY) == 0U) {
            out_style->stroke = child->stroke;
            out_style->stroke_geometry_generation = child->stroke_geometry_generation;
        }
        if ((flags & AF_STYLE_INHERIT_OPACITY) == 0U) {
            out_style->fill_opacity = child->fill_opacity;
            out_style->stroke_opacity = child->stroke_opacity;
        }
        out_style->id = id;
        out_style->node_id = node_id;
        out_style->parent_style_id = child->parent_style_id;
        out_style->inherit_flags = flags;
        out_style->paint_generation = child->paint_generation;
    }
    return AF_RESULT_SUCCESS;
}

static AfColor af_color_mix(AfColor first, AfColor second, float amount) {
    return (AfColor){.r = first.r + (second.r - first.r) * amount,
                     .g = first.g + (second.g - first.g) * amount,
                     .b = first.b + (second.b - first.b) * amount,
                     .a = first.a + (second.a - first.a) * amount};
}

AfResult af_paint_sample(const AfPaint *paint, AfVec2 point, float opacity, AfColor *out_color) {
    AfAffine2 inverse;
    AfVec2 local;
    float position = 0.0F;
    if (paint == NULL || out_color == NULL || !af_paint_valid(paint) || !isfinite(point.x) ||
        !isfinite(point.y) || !isfinite(opacity) || opacity < 0.0F || opacity > 1.0F) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (paint->type == AF_PAINT_SOLID) {
        *out_color = paint->solid;
        out_color->a *= opacity;
        return AF_RESULT_SUCCESS;
    }
    if (!af_affine_inverse(paint->transform, &inverse)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    local = af_affine_transform_point(inverse, point);
    if (paint->type == AF_PAINT_LINEAR_GRADIENT) {
        const AfVec2 delta = {.x = paint->end.x - paint->start.x,
                              .y = paint->end.y - paint->start.y};
        const float denominator = delta.x * delta.x + delta.y * delta.y;
        position =
            denominator <= AF_STYLE_EPSILON
                ? 0.0F
                : ((local.x - paint->start.x) * delta.x + (local.y - paint->start.y) * delta.y) /
                      denominator;
    } else if (paint->type == AF_PAINT_RADIAL_GRADIENT) {
        const float distance = hypotf(local.x - paint->start.x, local.y - paint->start.y);
        const float span = paint->end_radius - paint->start_radius;
        position = fabsf(span) <= AF_STYLE_EPSILON ? 0.0F : (distance - paint->start_radius) / span;
    } else {
        const float radians =
            atan2f(local.y - paint->start.y, local.x - paint->start.x) - paint->angle;
        position = radians / (2.0F * AF_STYLE_PI);
        position -= floorf(position);
    }
    if (position <= paint->stops[0].offset) {
        *out_color = paint->stops[0].color;
    } else if (position >= paint->stops[paint->stop_count - 1U].offset) {
        *out_color = paint->stops[paint->stop_count - 1U].color;
    } else {
        for (uint32_t index = 1U; index < paint->stop_count; ++index) {
            if (position <= paint->stops[index].offset) {
                const float span = paint->stops[index].offset - paint->stops[index - 1U].offset;
                const float amount = span <= AF_STYLE_EPSILON
                                         ? 1.0F
                                         : (position - paint->stops[index - 1U].offset) / span;
                *out_color =
                    af_color_mix(paint->stops[index - 1U].color, paint->stops[index].color, amount);
                break;
            }
        }
    }
    out_color->a *= opacity;
    return AF_RESULT_SUCCESS;
}

AfResult af_style_mark_paint_changed(AfStyle *style) {
    if (style == NULL || style->paint_generation == UINT64_MAX) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    ++style->paint_generation;
    return AF_RESULT_SUCCESS;
}

AfResult af_style_mark_stroke_geometry_changed(AfStyle *style) {
    if (style == NULL || style->stroke_geometry_generation == UINT64_MAX) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    ++style->stroke_geometry_generation;
    return AF_RESULT_SUCCESS;
}

static AfVec2 af_vec_add(AfVec2 first, AfVec2 second) {
    return (AfVec2){first.x + second.x, first.y + second.y};
}

static AfVec2 af_vec_subtract(AfVec2 first, AfVec2 second) {
    return (AfVec2){first.x - second.x, first.y - second.y};
}

static AfVec2 af_vec_scale(AfVec2 value, float scale) {
    return (AfVec2){value.x * scale, value.y * scale};
}

static bool af_emit_triangle(AfVec2 *vertices, uint32_t capacity, uint32_t *count, AfVec2 a,
                             AfVec2 b, AfVec2 c) {
    if (*count > capacity || capacity - *count < 3U) {
        return false;
    }
    vertices[(*count)++] = a;
    vertices[(*count)++] = b;
    vertices[(*count)++] = c;
    return true;
}

static bool af_emit_round_cap(AfVec2 *vertices, uint32_t capacity, uint32_t *count, AfVec2 center,
                              AfVec2 direction, float radius, bool start) {
    const float base =
        atan2f(direction.y, direction.x) + (start ? AF_STYLE_PI * 0.5F : -AF_STYLE_PI * 0.5F);
    for (uint32_t index = 0U; index < AF_STROKE_ROUND_SEGMENTS; ++index) {
        const float first_angle =
            base + AF_STYLE_PI * (float)index / (float)AF_STROKE_ROUND_SEGMENTS;
        const float second_angle =
            base + AF_STYLE_PI * (float)(index + 1U) / (float)AF_STROKE_ROUND_SEGMENTS;
        const AfVec2 first = {center.x + cosf(first_angle) * radius,
                              center.y + sinf(first_angle) * radius};
        const AfVec2 second = {center.x + cosf(second_angle) * radius,
                               center.y + sinf(second_angle) * radius};
        if (!af_emit_triangle(vertices, capacity, count, center, first, second)) {
            return false;
        }
    }
    return true;
}

static bool af_emit_segment(AfVec2 *vertices, uint32_t capacity, uint32_t *count, AfVec2 first,
                            AfVec2 second, float first_width, float second_width, AfStrokeCap cap,
                            bool cap_start, bool cap_end) {
    AfVec2 direction = af_vec_subtract(second, first);
    const float length = hypotf(direction.x, direction.y);
    AfVec2 normal;
    if (length <= AF_STYLE_EPSILON) {
        return true;
    }
    direction = af_vec_scale(direction, 1.0F / length);
    if (cap == AF_STROKE_CAP_SQUARE) {
        if (cap_start) {
            first = af_vec_subtract(first, af_vec_scale(direction, first_width * 0.5F));
        }
        if (cap_end) {
            second = af_vec_add(second, af_vec_scale(direction, second_width * 0.5F));
        }
    }
    normal = (AfVec2){-direction.y, direction.x};
    {
        const AfVec2 first_left = af_vec_add(first, af_vec_scale(normal, first_width * 0.5F));
        const AfVec2 first_right = af_vec_subtract(first, af_vec_scale(normal, first_width * 0.5F));
        const AfVec2 second_left = af_vec_add(second, af_vec_scale(normal, second_width * 0.5F));
        const AfVec2 second_right =
            af_vec_subtract(second, af_vec_scale(normal, second_width * 0.5F));
        if (!af_emit_triangle(vertices, capacity, count, first_left, first_right, second_right) ||
            !af_emit_triangle(vertices, capacity, count, first_left, second_right, second_left)) {
            return false;
        }
    }
    if (cap == AF_STROKE_CAP_ROUND && first_width > 0.0F && cap_start &&
        !af_emit_round_cap(vertices, capacity, count, first, direction, first_width * 0.5F, true)) {
        return false;
    }
    return cap != AF_STROKE_CAP_ROUND || second_width <= 0.0F || !cap_end ||
           af_emit_round_cap(vertices, capacity, count, second, direction, second_width * 0.5F,
                             false);
}

static bool af_emit_join(AfVec2 *vertices, uint32_t capacity, uint32_t *count, AfVec2 previous,
                         AfVec2 point, AfVec2 next, float width, const AfStroke *stroke) {
    AfVec2 incoming = af_vec_subtract(point, previous);
    AfVec2 outgoing = af_vec_subtract(next, point);
    const float incoming_length = hypotf(incoming.x, incoming.y);
    const float outgoing_length = hypotf(outgoing.x, outgoing.y);
    float cross = 0.0F;
    AfVec2 first_outer;
    AfVec2 second_outer;
    if (incoming_length <= AF_STYLE_EPSILON || outgoing_length <= AF_STYLE_EPSILON ||
        width <= 0.0F) {
        return true;
    }
    incoming = af_vec_scale(incoming, 1.0F / incoming_length);
    outgoing = af_vec_scale(outgoing, 1.0F / outgoing_length);
    cross = incoming.x * outgoing.y - incoming.y * outgoing.x;
    if (fabsf(cross) <= AF_STYLE_EPSILON) {
        return true;
    }
    {
        const float side = cross > 0.0F ? -1.0F : 1.0F;
        first_outer = af_vec_add(
            point, (AfVec2){-incoming.y * width * 0.5F * side, incoming.x * width * 0.5F * side});
        second_outer = af_vec_add(
            point, (AfVec2){-outgoing.y * width * 0.5F * side, outgoing.x * width * 0.5F * side});
    }
    if (stroke->join == AF_STROKE_JOIN_ROUND) {
        float first_angle = atan2f(first_outer.y - point.y, first_outer.x - point.x);
        float delta = atan2f(second_outer.y - point.y, second_outer.x - point.x) - first_angle;
        if (cross > 0.0F && delta < 0.0F) {
            delta += 2.0F * AF_STYLE_PI;
        } else if (cross < 0.0F && delta > 0.0F) {
            delta -= 2.0F * AF_STYLE_PI;
        }
        for (uint32_t index = 0U; index < AF_STROKE_ROUND_SEGMENTS; ++index) {
            const float a = first_angle + delta * (float)index / (float)AF_STROKE_ROUND_SEGMENTS;
            const float b =
                first_angle + delta * (float)(index + 1U) / (float)AF_STROKE_ROUND_SEGMENTS;
            if (!af_emit_triangle(
                    vertices, capacity, count, point,
                    (AfVec2){point.x + cosf(a) * width * 0.5F, point.y + sinf(a) * width * 0.5F},
                    (AfVec2){point.x + cosf(b) * width * 0.5F, point.y + sinf(b) * width * 0.5F})) {
                return false;
            }
        }
        return true;
    }
    if (stroke->join == AF_STROKE_JOIN_MITER) {
        const AfVec2 first_direction = incoming;
        const AfVec2 second_direction = outgoing;
        const float denominator =
            first_direction.x * second_direction.y - first_direction.y * second_direction.x;
        if (fabsf(denominator) > AF_STYLE_EPSILON) {
            const AfVec2 between = af_vec_subtract(second_outer, first_outer);
            const float distance =
                (between.x * second_direction.y - between.y * second_direction.x) / denominator;
            const AfVec2 miter = af_vec_add(first_outer, af_vec_scale(first_direction, distance));
            if (hypotf(miter.x - point.x, miter.y - point.y) <=
                stroke->miter_limit * width * 0.5F) {
                return af_emit_triangle(vertices, capacity, count, point, first_outer,
                                        second_outer) &&
                       af_emit_triangle(vertices, capacity, count, first_outer, miter,
                                        second_outer);
            }
        }
    }
    return af_emit_triangle(vertices, capacity, count, point, first_outer, second_outer);
}

static float af_width_at(const AfStroke *stroke, float amount) {
    return stroke->width + (stroke->end_width - stroke->width) * amount;
}

static AfResult af_stroke_solid_contour(const AfFlattenedPath *path, const AfPathContour *contour,
                                        const AfStroke *stroke, AfVec2 *vertices, uint32_t capacity,
                                        uint32_t *count) {
    const uint32_t segment_count =
        contour->closed ? contour->point_count : contour->point_count - 1U;
    for (uint32_t segment = 0U; segment < segment_count; ++segment) {
        const uint32_t first_index = contour->point_start + segment;
        const uint32_t second_index = contour->point_start + (segment + 1U) % contour->point_count;
        const float first_amount = (float)segment / (float)segment_count;
        const float second_amount = (float)(segment + 1U) / (float)segment_count;
        if (!af_emit_segment(vertices, capacity, count, path->points[first_index],
                             path->points[second_index], af_width_at(stroke, first_amount),
                             af_width_at(stroke, second_amount), stroke->cap,
                             !contour->closed && segment == 0U,
                             !contour->closed && segment + 1U == segment_count)) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
    }
    for (uint32_t point_index = contour->closed ? 0U : 1U;
         point_index + (contour->closed ? 0U : 1U) < contour->point_count; ++point_index) {
        const uint32_t previous =
            contour->point_start + (point_index + contour->point_count - 1U) % contour->point_count;
        const uint32_t current = contour->point_start + point_index;
        const uint32_t next = contour->point_start + (point_index + 1U) % contour->point_count;
        if (!af_emit_join(vertices, capacity, count, path->points[previous], path->points[current],
                          path->points[next],
                          af_width_at(stroke, (float)point_index / (float)contour->point_count),
                          stroke)) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
    }
    return AF_RESULT_SUCCESS;
}

static AfResult af_stroke_dashed_contour(const AfFlattenedPath *path, const AfPathContour *contour,
                                         const AfStroke *stroke, AfVec2 *vertices,
                                         uint32_t capacity, uint32_t *count) {
    float pattern_length = 0.0F;
    float phase = 0.0F;
    uint32_t dash_index = 0U;
    bool draw = true;
    for (uint32_t index = 0U; index < stroke->dash_count; ++index) {
        pattern_length += stroke->dashes[index];
    }
    phase = fmodf(stroke->dash_offset, pattern_length);
    if (phase < 0.0F) {
        phase += pattern_length;
    }
    while (phase >= stroke->dashes[dash_index]) {
        phase -= stroke->dashes[dash_index];
        dash_index = (dash_index + 1U) % stroke->dash_count;
        draw = !draw;
    }
    for (uint32_t segment = 0U;
         segment < (contour->closed ? contour->point_count : contour->point_count - 1U);
         ++segment) {
        const AfVec2 first = path->points[contour->point_start + segment];
        const AfVec2 second =
            path->points[contour->point_start + (segment + 1U) % contour->point_count];
        const AfVec2 delta = af_vec_subtract(second, first);
        const float length = hypotf(delta.x, delta.y);
        float position = 0.0F;
        if (length <= AF_STYLE_EPSILON) {
            continue;
        }
        while (position < length) {
            const float remaining_pattern = stroke->dashes[dash_index] - phase;
            const float take = fminf(remaining_pattern, length - position);
            if (draw && take > AF_STYLE_EPSILON) {
                const float start_amount = position / length;
                const float end_amount = (position + take) / length;
                const float contour_start =
                    ((float)segment + start_amount) /
                    (float)(contour->closed ? contour->point_count : contour->point_count - 1U);
                const float contour_end =
                    ((float)segment + end_amount) /
                    (float)(contour->closed ? contour->point_count : contour->point_count - 1U);
                if (!af_emit_segment(vertices, capacity, count,
                                     af_vec_add(first, af_vec_scale(delta, start_amount)),
                                     af_vec_add(first, af_vec_scale(delta, end_amount)),
                                     af_width_at(stroke, contour_start),
                                     af_width_at(stroke, contour_end), stroke->cap, true, true)) {
                    return AF_RESULT_LIMIT_EXCEEDED;
                }
            }
            position += take;
            phase += take;
            if (phase + AF_STYLE_EPSILON >= stroke->dashes[dash_index]) {
                phase = 0.0F;
                dash_index = (dash_index + 1U) % stroke->dash_count;
                draw = !draw;
            }
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_stroke_tessellate(const AfFlattenedPath *path, const AfStroke *stroke,
                              AfVec2 *out_vertices, uint32_t vertex_capacity,
                              uint32_t *out_vertex_count) {
    uint32_t count = 0U;
    if (path == NULL || stroke == NULL || out_vertices == NULL || out_vertex_count == NULL ||
        path->points == NULL || path->contours == NULL ||
        path->point_count > path->point_capacity || path->contour_count > path->contour_capacity ||
        vertex_capacity > AF_MAX_STROKE_VERTICES || !af_stroke_valid(stroke)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_vertex_count = 0U;
    if (!stroke->enabled || (stroke->width <= 0.0F && stroke->end_width <= 0.0F)) {
        return AF_RESULT_SUCCESS;
    }
    for (uint32_t contour_index = 0U; contour_index < path->contour_count; ++contour_index) {
        const AfPathContour *contour = &path->contours[contour_index];
        AfResult result = AF_RESULT_SUCCESS;
        if (contour->point_count < 2U || contour->point_start > path->point_count ||
            contour->point_count > path->point_count - contour->point_start) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        result = stroke->dash_count == 0U
                     ? af_stroke_solid_contour(path, contour, stroke, out_vertices, vertex_capacity,
                                               &count)
                     : af_stroke_dashed_contour(path, contour, stroke, out_vertices,
                                                vertex_capacity, &count);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
    }
    *out_vertex_count = count;
    return AF_RESULT_SUCCESS;
}

AfResult af_stroke_cache_update(const AfPathData *paths, const AfPath *path,
                                const AfStyleData *styles, const AfStyle *style, float tolerance,
                                AfPathTessellationScratch *scratch, AfStrokeGeometryCache *cache) {
    AfVec2 points[AF_MAX_PATH_FLATTENED_POINTS];
    AfPathContour contours[AF_MAX_PATH_CONTOURS];
    AfFlattenedPath flattened = {.points = points,
                                 .point_capacity = AF_MAX_PATH_FLATTENED_POINTS,
                                 .contours = contours,
                                 .contour_capacity = AF_MAX_PATH_CONTOURS};
    uint32_t vertex_count = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    (void)scratch;
    if (paths == NULL || path == NULL || styles == NULL || style == NULL || cache == NULL ||
        cache->vertices == NULL || cache->vertex_capacity > AF_MAX_STROKE_VERTICES ||
        !isfinite(tolerance) || tolerance <= 0.0F) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (cache->path_source == paths && cache->style_source == styles &&
        cache->path_id == path->id &&
        cache->path_geometry_generation == path->geometry_generation &&
        cache->stroke_geometry_generation == style->stroke_geometry_generation) {
        return AF_RESULT_SUCCESS;
    }
    result = af_path_flatten(paths, path, tolerance, &flattened);
    if (result == AF_RESULT_SUCCESS) {
        result = af_stroke_tessellate(&flattened, &style->stroke, cache->vertices,
                                      cache->vertex_capacity, &vertex_count);
    }
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    cache->path_source = paths;
    cache->style_source = styles;
    cache->path_id = path->id;
    cache->path_geometry_generation = path->geometry_generation;
    cache->stroke_geometry_generation = style->stroke_geometry_generation;
    cache->vertex_count = vertex_count;
    ++cache->rebuild_count;
    return AF_RESULT_SUCCESS;
}
