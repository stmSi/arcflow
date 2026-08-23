#include "editor/editor_gizmo.h"

#include <math.h>
#include <stddef.h>

static AfVec2 af_editor_gizmo_midpoint(AfVec2 first, AfVec2 second) {
    return (AfVec2){(first.x + second.x) * 0.5F, (first.y + second.y) * 0.5F};
}

static AfVec2 af_editor_gizmo_offset(AfVec2 point, AfVec2 center, float distance) {
    const float x = point.x - center.x;
    const float y = point.y - center.y;
    const float length = sqrtf(x * x + y * y);
    if (length <= 0.0001F) {
        return point;
    }
    return (AfVec2){point.x + x * distance / length, point.y + y * distance / length};
}

static AfVec2 af_editor_gizmo_axis(AfVec2 origin, AfVec2 endpoint, float length) {
    const float x = endpoint.x - origin.x;
    const float y = endpoint.y - origin.y;
    const float magnitude = sqrtf(x * x + y * y);
    if (magnitude <= 0.0001F) {
        return origin;
    }
    return (AfVec2){origin.x + x * length / magnitude, origin.y + y * length / magnitude};
}

static bool af_editor_gizmo_near(AfVec2 left, AfVec2 right, float radius) {
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    return x * x + y * y <= radius * radius;
}

bool af_editor_gizmo_geometry(const AfSceneNode *node, AfAffine2 camera,
                              AfEditorGizmoGeometry *out_geometry) {
    AfAffine2 world = {0};
    AfVec2 top = {0};
    AfVec2 right = {0};
    AfVec2 bottom = {0};
    if (node == NULL || out_geometry == NULL || node->type == AF_NODE_GROUP ||
        !node->world_visible || node->bounds.x <= 0.0F || node->bounds.y <= 0.0F) {
        return false;
    }
    world = af_affine_multiply(camera, node->world_transform);
    *out_geometry = (AfEditorGizmoGeometry){0};
    out_geometry->corners[0] = af_affine_transform_point(world, (AfVec2){0.0F, 0.0F});
    out_geometry->corners[1] = af_affine_transform_point(world, (AfVec2){node->bounds.x, 0.0F});
    out_geometry->corners[2] = af_affine_transform_point(world, node->bounds);
    out_geometry->corners[3] = af_affine_transform_point(world, (AfVec2){0.0F, node->bounds.y});
    out_geometry->origin = out_geometry->corners[0];
    out_geometry->center =
        af_editor_gizmo_midpoint(out_geometry->corners[0], out_geometry->corners[2]);
    top = af_editor_gizmo_midpoint(out_geometry->corners[0], out_geometry->corners[1]);
    right = af_editor_gizmo_midpoint(out_geometry->corners[1], out_geometry->corners[2]);
    bottom = af_editor_gizmo_midpoint(out_geometry->corners[2], out_geometry->corners[3]);
    out_geometry->move_x =
        af_editor_gizmo_axis(out_geometry->origin, out_geometry->corners[1], 56.0F);
    out_geometry->move_y =
        af_editor_gizmo_axis(out_geometry->origin, out_geometry->corners[3], 56.0F);
    out_geometry->scale_x = right;
    out_geometry->scale_y = bottom;
    out_geometry->scale_xy = out_geometry->corners[2];
    out_geometry->skew_x = af_editor_gizmo_offset(bottom, out_geometry->center, 18.0F);
    out_geometry->skew_y = af_editor_gizmo_offset(right, out_geometry->center, 18.0F);
    out_geometry->rotate = af_editor_gizmo_offset(top, out_geometry->center, 32.0F);
    return true;
}

AfEditorGizmoHandle af_editor_gizmo_pick(const AfEditorGizmoGeometry *geometry, AfVec2 point,
                                         float radius) {
    if (geometry == NULL || radius <= 0.0F) {
        return AF_EDITOR_GIZMO_NONE;
    }
    if (af_editor_gizmo_near(point, geometry->rotate, radius)) {
        return AF_EDITOR_GIZMO_ROTATE;
    }
    if (af_editor_gizmo_near(point, geometry->skew_x, radius)) {
        return AF_EDITOR_GIZMO_SKEW_X;
    }
    if (af_editor_gizmo_near(point, geometry->skew_y, radius)) {
        return AF_EDITOR_GIZMO_SKEW_Y;
    }
    if (af_editor_gizmo_near(point, geometry->scale_xy, radius)) {
        return AF_EDITOR_GIZMO_SCALE_XY;
    }
    if (af_editor_gizmo_near(point, geometry->scale_x, radius)) {
        return AF_EDITOR_GIZMO_SCALE_X;
    }
    if (af_editor_gizmo_near(point, geometry->scale_y, radius)) {
        return AF_EDITOR_GIZMO_SCALE_Y;
    }
    if (af_editor_gizmo_near(point, geometry->move_x, radius)) {
        return AF_EDITOR_GIZMO_MOVE_X;
    }
    if (af_editor_gizmo_near(point, geometry->move_y, radius)) {
        return AF_EDITOR_GIZMO_MOVE_Y;
    }
    return AF_EDITOR_GIZMO_NONE;
}

AfPropertyType af_editor_gizmo_property(AfEditorGizmoHandle handle) {
    if (handle == AF_EDITOR_GIZMO_ROTATE) {
        return AF_PROPERTY_ROTATION;
    }
    if (handle == AF_EDITOR_GIZMO_SCALE_X || handle == AF_EDITOR_GIZMO_SCALE_Y ||
        handle == AF_EDITOR_GIZMO_SCALE_XY) {
        return AF_PROPERTY_SCALE;
    }
    if (handle == AF_EDITOR_GIZMO_SKEW_X || handle == AF_EDITOR_GIZMO_SKEW_Y) {
        return AF_PROPERTY_SKEW;
    }
    return AF_PROPERTY_POSITION;
}
