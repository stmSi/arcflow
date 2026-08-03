#ifndef AF_PATH_H
#define AF_PATH_H

#include "arcflow/arcflow_result.h"
#include "core/math.h"

#include <stdbool.h>
#include <stdint.h>

#define AF_MAX_PATHS 512U
#define AF_MAX_PATH_COMMANDS 32768U
#define AF_MAX_PATH_COMMANDS_PER_PATH 4096U
#define AF_MAX_PATH_CONTOURS 1024U
#define AF_MAX_PATH_FLATTENED_POINTS 2048U
#define AF_MAX_PATH_TESSELLATED_VERTICES 262144U
#define AF_PATH_MAX_SUBDIVISION_DEPTH 10U

typedef enum AfPathCommandType {
    AF_PATH_MOVE = 0,
    AF_PATH_LINE,
    AF_PATH_CUBIC,
    AF_PATH_CLOSE
} AfPathCommandType;

typedef enum AfPathFillRule { AF_PATH_FILL_NON_ZERO = 0, AF_PATH_FILL_EVEN_ODD } AfPathFillRule;

typedef struct AfPathCommand {
    AfPathCommandType type;
    AfVec2 point;
    AfVec2 control1;
    AfVec2 control2;
} AfPathCommand;

typedef struct AfPath {
    uint64_t id;
    uint64_t node_id;
    uint32_t command_start;
    uint32_t command_count;
    AfPathFillRule fill_rule;
    uint64_t geometry_generation;
} AfPath;

typedef struct AfPathData {
    AfPath *paths;
    uint32_t path_count;
    uint32_t path_capacity;
    AfPathCommand *commands;
    uint32_t command_count;
    uint32_t command_capacity;
} AfPathData;

typedef struct AfPathContour {
    uint32_t point_start;
    uint32_t point_count;
    bool closed;
} AfPathContour;

typedef struct AfFlattenedPath {
    AfVec2 *points;
    uint32_t point_count;
    uint32_t point_capacity;
    AfPathContour *contours;
    uint32_t contour_count;
    uint32_t contour_capacity;
} AfFlattenedPath;

typedef struct AfPathTessellationScratch {
    AfVec2 points[AF_MAX_PATH_FLATTENED_POINTS];
    AfPathContour contours[AF_MAX_PATH_CONTOURS];
    float y_breaks[AF_MAX_PATH_FLATTENED_POINTS * 2U];
    uint32_t edge_indices[AF_MAX_PATH_FLATTENED_POINTS];
    float intersections[AF_MAX_PATH_FLATTENED_POINTS];
    int8_t winding[AF_MAX_PATH_FLATTENED_POINTS];
} AfPathTessellationScratch;

typedef struct AfPathGeometryCache {
    const AfPathData *source;
    uint64_t path_id;
    uint64_t geometry_generation;
    AfVec2 *vertices;
    uint32_t vertex_count;
    uint32_t vertex_capacity;
    uint64_t rebuild_count;
} AfPathGeometryCache;

const AfPath *af_path_find_const(const AfPathData *data, uint64_t path_id);
const AfPath *af_path_find_node_const(const AfPathData *data, uint64_t node_id);
AfResult af_path_data_validate(const AfPathData *data);
AfResult af_path_flatten(const AfPathData *data, const AfPath *path, float tolerance,
                         AfFlattenedPath *out_flattened);
AfResult af_path_tessellate(const AfPathData *data, const AfPath *path, float tolerance,
                            AfPathTessellationScratch *scratch, AfVec2 *out_vertices,
                            uint32_t vertex_capacity, uint32_t *out_vertex_count);
AfResult af_path_cache_update(const AfPathData *data, const AfPath *path, float tolerance,
                              AfPathTessellationScratch *scratch, AfPathGeometryCache *cache);
AfResult af_path_mark_geometry_changed(AfPath *path);
bool af_path_hit_test(const AfPathData *data, const AfPath *path, AfVec2 point, float tolerance,
                      AfPathTessellationScratch *scratch);

#endif
