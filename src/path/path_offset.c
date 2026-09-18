#include "path/path_offset.h"
#include <math.h>
#include <stdalign.h>
#include <string.h>

typedef struct AfOutlineScratch {
    AfVec2 points[AF_MAX_PATH_FLATTENED_POINTS];
    AfPathContour contours[AF_MAX_PATH_CONTOURS];
    AfVec2 vertices[AF_PATH_BOOLEAN_MAX_EDGES];
    AfPathCommand commands[AF_MAX_PATH_COMMANDS_PER_PATH * 2U];
} AfOutlineScratch;

AfResult af_path_outline(AfAllocator *allocator, const AfPathData *data, const AfPath *path,
                         const AfStroke *stroke, float tolerance, AfPathCommand *output,
                         uint32_t capacity, uint32_t *out_count) {
    if (!allocator || !stroke || !output || !out_count || !isfinite(tolerance) || tolerance <= 0)
        return AF_RESULT_INVALID_ARGUMENT;
    AfOutlineScratch *scratch = af_allocate(allocator, sizeof(*scratch), alignof(AfOutlineScratch));
    if (!scratch)
        return AF_RESULT_OUT_OF_MEMORY;
    AfFlattenedPath flat = {.points = scratch->points,
                            .point_capacity = AF_MAX_PATH_FLATTENED_POINTS,
                            .contours = scratch->contours,
                            .contour_capacity = AF_MAX_PATH_CONTOURS};
    AfResult result = af_path_flatten(data, path, tolerance, &flat);
    uint32_t vertices = 0U, count = 0U;
    if (result == AF_RESULT_SUCCESS)
        result = af_stroke_tessellate(&flat, stroke, scratch->vertices, AF_PATH_BOOLEAN_MAX_EDGES,
                                      &vertices);
    for (uint32_t i = 0; result == AF_RESULT_SUCCESS && i < vertices; i += 3U) {
        AfVec2 a = scratch->vertices[i], b = scratch->vertices[i + 1U],
               c = scratch->vertices[i + 2U];
        const double area =
            ((double)b.x - a.x) * ((double)c.y - a.y) - ((double)b.y - a.y) * ((double)c.x - a.x);
        if (area == 0)
            continue;
        if (area < 0) {
            const AfVec2 swap = b;
            b = c;
            c = swap;
        }
        scratch->commands[count++] = (AfPathCommand){.type = AF_PATH_MOVE, .point = a};
        scratch->commands[count++] = (AfPathCommand){.type = AF_PATH_LINE, .point = b};
        scratch->commands[count++] = (AfPathCommand){.type = AF_PATH_LINE, .point = c};
        scratch->commands[count++] = (AfPathCommand){.type = AF_PATH_CLOSE};
    }
    if (result == AF_RESULT_SUCCESS && count != 0U) {
        AfPath triangles = {.id = 1U,
                            .node_id = 1U,
                            .geometry_generation = 1U,
                            .command_count = count,
                            .fill_rule = AF_PATH_FILL_NON_ZERO};
        AfPathData soup = {.paths = &triangles,
                           .path_count = 1U,
                           .path_capacity = 1U,
                           .commands = scratch->commands,
                           .command_count = count,
                           .command_capacity = count};
        result = af_path_boolean(allocator, &soup, &triangles, NULL, AF_PATH_BOOLEAN_UNION,
                                 tolerance, output, capacity, out_count);
    } else if (result == AF_RESULT_SUCCESS)
        *out_count = 0U;
    af_deallocate(allocator, scratch);
    return result;
}

AfResult af_path_offset(AfAllocator *allocator, const AfPathData *data, const AfPath *path,
                        float distance, AfStrokeJoin join, float miter_limit, float tolerance,
                        AfPathCommand *output, uint32_t capacity, uint32_t *out_count) {
    if (!allocator || !data || !path || !output || !out_count || !isfinite(distance) ||
        distance == 0 || fabsf(distance) > 1000000.0F)
        return AF_RESULT_INVALID_ARGUMENT;
    AfOutlineScratch *scratch = af_allocate(allocator, sizeof(*scratch), alignof(AfOutlineScratch));
    if (!scratch)
        return AF_RESULT_OUT_OF_MEMORY;
    /* Normalize first: internal overlapping contours must not create inset cuts. */
    uint32_t base_count = 0U, ring_count = 0U;
    AfResult result =
        af_path_boolean(allocator, data, path, NULL, AF_PATH_BOOLEAN_UNION, tolerance,
                        scratch->commands, AF_MAX_PATH_COMMANDS_PER_PATH, &base_count);
    AfPath paths[2] = {
        {.id = 1U, .node_id = 1U, .geometry_generation = 1U, .command_count = base_count},
        {.id = 2U, .node_id = 2U, .geometry_generation = 1U, .command_start = base_count}};
    AfPathData normalized = {.paths = paths,
                             .path_count = 1U,
                             .path_capacity = 2U,
                             .commands = scratch->commands,
                             .command_count = base_count,
                             .command_capacity = AF_MAX_PATH_COMMANDS_PER_PATH * 2U};
    const AfStroke stroke = {.enabled = true,
                             .width = fabsf(distance) * 2.0F,
                             .end_width = fabsf(distance) * 2.0F,
                             .join = join,
                             .miter_limit = miter_limit};
    if (result == AF_RESULT_SUCCESS && base_count > 0U)
        result = af_path_outline(allocator, &normalized, paths, &stroke, tolerance,
                                 scratch->commands + base_count, AF_MAX_PATH_COMMANDS_PER_PATH,
                                 &ring_count);
    if (result == AF_RESULT_SUCCESS && base_count > 0U && ring_count > 0U) {
        paths[1].command_count = ring_count;
        normalized.path_count = 2U;
        normalized.command_count += ring_count;
        result = af_path_boolean(allocator, &normalized, paths, paths + 1U,
                                 distance > 0 ? AF_PATH_BOOLEAN_UNION : AF_PATH_BOOLEAN_SUBTRACT,
                                 tolerance, output, capacity, out_count);
    } else if (result == AF_RESULT_SUCCESS)
        *out_count = 0U;
    af_deallocate(allocator, scratch);
    return result;
}
