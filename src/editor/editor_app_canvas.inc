static bool af_editor_screen_bounds(const AfSceneNode *node, AfAffine2 camera, float *minimum_x,
                                    float *minimum_y, float *maximum_x, float *maximum_y) {
    AfAffine2 world;
    AfVec2 corners[4] = {0};
    if (node == NULL || node->type == AF_NODE_GROUP || !node->world_visible ||
        node->bounds.x <= 0.0F || node->bounds.y <= 0.0F) {
        return false;
    }
    world = af_affine_multiply(camera, node->world_transform);
    corners[0] = af_affine_transform_point(world, (AfVec2){0.0F, 0.0F});
    corners[1] = af_affine_transform_point(world, (AfVec2){node->bounds.x, 0.0F});
    corners[2] = af_affine_transform_point(world, node->bounds);
    corners[3] = af_affine_transform_point(world, (AfVec2){0.0F, node->bounds.y});
    *minimum_x = *maximum_x = corners[0].x;
    *minimum_y = *maximum_y = corners[0].y;
    for (uint32_t index = 1U; index < 4U; ++index) {
        *minimum_x = fminf(*minimum_x, corners[index].x);
        *minimum_y = fminf(*minimum_y, corners[index].y);
        *maximum_x = fmaxf(*maximum_x, corners[index].x);
        *maximum_y = fmaxf(*maximum_y, corners[index].y);
    }
    return true;
}

static void af_editor_render_outline(AfEditorState *editor, const AfSceneNode *node,
                                     AfAffine2 camera, AfColor color, int32_t layer) {
    AfEditorGizmoGeometry geometry = {0};
    if (!af_editor_gizmo_geometry(node, af_editor_node_pivot(editor, node), camera, &geometry)) {
        return;
    }
    for (uint32_t index = 0U; index < 4U; ++index) {
        af_editor_add_line(editor, geometry.corners[index], geometry.corners[(index + 1U) % 4U],
                           2.0F, color, layer);
    }
}

static void af_editor_render_selection_outlines(AfEditorState *editor, AfAffine2 camera) {
    for (uint32_t index = 0U; index < editor->selected_count; ++index) {
        const uint64_t id = editor->selected_ids[index];
        if (id != editor->selected_id) {
            af_editor_render_outline(editor, af_scene_find_const(&editor->preview_scene, id),
                                     camera, (AfColor){0.25F, 0.72F, 1.0F, 0.95F}, 89990);
        }
    }
}

static void af_editor_render_gizmo(AfEditorState *editor, AfAffine2 camera) {
    const AfSceneNode *selected = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    AfEditorGizmoGeometry geometry = {0};
    const AfColor move_x = {0.96F, 0.25F, 0.28F, 1.0F};
    const AfColor move_y = {0.28F, 0.86F, 0.38F, 1.0F};
    const AfColor scale = {0.25F, 0.72F, 1.0F, 1.0F};
    const AfColor skew = {0.96F, 0.46F, 0.88F, 1.0F};
    const AfColor rotate = {1.0F, 0.72F, 0.22F, 1.0F};
    const AfColor pivot = {1.0F, 0.86F, 0.3F, 1.0F};
    if (selected == NULL || selected->type == AF_NODE_GROUP) {
        return;
    }
    af_editor_render_outline(editor, selected, camera, (AfColor){0.98F, 0.8F, 0.22F, 1.0F}, 90000);
    if (editor->selected_count != 1U ||
        !af_editor_gizmo_geometry(selected, af_editor_node_pivot(editor, selected), camera,
                                  &geometry)) {
        return;
    }
    af_editor_add_line(editor, geometry.origin, geometry.move_x, 3.0F, move_x, 90010);
    af_editor_add_line(editor, geometry.origin, geometry.move_y, 3.0F, move_y, 90010);
    af_editor_add_rect(editor, geometry.move_x.x - 5.0F, geometry.move_x.y - 5.0F, 10.0F, 10.0F,
                       move_x, 90020);
    af_editor_add_rect(editor, geometry.move_y.x - 5.0F, geometry.move_y.y - 5.0F, 10.0F, 10.0F,
                       move_y, 90020);
    af_editor_add_rect(editor, geometry.scale_x.x - 5.0F, geometry.scale_x.y - 5.0F, 10.0F, 10.0F,
                       scale, 90020);
    af_editor_add_rect(editor, geometry.scale_y.x - 5.0F, geometry.scale_y.y - 5.0F, 10.0F, 10.0F,
                       scale, 90020);
    af_editor_add_rect(editor, geometry.scale_xy.x - 6.0F, geometry.scale_xy.y - 6.0F, 12.0F, 12.0F,
                       scale, 90020);
    {
        const AfVec2 skew_x_points[4] = {{geometry.skew_x.x, geometry.skew_x.y - 6.0F},
                                         {geometry.skew_x.x + 6.0F, geometry.skew_x.y},
                                         {geometry.skew_x.x, geometry.skew_x.y + 6.0F},
                                         {geometry.skew_x.x - 6.0F, geometry.skew_x.y}};
        const AfVec2 skew_y_points[4] = {{geometry.skew_y.x, geometry.skew_y.y - 6.0F},
                                         {geometry.skew_y.x + 6.0F, geometry.skew_y.y},
                                         {geometry.skew_y.x, geometry.skew_y.y + 6.0F},
                                         {geometry.skew_y.x - 6.0F, geometry.skew_y.y}};
        for (uint32_t index = 0U; index < 4U; ++index) {
            af_editor_add_line(editor, skew_x_points[index], skew_x_points[(index + 1U) % 4U], 2.0F,
                               skew, 90020);
            af_editor_add_line(editor, skew_y_points[index], skew_y_points[(index + 1U) % 4U], 2.0F,
                               skew, 90020);
        }
    }
    af_editor_add_shape(
        editor, AF_NODE_ELLIPSE,
        (AfAffine2){
            .m00 = 1.0F, .m11 = 1.0F, .tx = geometry.pivot.x - 7.0F, .ty = geometry.pivot.y - 7.0F},
        (AfVec2){14.0F, 14.0F}, pivot, 0.0F, 90028);
    af_editor_add_shape(
        editor, AF_NODE_ELLIPSE,
        (AfAffine2){
            .m00 = 1.0F, .m11 = 1.0F, .tx = geometry.pivot.x - 3.0F, .ty = geometry.pivot.y - 3.0F},
        (AfVec2){6.0F, 6.0F}, (AfColor){0.1F, 0.11F, 0.14F, 1.0F}, 0.0F, 90029);
    {
        const AfVec2 top = {(geometry.corners[0].x + geometry.corners[1].x) * 0.5F,
                            (geometry.corners[0].y + geometry.corners[1].y) * 0.5F};
        af_editor_add_line(editor, top, geometry.rotate, 2.0F, rotate, 90010);
        af_editor_add_shape(editor, AF_NODE_ELLIPSE,
                            (AfAffine2){.m00 = 1.0F,
                                        .m11 = 1.0F,
                                        .tx = geometry.rotate.x - 6.0F,
                                        .ty = geometry.rotate.y - 6.0F},
                            (AfVec2){12.0F, 12.0F}, rotate, 0.0F, 90020);
    }
    af_editor_add_text(editor, geometry.scale_xy.x + 9.0F, geometry.scale_xy.y + 7.0F, "SCALE",
                       0.55F, 34.0F, scale, 90025);
    af_editor_add_text(editor, geometry.skew_x.x + 9.0F, geometry.skew_x.y - 4.0F, "SKEW", 0.55F,
                       30.0F, skew, 90025);
    af_editor_add_text(editor, geometry.rotate.x + 9.0F, geometry.rotate.y - 4.0F, "ROTATE", 0.55F,
                       40.0F, rotate, 90025);
    af_editor_add_text(editor, geometry.pivot.x + 9.0F, geometry.pivot.y + 13.0F, "PIVOT", 0.5F,
                       34.0F, pivot, 90030);
}

static void af_editor_render_canvas_box(AfEditorState *editor) {
    const float minimum_x = fminf(editor->canvas_box_start.x, editor->canvas_box_current.x);
    const float minimum_y = fminf(editor->canvas_box_start.y, editor->canvas_box_current.y);
    const float maximum_x = fmaxf(editor->canvas_box_start.x, editor->canvas_box_current.x);
    const float maximum_y = fmaxf(editor->canvas_box_start.y, editor->canvas_box_current.y);
    const AfColor border = {0.28F, 0.74F, 1.0F, 1.0F};
    if (editor->drag_mode != AF_EDITOR_DRAG_CANVAS_BOX) {
        return;
    }
    af_editor_add_rect(editor, minimum_x, minimum_y, maximum_x - minimum_x, maximum_y - minimum_y,
                       (AfColor){0.2F, 0.62F, 1.0F, 0.16F}, 90030);
    af_editor_add_rect(editor, minimum_x, minimum_y, maximum_x - minimum_x, 2.0F, border, 90040);
    af_editor_add_rect(editor, minimum_x, maximum_y - 2.0F, maximum_x - minimum_x, 2.0F, border,
                       90040);
    af_editor_add_rect(editor, minimum_x, minimum_y, 2.0F, maximum_y - minimum_y, border, 90040);
    af_editor_add_rect(editor, maximum_x - 2.0F, minimum_y, 2.0F, maximum_y - minimum_y, border,
                       90040);
}

static AfVec2 af_editor_preview_cubic_point(AfVec2 start, const AfPathCommand *command, float t) {
    const float inverse = 1.0F - t;
    const float inverse_squared = inverse * inverse;
    const float t_squared = t * t;
    return (AfVec2){
        inverse_squared * inverse * start.x + 3.0F * inverse_squared * t * command->control1.x +
            3.0F * inverse * t_squared * command->control2.x + t_squared * t * command->point.x,
        inverse_squared * inverse * start.y + 3.0F * inverse_squared * t * command->control1.y +
            3.0F * inverse * t_squared * command->control2.y + t_squared * t * command->point.y};
}

static void af_editor_add_path_point(AfEditorState *editor, AfVec2 point, float size, AfColor color,
                                     int32_t layer) {
    af_editor_add_rect(editor, point.x - size * 0.5F - 2, point.y - size * 0.5F - 2, size + 4,
                       size + 4, (AfColor){.035F, .04F, .055F, 1}, layer);
    af_editor_add_rect(editor, point.x - size * 0.5F - 1, point.y - size * 0.5F - 1, size + 2,
                       size + 2, (AfColor){1, 1, 1, 1}, layer + 1);
    af_editor_add_rect(editor, point.x - size * 0.5F, point.y - size * 0.5F, size, size, color,
                       layer + 2);
}

static void af_editor_render_preview_segment(AfEditorState *editor, AfAffine2 camera, AfVec2 start,
                                             const AfPathCommand *command, uint32_t samples,
                                             float thickness, AfColor color, int32_t layer) {
    if (command->type != AF_PATH_CUBIC) {
        af_editor_add_line(editor, af_affine_transform_point(camera, start),
                           af_affine_transform_point(camera, command->point), thickness, color,
                           layer);
        return;
    }
    AfVec2 previous = af_affine_transform_point(camera, start);
    for (uint32_t sample = 1U; sample <= samples; ++sample) {
        const AfVec2 point = af_affine_transform_point(
            camera, af_editor_preview_cubic_point(start, command, (float)sample / (float)samples));
        af_editor_add_line(editor, previous, point, thickness, color, layer);
        previous = point;
    }
}

static void af_editor_render_preview_commands(AfEditorState *editor, AfAffine2 camera,
                                              AfVec2 origin, const AfPathCommand *commands,
                                              uint32_t command_count, AfColor color) {
    AfVec2 previous = {0};
    AfVec2 contour_start = {0};
    bool have_previous = false;
    for (uint32_t index = 0U; index < command_count; ++index) {
        AfPathCommand current = commands[index];
        current.point.x += origin.x;
        current.point.y += origin.y;
        current.control1.x += origin.x;
        current.control1.y += origin.y;
        current.control2.x += origin.x;
        current.control2.y += origin.y;
        if (current.type == AF_PATH_MOVE) {
            previous = current.point;
            contour_start = current.point;
            have_previous = true;
        } else if (current.type == AF_PATH_LINE && have_previous) {
            af_editor_add_line(editor, af_affine_transform_point(camera, previous),
                               af_affine_transform_point(camera, current.point), 2.5F, color,
                               90072);
            previous = current.point;
        } else if (current.type == AF_PATH_CUBIC && have_previous) {
            af_editor_render_preview_segment(editor, camera, previous, &current, 20U, 2.5F, color,
                                             90072);
            previous = current.point;
        } else if (current.type == AF_PATH_CLOSE && have_previous) {
            af_editor_add_line(editor, af_affine_transform_point(camera, previous),
                               af_affine_transform_point(camera, contour_start), 2.5F, color,
                               90072);
            have_previous = false;
        }
        if (current.type != AF_PATH_CLOSE) {
            const AfVec2 point = af_affine_transform_point(camera, current.point);
            af_editor_add_rect(editor, point.x - 2.5F, point.y - 2.5F, 5.0F, 5.0F, color, 90074);
        }
    }
}

static void af_editor_render_vector_tool_preview(AfEditorState *editor, int width, int height) {
    const AfAffine2 camera = af_editor_camera(editor, width, height);
    const AfColor line = {0.35F, 0.84F, 1.0F, 1.0F};
    const AfColor fill = {0.25F, 0.7F, 1.0F, 0.22F};
    if (editor->active_tool == AF_EDITOR_TOOL_PEN && editor->tool_command_count != 0U) {
        const uint32_t capacity = editor->render_scene.capacity;
        const uint32_t available = capacity - editor->render_scene.count;
        if (available <= AF_EDITOR_UI_RESERVE) {
            return;
        }
        const uint32_t budget = available - AF_EDITOR_UI_RESERVE;
        const uint32_t overhead = editor->tool_command_count + 32U;
        const uint32_t segments =
            editor->tool_command_count > 1U ? editor->tool_command_count - 1U : 1U;
        const uint32_t samples = (uint32_t)af_editor_clamp(
            budget > overhead ? (float)((budget - overhead) / segments) : 1.0F, 1.0F, 20.0F);
        const bool closing = editor->drag_mode != AF_EDITOR_DRAG_TOOL_CREATE &&
                             af_editor_pen_closes_at(editor, camera, editor->mouse_position);
        // Leave room for the inspector, timeline and menus even for a long Pen path.
        editor->render_scene.capacity = capacity - AF_EDITOR_UI_RESERVE;
        if (editor->drag_mode != AF_EDITOR_DRAG_TOOL_CREATE) {
            const AfVec2 anchor = editor->tool_commands[editor->tool_command_count - 1U].point;
            const AfVec2 target =
                closing ? editor->tool_commands[0].point
                        : af_editor_tool_snap_point(
                              editor, af_editor_screen_to_project(editor, width, height,
                                                                  editor->mouse_position));
            const AfPathCommand next =
                closing ? af_editor_pen_closing_segment(editor)
                        : (AfPathCommand){.type = editor->tool_pen_has_outgoing ? AF_PATH_CUBIC
                                                                                : AF_PATH_LINE,
                                          .point = target,
                                          .control1 = editor->tool_pen_outgoing,
                                          .control2 = target};
            af_editor_render_preview_segment(editor, camera, anchor, &next, 20U, 2.0F,
                                             closing ? (AfColor){1.0F, 0.74F, 0.2F, 1.0F}
                                                     : (AfColor){0.5F, 0.75F, 0.9F, 0.9F},
                                             90070);
        }
        if (editor->tool_pen_has_outgoing) {
            const AfVec2 anchor = af_affine_transform_point(
                camera, editor->tool_commands[editor->tool_command_count - 1U].point);
            const AfVec2 outgoing = af_affine_transform_point(camera, editor->tool_pen_outgoing);
            af_editor_add_line(editor, anchor, outgoing, 1.2F, (AfColor){0.72F, 0.54F, 1.0F, 0.95F},
                               90072);
            af_editor_add_rect(editor, outgoing.x - 3.0F, outgoing.y - 3.0F, 6.0F, 6.0F,
                               (AfColor){0.72F, 0.54F, 1.0F, 1.0F}, 90074);
        }
        for (uint32_t cursor = editor->tool_command_count; cursor > 0U; --cursor) {
            const uint32_t index = cursor - 1U;
            const AfVec2 point =
                af_affine_transform_point(camera, editor->tool_commands[index].point);
            const float size = index == 0U && closing ? 12.0F : 8.0F;
            af_editor_add_rect(editor, point.x - size * 0.5F, point.y - size * 0.5F, size, size,
                               index == 0U ? (AfColor){1.0F, 0.74F, 0.2F, 1.0F} : line, 90075);
        }
        for (uint32_t cursor = editor->tool_command_count; cursor > 1U; --cursor) {
            const uint32_t index = cursor - 1U;
            const AfPathCommand *command = &editor->tool_commands[index];
            const AfVec2 previous =
                af_affine_transform_point(camera, editor->tool_commands[index - 1U].point);
            const AfVec2 point = af_affine_transform_point(camera, command->point);
            if (command->type == AF_PATH_CUBIC && index + 1U == editor->tool_command_count) {
                const AfVec2 first = af_affine_transform_point(camera, command->control1);
                const AfVec2 second = af_affine_transform_point(camera, command->control2);
                af_editor_add_line(editor, previous, first, 1.2F,
                                   (AfColor){0.58F, 0.7F, 0.9F, 0.9F}, 90072);
                af_editor_add_line(editor, second, point, 1.2F, (AfColor){0.58F, 0.7F, 0.9F, 0.9F},
                                   90072);
                af_editor_add_rect(editor, first.x - 3.0F, first.y - 3.0F, 6.0F, 6.0F, line, 90074);
                af_editor_add_rect(editor, second.x - 3.0F, second.y - 3.0F, 6.0F, 6.0F, line,
                                   90074);
            }
            af_editor_render_preview_segment(editor, camera,
                                             editor->tool_commands[index - 1U].point, command,
                                             samples, 2.0F, line, 90070);
        }
        editor->render_scene.capacity = capacity;
        return;
    }
    if (editor->drag_mode != AF_EDITOR_DRAG_TOOL_CREATE) {
        return;
    }
    if (editor->active_tool == AF_EDITOR_TOOL_PENCIL) {
        for (uint32_t index = 1U; index < editor->tool_point_count; ++index) {
            af_editor_add_line(
                editor, af_affine_transform_point(camera, editor->tool_points[index - 1U]),
                af_affine_transform_point(camera, editor->tool_points[index]), 2.0F, line, 90070);
        }
        return;
    }
    if (editor->active_tool == AF_EDITOR_TOOL_LINE) {
        const AfVec2 end =
            af_editor_tool_line_end(editor->tool_start, editor->tool_current, SDL_GetModState());
        af_editor_add_line(editor, af_affine_transform_point(camera, editor->tool_start),
                           af_affine_transform_point(camera, end), 3.0F, line, 90070);
        return;
    }
    if (editor->active_tool == AF_EDITOR_TOOL_KNIFE) {
        const uint32_t capacity = editor->render_scene.capacity;
        if (capacity - editor->render_scene.count <= AF_EDITOR_UI_RESERVE)
            return;
        editor->render_scene.capacity = capacity - AF_EDITOR_UI_RESERVE;
        const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->selected_id);
        const AfPath *path =
            af_path_find_node_const(&editor->project.path_data, editor->selected_id);
        AfAffine2 inverse = {0};
        AfEditorKnifeResult cut = {0};
        AfPathCommand commands[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY];
        const bool animated = af_editor_selected_path_has_animation(editor);
        if (!animated && node != NULL && node->polygon_sides == 0U && path != NULL &&
            af_affine_inverse(node->world_transform, &inverse) &&
            af_editor_vector_tool_knife(
                &editor->project.path_data.commands[path->command_start], path->command_count,
                af_affine_transform_point(inverse, editor->tool_start),
                af_affine_transform_point(inverse, editor->tool_current), commands,
                AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY, &cut) == AF_RESULT_SUCCESS) {
            const AfAffine2 transform = af_affine_multiply(camera, node->world_transform);
            for (uint32_t index = 0U; index < cut.cut_count && index < 64U; ++index) {
                const uint32_t source = cut.source_commands[index];
                AfPathCommand segment =
                    editor->project.path_data.commands[path->command_start + source];
                if (segment.type == AF_PATH_CLOSE) {
                    uint32_t contour = source;
                    while (contour > 0U &&
                           editor->project.path_data.commands[path->command_start + contour].type !=
                               AF_PATH_MOVE) {
                        --contour;
                    }
                    segment.type = AF_PATH_LINE;
                    segment.point =
                        editor->project.path_data.commands[path->command_start + contour].point;
                }
                af_editor_render_preview_segment(
                    editor, transform,
                    editor->project.path_data.commands[path->command_start + source - 1U].point,
                    &segment, 24U, 3.5F, (AfColor){1.0F, 0.36F, 0.18F, 1.0F}, 90072);
                const AfVec2 p = af_affine_transform_point(transform, cut.cut_points[index]);
                af_editor_add_rect(editor, p.x - 7.0F, p.y - 7.0F, 14.0F, 14.0F,
                                   (AfColor){0.08F, 0.1F, 0.14F, 1.0F}, 90076);
                af_editor_add_line(editor, (AfVec2){p.x - 5, p.y - 5}, (AfVec2){p.x + 5, p.y + 5},
                                   2.5F, (AfColor){1, 0.8F, 0.25F, 1}, 90077);
                af_editor_add_line(editor, (AfVec2){p.x - 5, p.y + 5}, (AfVec2){p.x + 5, p.y - 5},
                                   2.5F, (AfColor){1, 0.8F, 0.25F, 1}, 90077);
            }
        }
        af_editor_add_line(editor, af_affine_transform_point(camera, editor->tool_start),
                           af_affine_transform_point(camera, editor->tool_current), 2.5F,
                           (AfColor){1.0F, 0.42F, 0.24F, 1.0F}, 90075);
        char label[80];
        (void)snprintf(label, sizeof(label),
                       animated ? "ANIMATED PATH: CUT UNAVAILABLE"
                                : (cut.cut_count == 0U ? "NO CUTS · DRAG ACROSS A SEGMENT"
                                                       : "%u CUTS · RELEASE TO CUT · ESC CANCEL"),
                       cut.cut_count);
        af_editor_add_text(editor, editor->left_width + 28.0F, editor->toolbar_height + 28.0F,
                           label, 0.8F,
                           (float)width - editor->left_width - editor->right_width - 56.0F,
                           (AfColor){1.0F, 0.78F, 0.4F, 1.0F}, 90080);
        editor->render_scene.capacity = capacity;
        return;
    }
    {
        AfEditorToolRect rect = {0};
        const SDL_Keymod modifiers = SDL_GetModState();
        if (!af_editor_vector_tool_rect(editor->tool_start, editor->tool_current,
                                        (modifiers & SDL_KMOD_CTRL) != 0U,
                                        (modifiers & SDL_KMOD_SHIFT) != 0U, &rect)) {
            return;
        }
        const AfVec2 top_left = af_affine_transform_point(camera, rect.position);
        const AfVec2 bottom_right = af_affine_transform_point(
            camera, (AfVec2){rect.position.x + rect.size.x, rect.position.y + rect.size.y});
        const AfVec2 screen_size = {bottom_right.x - top_left.x, bottom_right.y - top_left.y};
        if (editor->active_tool == AF_EDITOR_TOOL_RECTANGLE ||
            editor->active_tool == AF_EDITOR_TOOL_ROUNDED_RECTANGLE ||
            editor->active_tool == AF_EDITOR_TOOL_ELLIPSE) {
            const AfNodeType type = editor->active_tool == AF_EDITOR_TOOL_RECTANGLE
                                        ? AF_NODE_RECTANGLE
                                    : editor->active_tool == AF_EDITOR_TOOL_ROUNDED_RECTANGLE
                                        ? AF_NODE_ROUNDED_RECTANGLE
                                        : AF_NODE_ELLIPSE;
            af_editor_add_shape(
                editor, type,
                (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = top_left.x, .ty = top_left.y},
                screen_size, fill,
                editor->active_tool == AF_EDITOR_TOOL_ROUNDED_RECTANGLE
                    ? fminf(screen_size.x, screen_size.y) * 0.18F
                    : 0.0F,
                90065);
        } else if (editor->active_tool == AF_EDITOR_TOOL_POLYGON ||
                   editor->active_tool == AF_EDITOR_TOOL_STAR ||
                   editor->active_tool == AF_EDITOR_TOOL_ARC ||
                   editor->active_tool == AF_EDITOR_TOOL_SPIRAL ||
                   editor->active_tool == AF_EDITOR_TOOL_TEXT) {
            AfPathCommand commands[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 1U];
            uint32_t command_count = 0U;
            if (editor->active_tool == AF_EDITOR_TOOL_POLYGON) {
                command_count = af_editor_vector_tool_polygon(
                    rect.size, 6U, 0.0F, commands, AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 1U);
            } else if (editor->active_tool == AF_EDITOR_TOOL_STAR) {
                command_count = af_editor_vector_tool_polygon(
                    rect.size, 5U, 0.45F, commands, AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 1U);
            } else if (editor->active_tool == AF_EDITOR_TOOL_ARC) {
                command_count = af_editor_vector_tool_arc(
                    rect.size, commands, AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 1U);
            } else {
                command_count = af_editor_vector_tool_spiral(
                    rect.size, 3U, commands, AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 1U);
            }
            af_editor_render_preview_commands(editor, camera, rect.position, commands,
                                              command_count, line);
            af_editor_add_text(editor, top_left.x + 6.0F, top_left.y + 8.0F,
                               af_editor_vector_tool_name(editor->active_tool), 0.62F,
                               fmaxf(50.0F, screen_size.x - 12.0F),
                               (AfColor){0.82F, 0.94F, 1.0F, 1.0F}, 90075);
            return;
        }
        af_editor_add_line(editor, top_left, (AfVec2){bottom_right.x, top_left.y}, 2.0F, line,
                           90070);
        af_editor_add_line(editor, (AfVec2){bottom_right.x, top_left.y}, bottom_right, 2.0F, line,
                           90070);
        af_editor_add_line(editor, bottom_right, (AfVec2){top_left.x, bottom_right.y}, 2.0F, line,
                           90070);
        af_editor_add_line(editor, (AfVec2){top_left.x, bottom_right.y}, top_left, 2.0F, line,
                           90070);
        af_editor_add_text(editor, top_left.x + 6.0F, top_left.y + 8.0F,
                           af_editor_vector_tool_name(editor->active_tool), 0.62F,
                           fmaxf(50.0F, screen_size.x - 12.0F), (AfColor){0.82F, 0.94F, 1.0F, 1.0F},
                           90075);
    }
}

static void af_editor_render_path_hover(AfEditorState *editor, int width, int height,
                                        AfAffine2 camera) {
    if (editor->drag_mode != AF_EDITOR_DRAG_NONE || editor->selected_count != 1U ||
        (editor->active_tool != AF_EDITOR_TOOL_NODE &&
         editor->active_tool != AF_EDITOR_TOOL_SCISSORS &&
         editor->active_tool != AF_EDITOR_TOOL_SEGMENT_DELETE)) {
        return;
    }
    float x, y, w, h;
    af_editor_layout(editor, width, height, &x, &y, &w, &h);
    if (!af_editor_hovered_rect(editor, x, y + 20.0F, w, h - 20.0F)) {
        return;
    }
    uint32_t index = UINT32_MAX;
    float t = 0.0F;
    if (af_editor_path_segment_hit(editor, width, height, editor->mouse_position, &index, &t)) {
        const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->selected_id);
        const AfPath *path =
            af_path_find_node_const(&editor->render_path_data, editor->selected_id);
        const AfPathCommand *command =
            &editor->render_path_data.commands[path->command_start + index];
        const AfVec2 start =
            editor->render_path_data.commands[path->command_start + index - 1U].point;
        const AfAffine2 transform = af_affine_multiply(camera, node->world_transform);
        const AfColor color = editor->active_tool == AF_EDITOR_TOOL_NODE
                                  ? (AfColor){0.25F, 0.8F, 1.0F, 1.0F}
                                  : (AfColor){1.0F, 0.45F, 0.2F, 1.0F};
        af_editor_render_preview_segment(editor, transform, start, command, 32U, 3.5F, color,
                                         90032);
        const AfVec2 local = command->type == AF_PATH_CUBIC
                                 ? af_editor_preview_cubic_point(start, command, t)
                                 : (AfVec2){start.x + (command->point.x - start.x) * t,
                                            start.y + (command->point.y - start.y) * t};
        const AfVec2 point = af_affine_transform_point(transform, local);
        af_editor_add_rect(editor, point.x - 4, point.y - 4, 8, 8, color, 90035);
    }
}

static void af_editor_render_path_points(AfEditorState *editor, AfAffine2 camera) {
    if (af_editor_is_text(editor, editor->selected_id))
        return;
    const AfSceneNode *selected = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    const AfPath *path = af_path_find_node_const(&editor->render_path_data, editor->selected_id);
    AfAffine2 transform = {0};
    AfVec2 previous = {0};
    AfVec2 contour_start = {0};
    bool have_previous = false;
    if (selected == NULL || selected->type != AF_NODE_PATH || selected->polygon_sides != 0U ||
        path == NULL) {
        return;
    }
    transform = af_affine_multiply(camera, selected->world_transform);
    /* Keep connections visible even when the authored path has no stroke. Bound overlay work
     * so long paths leave room for their point handles and the docked UI. */
    const uint32_t remaining = editor->render_scene.capacity - editor->render_scene.count;
    const uint32_t reserve = AF_EDITOR_UI_RESERVE + path->command_count * 3U + 32U;
    const uint32_t samples =
        remaining > reserve && path->command_count != 0U
            ? (uint32_t)af_editor_clamp((float)((remaining - reserve) / path->command_count), 1, 20)
            : 0U;
    for (uint32_t index = 0U; index < path->command_count; ++index) {
        const AfPathCommand *command =
            &editor->render_path_data.commands[path->command_start + index];
        if (command->type == AF_PATH_MOVE) {
            contour_start = command->point;
            have_previous = false;
        } else if (have_previous && samples != 0U) {
            AfPathCommand connection = *command;
            if (connection.type == AF_PATH_CLOSE) {
                connection.type = AF_PATH_LINE;
                connection.point = contour_start;
            }
            af_editor_render_preview_segment(editor, transform, previous, &connection, samples,
                                             1.25F, (AfColor){0.35F, 0.75F, 1.0F, 1.0F}, 90025);
        }
        const bool primary = index == editor->selected_path_command;
        const bool chosen = primary || af_editor_path_selection_contains(editor, index);
        const AfColor point_color = primary ? (AfColor){1.0F, 0.82F, 0.2F, 1.0F}
                                            : (chosen ? (AfColor){0.24F, 0.78F, 1.0F, 1.0F}
                                                      : (AfColor){0.88F, 0.9F, 0.96F, 1.0F});
        if (command->type == AF_PATH_MOVE || command->type == AF_PATH_LINE ||
            command->type == AF_PATH_CUBIC) {
            const AfVec2 point = af_affine_transform_point(transform, command->point);
            if (editor->drag_mode == AF_EDITOR_DRAG_NONE &&
                hypotf(editor->mouse_position.x - point.x, editor->mouse_position.y - point.y) <=
                    9.0F) {
                af_editor_add_rect(editor, point.x - 9.0F, point.y - 9.0F, 18.0F, 18.0F,
                                   (AfColor){1.0F, 0.5F, 0.2F, 1.0F}, 90039);
            }
            bool keyed = false;
            for (uint32_t track_index = 0U; track_index < editor->project.track_count;
                 ++track_index) {
                const AfProjectTrack *track = &editor->project.tracks[track_index];
                if (track->clip_id == editor->selected_clip_id &&
                    track->node_id == editor->selected_id &&
                    track->property == AF_PROPERTY_PATH_POINT && track->element_index == index) {
                    keyed = true;
                    break;
                }
            }
            if (keyed) {
                af_editor_add_rect(editor, point.x - 9.0F, point.y - 9.0F, 18.0F, 18.0F,
                                   (AfColor){0.2F, 0.84F, 0.76F, 1.0F}, 90038);
            }
            af_editor_add_path_point(editor, point, primary ? 10.0F : (chosen ? 9.0F : 8.0F),
                                     point_color, 90040);
            if (primary && command->type == AF_PATH_CUBIC && have_previous) {
                const AfVec2 first = af_affine_transform_point(transform, command->control1);
                const AfVec2 second = af_affine_transform_point(transform, command->control2);
                const AfVec2 start = af_affine_transform_point(transform, previous);
                af_editor_add_line(editor, start, first, 3.5F, (AfColor){1, 1, 1, 1}, 90029);
                af_editor_add_line(editor, second, point, 3.5F, (AfColor){1, 1, 1, 1}, 90029);
                af_editor_add_line(editor, start, first, 1.5F, (AfColor){0.54F, 0.72F, 1.0F, 1.0F},
                                   90030);
                af_editor_add_line(editor, second, point, 1.5F, (AfColor){0.54F, 0.72F, 1.0F, 1.0F},
                                   90030);
                const AfVec2 handles[] = {first, second};
                for (uint32_t handle = 0U; handle < 2U; ++handle) {
                    const AfVec2 p = handles[handle];
                    if (editor->drag_mode == AF_EDITOR_DRAG_NONE &&
                        hypotf(editor->mouse_position.x - p.x, editor->mouse_position.y - p.y) <=
                            9.0F) {
                        af_editor_add_rect(editor, p.x - 7, p.y - 7, 14, 14,
                                           (AfColor){1.0F, 0.55F, 0.2F, 1.0F}, 90039);
                    }
                    af_editor_add_path_point(editor, p, 8, (AfColor){0.34F, 0.7F, 1.0F, 1}, 90040);
                }
                af_editor_add_text(editor, first.x + 7.0F, first.y - 7.0F, "OUT", 0.62F, 28.0F,
                                   (AfColor){0.42F, 0.76F, 1.0F, 1.0F}, 90045);
                af_editor_add_text(editor, second.x + 7.0F, second.y - 7.0F, "IN", 0.62F, 24.0F,
                                   (AfColor){0.72F, 0.52F, 1.0F, 1.0F}, 90045);
            }
            previous = command->point;
            have_previous = true;
        } else if (command->type == AF_PATH_CLOSE) {
            have_previous = false;
        }
    }
}

static bool af_editor_path_action_fits(float x, float width, float canvas_right) {
    return x + width <= canvas_right - 8.0F;
}

static void af_editor_render_path_actions(AfEditorState *editor, int width, int height) {
    if (af_editor_is_text(editor, editor->selected_id))
        return;
    float canvas_x = 0.0F;
    float canvas_y = 0.0F;
    float canvas_width = 0.0F;
    float canvas_height = 0.0F;
    const AfPath *path = af_path_find_node_const(&editor->render_path_data, editor->selected_id);
    const AfPathCommand *command = NULL;
    const AfProjectTrack *point_track = NULL;
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    float x = 0.0F;
    float y = 0.0F;
    float right = 0.0F;
    char point_label[32];
    const uint32_t node_selection_count =
        editor->path_command_selection_node_id == editor->selected_id
            ? editor->selected_path_command_count
            : 0U;
    const bool multi = node_selection_count > 1U;
    bool segment_editable = false;
    if (path == NULL || editor->selected_path_command >= path->command_count) {
        return;
    }
    command =
        &editor->render_path_data.commands[path->command_start + editor->selected_path_command];
    if (command->type == AF_PATH_CLOSE) {
        return;
    }
    segment_editable = !multi && editor->selected_path_command != 0U &&
                       (command->type == AF_PATH_LINE || command->type == AF_PATH_CUBIC);
    af_editor_layout(editor, width, height, &canvas_x, &canvas_y, &canvas_width, &canvas_height);
    (void)canvas_height;
    x = canvas_x + 12.0F;
    y = canvas_y + AF_EDITOR_PATH_ACTION_Y;
    right = canvas_x + canvas_width;
    if (multi) {
        (void)snprintf(point_label, sizeof(point_label), "EDIT PATH  %u NODES",
                       node_selection_count);
    } else {
        (void)snprintf(point_label, sizeof(point_label), "EDIT PATH  POINT #%u",
                       editor->selected_path_command + 1U);
    }
    af_editor_add_rect(editor, x, y, 116.0F, AF_EDITOR_PATH_ACTION_HEIGHT,
                       (AfColor){0.11F, 0.13F, 0.17F, 0.96F}, 90050);
    af_editor_add_text(editor, x + 8.0F, y + 10.0F, point_label, 0.6F, 102.0F,
                       (AfColor){0.82F, 0.85F, 0.92F, 1.0F}, 90060);
    point_track = af_editor_find_track(editor, editor->selected_id, AF_PROPERTY_PATH_POINT,
                                       editor->selected_path_command);
    if (af_editor_path_action_fits(x + 122.0F, 96.0F, right)) {
        const bool has_key =
            point_track != NULL && clip != NULL &&
            af_editor_track_key_at(editor, point_track->id,
                                   af_editor_key_time(editor, clip, editor->playhead)) != NULL;
        af_editor_add_button_rect(editor, x + 122.0F, y, 96.0F, AF_EDITOR_PATH_ACTION_HEIGHT,
                                  multi ? (AfColor){0.14F, 0.15F, 0.18F, 0.92F}
                                        : (has_key ? (AfColor){0.62F, 0.43F, 0.14F, 0.97F}
                                                   : (AfColor){0.12F, 0.48F, 0.64F, 0.97F}),
                                  90050);
        af_editor_add_text(
            editor, x + 131.0F, y + 12.0F,
            multi ? "KEY 1 NODE" : (has_key ? "UPDATE KEY" : "+ KEY POINT"), 0.58F, 80.0F,
            multi ? (AfColor){0.38F, 0.4F, 0.46F, 1.0F} : (AfColor){0.96F, 0.97F, 1.0F, 1.0F},
            90060);
    }
    if (af_editor_path_action_fits(x + 224.0F, 82.0F, right)) {
        af_editor_add_button_rect(editor, x + 224.0F, y, 82.0F, AF_EDITOR_PATH_ACTION_HEIGHT,
                                  segment_editable ? (AfColor){0.2F, 0.23F, 0.29F, 0.97F}
                                                   : (AfColor){0.14F, 0.15F, 0.18F, 0.92F},
                                  90050);
        af_editor_add_text(editor, x + 232.0F, y + 12.0F,
                           command->type == AF_PATH_CUBIC ? "TO LINE" : "TO CURVE", 0.58F, 68.0F,
                           segment_editable ? (AfColor){0.82F, 0.85F, 0.92F, 1.0F}
                                            : (AfColor){0.38F, 0.4F, 0.46F, 1.0F},
                           90060);
    }
    if (af_editor_path_action_fits(x + 312.0F, 62.0F, right)) {
        af_editor_add_button_rect(editor, x + 312.0F, y, 62.0F, AF_EDITOR_PATH_ACTION_HEIGHT,
                                  segment_editable ? (AfColor){0.2F, 0.23F, 0.29F, 0.97F}
                                                   : (AfColor){0.14F, 0.15F, 0.18F, 0.92F},
                                  90050);
        af_editor_add_text(editor, x + 320.0F, y + 12.0F, "INSERT", 0.58F, 48.0F,
                           segment_editable ? (AfColor){0.82F, 0.85F, 0.92F, 1.0F}
                                            : (AfColor){0.38F, 0.4F, 0.46F, 1.0F},
                           90060);
    }
    if (af_editor_path_action_fits(x + 380.0F, 80.0F, right)) {
        af_editor_add_button_rect(editor, x + 380.0F, y, 80.0F, AF_EDITOR_PATH_ACTION_HEIGHT,
                                  (AfColor){0.2F, 0.23F, 0.29F, 0.97F}, 90050);
        af_editor_add_text(editor, x + 388.0F, y + 12.0F, "COMBINE v", 0.53F, 66.0F,
                           (AfColor){0.82F, 0.85F, 0.92F, 1.0F}, 90060);
    }
    if (af_editor_path_action_fits(x + 466.0F, 68.0F, right)) {
        const bool delete_enabled = multi || segment_editable;
        af_editor_add_button_rect(editor, x + 466.0F, y, 68.0F, AF_EDITOR_PATH_ACTION_HEIGHT,
                                  delete_enabled ? (AfColor){0.48F, 0.2F, 0.24F, 0.97F}
                                                 : (AfColor){0.18F, 0.14F, 0.16F, 0.92F},
                                  90050);
        af_editor_add_text(editor, x + 474.0F, y + 12.0F, "DELETE", 0.58F, 52.0F,
                           delete_enabled ? (AfColor){0.98F, 0.88F, 0.9F, 1.0F}
                                          : (AfColor){0.42F, 0.36F, 0.38F, 1.0F},
                           90060);
    }
    if (af_editor_path_action_fits(x + 540.0F, 70.0F, right)) {
        const bool enabled = node_selection_count >= 2U;
        af_editor_add_button_rect(editor, x + 540.0F, y, 70.0F, AF_EDITOR_PATH_ACTION_HEIGHT,
                                  enabled ? (AfColor){0.16F, 0.38F, 0.46F, 0.97F}
                                          : (AfColor){0.14F, 0.15F, 0.18F, 0.92F},
                                  90050);
        af_editor_add_text(editor, x + 548.0F, y + 12.0F, "ALIGN X", 0.55F, 56.0F,
                           enabled ? (AfColor){0.86F, 0.96F, 1.0F, 1.0F}
                                   : (AfColor){0.38F, 0.4F, 0.46F, 1.0F},
                           90060);
    }
    if (af_editor_path_action_fits(x + 616.0F, 70.0F, right)) {
        const bool enabled = node_selection_count >= 2U;
        af_editor_add_button_rect(editor, x + 616.0F, y, 70.0F, AF_EDITOR_PATH_ACTION_HEIGHT,
                                  enabled ? (AfColor){0.16F, 0.38F, 0.46F, 0.97F}
                                          : (AfColor){0.14F, 0.15F, 0.18F, 0.92F},
                                  90050);
        af_editor_add_text(editor, x + 624.0F, y + 12.0F, "ALIGN Y", 0.55F, 56.0F,
                           enabled ? (AfColor){0.86F, 0.96F, 1.0F, 1.0F}
                                   : (AfColor){0.38F, 0.4F, 0.46F, 1.0F},
                           90060);
    }
    if (af_editor_path_action_fits(x + 692.0F, 70.0F, right)) {
        const bool enabled = node_selection_count >= 3U;
        af_editor_add_button_rect(editor, x + 692.0F, y, 70.0F, AF_EDITOR_PATH_ACTION_HEIGHT,
                                  enabled ? (AfColor){0.23F, 0.31F, 0.5F, 0.97F}
                                          : (AfColor){0.14F, 0.15F, 0.18F, 0.92F},
                                  90050);
        af_editor_add_text(editor, x + 700.0F, y + 12.0F, "DIST X", 0.55F, 56.0F,
                           enabled ? (AfColor){0.9F, 0.92F, 1.0F, 1.0F}
                                   : (AfColor){0.38F, 0.4F, 0.46F, 1.0F},
                           90060);
    }
    if (af_editor_path_action_fits(x + 768.0F, 70.0F, right)) {
        const bool enabled = node_selection_count >= 3U;
        af_editor_add_button_rect(editor, x + 768.0F, y, 70.0F, AF_EDITOR_PATH_ACTION_HEIGHT,
                                  enabled ? (AfColor){0.23F, 0.31F, 0.5F, 0.97F}
                                          : (AfColor){0.14F, 0.15F, 0.18F, 0.92F},
                                  90050);
        af_editor_add_text(editor, x + 776.0F, y + 12.0F, "DIST Y", 0.55F, 56.0F,
                           enabled ? (AfColor){0.9F, 0.92F, 1.0F, 1.0F}
                                   : (AfColor){0.38F, 0.4F, 0.46F, 1.0F},
                           90060);
    }
    if (af_editor_path_action_fits(x + 844.0F, 78.0F, right)) {
        const bool enabled = node_selection_count >= 1U;
        af_editor_add_button_rect(editor, x + 844.0F, y, 78.0F, AF_EDITOR_PATH_ACTION_HEIGHT,
                                  enabled ? (AfColor){0.36F, 0.26F, 0.5F, 0.97F}
                                          : (AfColor){0.14F, 0.15F, 0.18F, 0.92F},
                                  90050);
        af_editor_add_text(
            editor, x + 852.0F, y + 12.0F,
            command->handle_mode == AF_PATH_HANDLE_CUSP ? "SMOOTH" : "CUSP", 0.55F, 64.0F,
            enabled ? (AfColor){0.96F, 0.9F, 1.0F, 1.0F} : (AfColor){0.38F, 0.4F, 0.46F, 1.0F},
            90060);
    }
}

static void af_editor_prepare_render_paths(AfEditorState *editor) {
    editor->render_path_data.path_count = editor->project.path_data.path_count;
    editor->render_path_data.command_count = editor->project.path_data.command_count;
    if (editor->project.path_data.path_count != 0U) {
        memcpy(editor->render_path_data.paths, editor->project.path_data.paths,
               (size_t)editor->project.path_data.path_count * sizeof(*editor->render_paths));
    }
    if (editor->project.path_data.command_count != 0U) {
        memcpy(editor->render_path_data.commands, editor->project.path_data.commands,
               (size_t)editor->project.path_data.command_count *
                   sizeof(*editor->render_path_commands));
    }
}

static void af_editor_build_render_scene(AfEditorState *editor, int width, int height) {
    float canvas_x = 0.0F;
    float canvas_y = 0.0F;
    float canvas_width = 0.0F;
    float canvas_height = 0.0F;
    AfAffine2 camera = af_editor_camera(editor, width, height);
    uint32_t index = 0U;
    const uint64_t ui_now = SDL_GetTicksNS();
    editor->ui.delta_seconds =
        editor->ui_previous_ticks == 0
            ? 1.0F / 60.0F
            : fminf(0.05F, (float)(ui_now - editor->ui_previous_ticks) / 1000000000.0F);
    editor->ui_previous_ticks = ui_now;
    editor->render_scene.count = 0U;
    const AfEditorPalette palette = af_editor_theme_palette(&editor->preferences);
#ifdef __EMSCRIPTEN__
    if (af_editor_theme_rgb(palette.panel) != af_editor_theme_rgb(editor->palette.panel) ||
        af_editor_theme_rgb(palette.accent) != af_editor_theme_rgb(editor->palette.accent))
        af_web_set_ui_colors(af_editor_theme_rgb(palette.panel),
                             af_editor_theme_rgb(palette.accent),
                             af_editor_theme_rgb(palette.text));
#endif
    editor->palette = palette;
    editor->preview_style_data.count = editor->project.style_data.count;
    editor->render_scene.styles = &editor->preview_style_data;
    editor->render_scene.images = &editor->project.image_data;
    if (editor->export_panel_visible) {
        af_editor_render_export_panel(editor, width, height);
        return;
    }
    af_editor_layout(editor, width, height, &canvas_x, &canvas_y, &canvas_width, &canvas_height);
    if (editor->welcome_visible || editor->example_browser_visible ||
        editor->guide_browser_visible) {
        editor->render_path_data.path_count = 0U;
        editor->render_path_data.command_count = 0U;
        af_editor_add_rect(editor, 0.0F, 0.0F, (float)width, (float)height,
                           (AfColor){0.055F, 0.06F, 0.075F, 1.0F}, -100000);
        af_editor_add_rect(editor, 0.0F, 0.0F, editor->left_width, (float)height, AF_UI_PANEL,
                           -99990);
        af_editor_add_rect(editor, (float)width - editor->right_width, 0.0F, editor->right_width,
                           (float)height, AF_UI_PANEL, -99990);
        af_editor_add_rect(editor, editor->left_width, 0.0F,
                           (float)width - editor->left_width - editor->right_width,
                           editor->toolbar_height, AF_UI_SURFACE, -99980);
        af_editor_add_rect(editor, canvas_x, canvas_y, canvas_width, canvas_height, AF_UI_CANVAS,
                           -99990);
        af_editor_add_text(editor, editor->left_width + 18.0F, 18.0F, "ARCFLOW EDITOR", 0.82F,
                           180.0F, (AfColor){0.6F, 0.68F, 0.8F, 1.0F}, -99970);
        af_editor_render_onboarding(editor, width, height);
        af_editor_render_appearance(editor, width, height);
        return;
    }
    af_editor_add_rect(editor, canvas_x, canvas_y, canvas_width, canvas_height, AF_UI_CANVAS,
                       -100000);
    af_editor_render_grid(editor, width, height, camera);
    af_editor_render_canvas_workflow(editor, width, height, camera);
    if (editor->text_panel_open)
        af_editor_text_preview_update(editor);
    for (index = 0U; !editor->export_panel_visible && index < editor->preview_scene.count;
         ++index) {
        const AfSceneNode *source = &editor->preview_scene.nodes[index];
        AfSceneNode *target = NULL;
        if (editor->render_scene.count >= AF_MAX_NODES) {
            continue;
        }
        if (editor->text_panel_open && source->id == editor->text_panel_id &&
            !editor->text_panel_draft.utf8[0])
            continue;
        target = &editor->render_scene.nodes[editor->render_scene.count++];
        *target = *source;
        target->world_transform = af_affine_multiply(camera, source->world_transform);
        target->layer_order = af_editor_clamp_layer(source->layer_order);
    }
    af_editor_render_text_canvas(editor, width, height);
    af_editor_render_gradient_handles(editor, width, height);
    if (!editor->text_panel_open)
        af_editor_render_selection_outlines(editor, camera);
    if (!editor->text_panel_open && editor->active_tool == AF_EDITOR_TOOL_SELECT) {
        af_editor_render_gizmo(editor, camera);
    } else if (editor->active_tool == AF_EDITOR_TOOL_NODE ||
               editor->active_tool == AF_EDITOR_TOOL_GRADIENT) {
        af_editor_render_outline(editor,
                                 af_scene_find_const(&editor->preview_scene, editor->selected_id),
                                 camera, (AfColor){0.98F, 0.8F, 0.22F, 1.0F}, 90000);
    }
    af_editor_render_path_hover(editor, width, height, camera);
    if (editor->active_tool == AF_EDITOR_TOOL_NODE && editor->selected_count == 1U) {
        af_editor_render_path_points(editor, camera);
        af_editor_render_path_actions(editor, width, height);
    }
    af_editor_render_vector_tool_preview(editor, width, height);
    af_editor_render_canvas_box(editor);
    af_editor_add_panel(editor, 0.0F, 0.0F, editor->left_width, (float)height, AF_UI_PANEL, 0,
                        100000);
    af_editor_add_panel(editor, (float)width - editor->right_width, 0.0F, editor->right_width,
                        (float)height, AF_UI_PANEL, 0, 100000);
    af_editor_add_panel(editor, editor->left_width, 0.0F,
                        (float)width - editor->left_width - editor->right_width,
                        editor->toolbar_height, AF_UI_SURFACE, 0, 100000);
    af_editor_add_rect(editor, editor->left_width, editor->toolbar_height - 1,
                       (float)width - editor->left_width - editor->right_width, 1, AF_UI_BORDER,
                       100101);
    for (uint32_t i = 0; i < 4; ++i) {
        const AfColor shadow = {0, 0, 0, .055F * (float)(4 - i)};
        af_editor_add_rect(editor, editor->left_width + (float)i, editor->toolbar_height, 1,
                           (float)height - editor->toolbar_height, shadow, 99999);
        af_editor_add_rect(editor, (float)width - editor->right_width - (float)i - 1,
                           editor->toolbar_height, 1, (float)height - editor->toolbar_height,
                           shadow, 99999);
    }
    af_editor_add_rect(editor, editor->left_width, (float)height - editor->status_height,
                       (float)width - editor->left_width - editor->right_width,
                       editor->status_height, (AfColor){0.08F, 0.087F, 0.105F, 1.0F}, 100000);
    af_editor_add_rect(editor, editor->left_width - 0.5F, 0.0F, 1.0F, (float)height, AF_UI_BORDER,
                       100100);
    af_editor_add_rect(editor, (float)width - editor->right_width - 0.5F, 0.0F, 1.0F, (float)height,
                       AF_UI_BORDER, 100100);
    af_editor_render_hierarchy(editor, height);
    af_editor_render_asset_browser(editor, height);
    af_editor_render_toolbar(editor, width);
    af_editor_render_inspector(editor, width, height);
    af_editor_render_timeline(editor, width, height);
    af_editor_render_splitters(editor, width, height);
    af_editor_render_context_menu(editor, width, height);
    af_editor_render_tooltip(editor, width, height);
    af_editor_render_top_menu(editor, width, height);
    af_editor_render_workflow(editor, width, height);
}

static void af_editor_update_title(AfEditorState *editor) {
    char title[768];
    const char *name = editor->project_path;
    for (const char *cursor = name; *cursor; ++cursor) {
        if (*cursor == '/' || *cursor == '\\')
            name = cursor + 1;
    }
    (void)snprintf(title, sizeof(title), "%s%s — Arcflow", editor->dirty ? "● " : "",
                   name[0] ? name : "Untitled");
    if (editor->rename_active)
        (void)snprintf(title, sizeof(title), "Rename: %s — Arcflow", editor->rename_buffer);
    if (strcmp(SDL_GetWindowTitle(editor->window.window), title) != 0)
        (void)SDL_SetWindowTitle(editor->window.window, title);
}
