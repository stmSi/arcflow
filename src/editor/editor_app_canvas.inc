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

static void af_editor_render_vector_tool_preview(AfEditorState *editor, int width, int height) {
    const AfAffine2 camera = af_editor_camera(editor, width, height);
    const AfColor line = {0.35F, 0.84F, 1.0F, 1.0F};
    const AfColor fill = {0.25F, 0.7F, 1.0F, 0.22F};
    if (editor->active_tool == AF_EDITOR_TOOL_PEN && editor->tool_command_count != 0U) {
        for (uint32_t index = 1U; index < editor->tool_command_count; ++index) {
            const AfPathCommand *command = &editor->tool_commands[index];
            const AfVec2 previous =
                af_affine_transform_point(camera, editor->tool_commands[index - 1U].point);
            const AfVec2 point = af_affine_transform_point(camera, command->point);
            af_editor_add_line(editor, previous, point, 2.0F, line, 90070);
            if (command->type == AF_PATH_CUBIC) {
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
        }
        if (editor->drag_mode != AF_EDITOR_DRAG_TOOL_CREATE) {
            af_editor_add_line(
                editor,
                af_affine_transform_point(
                    camera, editor->tool_commands[editor->tool_command_count - 1U].point),
                editor->mouse_position, 1.5F, (AfColor){0.5F, 0.75F, 0.9F, 0.8F}, 90070);
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
        for (uint32_t index = 0U; index < editor->tool_command_count; ++index) {
            const AfVec2 point =
                af_affine_transform_point(camera, editor->tool_commands[index].point);
            af_editor_add_rect(editor, point.x - 4.0F, point.y - 4.0F, 8.0F, 8.0F,
                               index == 0U ? (AfColor){1.0F, 0.74F, 0.2F, 1.0F} : line, 90075);
        }
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

static void af_editor_render_path_points(AfEditorState *editor, AfAffine2 camera) {
    const AfSceneNode *selected = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    const AfPath *path = af_path_find_node_const(&editor->render_path_data, editor->selected_id);
    AfAffine2 transform = {0};
    AfVec2 previous = {0};
    bool have_previous = false;
    if (selected == NULL || selected->type != AF_NODE_PATH || path == NULL) {
        return;
    }
    transform = af_affine_multiply(camera, selected->world_transform);
    for (uint32_t index = 0U; index < path->command_count; ++index) {
        const AfPathCommand *command =
            &editor->render_path_data.commands[path->command_start + index];
        const bool primary = index == editor->selected_path_command;
        const bool chosen = primary || af_editor_path_selection_contains(editor, index);
        const AfColor point_color = primary ? (AfColor){1.0F, 0.82F, 0.2F, 1.0F}
                                            : (chosen ? (AfColor){0.24F, 0.78F, 1.0F, 1.0F}
                                                      : (AfColor){0.88F, 0.9F, 0.96F, 1.0F});
        if (command->type == AF_PATH_MOVE || command->type == AF_PATH_LINE ||
            command->type == AF_PATH_CUBIC) {
            const AfVec2 point = af_affine_transform_point(transform, command->point);
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
                af_editor_add_rect(editor, point.x - 6.0F, point.y - 6.0F, 12.0F, 12.0F,
                                   (AfColor){0.2F, 0.84F, 0.76F, 1.0F}, 90038);
            }
            af_editor_add_rect(editor, point.x - (primary ? 5.0F : (chosen ? 4.5F : 4.0F)),
                               point.y - (primary ? 5.0F : (chosen ? 4.5F : 4.0F)),
                               primary ? 10.0F : (chosen ? 9.0F : 8.0F),
                               primary ? 10.0F : (chosen ? 9.0F : 8.0F), point_color, 90040);
            if (primary && command->type == AF_PATH_CUBIC && have_previous) {
                const AfVec2 first = af_affine_transform_point(transform, command->control1);
                const AfVec2 second = af_affine_transform_point(transform, command->control2);
                const AfVec2 start = af_affine_transform_point(transform, previous);
                af_editor_add_line(editor, start, first, 1.5F, (AfColor){0.54F, 0.72F, 1.0F, 1.0F},
                                   90030);
                af_editor_add_line(editor, second, point, 1.5F, (AfColor){0.54F, 0.72F, 1.0F, 1.0F},
                                   90030);
                af_editor_add_rect(editor, first.x - 4.0F, first.y - 4.0F, 8.0F, 8.0F,
                                   (AfColor){0.34F, 0.7F, 1.0F, 1.0F}, 90040);
                af_editor_add_rect(editor, second.x - 4.0F, second.y - 4.0F, 8.0F, 8.0F,
                                   (AfColor){0.34F, 0.7F, 1.0F, 1.0F}, 90040);
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
        af_editor_add_text(editor, x + 388.0F, y + 12.0F, "OPEN/CLOSE", 0.53F, 66.0F,
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
        af_editor_add_text(editor, x + 852.0F, y + 12.0F, "SMOOTH", 0.55F, 64.0F,
                           enabled ? (AfColor){0.96F, 0.9F, 1.0F, 1.0F}
                                   : (AfColor){0.38F, 0.4F, 0.46F, 1.0F},
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
    editor->render_scene.count = 0U;
    af_editor_layout(editor, width, height, &canvas_x, &canvas_y, &canvas_width, &canvas_height);
    if (editor->welcome_visible || editor->example_browser_visible ||
        editor->guide_browser_visible) {
        editor->render_path_data.path_count = 0U;
        editor->render_path_data.command_count = 0U;
        af_editor_add_rect(editor, 0.0F, 0.0F, (float)width, (float)height,
                           (AfColor){0.055F, 0.06F, 0.075F, 1.0F}, -100000);
        af_editor_add_rect(editor, 0.0F, 0.0F, editor->left_width, (float)height,
                           (AfColor){0.085F, 0.092F, 0.115F, 1.0F}, -99990);
        af_editor_add_rect(editor, (float)width - editor->right_width, 0.0F, editor->right_width,
                           (float)height, (AfColor){0.095F, 0.102F, 0.125F, 1.0F}, -99990);
        af_editor_add_rect(editor, editor->left_width, 0.0F,
                           (float)width - editor->left_width - editor->right_width,
                           AF_EDITOR_TOOLBAR_HEIGHT, (AfColor){0.11F, 0.12F, 0.145F, 1.0F}, -99980);
        af_editor_add_rect(editor, canvas_x, canvas_y, canvas_width, canvas_height,
                           (AfColor){0.145F, 0.155F, 0.18F, 1.0F}, -99990);
        af_editor_add_text(editor, editor->left_width + 18.0F, 18.0F, "ARCFLOW EDITOR", 0.82F,
                           180.0F, (AfColor){0.6F, 0.68F, 0.8F, 1.0F}, -99970);
        af_editor_render_onboarding(editor, width, height);
        return;
    }
    af_editor_add_rect(editor, canvas_x, canvas_y, canvas_width, canvas_height,
                       (AfColor){0.145F, 0.155F, 0.18F, 1.0F}, -100000);
    af_editor_add_text(editor, canvas_x + 12.0F, canvas_y + 12.0F, "CANVAS", 1.0F, 54.0F,
                       (AfColor){0.46F, 0.5F, 0.58F, 1.0F}, -89990);
    af_editor_render_grid(editor, width, height, camera);
    af_editor_render_canvas_workflow(editor, width, height, camera);
    for (index = 0U; index < editor->preview_scene.count; ++index) {
        const AfSceneNode *source = &editor->preview_scene.nodes[index];
        AfSceneNode *target = NULL;
        if (editor->render_scene.count >= AF_MAX_NODES) {
            continue;
        }
        target = &editor->render_scene.nodes[editor->render_scene.count++];
        *target = *source;
        target->world_transform = af_affine_multiply(camera, source->world_transform);
        target->layer_order = af_editor_clamp_layer(source->layer_order);
    }
    af_editor_render_selection_outlines(editor, camera);
    if (editor->active_tool == AF_EDITOR_TOOL_SELECT) {
        af_editor_render_gizmo(editor, camera);
    } else if (editor->active_tool == AF_EDITOR_TOOL_NODE) {
        af_editor_render_outline(editor,
                                 af_scene_find_const(&editor->preview_scene, editor->selected_id),
                                 camera, (AfColor){0.98F, 0.8F, 0.22F, 1.0F}, 90000);
    }
    if (editor->active_tool == AF_EDITOR_TOOL_NODE && editor->selected_count == 1U) {
        af_editor_render_path_points(editor, camera);
        af_editor_render_path_actions(editor, width, height);
    }
    af_editor_render_vector_tool_preview(editor, width, height);
    af_editor_render_canvas_box(editor);
    af_editor_add_rect(editor, 0.0F, 0.0F, editor->left_width, (float)height,
                       (AfColor){0.085F, 0.092F, 0.115F, 1.0F}, 100000);
    af_editor_add_rect(editor, (float)width - editor->right_width, 0.0F, editor->right_width,
                       (float)height, (AfColor){0.095F, 0.102F, 0.125F, 1.0F}, 100000);
    af_editor_add_rect(editor, editor->left_width, 0.0F,
                       (float)width - editor->left_width - editor->right_width,
                       AF_EDITOR_TOOLBAR_HEIGHT, (AfColor){0.11F, 0.12F, 0.145F, 1.0F}, 100000);
    af_editor_add_rect(editor, editor->left_width, (float)height - AF_EDITOR_STATUS_HEIGHT,
                       (float)width - editor->left_width - editor->right_width,
                       AF_EDITOR_STATUS_HEIGHT, (AfColor){0.08F, 0.087F, 0.105F, 1.0F}, 100000);
    af_editor_add_rect(editor, editor->left_width - 2.0F, 0.0F, 4.0F, (float)height,
                       (AfColor){0.23F, 0.25F, 0.3F, 1.0F}, 100100);
    af_editor_add_rect(editor, (float)width - editor->right_width - 2.0F, 0.0F, 4.0F, (float)height,
                       (AfColor){0.23F, 0.25F, 0.3F, 1.0F}, 100100);
    af_editor_render_hierarchy(editor, height);
    af_editor_render_asset_browser(editor, height);
    af_editor_render_toolbar(editor);
    af_editor_render_inspector(editor, width);
    af_editor_render_timeline(editor, width, height);
    af_editor_add_text(editor, editor->left_width + 12.0F,
                       (float)height - AF_EDITOR_STATUS_HEIGHT + 9.0F, editor->status, 1.0F,
                       (float)width - editor->left_width - editor->right_width - 24.0F,
                       (AfColor){0.62F, 0.67F, 0.76F, 1.0F}, 100140);
    af_editor_render_context_menu(editor, width, height);
    af_editor_render_tooltip(editor, width, height);
    af_editor_render_top_menu(editor, width, height);
    af_editor_render_workflow(editor, width, height);
}

static void af_editor_update_title(AfEditorState *editor) {
    char title[768] = {0};
    const AfSceneNode *selected = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    const char *name = af_project_name(&editor->project, editor->selected_id);
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    static const char *const properties[] = {"position",  "rotation",  "scale", "opacity",
                                             "fill",      "radius",    "skew",  "anchor",
                                             "handle in", "handle out"};
    if (editor->rename_active) {
        (void)snprintf(title, sizeof(title),
                       "Arcflow Editor | Rename: %s | Enter commit, Esc cancel",
                       editor->rename_buffer);
    } else if (editor->selected_count > 1U) {
        (void)snprintf(title, sizeof(title),
                       "Arcflow Editor%s | %u objects selected | drag, arrows, color, style, "
                       "rotate, scale, layer, duplicate and delete apply to all | %s",
                       editor->dirty ? " *" : "", editor->selected_count, editor->status);
    } else if (selected != NULL && name != NULL) {
        (void)snprintf(
            title, sizeof(title),
            "Arcflow Editor%s | %s | pos %.1f,%.1f scale %.2f,%.2f rot %.2f skew %.2f,%.2f "
            "opacity %.2f "
            "radius %.1f layer %d | %s %.2fs %s (%u keys) | %s",
            editor->dirty ? " *" : "", name, selected->local_transform.position.x,
            selected->local_transform.position.y, selected->local_transform.scale.x,
            selected->local_transform.scale.y, selected->local_transform.rotation,
            selected->local_transform.skew.x, selected->local_transform.skew.y, selected->opacity,
            selected->corner_radius, selected->layer_order, clip != NULL ? clip->name : "No clip",
            editor->playhead, properties[editor->selected_property],
            editor->selected_keyframe_count, editor->status);
    } else {
        (void)snprintf(
            title, sizeof(title),
            "Arcflow Editor%s | 1/2/3/4 create, Ctrl+D duplicate, Del delete, F2 rename, "
            "Shift-click parent | %s",
            editor->dirty ? " *" : "", editor->status);
    }
    (void)SDL_SetWindowTitle(editor->window.window, title);
}
