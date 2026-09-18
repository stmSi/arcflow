#ifndef AF_PATH_OFFSET_H
#define AF_PATH_OFFSET_H
#include "path/path_boolean.h"
#include "style/style.h"

/* Converts the rendered stroke (caps, joins, taper and dashes) to nonzero filled boundaries.
 * Offset expands (+) or erodes (-) a closed fill with a solid constant-width stroke.
 * Curves are flattened. All outputs are unchanged on failure; empty offsets succeed. */
AfResult af_path_outline(AfAllocator *allocator, const AfPathData *data, const AfPath *path,
                         const AfStroke *stroke, float tolerance, AfPathCommand *output,
                         uint32_t capacity, uint32_t *out_count);
AfResult af_path_offset(AfAllocator *allocator, const AfPathData *data, const AfPath *path,
                        float distance, AfStrokeJoin join, float miter_limit, float tolerance,
                        AfPathCommand *output, uint32_t capacity, uint32_t *out_count);
#endif
