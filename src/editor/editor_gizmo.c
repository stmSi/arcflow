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

bool af_editor_gizmo_geometry(const AfSceneNode *node, AfVec2 pivot, AfAffine2 camera,
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
    out_geometry->pivot = af_affine_transform_point(world, pivot);
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
    if (af_editor_gizmo_near(point, geometry->pivot, radius * 0.8F)) {
        return AF_EDITOR_GIZMO_PIVOT;
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

bool af_editor_gizmo_scale_transform(AfTransform2D start, AfVec2 pivot, AfVec2 bounds,
                                     AfEditorGizmoHandle handle, AfVec2 pointer, bool proportional,
                                     bool from_center, AfTransform2D *out_transform) {
    AfVec2 anchor = {0};
    AfVec2 handle_point = {0};
    float x_ratio = 1.0F;
    float y_ratio = 1.0F;
    AfAffine2 original;
    AfTransform2D linear_transform;
    AfAffine2 linear;
    AfVec2 fixed;
    if (out_transform == NULL || bounds.x <= 0.0F || bounds.y <= 0.0F ||
        (handle != AF_EDITOR_GIZMO_SCALE_X && handle != AF_EDITOR_GIZMO_SCALE_Y &&
         handle != AF_EDITOR_GIZMO_SCALE_XY)) {
        return false;
    }
    if (handle == AF_EDITOR_GIZMO_SCALE_X) {
        handle_point = (AfVec2){bounds.x, bounds.y * 0.5F};
        anchor = from_center ? (AfVec2){bounds.x * 0.5F, bounds.y * 0.5F}
                             : (AfVec2){0.0F, bounds.y * 0.5F};
    } else if (handle == AF_EDITOR_GIZMO_SCALE_Y) {
        handle_point = (AfVec2){bounds.x * 0.5F, bounds.y};
        anchor = from_center ? (AfVec2){bounds.x * 0.5F, bounds.y * 0.5F}
                             : (AfVec2){bounds.x * 0.5F, 0.0F};
    } else {
        handle_point = bounds;
        anchor = from_center ? (AfVec2){bounds.x * 0.5F, bounds.y * 0.5F} : (AfVec2){0};
    }
    if (fabsf(handle_point.x - anchor.x) > 0.0001F) {
        x_ratio = (pointer.x - anchor.x) / (handle_point.x - anchor.x);
    }
    if (fabsf(handle_point.y - anchor.y) > 0.0001F) {
        y_ratio = (pointer.y - anchor.y) / (handle_point.y - anchor.y);
    }
    if (handle == AF_EDITOR_GIZMO_SCALE_X) {
        y_ratio = proportional ? x_ratio : 1.0F;
    } else if (handle == AF_EDITOR_GIZMO_SCALE_Y) {
        x_ratio = proportional ? y_ratio : 1.0F;
    } else if (proportional) {
        const float ratio = fabsf(x_ratio - 1.0F) >= fabsf(y_ratio - 1.0F) ? x_ratio : y_ratio;
        x_ratio = ratio;
        y_ratio = ratio;
    }
    *out_transform = start;
    out_transform->scale.x = fmaxf(0.05F, start.scale.x * x_ratio);
    out_transform->scale.y = fmaxf(0.05F, start.scale.y * y_ratio);
    original = af_affine_from_transform_pivot(start, pivot);
    fixed = af_affine_transform_point(original, anchor);
    linear_transform = *out_transform;
    linear_transform.position = (AfVec2){0};
    linear = af_affine_from_transform(linear_transform);
    out_transform->position = (AfVec2){
        fixed.x - pivot.x - linear.m00 * (anchor.x - pivot.x) - linear.m01 * (anchor.y - pivot.y),
        fixed.y - pivot.y - linear.m10 * (anchor.x - pivot.x) - linear.m11 * (anchor.y - pivot.y)};
    return true;
}
