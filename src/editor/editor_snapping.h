#ifndef AF_EDITOR_SNAPPING_H
#define AF_EDITOR_SNAPPING_H
#include "af/scene/scene.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct AfEditorSnapBounds {
    AfVec2 minimum, maximum;
} AfEditorSnapBounds;
typedef struct AfEditorSnapTarget {
    AfEditorSnapBounds bounds;
    uint64_t id;
} AfEditorSnapTarget;
typedef struct AfEditorSnapMatch {
    bool active;
    float coordinate, minimum, maximum;
    uint32_t feature; /* 0: left/top, 1: center, 2: right/bottom. */
    uint64_t id;
} AfEditorSnapMatch;
typedef struct AfEditorSnapResult {
    AfVec2 correction;
    AfEditorSnapMatch axes[2];
} AfEditorSnapResult;
/* Visual grid only: minor spacing stays within [10,50) pixels. */
float af_editor_grid_interval(float base, float zoom);
/* World bounds use curve extrema, including path points outside the authored box. */
bool af_editor_scene_node_bounds(const AfScene *scene, const AfSceneNode *node, bool include_stroke,
                                 AfEditorSnapBounds *bounds);
/* A nonzero constraint restricts correction to that world-space direction. */
AfEditorSnapResult af_editor_snap_bounds(AfEditorSnapBounds moving,
                                         const AfEditorSnapTarget *targets, uint32_t count,
                                         float zoom, AfVec2 constraint);
#endif
