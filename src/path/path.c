#include "path/path.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

typedef struct AfPathEdge {
    AfVec2 first;
    AfVec2 second;
} AfPathEdge;

static bool af_path_point_finite(AfVec2 point) {
    return isfinite(point.x) && isfinite(point.y) && fabsf(point.x) <= 1000000.0F &&
           fabsf(point.y) <= 1000000.0F;
}

const AfPath *af_path_find_const(const AfPathData *data, uint64_t path_id) {
    if (data == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < data->path_count; ++index) {
        if (data->paths[index].id == path_id) {
            return &data->paths[index];
        }
    }
    return NULL;
}

const AfPath *af_path_find_node_const(const AfPathData *data, uint64_t node_id) {
    if (data == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < data->path_count; ++index) {
        if (data->paths[index].node_id == node_id) {
            return &data->paths[index];
        }
    }
    return NULL;
}

AfResult af_path_data_validate(const AfPathData *data) {
    if (data == NULL || (data->path_count != 0U && data->paths == NULL) ||
        (data->command_count != 0U && data->commands == NULL) ||
        data->path_count > data->path_capacity || data->path_capacity > AF_MAX_PATHS ||
        data->command_count > data->command_capacity ||
        data->command_capacity > AF_MAX_PATH_COMMANDS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t path_index = 0U; path_index < data->path_count; ++path_index) {
        const AfPath *path = &data->paths[path_index];
        bool contour_open = false;
        bool has_segment = false;
        if (path->id == 0U || path->node_id == 0U || path->fill_rule > AF_PATH_FILL_EVEN_ODD ||
            path->geometry_generation == 0U || path->command_count == 0U ||
            path->command_count > AF_MAX_PATH_COMMANDS_PER_PATH ||
            path->command_start > data->command_count ||
            path->command_count > data->command_count - path->command_start) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t other = path_index + 1U; other < data->path_count; ++other) {
            const AfPath *candidate = &data->paths[other];
            const uint64_t path_end = (uint64_t)path->command_start + path->command_count;
            const uint64_t candidate_end =
                (uint64_t)candidate->command_start + candidate->command_count;
            if (path->id == candidate->id || path->node_id == candidate->node_id ||
                ((uint64_t)path->command_start < candidate_end &&
                 (uint64_t)candidate->command_start < path_end)) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        for (uint32_t command_index = 0U; command_index < path->command_count; ++command_index) {
            const AfPathCommand *command = &data->commands[path->command_start + command_index];
            if (command->type > AF_PATH_CLOSE || !af_path_point_finite(command->point) ||
                !af_path_point_finite(command->control1) ||
                !af_path_point_finite(command->control2)) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            if (command->type == AF_PATH_MOVE) {
                if (contour_open && !has_segment) {
                    return AF_RESULT_INVALID_ARGUMENT;
                }
                contour_open = true;
                has_segment = false;
            } else if (command->type == AF_PATH_LINE || command->type == AF_PATH_CUBIC) {
                if (!contour_open) {
                    return AF_RESULT_INVALID_ARGUMENT;
                }
                has_segment = true;
            } else {
                if (!contour_open || !has_segment) {
                    return AF_RESULT_INVALID_ARGUMENT;
                }
                contour_open = false;
                has_segment = false;
            }
        }
        if (contour_open && !has_segment) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    return AF_RESULT_SUCCESS;
}

static float af_path_distance_to_line_squared(AfVec2 point, AfVec2 first, AfVec2 second) {
    const float dx = second.x - first.x;
    const float dy = second.y - first.y;
    const float length_squared = dx * dx + dy * dy;
    float parameter = 0.0F;
    if (length_squared > 1.0e-12F) {
        parameter = ((point.x - first.x) * dx + (point.y - first.y) * dy) / length_squared;
    }
    parameter = parameter < 0.0F ? 0.0F : (parameter > 1.0F ? 1.0F : parameter);
    {
        const float x = first.x + dx * parameter;
        const float y = first.y + dy * parameter;
        const float px = point.x - x;
        const float py = point.y - y;
        return px * px + py * py;
    }
}

static AfResult af_path_append_point(AfFlattenedPath *flattened, AfVec2 point) {
    if (flattened->point_count >= flattened->point_capacity ||
        flattened->point_count >= AF_MAX_PATH_FLATTENED_POINTS) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    flattened->points[flattened->point_count++] = point;
    return AF_RESULT_SUCCESS;
}

static AfResult af_path_flatten_cubic(AfVec2 first, AfVec2 control1, AfVec2 control2, AfVec2 last,
                                      float tolerance_squared, uint32_t depth,
                                      AfFlattenedPath *flattened) {
    const bool flat =
        af_path_distance_to_line_squared(control1, first, last) <= tolerance_squared &&
        af_path_distance_to_line_squared(control2, first, last) <= tolerance_squared;
    if (flat || depth >= AF_PATH_MAX_SUBDIVISION_DEPTH) {
        return af_path_append_point(flattened, last);
    }
    {
        const AfVec2 first_control = {(first.x + control1.x) * 0.5F, (first.y + control1.y) * 0.5F};
        const AfVec2 middle_control = {(control1.x + control2.x) * 0.5F,
                                       (control1.y + control2.y) * 0.5F};
        const AfVec2 last_control = {(control2.x + last.x) * 0.5F, (control2.y + last.y) * 0.5F};
        const AfVec2 left_control = {(first_control.x + middle_control.x) * 0.5F,
                                     (first_control.y + middle_control.y) * 0.5F};
        const AfVec2 right_control = {(middle_control.x + last_control.x) * 0.5F,
                                      (middle_control.y + last_control.y) * 0.5F};
        const AfVec2 middle = {(left_control.x + right_control.x) * 0.5F,
                               (left_control.y + right_control.y) * 0.5F};
        AfResult result = af_path_flatten_cubic(first, first_control, left_control, middle,
                                                tolerance_squared, depth + 1U, flattened);
        return result == AF_RESULT_SUCCESS
                   ? af_path_flatten_cubic(middle, right_control, last_control, last,
                                           tolerance_squared, depth + 1U, flattened)
                   : result;
    }
}

AfResult af_path_flatten(const AfPathData *data, const AfPath *path, float tolerance,
                         AfFlattenedPath *out_flattened) {
    AfVec2 current = {0};
    bool contour_open = false;
    if (data == NULL || path == NULL || out_flattened == NULL || out_flattened->points == NULL ||
        out_flattened->contours == NULL || !isfinite(tolerance) || tolerance <= 0.0F ||
        af_path_data_validate(data) != AF_RESULT_SUCCESS ||
        af_path_find_const(data, path->id) != path) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    out_flattened->point_count = 0U;
    out_flattened->contour_count = 0U;
    for (uint32_t index = 0U; index < path->command_count; ++index) {
        const AfPathCommand *command = &data->commands[path->command_start + index];
        AfResult result = AF_RESULT_SUCCESS;
        if (command->type == AF_PATH_MOVE) {
            if (out_flattened->contour_count >= out_flattened->contour_capacity ||
                out_flattened->contour_count >= AF_MAX_PATH_CONTOURS) {
                return AF_RESULT_LIMIT_EXCEEDED;
            }
            out_flattened->contours[out_flattened->contour_count++] =
                (AfPathContour){.point_start = out_flattened->point_count};
            result = af_path_append_point(out_flattened, command->point);
            current = command->point;
            contour_open = true;
        } else if (command->type == AF_PATH_LINE) {
            result = af_path_append_point(out_flattened, command->point);
            current = command->point;
        } else if (command->type == AF_PATH_CUBIC) {
            result =
                af_path_flatten_cubic(current, command->control1, command->control2, command->point,
                                      tolerance * tolerance, 0U, out_flattened);
            current = command->point;
        } else {
            out_flattened->contours[out_flattened->contour_count - 1U].closed = true;
            contour_open = false;
        }
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
        if ((command->type == AF_PATH_MOVE && out_flattened->contour_count > 1U) ||
            command->type == AF_PATH_CLOSE) {
            const uint32_t contour_index = command->type == AF_PATH_MOVE
                                               ? out_flattened->contour_count - 2U
                                               : out_flattened->contour_count - 1U;
            AfPathContour *contour = &out_flattened->contours[contour_index];
            const uint32_t end = command->type == AF_PATH_MOVE ? out_flattened->point_count - 1U
                                                               : out_flattened->point_count;
            contour->point_count = end - contour->point_start;
        }
    }
    if (contour_open && out_flattened->contour_count != 0U) {
        AfPathContour *contour = &out_flattened->contours[out_flattened->contour_count - 1U];
        contour->point_count = out_flattened->point_count - contour->point_start;
    }
    return AF_RESULT_SUCCESS;
}

static float af_path_edge_x(const AfPathEdge *edge, float y) {
    const float dy = edge->second.y - edge->first.y;
    if (fabsf(dy) <= 1.0e-12F) {
        return edge->first.x;
    }
    return edge->first.x + (edge->second.x - edge->first.x) * (y - edge->first.y) / dy;
}

static bool af_path_add_break(float *breaks, uint32_t *count, uint32_t capacity, float value) {
    if (!isfinite(value)) {
        return false;
    }
    for (uint32_t index = 0U; index < *count; ++index) {
        if (fabsf(breaks[index] - value) <= 1.0e-5F) {
            return true;
        }
    }
    if (*count >= capacity) {
        return false;
    }
    breaks[(*count)++] = value;
    return true;
}

static void af_path_sort_breaks(float *values, uint32_t count) {
    for (uint32_t index = 1U; index < count; ++index) {
        const float value = values[index];
        uint32_t position = index;
        while (position > 0U && values[position - 1U] > value) {
            values[position] = values[position - 1U];
            --position;
        }
        values[position] = value;
    }
}

static bool af_path_edge_crossing(const AfPathEdge *first, const AfPathEdge *second, float *out_y) {
    const float ax = first->second.x - first->first.x;
    const float ay = first->second.y - first->first.y;
    const float bx = second->second.x - second->first.x;
    const float by = second->second.y - second->first.y;
    const float denominator = ax * by - ay * bx;
    const float dx = second->first.x - first->first.x;
    const float dy = second->first.y - first->first.y;
    if (fabsf(denominator) <= 1.0e-8F) {
        return false;
    }
    {
        const float first_parameter = (dx * by - dy * bx) / denominator;
        const float second_parameter = (dx * ay - dy * ax) / denominator;
        if (first_parameter <= 1.0e-5F || first_parameter >= 0.99999F ||
            second_parameter <= 1.0e-5F || second_parameter >= 0.99999F) {
            return false;
        }
        *out_y = first->first.y + ay * first_parameter;
        return true;
    }
}

static AfResult af_path_emit_triangle(AfVec2 *vertices, uint32_t capacity, uint32_t *count,
                                      AfVec2 first, AfVec2 second, AfVec2 third) {
    const float area =
        (second.x - first.x) * (third.y - first.y) - (second.y - first.y) * (third.x - first.x);
    if (fabsf(area) <= 1.0e-8F) {
        return AF_RESULT_SUCCESS;
    }
    if (*count > capacity || capacity - *count < 3U) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    vertices[(*count)++] = first;
    vertices[(*count)++] = second;
    vertices[(*count)++] = third;
    return AF_RESULT_SUCCESS;
}

AfResult af_path_tessellate(const AfPathData *data, const AfPath *path, float tolerance,
                            AfPathTessellationScratch *scratch, AfVec2 *out_vertices,
                            uint32_t vertex_capacity, uint32_t *out_vertex_count) {
    AfFlattenedPath flattened = {0};
    AfPathEdge edges[AF_MAX_PATH_FLATTENED_POINTS];
    uint32_t edge_count = 0U;
    uint32_t break_count = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (scratch == NULL || out_vertices == NULL || out_vertex_count == NULL ||
        vertex_capacity > AF_MAX_PATH_TESSELLATED_VERTICES) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_vertex_count = 0U;
    flattened = (AfFlattenedPath){.points = scratch->points,
                                  .point_capacity = AF_MAX_PATH_FLATTENED_POINTS,
                                  .contours = scratch->contours,
                                  .contour_capacity = AF_MAX_PATH_CONTOURS};
    result = af_path_flatten(data, path, tolerance, &flattened);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    for (uint32_t contour_index = 0U; contour_index < flattened.contour_count; ++contour_index) {
        const AfPathContour *contour = &flattened.contours[contour_index];
        if (contour->point_count < 2U) {
            continue;
        }
        for (uint32_t point_index = 0U; point_index < contour->point_count; ++point_index) {
            const AfVec2 first = flattened.points[contour->point_start + point_index];
            const AfVec2 second =
                flattened
                    .points[contour->point_start + ((point_index + 1U) % contour->point_count)];
            if (fabsf(first.y - second.y) <= 1.0e-8F) {
                continue;
            }
            if (edge_count >= AF_MAX_PATH_FLATTENED_POINTS ||
                !af_path_add_break(scratch->y_breaks, &break_count,
                                   AF_MAX_PATH_FLATTENED_POINTS * 2U, first.y) ||
                !af_path_add_break(scratch->y_breaks, &break_count,
                                   AF_MAX_PATH_FLATTENED_POINTS * 2U, second.y)) {
                return AF_RESULT_LIMIT_EXCEEDED;
            }
            edges[edge_count++] = (AfPathEdge){.first = first, .second = second};
        }
    }
    for (uint32_t first = 0U; first < edge_count; ++first) {
        for (uint32_t second = first + 1U; second < edge_count; ++second) {
            float crossing_y = 0.0F;
            if (af_path_edge_crossing(&edges[first], &edges[second], &crossing_y) &&
                !af_path_add_break(scratch->y_breaks, &break_count,
                                   AF_MAX_PATH_FLATTENED_POINTS * 2U, crossing_y)) {
                return AF_RESULT_LIMIT_EXCEEDED;
            }
        }
    }
    af_path_sort_breaks(scratch->y_breaks, break_count);
    for (uint32_t band = 0U; band + 1U < break_count; ++band) {
        const float top = scratch->y_breaks[band];
        const float bottom = scratch->y_breaks[band + 1U];
        const float middle = (top + bottom) * 0.5F;
        uint32_t intersection_count = 0U;
        int32_t winding_count = 0;
        uint32_t span_start = UINT32_MAX;
        if (bottom - top <= 1.0e-6F) {
            continue;
        }
        for (uint32_t edge_index = 0U; edge_index < edge_count; ++edge_index) {
            const AfPathEdge *edge = &edges[edge_index];
            const float minimum = fminf(edge->first.y, edge->second.y);
            const float maximum = fmaxf(edge->first.y, edge->second.y);
            if (middle < minimum || middle >= maximum) {
                continue;
            }
            scratch->intersections[intersection_count] = af_path_edge_x(edge, middle);
            scratch->edge_indices[intersection_count] = edge_index;
            scratch->winding[intersection_count] = edge->second.y > edge->first.y ? 1 : -1;
            ++intersection_count;
        }
        for (uint32_t index = 1U; index < intersection_count; ++index) {
            const float x = scratch->intersections[index];
            const uint32_t edge_index = scratch->edge_indices[index];
            const int8_t winding = scratch->winding[index];
            uint32_t position = index;
            while (position > 0U && scratch->intersections[position - 1U] > x) {
                scratch->intersections[position] = scratch->intersections[position - 1U];
                scratch->edge_indices[position] = scratch->edge_indices[position - 1U];
                scratch->winding[position] = scratch->winding[position - 1U];
                --position;
            }
            scratch->intersections[position] = x;
            scratch->edge_indices[position] = edge_index;
            scratch->winding[position] = winding;
        }
        for (uint32_t intersection = 0U; intersection < intersection_count; ++intersection) {
            const bool was_inside = path->fill_rule == AF_PATH_FILL_EVEN_ODD
                                        ? (winding_count & 1) != 0
                                        : winding_count != 0;
            winding_count += path->fill_rule == AF_PATH_FILL_EVEN_ODD
                                 ? 1
                                 : (int32_t)scratch->winding[intersection];
            {
                const bool is_inside = path->fill_rule == AF_PATH_FILL_EVEN_ODD
                                           ? (winding_count & 1) != 0
                                           : winding_count != 0;
                if (!was_inside && is_inside) {
                    span_start = intersection;
                } else if (was_inside && !is_inside && span_start != UINT32_MAX) {
                    const AfPathEdge *left = &edges[scratch->edge_indices[span_start]];
                    const AfPathEdge *right = &edges[scratch->edge_indices[intersection]];
                    const AfVec2 top_left = {af_path_edge_x(left, top), top};
                    const AfVec2 top_right = {af_path_edge_x(right, top), top};
                    const AfVec2 bottom_left = {af_path_edge_x(left, bottom), bottom};
                    const AfVec2 bottom_right = {af_path_edge_x(right, bottom), bottom};
                    result = af_path_emit_triangle(out_vertices, vertex_capacity, out_vertex_count,
                                                   top_left, bottom_left, bottom_right);
                    if (result == AF_RESULT_SUCCESS) {
                        result =
                            af_path_emit_triangle(out_vertices, vertex_capacity, out_vertex_count,
                                                  top_left, bottom_right, top_right);
                    }
                    if (result != AF_RESULT_SUCCESS) {
                        return result;
                    }
                    span_start = UINT32_MAX;
                }
            }
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_path_cache_update(const AfPathData *data, const AfPath *path, float tolerance,
                              AfPathTessellationScratch *scratch, AfPathGeometryCache *cache) {
    AfResult result = AF_RESULT_SUCCESS;
    if (path == NULL || cache == NULL || cache->vertices == NULL ||
        cache->vertex_capacity > AF_MAX_PATH_TESSELLATED_VERTICES) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (cache->source == data && cache->path_id == path->id &&
        cache->geometry_generation == path->geometry_generation) {
        return AF_RESULT_SUCCESS;
    }
    result = af_path_tessellate(data, path, tolerance, scratch, cache->vertices,
                                cache->vertex_capacity, &cache->vertex_count);
    if (result == AF_RESULT_SUCCESS) {
        cache->source = data;
        cache->path_id = path->id;
        cache->geometry_generation = path->geometry_generation;
        ++cache->rebuild_count;
    }
    return result;
}

AfResult af_path_mark_geometry_changed(AfPath *path) {
    if (path == NULL || path->geometry_generation == 0U ||
        path->geometry_generation == UINT64_MAX) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    ++path->geometry_generation;
    return AF_RESULT_SUCCESS;
}

bool af_path_hit_test(const AfPathData *data, const AfPath *path, AfVec2 point, float tolerance,
                      AfPathTessellationScratch *scratch) {
    AfFlattenedPath flattened = {0};
    int32_t winding = 0;
    if (scratch == NULL || !af_path_point_finite(point)) {
        return false;
    }
    flattened = (AfFlattenedPath){.points = scratch->points,
                                  .point_capacity = AF_MAX_PATH_FLATTENED_POINTS,
                                  .contours = scratch->contours,
                                  .contour_capacity = AF_MAX_PATH_CONTOURS};
    if (af_path_flatten(data, path, tolerance, &flattened) != AF_RESULT_SUCCESS) {
        return false;
    }
    for (uint32_t contour_index = 0U; contour_index < flattened.contour_count; ++contour_index) {
        const AfPathContour *contour = &flattened.contours[contour_index];
        for (uint32_t index = 0U; index < contour->point_count; ++index) {
            const AfVec2 first = flattened.points[contour->point_start + index];
            const AfVec2 second =
                flattened.points[contour->point_start + ((index + 1U) % contour->point_count)];
            if ((first.y <= point.y && second.y > point.y) ||
                (second.y <= point.y && first.y > point.y)) {
                const float x =
                    first.x + (second.x - first.x) * (point.y - first.y) / (second.y - first.y);
                if (x > point.x) {
                    if (path->fill_rule == AF_PATH_FILL_EVEN_ODD) {
                        winding ^= 1;
                    } else {
                        winding += second.y > first.y ? 1 : -1;
                    }
                }
            }
        }
    }
    return winding != 0;
}
