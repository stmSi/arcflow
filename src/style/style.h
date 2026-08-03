#ifndef AF_STYLE_H
#define AF_STYLE_H

#include "arcflow/arcflow_result.h"
#include "core/math.h"
#include "path/path.h"

#include <stdbool.h>
#include <stdint.h>

#define AF_MAX_STYLES 4096U
#define AF_MAX_STYLE_PRESETS 128U
#define AF_MAX_GRADIENT_STOPS 16U
#define AF_MAX_STROKE_DASHES 16U
#define AF_MAX_STROKE_VERTICES AF_MAX_PATH_TESSELLATED_VERTICES
#define AF_STROKE_ROUND_SEGMENTS 12U

typedef enum AfPaintType {
    AF_PAINT_SOLID = 0,
    AF_PAINT_LINEAR_GRADIENT,
    AF_PAINT_RADIAL_GRADIENT,
    AF_PAINT_CONIC_GRADIENT
} AfPaintType;

typedef enum AfStrokeCap {
    AF_STROKE_CAP_BUTT = 0,
    AF_STROKE_CAP_ROUND,
    AF_STROKE_CAP_SQUARE
} AfStrokeCap;

typedef enum AfStrokeJoin {
    AF_STROKE_JOIN_MITER = 0,
    AF_STROKE_JOIN_ROUND,
    AF_STROKE_JOIN_BEVEL
} AfStrokeJoin;

typedef struct AfGradientStop {
    float offset;
    AfColor color;
} AfGradientStop;

typedef struct AfPaint {
    AfPaintType type;
    AfColor solid;
    AfVec2 start;
    AfVec2 end;
    float start_radius;
    float end_radius;
    float angle;
    AfAffine2 transform;
    AfGradientStop stops[AF_MAX_GRADIENT_STOPS];
    uint32_t stop_count;
} AfPaint;

typedef struct AfStroke {
    bool enabled;
    float width;
    float end_width;
    AfStrokeCap cap;
    AfStrokeJoin join;
    float miter_limit;
    float dashes[AF_MAX_STROKE_DASHES];
    uint32_t dash_count;
    float dash_offset;
} AfStroke;

enum {
    AF_STYLE_INHERIT_FILL = 1U << 0U,
    AF_STYLE_INHERIT_STROKE_PAINT = 1U << 1U,
    AF_STYLE_INHERIT_STROKE_GEOMETRY = 1U << 2U,
    AF_STYLE_INHERIT_OPACITY = 1U << 3U
};

typedef struct AfStyle {
    uint64_t id;
    uint64_t node_id;
    uint64_t parent_style_id;
    uint32_t inherit_flags;
    AfPaint fill;
    AfPaint stroke_paint;
    AfStroke stroke;
    float fill_opacity;
    float stroke_opacity;
    uint64_t paint_generation;
    uint64_t stroke_geometry_generation;
} AfStyle;

typedef struct AfStyleData {
    AfStyle *styles;
    uint32_t count;
    uint32_t capacity;
} AfStyleData;

typedef struct AfStrokeGeometryCache {
    const AfPathData *path_source;
    const AfStyleData *style_source;
    uint64_t path_id;
    uint64_t path_geometry_generation;
    uint64_t stroke_geometry_generation;
    AfVec2 *vertices;
    uint32_t vertex_count;
    uint32_t vertex_capacity;
    uint64_t rebuild_count;
} AfStrokeGeometryCache;

AfPaint af_paint_solid(AfColor color);
AfStyle af_style_default(uint64_t id, uint64_t node_id, AfColor fill);
AfResult af_style_validate(const AfStyle *style);
const AfStyle *af_style_find_const(const AfStyleData *data, uint64_t style_id);
const AfStyle *af_style_find_node_const(const AfStyleData *data, uint64_t node_id);
AfResult af_style_data_validate(const AfStyleData *data);
AfResult af_style_resolve(const AfStyleData *data, const AfStyle *style, AfStyle *out_style);
AfResult af_paint_sample(const AfPaint *paint, AfVec2 point, float opacity, AfColor *out_color);
AfResult af_style_mark_paint_changed(AfStyle *style);
AfResult af_style_mark_stroke_geometry_changed(AfStyle *style);
AfResult af_stroke_tessellate(const AfFlattenedPath *path, const AfStroke *stroke,
                              AfVec2 *out_vertices, uint32_t vertex_capacity,
                              uint32_t *out_vertex_count);
AfResult af_stroke_cache_update(const AfPathData *paths, const AfPath *path,
                                const AfStyleData *styles, const AfStyle *style, float tolerance,
                                AfPathTessellationScratch *scratch, AfStrokeGeometryCache *cache);

#endif
