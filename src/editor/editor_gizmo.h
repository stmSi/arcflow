#ifndef AF_EDITOR_GIZMO_H
#define AF_EDITOR_GIZMO_H

#include "af/animation/animation.h"
#include "af/scene/scene.h"

#include <stdbool.h>

typedef enum AfEditorGizmoHandle {
    AF_EDITOR_GIZMO_NONE = 0,
    AF_EDITOR_GIZMO_MOVE_X,
    AF_EDITOR_GIZMO_MOVE_Y,
    AF_EDITOR_GIZMO_SCALE_X,
    AF_EDITOR_GIZMO_SCALE_Y,
    AF_EDITOR_GIZMO_SCALE_XY,
    AF_EDITOR_GIZMO_SKEW_X,
    AF_EDITOR_GIZMO_SKEW_Y,
    AF_EDITOR_GIZMO_ROTATE,
    AF_EDITOR_GIZMO_PIVOT
} AfEditorGizmoHandle;

typedef struct AfEditorGizmoGeometry {
    AfVec2 corners[4];
    AfVec2 center;
    AfVec2 pivot;
    AfVec2 origin;
    AfVec2 move_x;
    AfVec2 move_y;
    AfVec2 scale_x;
    AfVec2 scale_y;
    AfVec2 scale_xy;
    AfVec2 skew_x;
    AfVec2 skew_y;
    AfVec2 rotate;
} AfEditorGizmoGeometry;

bool af_editor_gizmo_geometry(const AfSceneNode *node, AfVec2 pivot, AfAffine2 camera,
                              AfEditorGizmoGeometry *out_geometry);
AfEditorGizmoHandle af_editor_gizmo_pick(const AfEditorGizmoGeometry *geometry, AfVec2 point,
                                         float radius);
AfPropertyType af_editor_gizmo_property(AfEditorGizmoHandle handle);
bool af_editor_gizmo_scale_transform(AfTransform2D start, AfVec2 pivot, AfVec2 bounds,
                                     AfEditorGizmoHandle handle, AfVec2 pointer, bool proportional,
                                     bool from_center, AfTransform2D *out_transform);

#endif
