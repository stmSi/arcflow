#ifndef AF_PATH_BOOLEAN_H
#define AF_PATH_BOOLEAN_H

#include "core/allocator.h"
#include "path/path.h"

typedef enum AfPathBooleanOperation {
    AF_PATH_BOOLEAN_UNION = 0,
    AF_PATH_BOOLEAN_SUBTRACT,
    AF_PATH_BOOLEAN_INTERSECT
} AfPathBooleanOperation;

#define AF_PATH_BOOLEAN_MAX_EDGES 1024U
#define AF_PATH_BOOLEAN_MAX_PIECES 4096U

/* Closed contours only. Cubics are flattened at the supplied local-coordinate tolerance.
 * Both input fill rules are respected. Output uses nonzero filling and contains closed
 * line contours. Empty results succeed with zero commands. Output/count are unchanged
 * on failure, including complexity/precision rejection; input and output may alias. */
AfResult af_path_boolean(AfAllocator *allocator, const AfPathData *data, const AfPath *a,
                         const AfPath *b, AfPathBooleanOperation operation, float tolerance,
                         AfPathCommand *output, uint32_t capacity, uint32_t *out_count);

#endif
