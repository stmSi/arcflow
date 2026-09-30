#include "editor/editor_snapping.h"
#include <float.h>
#include <math.h>

float af_editor_grid_interval(float base, float zoom) {
    if (!isfinite(base) || !isfinite(zoom) || base <= 0 || zoom <= 0)
        return 0;
    double step = base;
    while (step * zoom < 10)
        step *= 5;
    while (step * zoom >= 50)
        step /= 5;
    return step > FLT_MAX ? 0 : (float)step;
}
static float af_snap_feature(AfEditorSnapBounds b, uint32_t axis, uint32_t feature) {
    const float low = axis ? b.minimum.y : b.minimum.x, high = axis ? b.maximum.y : b.maximum.x;
    return low + (high - low) * (float)feature * .5F;
}
AfEditorSnapResult af_editor_snap_bounds(AfEditorSnapBounds moving,
                                         const AfEditorSnapTarget *targets, uint32_t count,
                                         float zoom, AfVec2 constraint) {
    AfEditorSnapResult result = {0};
    if (!isfinite(zoom) || zoom <= 0 || (!targets && count))
        return result;
    const float threshold = 8 / zoom;
    float best[2] = {threshold, threshold};
    const float length = hypotf(constraint.x, constraint.y);
    const bool constrained = length > .00001F;
    if (constrained) {
        constraint.x /= length;
        constraint.y /= length;
    }
    for (uint32_t axis = 0; axis < 2; ++axis) {
        const float component = axis ? constraint.y : constraint.x;
        if (constrained && fabsf(component) < .00001F)
            continue;
        for (uint32_t t = 0; t < count; ++t) {
            for (uint32_t source = 0; source < 3; ++source) {
                const float from = af_snap_feature(moving, axis, source);
                for (uint32_t feature = 0; feature < 3; ++feature) {
                    const float to = af_snap_feature(targets[t].bounds, axis, feature);
                    const float delta = to - from;
                    const float distance = fabsf(constrained ? delta / component : delta);
                    if (!isfinite(distance) || distance > best[axis] ||
                        (result.axes[axis].active && distance >= best[axis]))
                        continue;
                    if (constrained) {
                        if ((result.axes[0].active || result.axes[1].active) &&
                            distance >= fminf(best[0], best[1]))
                            continue;
                        result.axes[0].active = result.axes[1].active = false;
                        result.correction = (AfVec2){constraint.x * delta / component,
                                                     constraint.y * delta / component};
                        best[0] = best[1] = distance;
                    } else {
                        if (axis)
                            result.correction.y = delta;
                        else
                            result.correction.x = delta;
                        best[axis] = distance;
                    }
                    result.axes[axis] = (AfEditorSnapMatch){
                        .active = true,
                        .coordinate = to,
                        .minimum = fminf(af_snap_feature(moving, 1 - axis, 0),
                                         af_snap_feature(targets[t].bounds, 1 - axis, 0)),
                        .maximum = fmaxf(af_snap_feature(moving, 1 - axis, 2),
                                         af_snap_feature(targets[t].bounds, 1 - axis, 2)),
                        .feature = feature,
                        .id = targets[t].id};
                }
            }
        }
    }
    return result;
}

static void af_editor_bounds_point(AfEditorSnapBounds *bounds, AfVec2 point, bool *first) {
    if (*first) {
        *bounds = (AfEditorSnapBounds){point, point};
        *first = false;
        return;
    }
    bounds->minimum.x = fminf(bounds->minimum.x, point.x);
    bounds->maximum.x = fmaxf(bounds->maximum.x, point.x);
    bounds->minimum.y = fminf(bounds->minimum.y, point.y);
    bounds->maximum.y = fmaxf(bounds->maximum.y, point.y);
}
static void af_editor_bounds_cubic(AfEditorSnapBounds *bounds, AfVec2 a, AfVec2 b, AfVec2 c,
                                   AfVec2 d, bool *first) {
    af_editor_bounds_point(bounds, a, first);
    af_editor_bounds_point(bounds, d, first);
    for (uint32_t axis = 0; axis < 2; ++axis) {
        const double p0 = axis ? a.y : a.x, p1 = axis ? b.y : b.x, p2 = axis ? c.y : c.x,
                     p3 = axis ? d.y : d.x;
        const double aa = -p0 + 3 * p1 - 3 * p2 + p3, bb = 2 * (p0 - 2 * p1 + p2), cc = p1 - p0;
        double roots[2] = {0};
        uint32_t count = 0;
        if (fabs(aa) < 1e-12) {
            if (fabs(bb) > 1e-12)
                roots[count++] = -cc / bb;
        } else {
            const double discriminant = bb * bb - 4 * aa * cc;
            if (discriminant >= 0) {
                const double root = sqrt(discriminant);
                roots[count++] = (-bb + root) / (2 * aa);
                roots[count++] = (-bb - root) / (2 * aa);
            }
        }
        for (uint32_t i = 0; i < count; ++i) {
            const double t = roots[i], u = 1 - t;
            if (t <= 0 || t >= 1)
                continue;
            const AfVec2 p = {(float)(u * u * u * a.x + 3 * u * u * t * b.x + 3 * u * t * t * c.x +
                                      t * t * t * d.x),
                              (float)(u * u * u * a.y + 3 * u * u * t * b.y + 3 * u * t * t * c.y +
                                      t * t * t * d.y)};
            af_editor_bounds_point(bounds, p, first);
        }
    }
}
bool af_editor_scene_node_bounds(const AfScene *scene, const AfSceneNode *node, bool include_stroke,
                                 AfEditorSnapBounds *bounds) {
    if (!scene || !node || !bounds || !node->world_visible || node->world_opacity <= 0 ||
        node->type == AF_NODE_GROUP)
        return false;
    bool first = true;
    const AfPath *path = node->type == AF_NODE_PATH && scene->paths
                             ? af_path_find_node_const(scene->paths, node->id)
                             : NULL;
    if (path) {
        AfVec2 previous = {0};
        for (uint32_t i = 0; i < path->command_count; ++i) {
            const AfPathCommand *command = &scene->paths->commands[path->command_start + i];
            if (command->type == AF_PATH_CLOSE)
                continue;
            const AfVec2 point = af_affine_transform_point(node->world_transform, command->point);
            if (command->type == AF_PATH_CUBIC)
                af_editor_bounds_cubic(
                    bounds, previous,
                    af_affine_transform_point(node->world_transform, command->control1),
                    af_affine_transform_point(node->world_transform, command->control2), point,
                    &first);
            else
                af_editor_bounds_point(bounds, point, &first);
            previous = point;
        }
    } else {
        if (node->resolved_bounds.x <= 0 || node->resolved_bounds.y <= 0)
            return false;
        for (uint32_t i = 0; i < 4; ++i)
            af_editor_bounds_point(
                bounds,
                af_affine_transform_point(node->world_transform,
                                          (AfVec2){i & 1 ? node->resolved_bounds.x : 0,
                                                   i & 2 ? node->resolved_bounds.y : 0}),
                &first);
    }
    if (first)
        return false;
    if (include_stroke && scene->styles) {
        const AfStyle *source = af_style_find_node_const(scene->styles, node->id);
        AfStyle style;
        if (source && af_style_resolve(scene->styles, source, &style) == AF_RESULT_SUCCESS &&
            style.stroke.enabled) {
            float radius = .5F * fmaxf(style.stroke.width, style.stroke.end_width);
            /* Miter joins and square caps can extend beyond half the stroke width. */
            if (node->type == AF_NODE_PATH && style.stroke.join == AF_STROKE_JOIN_MITER)
                radius *= fmaxf(1, style.stroke.miter_limit);
            if (node->type == AF_NODE_PATH && style.stroke.cap == AF_STROKE_CAP_SQUARE)
                radius *= 1.4142135624F;
            const float dx = radius * hypotf(node->world_transform.m00, node->world_transform.m01);
            const float dy = radius * hypotf(node->world_transform.m10, node->world_transform.m11);
            bounds->minimum.x -= dx;
            bounds->maximum.x += dx;
            bounds->minimum.y -= dy;
            bounds->maximum.y += dy;
        }
    }
    return true;
}
