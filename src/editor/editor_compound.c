#include "editor/editor_commands.h"
#include "editor/editor_vector_tools.h"
#include "path/path_offset.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Compound construction is a structural edit. Reject dependencies whose meaning would
 * change when an object disappears or its local path coordinates are rebased. */
static AfResult af_compound_source(const AfProject *project, uint64_t id,
                                   AfProjectNodeSnapshot *snapshot) {
    AfResult result = af_project_snapshot(project, id, snapshot);
    if (result != AF_RESULT_SUCCESS)
        return result;
    if (snapshot->text.mode != AF_PROJECT_TEXT_NONE)
        return AF_RESULT_UNSUPPORTED_FEATURE;
    const AfSceneNode *node = &snapshot->node;
    if (node->type < AF_NODE_RECTANGLE || node->type > AF_NODE_PATH || snapshot->has_emitter ||
        snapshot->has_image_binding)
        return AF_RESULT_UNSUPPORTED_FEATURE;
    for (uint32_t i = 0; i < project->scene.count; ++i)
        if (project->scene.nodes[i].parent_id == id)
            return AF_RESULT_UNSUPPORTED_FEATURE;
    for (uint32_t i = 0; i < project->track_count; ++i)
        if (project->tracks[i].node_id == id)
            return AF_RESULT_UNSUPPORTED_FEATURE;
    for (uint32_t i = 0; i < project->binding_data.target_count; ++i)
        if (project->binding_data.targets[i].type == AF_BINDING_TARGET_NODE &&
            project->binding_data.targets[i].object_id == id)
            return AF_RESULT_UNSUPPORTED_FEATURE;
    for (uint32_t i = 0; i < project->style_data.count; ++i)
        if (project->style_data.styles[i].parent_style_id == snapshot->style.id)
            return AF_RESULT_UNSUPPORTED_FEATURE;
    const AfLayoutNode *parent = af_layout_find_const(&project->layout_data, node->parent_id);
    if (parent != NULL && parent->direction != AF_LAYOUT_DIRECTION_NONE)
        return AF_RESULT_UNSUPPORTED_FEATURE;
    AfStyle resolved;
    result = af_style_resolve(&project->style_data, &snapshot->style, &resolved);
    if (result != AF_RESULT_SUCCESS)
        return result;
    if (resolved.fill.type != AF_PAINT_SOLID || resolved.stroke_paint.type != AF_PAINT_SOLID)
        return AF_RESULT_UNSUPPORTED_FEATURE;
    return AF_RESULT_SUCCESS;
}

static AfResult af_compound_capture_path(const AfProject *project, uint64_t node_id,
                                         AfEditorCommand *command) {
    const AfPath *path = af_path_find_node_const(&project->path_data, node_id);
    if (path == NULL)
        return AF_RESULT_SUCCESS;
    if (command->path_before_count == AF_EDITOR_COMMAND_MAX_PATHS ||
        path->command_count >
            AF_EDITOR_COMMAND_MAX_PATH_COMMANDS - command->path_command_before_count)
        return AF_RESULT_LIMIT_EXCEEDED;
    AfProjectPathSnapshot *snapshot = &command->path_before[command->path_before_count];
    AfResult result = af_project_path_snapshot(
        project, path->id, snapshot,
        command->path_command_before + command->path_command_before_count,
        AF_EDITOR_COMMAND_MAX_PATH_COMMANDS - command->path_command_before_count);
    if (result == AF_RESULT_SUCCESS) {
        snapshot->command_offset = command->path_command_before_count;
        command->path_command_before_count += path->command_count;
        ++command->path_before_count;
    }
    return result;
}

static uint64_t af_compound_free_id(const AfProject *project, uint64_t first) {
    while (first < AF_INVALID_NODE_ID - 1U &&
           (af_scene_find_const(&project->scene, first) != NULL ||
            af_path_find_const(&project->path_data, first) != NULL ||
            af_style_find_const(&project->style_data, first) != NULL))
        ++first;
    return first < AF_INVALID_NODE_ID - 1U ? first : 0U;
}

static void af_compound_bound(AfVec2 point, AfVec2 *minimum, AfVec2 *maximum) {
    minimum->x = fminf(minimum->x, point.x);
    minimum->y = fminf(minimum->y, point.y);
    maximum->x = fmaxf(maximum->x, point.x);
    maximum->y = fmaxf(maximum->y, point.y);
}

/* Rebase controls without flattening curves. Keep the primary object's linear transform
 * (and therefore its stroke scaling); bake resolved layout into fixed bounds/placement. */
static AfResult af_compound_output(const AfProject *project, const AfProjectNodeSnapshot *source,
                                   uint64_t id, AfPathCommand *commands, uint32_t count,
                                   AfProjectNodeSnapshot *output) {
    AfVec2 flat_points[AF_MAX_PATH_FLATTENED_POINTS];
    AfPathContour contours[AF_MAX_PATH_CONTOURS];
    AfPath render_path = {.id = 1, .node_id = 1, .geometry_generation = 1, .command_count = count};
    AfPathData render_data = {.paths = &render_path,
                              .path_count = 1,
                              .path_capacity = 1,
                              .commands = commands,
                              .command_count = count,
                              .command_capacity = count};
    AfFlattenedPath flat = {.points = flat_points,
                            .point_capacity = AF_MAX_PATH_FLATTENED_POINTS,
                            .contours = contours,
                            .contour_capacity = AF_MAX_PATH_CONTOURS};
    AfResult flatten_result = af_path_flatten(&render_data, &render_path, .25F, &flat);
    if (flatten_result != AF_RESULT_SUCCESS)
        return flatten_result;
    AfVec2 minimum = commands[0].point, maximum = minimum;
    for (uint32_t i = 0; i < count; ++i) {
        if (commands[i].type != AF_PATH_CLOSE)
            af_compound_bound(commands[i].point, &minimum, &maximum);
        if (commands[i].type == AF_PATH_CUBIC) {
            af_compound_bound(commands[i].control1, &minimum, &maximum);
            af_compound_bound(commands[i].control2, &minimum, &maximum);
        }
    }
    *output = *source;
    AfAffine2 placement = source->node.world_transform;
    AfVec2 content_origin = {0};
    if (source->node.parent_id != AF_INVALID_NODE_ID) {
        const AfSceneNode *parent = af_scene_find_const(&project->scene, source->node.parent_id);
        AfAffine2 inverse;
        if (parent == NULL || !af_affine_inverse(parent->world_transform, &inverse))
            return AF_RESULT_INVALID_ARGUMENT;
        placement = af_affine_multiply(inverse, placement);
        const AfLayoutNode *layout = af_layout_find_const(&project->layout_data, parent->id);
        if (layout != NULL)
            content_origin = (AfVec2){layout->padding.left, layout->padding.top};
    } else {
        const AfLayoutData *layout = &project->layout_data;
        float sx = layout->target_size.x / layout->reference_size.x;
        float sy = layout->target_size.y / layout->reference_size.y;
        if (layout->scale_mode == AF_CANVAS_SCALE_NONE)
            sx = sy = 1.0F;
        else if (layout->scale_mode == AF_CANVAS_SCALE_FIT)
            sx = sy = fminf(sx, sy);
        else if (layout->scale_mode == AF_CANVAS_SCALE_FILL)
            sx = sy = fmaxf(sx, sy);
        const AfAffine2 canvas = {
            .m00 = sx,
            .m11 = sy,
            .tx = (layout->target_size.x - layout->reference_size.x * sx) * 0.5F,
            .ty = (layout->target_size.y - layout->reference_size.y * sy) * 0.5F};
        AfAffine2 inverse;
        if (!af_affine_inverse(canvas, &inverse))
            return AF_RESULT_INVALID_ARGUMENT;
        placement = af_affine_multiply(inverse, placement);
        content_origin = (AfVec2){layout->safe_area.left / sx, layout->safe_area.top / sy};
    }
    output->node.id = id;
    output->node.type = AF_NODE_PATH;
    output->node.polygon_sides = 0U;
    output->node.polygon_inset = 0.0F;
    output->node.corner_radius = 0.0F;
    output->node.bounds =
        (AfVec2){fmaxf(maximum.x - minimum.x, 0.1F), fmaxf(maximum.y - minimum.y, 0.1F)};
    output->node.local_transform.position = af_affine_transform_point(placement, minimum);
    output->node.local_transform.position.x -= content_origin.x;
    output->node.local_transform.position.y -= content_origin.y;
    output->node.world_transform =
        af_affine_multiply(source->node.world_transform,
                           (AfAffine2){.m00 = 1, .m11 = 1, .tx = minimum.x, .ty = minimum.y});
    output->node.resolved_bounds = output->node.bounds;
    output->layout = af_layout_node_default(id);
    output->layout.width = (AfLayoutValue){AF_LAYOUT_VALUE_FIXED, output->node.bounds.x};
    output->layout.height = (AfLayoutValue){AF_LAYOUT_VALUE_FIXED, output->node.bounds.y};
    output->has_layout = true;
    AfResult result = af_style_resolve(&project->style_data, &source->style, &output->style);
    if (result != AF_RESULT_SUCCESS)
        return result;
    output->style.id = id == source->node.id ? source->style.id : id;
    output->style.node_id = id;
    output->style.parent_style_id = 0U;
    output->style.inherit_flags = 0U;
    output->node.fill = output->style.fill.solid;
    for (uint32_t i = 0; i < count; ++i) {
        if (commands[i].type != AF_PATH_CLOSE) {
            commands[i].point.x -= minimum.x;
            commands[i].point.y -= minimum.y;
        }
        if (commands[i].type == AF_PATH_CUBIC) {
            commands[i].control1.x -= minimum.x;
            commands[i].control1.y -= minimum.y;
            commands[i].control2.x -= minimum.x;
            commands[i].control2.y -= minimum.y;
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_command_combine(AfProject *project, const uint64_t *ids, uint32_t count,
                                   uint64_t primary_id, AfEditorCommand *out_command) {
    if (project == NULL || ids == NULL || out_command == NULL || count < 2U ||
        count > AF_EDITOR_COMMAND_MAX_SNAPSHOTS)
        return AF_RESULT_INVALID_ARGUMENT;
    AfResult result = af_layout_evaluate_scene(&project->scene);
    if (result != AF_RESULT_SUCCESS)
        return result;
    AfProjectNodeSnapshot primary;
    result = af_compound_source(project, primary_id, &primary);
    if (result != AF_RESULT_SUCCESS)
        return result;
    AfAffine2 inverse;
    if (!af_affine_inverse(primary.node.world_transform, &inverse))
        return AF_RESULT_INVALID_ARGUMENT;
    *out_command = (AfEditorCommand){
        .type = AF_EDITOR_COMMAND_COMPOUND, .target_id = primary_id, .after_count = 1U};
    bool found_primary = false;
    uint32_t preceding = 0U;
    for (uint32_t i = 0; i < count; ++i) {
        for (uint32_t j = 0; j < i; ++j)
            if (ids[j] == ids[i])
                return AF_RESULT_INVALID_ARGUMENT;
        AfProjectNodeSnapshot *source = &out_command->before[i];
        result = af_compound_source(project, ids[i], source);
        if (result != AF_RESULT_SUCCESS)
            return result;
        if (source->node.parent_id != primary.node.parent_id)
            return AF_RESULT_UNSUPPORTED_FEATURE;
        found_primary |= ids[i] == primary_id;
        preceding += source->index < primary.index;
        ++out_command->before_count;
        result = af_compound_capture_path(project, ids[i], out_command);
        if (result != AF_RESULT_SUCCESS)
            return result;
        AfPathCommand *output =
            out_command->path_command_after + out_command->path_command_after_count;
        const uint32_t available =
            AF_EDITOR_COMMAND_MAX_PATH_COMMANDS - out_command->path_command_after_count;
        const AfPath *path = af_path_find_node_const(&project->path_data, ids[i]);
        uint32_t added = 0U;
        if (path != NULL) {
            if (path->command_count > available)
                return AF_RESULT_LIMIT_EXCEEDED;
            added = path->command_count;
            memcpy(output, project->path_data.commands + path->command_start,
                   added * sizeof(*output));
        } else {
            added = af_editor_vector_primitive_path(source->node.type, source->node.resolved_bounds,
                                                    source->node.corner_radius, output, available);
            if (added == 0U)
                return AF_RESULT_LIMIT_EXCEEDED;
        }
        const AfAffine2 transform = af_affine_multiply(inverse, source->node.world_transform);
        for (uint32_t j = 0; j < added; ++j) {
            if (output[j].type != AF_PATH_CLOSE)
                output[j].point = af_affine_transform_point(transform, output[j].point);
            if (output[j].type == AF_PATH_CUBIC) {
                output[j].control1 = af_affine_transform_point(transform, output[j].control1);
                output[j].control2 = af_affine_transform_point(transform, output[j].control2);
            }
        }
        out_command->path_command_after_count += added;
    }
    if (!found_primary)
        return AF_RESULT_INVALID_ARGUMENT;
    /* Snapshot insertion order is also the exact order restored by undo. */
    for (uint32_t i = 1; i < count; ++i) {
        AfProjectNodeSnapshot value = out_command->before[i];
        uint32_t j = i;
        while (j > 0U && out_command->before[j - 1U].index > value.index) {
            out_command->before[j] = out_command->before[j - 1U];
            --j;
        }
        out_command->before[j] = value;
    }
    result = af_compound_output(project, &primary, primary_id, out_command->path_command_after,
                                out_command->path_command_after_count, &out_command->after[0]);
    if (result != AF_RESULT_SUCCESS)
        return result;
    out_command->after[0].index = primary.index - preceding;
    const AfPath *existing = af_path_find_node_const(&project->path_data, primary_id);
    const uint64_t path_id =
        existing != NULL ? existing->id : af_compound_free_id(project, project->next_id);
    if (path_id == 0U || (existing != NULL && existing->geometry_generation == UINT64_MAX))
        return AF_RESULT_LIMIT_EXCEEDED;
    out_command->path_after_count = 1U;
    out_command->path_after[0] = (AfProjectPathSnapshot){
        .path = {.id = path_id,
                 .node_id = primary_id,
                 .command_count = out_command->path_command_after_count,
                 .fill_rule = AF_PATH_FILL_EVEN_ODD,
                 .geometry_generation = existing != NULL ? existing->geometry_generation + 1U : 1U},
        .index = project->path_data.path_count - out_command->path_before_count};
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_command_break_apart(AfProject *project, uint64_t node_id,
                                       AfEditorCommand *out_command) {
    if (project == NULL || out_command == NULL)
        return AF_RESULT_INVALID_ARGUMENT;
    AfResult result = af_layout_evaluate_scene(&project->scene);
    if (result != AF_RESULT_SUCCESS)
        return result;
    *out_command = (AfEditorCommand){
        .type = AF_EDITOR_COMMAND_COMPOUND, .target_id = node_id, .before_count = 1U};
    AfProjectNodeSnapshot *source = &out_command->before[0];
    result = af_compound_source(project, node_id, source);
    if (result != AF_RESULT_SUCCESS)
        return result;
    const AfPath *path = af_path_find_node_const(&project->path_data, node_id);
    if (path == NULL || path->geometry_generation == UINT64_MAX)
        return AF_RESULT_INVALID_ARGUMENT;
    result = af_compound_capture_path(project, node_id, out_command);
    if (result != AF_RESULT_SUCCESS)
        return result;
    const AfPathCommand *input = out_command->path_command_before;
    uint64_t next_id = project->next_id;
    for (uint32_t start = 0U; start < path->command_count;) {
        uint32_t end = start + 1U;
        while (end < path->command_count && input[end].type != AF_PATH_MOVE)
            ++end;
        if (out_command->after_count == AF_EDITOR_COMMAND_MAX_PATHS)
            return AF_RESULT_LIMIT_EXCEEDED;
        const uint32_t index = out_command->after_count;
        const uint64_t id = index == 0U ? node_id : af_compound_free_id(project, next_id);
        if (id == 0U)
            return AF_RESULT_LIMIT_EXCEEDED;
        if (index != 0U)
            next_id = id + 1U;
        AfPathCommand *output = out_command->path_command_after + start;
        memcpy(output, input + start, (end - start) * sizeof(*output));
        result = af_compound_output(project, source, id, output, end - start,
                                    &out_command->after[index]);
        if (result != AF_RESULT_SUCCESS)
            return result;
        out_command->after[index].index = source->index + index;
        if (index != 0U)
            (void)snprintf(out_command->after[index].name, AF_PROJECT_NAME_CAPACITY, "Path %llu",
                           (unsigned long long)id);
        out_command->path_after[index] =
            (AfProjectPathSnapshot){.path = {.id = index == 0U ? path->id : id,
                                             .node_id = id,
                                             .command_count = end - start,
                                             .fill_rule = path->fill_rule,
                                             .geometry_generation = path->geometry_generation + 1U},
                                    .index = out_command->path_before[0].index + index,
                                    .command_offset = start};
        ++out_command->after_count;
        start = end;
    }
    if (out_command->after_count < 2U)
        return AF_RESULT_INVALID_ARGUMENT;
    out_command->path_after_count = out_command->after_count;
    out_command->path_command_after_count = path->command_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_command_boolean(AfProject *project, uint64_t primary_id, uint64_t other_id,
                                   AfPathBooleanOperation operation, AfEditorCommand *out_command) {
    const uint64_t ids[] = {primary_id, other_id};
    AfResult result = af_editor_command_combine(project, ids, 2U, primary_id, out_command);
    if (result != AF_RESULT_SUCCESS)
        return result;
    /* Combine has already captured originals and placed both inputs in the primary frame.
     * Count the first operand's commands before its generated metadata is discarded. */
    const AfPath *original = af_path_find_node_const(&project->path_data, primary_id);
    AfPathCommand primitive[16];
    const AfSceneNode *node = af_scene_find_const(&project->scene, primary_id);
    const uint32_t first_count =
        original != NULL ? original->command_count
                         : af_editor_vector_primitive_path(node->type, node->resolved_bounds,
                                                           node->corner_radius, primitive, 16U);
    if (first_count == 0U)
        return AF_RESULT_INVALID_ARGUMENT;
    const AfPath *other = af_path_find_node_const(&project->path_data, other_id);
    AfPath paths[] = {{.id = 1U,
                       .node_id = 1U,
                       .command_count = first_count,
                       .geometry_generation = 1U,
                       .fill_rule = original != NULL ? original->fill_rule : AF_PATH_FILL_NON_ZERO},
                      {.id = 2U,
                       .node_id = 2U,
                       .command_start = first_count,
                       .command_count = out_command->path_command_after_count - first_count,
                       .geometry_generation = 1U,
                       .fill_rule = other != NULL ? other->fill_rule : AF_PATH_FILL_NON_ZERO}};
    AfPathData data = {.paths = paths,
                       .path_count = 2U,
                       .path_capacity = 2U,
                       .commands = out_command->path_command_after,
                       .command_count = out_command->path_command_after_count,
                       .command_capacity = AF_EDITOR_COMMAND_MAX_PATH_COMMANDS};
    const AfAffine2 world = node->world_transform;
    const float scale = sqrtf(world.m00 * world.m00 + world.m01 * world.m01 +
                              world.m10 * world.m10 + world.m11 * world.m11);
    uint32_t count = 0U;
    result = af_path_boolean(project->allocator, &data, &paths[0], &paths[1], operation,
                             0.1F / fmaxf(scale, 0.0001F), out_command->path_command_after,
                             AF_EDITOR_COMMAND_MAX_PATH_COMMANDS, &count);
    if (result != AF_RESULT_SUCCESS)
        return result;
    if (count == 0U)
        return AF_RESULT_NOT_FOUND;
    const AfProjectNodeSnapshot combined = out_command->after[0];
    result = af_compound_output(project, &combined, primary_id, out_command->path_command_after,
                                count, &out_command->after[0]);
    if (result != AF_RESULT_SUCCESS)
        return result;
    out_command->path_command_after_count = count;
    out_command->path_after[0].path.command_count = count;
    out_command->path_after[0].path.fill_rule = AF_PATH_FILL_NON_ZERO;
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_command_outline(AfProject *project, uint64_t id, bool offset, float distance,
                                   AfEditorCommand *command) {
    if (!project || !command)
        return AF_RESULT_INVALID_ARGUMENT;
    AfResult result = af_layout_evaluate_scene(&project->scene);
    if (result != AF_RESULT_SUCCESS)
        return result;
    AfProjectNodeSnapshot source;
    result = af_compound_source(project, id, &source);
    if (result != AF_RESULT_SUCCESS)
        return result;
    AfStyle style;
    result = af_style_resolve(&project->style_data, &source.style, &style);
    if (result != AF_RESULT_SUCCESS)
        return result;
    if (!offset &&
        (!style.stroke.enabled || style.stroke_opacity == 0 || style.stroke_paint.solid.a == 0))
        return AF_RESULT_INVALID_ARGUMENT;
    *command = (AfEditorCommand){.type = AF_EDITOR_COMMAND_COMPOUND,
                                 .target_id = id,
                                 .before_count = 1U,
                                 .after_count = 1U,
                                 .before = {source}};
    result = af_compound_capture_path(project, id, command);
    if (result != AF_RESULT_SUCCESS)
        return result;
    const AfPath *original = af_path_find_node_const(&project->path_data, id);
    AfPathCommand input[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    uint32_t count =
        original ? original->command_count
                 : af_editor_vector_primitive_path(source.node.type, source.node.resolved_bounds,
                                                   source.node.corner_radius, input,
                                                   AF_EDITOR_COMMAND_MAX_PATH_COMMANDS);
    if (!count || count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS)
        return AF_RESULT_LIMIT_EXCEEDED;
    if (original)
        memcpy(input, project->path_data.commands + original->command_start,
               count * sizeof(*input));
    AfPath path = {.id = 1U,
                   .node_id = 1U,
                   .geometry_generation = 1U,
                   .command_count = count,
                   .fill_rule = original ? original->fill_rule : AF_PATH_FILL_NON_ZERO};
    AfPathData data = {.paths = &path,
                       .path_count = 1U,
                       .path_capacity = 1U,
                       .commands = input,
                       .command_count = count,
                       .command_capacity = count};
    const AfAffine2 world = source.node.world_transform;
    const float tolerance =
        0.1F / fmaxf(0.0001F, sqrtf(world.m00 * world.m00 + world.m01 * world.m01 +
                                    world.m10 * world.m10 + world.m11 * world.m11));
    uint32_t output_count = 0U;
    result = offset
                 ? af_path_offset(project->allocator, &data, &path, distance, style.stroke.join,
                                  style.stroke.miter_limit, tolerance, command->path_command_after,
                                  AF_EDITOR_COMMAND_MAX_PATH_COMMANDS, &output_count)
                 : af_path_outline(project->allocator, &data, &path, &style.stroke, tolerance,
                                   command->path_command_after, AF_EDITOR_COMMAND_MAX_PATH_COMMANDS,
                                   &output_count);
    if (result != AF_RESULT_SUCCESS)
        return result;
    if (!output_count)
        return AF_RESULT_NOT_FOUND;
    const bool keep_fill = !offset && style.fill_opacity > 0 && style.fill.solid.a > 0;
    const uint64_t output_id = keep_fill ? af_compound_free_id(project, project->next_id) : id;
    const uint64_t path_id = keep_fill  ? output_id
                             : original ? original->id
                                        : af_compound_free_id(project, project->next_id);
    if (!path_id || !output_id || (original && original->geometry_generation == UINT64_MAX))
        return AF_RESULT_LIMIT_EXCEEDED;
    const uint32_t output_index = keep_fill ? 1U : 0U;
    AfProjectNodeSnapshot *expanded = &command->after[output_index];
    result = af_compound_output(project, &source, output_id, command->path_command_after,
                                output_count, expanded);
    if (result != AF_RESULT_SUCCESS)
        return result;
    if (!offset) {
        expanded->style.fill = style.stroke_paint;
        expanded->style.fill_opacity = style.stroke_opacity;
        expanded->style.stroke.enabled = false;
        expanded->node.fill = style.stroke_paint.solid;
        (void)af_style_mark_paint_changed(&expanded->style);
        (void)af_style_mark_stroke_geometry_changed(&expanded->style);
        (void)snprintf(expanded->name, sizeof(expanded->name), "Outline %llu",
                       (unsigned long long)output_id);
    }
    command->path_after_count = 1U;
    command->path_command_after_count = output_count;
    command->path_after[0] = (AfProjectPathSnapshot){
        .path = {.id = path_id,
                 .node_id = output_id,
                 .command_count = output_count,
                 .fill_rule = AF_PATH_FILL_NON_ZERO,
                 .geometry_generation = original ? original->geometry_generation + 1U : 1U},
        .index = project->path_data.path_count - command->path_before_count};
    if (keep_fill) {
        command->after[0] = source;
        command->after[0].style = style;
        command->after[0].style.parent_style_id = 0U;
        command->after[0].style.inherit_flags = 0U;
        command->after[0].style.stroke.enabled = false;
        (void)af_style_mark_stroke_geometry_changed(&command->after[0].style);
        command->after_count = 2U;
        expanded->index = source.index + 1U;
        if (original) {
            if (output_count + count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS)
                return AF_RESULT_LIMIT_EXCEEDED;
            command->path_after[1] = command->path_before[0];
            command->path_after[1].command_offset = output_count;
            command->path_after[1].index = command->path_after[0].index + 1U;
            memcpy(command->path_command_after + output_count, input, count * sizeof(*input));
            command->path_command_after_count += count;
            command->path_after_count = 2U;
        }
    }
    return AF_RESULT_SUCCESS;
}
