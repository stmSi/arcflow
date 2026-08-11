static void af_editor_begin_selected_key_drag(AfEditorState *editor) {
    uint32_t index = 0U;
    editor->key_drag_count = 0U;
    for (index = 0U; index < editor->selected_keyframe_count; ++index) {
        if (af_project_keyframe_snapshot(&editor->project, editor->selected_keyframes[index],
                                         &editor->key_drag_before[editor->key_drag_count]) ==
            AF_RESULT_SUCCESS) {
            ++editor->key_drag_count;
        }
    }
}

static void af_editor_timeline_commit_keys(AfEditorState *editor, AfEditorMergeKey merge_key) {
    AfEditorCommand command = {0};
    uint32_t index = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (editor->key_drag_count == 0U ||
        af_editor_command_animation(merge_key, editor->key_drag_before[0].keyframe.id, &command) !=
            AF_RESULT_SUCCESS) {
        return;
    }
    command.keyframe_before_count = editor->key_drag_count;
    command.keyframe_after_count = editor->key_drag_count;
    for (index = 0U; index < editor->key_drag_count; ++index) {
        command.keyframe_before[index] = editor->key_drag_before[index];
        result = af_project_keyframe_snapshot(&editor->project,
                                              editor->key_drag_before[index].keyframe.id,
                                              &command.keyframe_after[index]);
        if (result != AF_RESULT_SUCCESS) {
            return;
        }
    }
    result = af_editor_history_execute(&editor->history, &editor->project, &command, true, false);
    if (result == AF_RESULT_SUCCESS) {
        editor->dirty = true;
        af_editor_status(editor, "Timeline edit committed");
    }
}

static void af_editor_timeline_commit_clip(AfEditorState *editor) {
    AfEditorCommand command = {0};
    if (af_editor_command_animation(AF_EDITOR_MERGE_LOOP_REGION, editor->clip_drag_before.clip.id,
                                    &command) != AF_RESULT_SUCCESS) {
        return;
    }
    command.clip_before[0] = editor->clip_drag_before;
    command.clip_before_count = 1U;
    if (af_project_clip_snapshot(&editor->project, editor->clip_drag_before.clip.id,
                                 &command.clip_after[0]) != AF_RESULT_SUCCESS) {
        return;
    }
    command.clip_after_count = 1U;
    if (af_editor_history_execute(&editor->history, &editor->project, &command, true, false) ==
        AF_RESULT_SUCCESS) {
        editor->dirty = true;
        af_editor_status(editor, "Loop region edited");
    }
}

static bool af_editor_timeline_mouse_down(AfEditorState *editor, int width, int height,
                                          const SDL_MouseButtonEvent *event, SDL_Keymod modifiers) {
    const float top = af_editor_timeline_top(editor, height);
    const float ruler_x = af_editor_timeline_ruler_x(editor);
    const float center_right = (float)width - editor->right_width;
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    uint32_t index = 0U;
    if (event->y < top - 5.0F || event->x < editor->left_width || event->x > center_right) {
        return false;
    }
    editor->timeline_focus = true;
    if (fabsf(event->y - top) <= 5.0F && event->button == SDL_BUTTON_LEFT) {
        editor->drag_mode = AF_EDITOR_DRAG_TIMELINE_HEIGHT;
        return true;
    }
    if (event->button == SDL_BUTTON_MIDDLE) {
        editor->drag_mode = AF_EDITOR_DRAG_TIMELINE_PAN;
        editor->drag_start_mouse = (AfVec2){event->x, event->y};
        return true;
    }
    if (event->button != SDL_BUTTON_LEFT) {
        return true;
    }
    if (event->y < top + 40.0F) {
        const float local_x = event->x - editor->left_width;
        if (local_x >= 10.0F && local_x < 36.0F) {
            editor->preview_playing = !editor->preview_playing;
        } else if (local_x >= 42.0F && local_x < 68.0F) {
            editor->preview_playing = false;
            editor->playhead = 0.0F;
        } else if (local_x >= 73.0F && local_x < 107.0F) {
            editor->curve_visible = !editor->curve_visible;
        } else if (local_x >= 116.0F && local_x < 296.0F) {
            const uint32_t property = (uint32_t)((local_x - 116.0F) / 20.0F);
            const AfPath *path =
                af_path_find_node_const(&editor->project.path_data, editor->selected_id);
            const AfPathCommand *command =
                path != NULL && editor->selected_path_command < path->command_count
                    ? &editor->project.path_data
                           .commands[path->command_start + editor->selected_path_command]
                    : NULL;
            if (property <= AF_PROPERTY_PATH_CONTROL_OUT &&
                (property <= AF_PROPERTY_CORNER_RADIUS ||
                 (command != NULL && command->type != AF_PATH_CLOSE &&
                  (property == AF_PROPERTY_PATH_POINT || command->type == AF_PATH_CUBIC)))) {
                editor->selected_property = (AfPropertyType)property;
                af_editor_clear_key_selection(editor);
            }
        } else {
            for (index = 0U; index < editor->project.clip_count && index < 8U; ++index) {
                const float x = center_right - 74.0F * (float)(index + 1U);
                if (event->x >= x && event->x <= x + 68.0F) {
                    editor->selected_clip_id = editor->project.clips[index].id;
                    editor->playhead = 0.0F;
                    af_editor_clear_key_selection(editor);
                    break;
                }
            }
        }
        return true;
    }
    if (clip == NULL) {
        return true;
    }
    if (event->y < top + AF_EDITOR_TIMELINE_RULER_HEIGHT) {
        const float start_x = af_editor_time_to_x(editor, clip->loop_start);
        const float end_x = af_editor_time_to_x(editor, clip->loop_end);
        if (clip->loop && fabsf(event->x - start_x) <= 7.0F) {
            editor->drag_mode = AF_EDITOR_DRAG_LOOP_START;
            (void)af_project_clip_snapshot(&editor->project, clip->id, &editor->clip_drag_before);
        } else if (clip->loop && fabsf(event->x - end_x) <= 7.0F) {
            editor->drag_mode = AF_EDITOR_DRAG_LOOP_END;
            (void)af_project_clip_snapshot(&editor->project, clip->id, &editor->clip_drag_before);
        } else if (event->x >= ruler_x) {
            editor->drag_mode = AF_EDITOR_DRAG_PLAYHEAD;
            editor->preview_playing = false;
            editor->playhead =
                af_editor_clamp(af_editor_x_to_time(editor, event->x), 0.0F, clip->duration);
        }
        return true;
    }
    if (editor->curve_visible) {
        if (editor->selected_keyframe_count == 1U) {
            AfProjectKeyframe *key =
                af_project_find_keyframe(&editor->project, editor->selected_keyframes[0]);
            if (key == NULL) {
                af_editor_clear_key_selection(editor);
                return true;
            }
            const float curve_top = top + AF_EDITOR_TIMELINE_RULER_HEIGHT;
            const float bottom = (float)height - AF_EDITOR_STATUS_HEIGHT - 12.0F;
            const float out_x = ruler_x + 30.0F + key->keyframe.easing[0] * 100.0F;
            const float out_y = bottom - 30.0F - key->keyframe.easing[1] * 80.0F;
            const float in_x = center_right - 130.0F + key->keyframe.easing[2] * 100.0F;
            const float in_y = bottom - key->keyframe.easing[3] * 80.0F;
            (void)curve_top;
            if (fabsf(event->x - out_x) <= 10.0F && fabsf(event->y - out_y) <= 10.0F) {
                editor->drag_mode = AF_EDITOR_DRAG_TANGENT_OUT;
                af_editor_begin_selected_key_drag(editor);
            } else if (fabsf(event->x - in_x) <= 10.0F && fabsf(event->y - in_y) <= 10.0F) {
                editor->drag_mode = AF_EDITOR_DRAG_TANGENT_IN;
                af_editor_begin_selected_key_drag(editor);
            }
        }
        return true;
    }
    {
        uint64_t hit_id = 0U;
        float hit_distance = 10.0F;
        for (index = 0U; index < editor->project.keyframe_count; ++index) {
            const AfProjectKeyframe *key = &editor->project.keyframes[index];
            const uint32_t row = af_editor_track_row(editor, key->track_id);
            const float x = af_editor_time_to_x(editor, key->keyframe.time);
            const float y = top + AF_EDITOR_TIMELINE_RULER_HEIGHT +
                            (float)row * AF_EDITOR_TIMELINE_TRACK_HEIGHT + 12.0F;
            const float distance = fabsf(event->x - x) + fabsf(event->y - y);
            if (row != UINT32_MAX && distance < hit_distance) {
                hit_id = key->id;
                hit_distance = distance;
            }
        }
        if (hit_id != 0U) {
            af_editor_select_keyframe(editor, hit_id, (modifiers & SDL_KMOD_CTRL) != 0U);
            if (af_editor_keyframe_selected(editor, hit_id)) {
                editor->drag_mode = AF_EDITOR_DRAG_KEYFRAMES;
                editor->drag_start_mouse = (AfVec2){event->x, event->y};
                af_editor_begin_selected_key_drag(editor);
            }
        } else {
            if ((modifiers & SDL_KMOD_CTRL) == 0U) {
                af_editor_clear_key_selection(editor);
            }
            editor->drag_mode = AF_EDITOR_DRAG_KEY_BOX;
            editor->key_box_start = (AfVec2){event->x, event->y};
            editor->key_box_current = editor->key_box_start;
        }
    }
    return true;
}

static void af_editor_timeline_box_select(AfEditorState *editor, int height) {
    const float top = af_editor_timeline_top(editor, height);
    const float left = fminf(editor->key_box_start.x, editor->key_box_current.x);
    const float right = fmaxf(editor->key_box_start.x, editor->key_box_current.x);
    const float box_top = fminf(editor->key_box_start.y, editor->key_box_current.y);
    const float bottom = fmaxf(editor->key_box_start.y, editor->key_box_current.y);
    uint32_t index = 0U;
    for (index = 0U; index < editor->project.keyframe_count; ++index) {
        const AfProjectKeyframe *key = &editor->project.keyframes[index];
        const uint32_t row = af_editor_track_row(editor, key->track_id);
        const float x = af_editor_time_to_x(editor, key->keyframe.time);
        const float y = top + AF_EDITOR_TIMELINE_RULER_HEIGHT +
                        (float)row * AF_EDITOR_TIMELINE_TRACK_HEIGHT + 12.0F;
        if (row != UINT32_MAX && x >= left && x <= right && y >= box_top && y <= bottom &&
            editor->selected_keyframe_count < AF_EDITOR_COMMAND_MAX_KEYFRAMES) {
            editor->selected_keyframes[editor->selected_keyframe_count++] = key->id;
        }
    }
}

static bool af_editor_begin_path_drag(AfEditorState *editor, AfPath *path, AfEditorDragMode mode) {
    const AfProjectTrack *track = af_editor_selected_track(editor);
    float key_time = editor->playhead;
    AfProjectKeyframe *key = NULL;
    editor->path_key_drag = false;
    editor->path_drag_key_id = 0U;
    if (editor->snapping) {
        key_time = roundf(key_time * 10.0F) * 0.1F;
    }
    if (track != NULL && track->property >= AF_PROPERTY_PATH_POINT) {
        key = af_editor_track_key_at(editor, track->id, key_time);
        if (key == NULL && af_editor_add_keyframe(editor) == AF_RESULT_SUCCESS) {
            track = af_editor_selected_track(editor);
            key = track == NULL ? NULL : af_editor_track_key_at(editor, track->id, key_time);
        }
        if (key != NULL &&
            af_project_keyframe_snapshot(&editor->project, key->id,
                                         &editor->path_key_drag_before) == AF_RESULT_SUCCESS) {
            editor->path_key_drag = true;
            editor->path_drag_key_id = key->id;
            editor->drag_mode = mode;
            af_editor_status(editor, "Vector auto-key active at playhead");
            return true;
        }
    }
    if (path == NULL || path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS ||
        path->geometry_generation == UINT64_MAX ||
        editor->selected_path_command >= path->command_count ||
        af_project_path_snapshot(&editor->project, path->id, &editor->path_drag_before,
                                 editor->path_drag_commands,
                                 AF_EDITOR_COMMAND_MAX_PATH_COMMANDS) != AF_RESULT_SUCCESS) {
        af_editor_status(editor, "Path is too large for bounded drag undo");
        return false;
    }
    editor->path_drag_before.command_offset = 0U;
    editor->drag_mode = mode;
    return true;
}

static bool af_editor_path_mouse_down(AfEditorState *editor, int width, int height, AfVec2 mouse) {
    const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    AfPath *path = af_editor_selected_path(editor);
    AfAffine2 transform = {0};
    const float threshold_squared = 100.0F;
    if (node == NULL || path == NULL) {
        return false;
    }
    transform = af_affine_multiply(af_editor_camera(editor, width, height), node->world_transform);
    if (editor->selected_path_command < path->command_count) {
        const AfPath *preview_path =
            af_path_find_node_const(&editor->render_path_data, editor->selected_id);
        const AfPathCommand *selected =
            preview_path == NULL
                ? NULL
                : &editor->render_path_data
                       .commands[preview_path->command_start + editor->selected_path_command];
        if (selected != NULL && selected->type == AF_PATH_CUBIC) {
            const AfVec2 first = af_affine_transform_point(transform, selected->control1);
            const AfVec2 second = af_affine_transform_point(transform, selected->control2);
            const float first_x = mouse.x - first.x;
            const float first_y = mouse.y - first.y;
            const float second_x = mouse.x - second.x;
            const float second_y = mouse.y - second.y;
            if (first_x * first_x + first_y * first_y <= threshold_squared) {
                editor->selected_property = AF_PROPERTY_PATH_CONTROL_OUT;
                return af_editor_begin_path_drag(editor, path, AF_EDITOR_DRAG_PATH_CONTROL1);
            }
            if (second_x * second_x + second_y * second_y <= threshold_squared) {
                editor->selected_property = AF_PROPERTY_PATH_CONTROL_IN;
                return af_editor_begin_path_drag(editor, path, AF_EDITOR_DRAG_PATH_CONTROL2);
            }
        }
    }
    for (uint32_t index = path->command_count; index > 0U;) {
        const uint32_t command_index = --index;
        const AfPath *preview_path =
            af_path_find_node_const(&editor->render_path_data, editor->selected_id);
        const AfPathCommand *command =
            preview_path == NULL
                ? NULL
                : &editor->render_path_data.commands[preview_path->command_start + command_index];
        AfVec2 point = {0};
        float x = 0.0F;
        float y = 0.0F;
        if (command == NULL || command->type == AF_PATH_CLOSE) {
            continue;
        }
        point = af_affine_transform_point(transform, command->point);
        x = mouse.x - point.x;
        y = mouse.y - point.y;
        if (x * x + y * y <= threshold_squared) {
            editor->selected_path_command = command_index;
            editor->selected_property = AF_PROPERTY_PATH_POINT;
            return af_editor_begin_path_drag(editor, path, AF_EDITOR_DRAG_PATH_POINT);
        }
    }
    return false;
}

static void af_editor_drag_path_handle(AfEditorState *editor, int width, int height, AfVec2 mouse) {
    const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    AfPath *path = af_editor_selected_path(editor);
    AfAffine2 inverse = {0};
    AfVec2 point = {0};
    AfPathCommand *command = NULL;
    const AfPathCommand *before = NULL;
    const uint32_t selected = editor->selected_path_command;
    if (node == NULL || path == NULL || selected >= path->command_count ||
        !af_affine_inverse(node->world_transform, &inverse)) {
        return;
    }
    point = af_affine_transform_point(inverse,
                                      af_editor_screen_to_project(editor, width, height, mouse));
    if (!isfinite(point.x) || !isfinite(point.y)) {
        return;
    }
    point.x = af_editor_clamp(point.x, -1000000.0F, 1000000.0F);
    point.y = af_editor_clamp(point.y, -1000000.0F, 1000000.0F);
    if (editor->snapping) {
        point.x = roundf(point.x / editor->grid_step) * editor->grid_step;
        point.y = roundf(point.y / editor->grid_step) * editor->grid_step;
    }
    if (editor->path_key_drag) {
        AfProjectKeyframe *key =
            af_project_find_keyframe(&editor->project, editor->path_drag_key_id);
        if (key != NULL) {
            key->keyframe.value[0] = point.x;
            key->keyframe.value[1] = point.y;
        }
        return;
    }
    if (path->command_count != editor->path_drag_before.path.command_count) {
        return;
    }
    command = &editor->project.path_data.commands[path->command_start + selected];
    before = &editor->path_drag_commands[selected];
    if (editor->drag_mode == AF_EDITOR_DRAG_PATH_POINT && command->type != AF_PATH_CLOSE) {
        const AfVec2 delta = {point.x - before->point.x, point.y - before->point.y};
        command->point = point;
        if (command->type == AF_PATH_CUBIC) {
            command->control2 =
                (AfVec2){before->control2.x + delta.x, before->control2.y + delta.y};
        }
        if (selected + 1U < path->command_count) {
            AfPathCommand *next = command + 1U;
            const AfPathCommand *next_before = before + 1U;
            if (next->type == AF_PATH_CUBIC) {
                next->control1 =
                    (AfVec2){next_before->control1.x + delta.x, next_before->control1.y + delta.y};
            }
        }
    } else if (editor->drag_mode == AF_EDITOR_DRAG_PATH_CONTROL1 &&
               command->type == AF_PATH_CUBIC) {
        command->control1 = point;
    } else if (editor->drag_mode == AF_EDITOR_DRAG_PATH_CONTROL2 &&
               command->type == AF_PATH_CUBIC) {
        command->control2 = point;
    } else {
        return;
    }
    command->point.x = af_editor_clamp(command->point.x, -1000000.0F, 1000000.0F);
    command->point.y = af_editor_clamp(command->point.y, -1000000.0F, 1000000.0F);
    command->control1.x = af_editor_clamp(command->control1.x, -1000000.0F, 1000000.0F);
    command->control1.y = af_editor_clamp(command->control1.y, -1000000.0F, 1000000.0F);
    command->control2.x = af_editor_clamp(command->control2.x, -1000000.0F, 1000000.0F);
    command->control2.y = af_editor_clamp(command->control2.y, -1000000.0F, 1000000.0F);
    if (selected + 1U < path->command_count) {
        AfPathCommand *next = command + 1U;
        next->control1.x = af_editor_clamp(next->control1.x, -1000000.0F, 1000000.0F);
        next->control1.y = af_editor_clamp(next->control1.y, -1000000.0F, 1000000.0F);
    }
    (void)af_path_mark_geometry_changed(path);
}

static void af_editor_commit_path_drag(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    AfEditorCommand command = {0};
    AfResult result = AF_RESULT_NOT_FOUND;
    if (editor->path_key_drag) {
        AfProjectKeyframeSnapshot after = {0};
        result = af_project_keyframe_snapshot(&editor->project, editor->path_drag_key_id, &after);
        if (result == AF_RESULT_SUCCESS) {
            result = af_editor_command_animation(AF_EDITOR_MERGE_KEYFRAME_VALUE,
                                                 editor->path_drag_key_id, &command);
        }
        if (result == AF_RESULT_SUCCESS) {
            command.keyframe_before_count = 1U;
            command.keyframe_before[0] = editor->path_key_drag_before;
            command.keyframe_after_count = 1U;
            command.keyframe_after[0] = after;
            result =
                af_editor_history_execute(&editor->history, &editor->project, &command, true, true);
        }
        if (result == AF_RESULT_SUCCESS) {
            editor->dirty = true;
            af_editor_status(editor, "Vector keyframe edited");
        }
        editor->path_key_drag = false;
        editor->path_drag_key_id = 0U;
        return;
    }
    if (path == NULL || path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS ||
        path->id != editor->path_drag_before.path.id) {
        return;
    }
    result = af_editor_command_path(AF_EDITOR_MERGE_PATH_POINT, path->id, &command);
    if (result == AF_RESULT_SUCCESS) {
        command.path_before_count = 1U;
        command.path_before[0] = editor->path_drag_before;
        command.path_before[0].command_offset = 0U;
        command.path_command_before_count = editor->path_drag_before.path.command_count;
        memcpy(command.path_command_before, editor->path_drag_commands,
               (size_t)command.path_command_before_count * sizeof(*command.path_command_before));
        command.path_after_count = 1U;
        result = af_project_path_snapshot(&editor->project, path->id, &command.path_after[0],
                                          command.path_command_after,
                                          AF_EDITOR_COMMAND_MAX_PATH_COMMANDS);
    }
    if (result == AF_RESULT_SUCCESS) {
        command.path_after[0].command_offset = 0U;
        command.path_command_after_count = path->command_count;
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, true, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        editor->dirty = true;
        af_editor_status(editor, "Path point edited");
    }
}

static void af_editor_canvas_box_select(AfEditorState *editor, int width, int height) {
    const AfAffine2 camera = af_editor_camera(editor, width, height);
    const float box_minimum_x = fminf(editor->canvas_box_start.x, editor->canvas_box_current.x);
    const float box_minimum_y = fminf(editor->canvas_box_start.y, editor->canvas_box_current.y);
    const float box_maximum_x = fmaxf(editor->canvas_box_start.x, editor->canvas_box_current.x);
    const float box_maximum_y = fmaxf(editor->canvas_box_start.y, editor->canvas_box_current.y);
    if (!editor->canvas_box_additive) {
        af_editor_select_only(editor, 0U);
    }
    for (uint32_t index = 0U; index < editor->preview_scene.count; ++index) {
        const AfSceneNode *node = &editor->preview_scene.nodes[index];
        float minimum_x = 0.0F;
        float minimum_y = 0.0F;
        float maximum_x = 0.0F;
        float maximum_y = 0.0F;
        if (af_editor_screen_bounds(node, camera, &minimum_x, &minimum_y, &maximum_x, &maximum_y) &&
            maximum_x >= box_minimum_x && minimum_x <= box_maximum_x &&
            maximum_y >= box_minimum_y && minimum_y <= box_maximum_y) {
            af_editor_selection_add(editor, node->id);
        }
    }
    editor->selected_path_command =
        af_path_find_node_const(&editor->project.path_data, editor->selected_id) != NULL
            ? 0U
            : UINT32_MAX;
    if (editor->selected_count == 0U) {
        af_editor_status(editor, "Selection cleared");
    } else {
        char status[64];
        (void)snprintf(status, sizeof(status), "%u objects selected", editor->selected_count);
        af_editor_status(editor, status);
    }
}

static void af_editor_prepare_selection_smoke(AfEditorState *editor, int width, int height) {
    const AfAffine2 camera = af_editor_camera(editor, width, height);
    bool found = false;
    float box_minimum_x = 0.0F;
    float box_minimum_y = 0.0F;
    float box_maximum_x = 0.0F;
    float box_maximum_y = 0.0F;
    for (uint32_t index = 0U; index < editor->preview_scene.count; ++index) {
        float minimum_x = 0.0F;
        float minimum_y = 0.0F;
        float maximum_x = 0.0F;
        float maximum_y = 0.0F;
        if (!af_editor_screen_bounds(&editor->preview_scene.nodes[index], camera, &minimum_x,
                                     &minimum_y, &maximum_x, &maximum_y)) {
            continue;
        }
        if (!found) {
            box_minimum_x = minimum_x;
            box_minimum_y = minimum_y;
            box_maximum_x = maximum_x;
            box_maximum_y = maximum_y;
            found = true;
        } else {
            box_minimum_x = fminf(box_minimum_x, minimum_x);
            box_minimum_y = fminf(box_minimum_y, minimum_y);
            box_maximum_x = fmaxf(box_maximum_x, maximum_x);
            box_maximum_y = fmaxf(box_maximum_y, maximum_y);
        }
    }
    if (found) {
        editor->canvas_box_start = (AfVec2){box_minimum_x - 8.0F, box_minimum_y - 8.0F};
        editor->canvas_box_current = (AfVec2){box_maximum_x + 8.0F, box_maximum_y + 8.0F};
        editor->canvas_box_additive = false;
        af_editor_canvas_box_select(editor, width, height);
        editor->drag_mode = AF_EDITOR_DRAG_CANVAS_BOX;
    }
}

static void af_editor_mouse_down(AfEditorState *editor, int width, int height,
                                 const SDL_MouseButtonEvent *event, SDL_Keymod modifiers) {
    const float x = event->x;
    const float y = event->y;
    float canvas_x = 0.0F;
    float canvas_y = 0.0F;
    float canvas_width = 0.0F;
    float canvas_height = 0.0F;
    editor->mouse_position = (AfVec2){x, y};
    if (event->button == SDL_BUTTON_RIGHT) {
        af_editor_open_context_menu(editor, width, height, x, y);
        return;
    }
    if (editor->context_menu != AF_EDITOR_CONTEXT_NONE) {
        const int32_t item = af_editor_context_item_at(editor, width, height, x, y);
        if (event->button == SDL_BUTTON_LEFT && item >= 0) {
            af_editor_execute_context_item(editor, (uint32_t)item, width, height);
        }
        editor->context_menu = AF_EDITOR_CONTEXT_NONE;
        return;
    }
    if (af_editor_timeline_mouse_down(editor, width, height, event, modifiers)) {
        return;
    }
    editor->timeline_focus = false;
    if (event->button == SDL_BUTTON_MIDDLE) {
        editor->drag_mode = AF_EDITOR_DRAG_PAN;
        editor->drag_start_mouse = (AfVec2){x, y};
        return;
    }
    if (event->button != SDL_BUTTON_LEFT) {
        return;
    }
    if (fabsf(x - editor->left_width) <= 5.0F) {
        editor->drag_mode = AF_EDITOR_DRAG_LEFT_DOCK;
        return;
    }
    if (fabsf(x - ((float)width - editor->right_width)) <= 5.0F) {
        editor->drag_mode = AF_EDITOR_DRAG_RIGHT_DOCK;
        return;
    }
    if (x < editor->left_width && y >= AF_EDITOR_TOOLBAR_HEIGHT + 8.0F) {
        const uint32_t row =
            (uint32_t)((y - AF_EDITOR_TOOLBAR_HEIGHT - 8.0F) / AF_EDITOR_ROW_HEIGHT);
        const uint32_t hierarchy_index = editor->hierarchy_scroll + row;
        if (hierarchy_index < editor->hierarchy_count) {
            const uint64_t clicked =
                editor->project.scene.nodes[editor->hierarchy_order[hierarchy_index]].id;
            if ((modifiers & SDL_KMOD_SHIFT) != 0U) {
                af_editor_hierarchy_select_range(editor, hierarchy_index,
                                                 (modifiers & SDL_KMOD_CTRL) != 0U);
            } else if ((modifiers & SDL_KMOD_CTRL) != 0U) {
                af_editor_selection_toggle(editor, clicked);
                editor->hierarchy_anchor_id = clicked;
            } else {
                af_editor_select_only(editor, clicked);
                editor->hierarchy_anchor_id = clicked;
            }
            editor->selected_path_command =
                af_path_find_node_const(&editor->project.path_data, editor->selected_id) != NULL
                    ? 0U
                    : UINT32_MAX;
        }
        return;
    }
    if (y < AF_EDITOR_TOOLBAR_HEIGHT && x >= editor->left_width) {
        const float toolbar_x = x - editor->left_width - 14.0F;
        if (toolbar_x >= 0.0F && toolbar_x < 34.0F) {
            (void)af_editor_create_primitive(editor, AF_NODE_RECTANGLE, width, height);
        } else if (toolbar_x >= 42.0F && toolbar_x < 76.0F) {
            (void)af_editor_create_primitive(editor, AF_NODE_ROUNDED_RECTANGLE, width, height);
        } else if (toolbar_x >= 84.0F && toolbar_x < 118.0F) {
            (void)af_editor_create_primitive(editor, AF_NODE_ELLIPSE, width, height);
        } else if (toolbar_x >= 126.0F && toolbar_x < 160.0F) {
            (void)af_editor_create_path(editor, width, height);
        } else if (toolbar_x >= 172.0F && toolbar_x < 260.0F) {
            if (y < 22.0F) {
                editor->grid_visible = !editor->grid_visible;
            } else {
                editor->snapping = !editor->snapping;
            }
        }
        return;
    }
    if (x >= (float)width - editor->right_width) {
        af_editor_handle_inspector(editor, width, x, y);
        return;
    }
    af_editor_layout(editor, width, height, &canvas_x, &canvas_y, &canvas_width, &canvas_height);
    if (x >= canvas_x && x < canvas_x + canvas_width && y >= canvas_y &&
        y < canvas_y + canvas_height) {
        AfAffine2 camera = af_editor_camera(editor, width, height);
        const AfSceneNode *selected =
            af_scene_find_const(&editor->project.scene, editor->selected_id);
        AfVec2 origin = {0};
        if (editor->selected_count <= 1U &&
            af_editor_path_mouse_down(editor, width, height, (AfVec2){x, y})) {
            return;
        }
        if (selected != NULL) {
            origin = af_affine_transform_point(
                af_affine_multiply(camera, selected->world_transform), (AfVec2){0});
            if (fabsf(x - (origin.x + 56.0F)) <= 10.0F && fabsf(y - origin.y) <= 10.0F) {
                editor->drag_mode = AF_EDITOR_DRAG_X;
            } else if (fabsf(x - origin.x) <= 10.0F && fabsf(y - (origin.y + 56.0F)) <= 10.0F) {
                editor->drag_mode = AF_EDITOR_DRAG_Y;
            }
        }
        if (editor->drag_mode == AF_EDITOR_DRAG_NONE) {
            const uint64_t hit = af_editor_hit_test(
                editor, af_editor_screen_to_project(editor, width, height, (AfVec2){x, y}));
            const bool control = (modifiers & SDL_KMOD_CTRL) != 0U;
            const bool additive = (modifiers & (SDL_KMOD_CTRL | SDL_KMOD_SHIFT)) != 0U;
            if (hit != 0U) {
                if (control) {
                    af_editor_selection_toggle(editor, hit);
                } else if ((modifiers & SDL_KMOD_SHIFT) != 0U) {
                    af_editor_selection_add(editor, hit);
                } else if (!af_editor_selection_contains(editor, hit)) {
                    af_editor_select_only(editor, hit);
                } else {
                    editor->selected_id = hit;
                }
                editor->selected_path_command =
                    af_path_find_node_const(&editor->project.path_data, editor->selected_id) != NULL
                        ? 0U
                        : UINT32_MAX;
                if (af_editor_selection_contains(editor, hit)) {
                    editor->drag_mode = AF_EDITOR_DRAG_MOVE;
                }
            } else {
                if (!additive) {
                    af_editor_select_only(editor, 0U);
                    editor->selected_path_command = UINT32_MAX;
                }
                editor->canvas_box_start = (AfVec2){x, y};
                editor->canvas_box_current = editor->canvas_box_start;
                editor->canvas_box_additive = additive;
                editor->drag_mode = AF_EDITOR_DRAG_CANVAS_BOX;
            }
        }
        if (editor->drag_mode == AF_EDITOR_DRAG_MOVE || editor->drag_mode == AF_EDITOR_DRAG_X ||
            editor->drag_mode == AF_EDITOR_DRAG_Y) {
            editor->drag_start_mouse =
                af_editor_screen_to_project(editor, width, height, (AfVec2){x, y});
            af_editor_begin_selection_modify(editor, true);
        }
    }
}

static void af_editor_mouse_motion(AfEditorState *editor, int width, int height,
                                   const SDL_MouseMotionEvent *event) {
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    editor->mouse_position = (AfVec2){event->x, event->y};
    if (editor->drag_mode == AF_EDITOR_DRAG_COLOR_SV ||
        editor->drag_mode == AF_EDITOR_DRAG_COLOR_HUE ||
        editor->drag_mode == AF_EDITOR_DRAG_COLOR_ALPHA) {
        af_editor_update_color_picker(editor, width, event->x, event->y);
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_CANVAS_BOX) {
        editor->canvas_box_current = (AfVec2){event->x, event->y};
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_TIMELINE_HEIGHT) {
        editor->timeline_height = af_editor_clamp(
            (float)height - AF_EDITOR_STATUS_HEIGHT - event->y, AF_EDITOR_TIMELINE_MIN_HEIGHT,
            (float)height - AF_EDITOR_TOOLBAR_HEIGHT - 150.0F);
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_TIMELINE_PAN) {
        editor->timeline_pan =
            fmaxf(0.0F, editor->timeline_pan - event->xrel / editor->timeline_zoom);
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_PLAYHEAD && clip != NULL) {
        editor->playhead =
            af_editor_clamp(af_editor_x_to_time(editor, event->x), 0.0F, clip->duration);
        return;
    }
    if ((editor->drag_mode == AF_EDITOR_DRAG_LOOP_START ||
         editor->drag_mode == AF_EDITOR_DRAG_LOOP_END) &&
        clip != NULL) {
        AfProjectClip *mutable_clip = af_project_find_clip(&editor->project, clip->id);
        float time = af_editor_clamp(af_editor_x_to_time(editor, event->x), 0.0F, clip->duration);
        if (editor->snapping) {
            time = roundf(time * 10.0F) * 0.1F;
        }
        if (editor->drag_mode == AF_EDITOR_DRAG_LOOP_START) {
            mutable_clip->loop_start = fminf(time, mutable_clip->loop_end);
        } else {
            mutable_clip->loop_end = fmaxf(time, mutable_clip->loop_start);
        }
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_KEYFRAMES && clip != NULL) {
        const float delta = (event->x - editor->drag_start_mouse.x) / editor->timeline_zoom;
        uint32_t index = 0U;
        for (index = 0U; index < editor->key_drag_count; ++index) {
            AfProjectKeyframe *key = af_project_find_keyframe(
                &editor->project, editor->key_drag_before[index].keyframe.id);
            float time = editor->key_drag_before[index].keyframe.keyframe.time + delta;
            if (key == NULL) {
                continue;
            }
            time = af_editor_clamp(time, 0.0F, clip->duration);
            if (editor->snapping) {
                time = roundf(time * 10.0F) * 0.1F;
            }
            key->keyframe.time = time;
        }
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_KEY_BOX) {
        editor->key_box_current = (AfVec2){event->x, event->y};
        return;
    }
    if ((editor->drag_mode == AF_EDITOR_DRAG_TANGENT_IN ||
         editor->drag_mode == AF_EDITOR_DRAG_TANGENT_OUT) &&
        editor->key_drag_count == 1U) {
        AfProjectKeyframe *key =
            af_project_find_keyframe(&editor->project, editor->key_drag_before[0].keyframe.id);
        const float top = af_editor_timeline_top(editor, height) + AF_EDITOR_TIMELINE_RULER_HEIGHT;
        const float bottom = (float)height - AF_EDITOR_STATUS_HEIGHT - 12.0F;
        const float right = (float)width - editor->right_width;
        if (key == NULL) {
            return;
        }
        key->keyframe.interpolation = AF_INTERPOLATION_CUBIC_BEZIER;
        if (editor->drag_mode == AF_EDITOR_DRAG_TANGENT_OUT) {
            key->keyframe.easing[0] = af_editor_clamp(
                (event->x - af_editor_timeline_ruler_x(editor) - 30.0F) / 100.0F, 0.0F, 1.0F);
            key->keyframe.easing[1] =
                af_editor_clamp((bottom - 30.0F - event->y) / 80.0F, -2.0F, 2.0F);
        } else {
            key->keyframe.easing[2] =
                af_editor_clamp((event->x - (right - 130.0F)) / 100.0F, 0.0F, 1.0F);
            key->keyframe.easing[3] = af_editor_clamp((bottom - event->y) / 80.0F, -2.0F, 2.0F);
        }
        (void)top;
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_PATH_POINT ||
        editor->drag_mode == AF_EDITOR_DRAG_PATH_CONTROL1 ||
        editor->drag_mode == AF_EDITOR_DRAG_PATH_CONTROL2) {
        af_editor_drag_path_handle(editor, width, height, (AfVec2){event->x, event->y});
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_PAN) {
        editor->pan.x += event->xrel;
        editor->pan.y += event->yrel;
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_LEFT_DOCK) {
        editor->left_width =
            af_editor_clamp(event->x, AF_EDITOR_MIN_DOCK_WIDTH,
                            (float)width - editor->right_width - AF_EDITOR_MIN_VIEW_WIDTH);
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_RIGHT_DOCK) {
        editor->right_width =
            af_editor_clamp((float)width - event->x, AF_EDITOR_MIN_DOCK_WIDTH,
                            (float)width - editor->left_width - AF_EDITOR_MIN_VIEW_WIDTH);
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_MOVE || editor->drag_mode == AF_EDITOR_DRAG_X ||
        editor->drag_mode == AF_EDITOR_DRAG_Y) {
        const AfVec2 project_current =
            af_editor_screen_to_project(editor, width, height, (AfVec2){event->x, event->y});
        for (uint32_t index = 0U; index < editor->selection_drag_count; ++index) {
            const AfProjectNodeSnapshot *before = &editor->selection_drag_before[index];
            AfSceneNode *node = af_scene_find(&editor->project.scene, before->node.id);
            AfVec2 current = project_current;
            AfVec2 start = editor->drag_start_mouse;
            if (node == NULL) {
                continue;
            }
            if (before->node.parent_id != AF_INVALID_NODE_ID) {
                const AfSceneNode *parent =
                    af_scene_find_const(&editor->project.scene, before->node.parent_id);
                AfAffine2 parent_inverse = af_affine_identity();
                if (parent != NULL && af_affine_inverse(parent->world_transform, &parent_inverse)) {
                    current = af_affine_transform_point(parent_inverse, current);
                    start = af_affine_transform_point(parent_inverse, start);
                }
            }
            node->local_transform.position = before->node.local_transform.position;
            if (editor->drag_mode != AF_EDITOR_DRAG_Y) {
                node->local_transform.position.x += current.x - start.x;
            }
            if (editor->drag_mode != AF_EDITOR_DRAG_X) {
                node->local_transform.position.y += current.y - start.y;
            }
            if (editor->snapping) {
                node->local_transform.position.x =
                    roundf(node->local_transform.position.x / editor->grid_step) *
                    editor->grid_step;
                node->local_transform.position.y =
                    roundf(node->local_transform.position.y / editor->grid_step) *
                    editor->grid_step;
            }
        }
        (void)af_scene_evaluate_world(&editor->project.scene);
    }
}

static void af_editor_mouse_up(AfEditorState *editor, int width, int height) {
    if (editor->drag_mode == AF_EDITOR_DRAG_COLOR_SV ||
        editor->drag_mode == AF_EDITOR_DRAG_COLOR_HUE ||
        editor->drag_mode == AF_EDITOR_DRAG_COLOR_ALPHA) {
        (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_FILL, false);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_MOVE || editor->drag_mode == AF_EDITOR_DRAG_X ||
               editor->drag_mode == AF_EDITOR_DRAG_Y) {
        (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_TRANSFORM, false);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_CANVAS_BOX) {
        af_editor_canvas_box_select(editor, width, height);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_KEYFRAMES) {
        af_editor_timeline_commit_keys(editor, AF_EDITOR_MERGE_KEYFRAME_TIME);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_LOOP_START ||
               editor->drag_mode == AF_EDITOR_DRAG_LOOP_END) {
        af_editor_timeline_commit_clip(editor);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_TANGENT_IN ||
               editor->drag_mode == AF_EDITOR_DRAG_TANGENT_OUT) {
        af_editor_timeline_commit_keys(editor, AF_EDITOR_MERGE_TANGENT);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_KEY_BOX) {
        af_editor_timeline_box_select(editor, height);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_PATH_POINT ||
               editor->drag_mode == AF_EDITOR_DRAG_PATH_CONTROL1 ||
               editor->drag_mode == AF_EDITOR_DRAG_PATH_CONTROL2) {
        af_editor_commit_path_drag(editor);
    }
    editor->drag_mode = AF_EDITOR_DRAG_NONE;
}

static void af_editor_mouse_wheel(AfEditorState *editor, int width, int height,
                                  const SDL_MouseWheelEvent *event) {
    if (event->mouse_x < editor->left_width) {
        if (event->y > 0.0F && editor->hierarchy_scroll > 0U) {
            --editor->hierarchy_scroll;
        } else if (event->y < 0.0F && editor->hierarchy_scroll < editor->hierarchy_count) {
            ++editor->hierarchy_scroll;
        }
        return;
    }
    if (event->mouse_y >= af_editor_timeline_top(editor, height)) {
        const float anchor_time = af_editor_x_to_time(editor, event->mouse_x);
        if ((SDL_GetModState() & SDL_KMOD_SHIFT) != 0U) {
            editor->timeline_pan = fmaxf(0.0F, editor->timeline_pan - event->y * 0.25F);
        } else {
            editor->timeline_zoom = af_editor_clamp(
                editor->timeline_zoom * (event->y > 0.0F ? 1.12F : 0.89F), 24.0F, 800.0F);
            editor->timeline_pan =
                fmaxf(0.0F, anchor_time - (event->mouse_x - af_editor_timeline_ruler_x(editor)) /
                                              editor->timeline_zoom);
        }
        return;
    }
    if (event->mouse_x >= (float)width - editor->right_width) {
        return;
    }
    {
        const AfVec2 mouse = {event->mouse_x, event->mouse_y};
        const AfVec2 project = af_editor_screen_to_project(editor, width, height, mouse);
        float canvas_x = 0.0F;
        float canvas_y = 0.0F;
        float canvas_width = 0.0F;
        float canvas_height = 0.0F;
        af_editor_layout(editor, width, height, &canvas_x, &canvas_y, &canvas_width,
                         &canvas_height);
        (void)canvas_width;
        (void)canvas_height;
        editor->zoom =
            af_editor_clamp(editor->zoom * (event->y > 0.0F ? 1.12F : 0.89F), 0.1F, 8.0F);
        editor->pan.x = mouse.x - canvas_x - project.x * editor->zoom;
        editor->pan.y = mouse.y - canvas_y - project.y * editor->zoom;
    }
}

static void af_editor_key(AfEditorState *editor, int width, int height,
                          const SDL_KeyboardEvent *event) {
    const bool control = (event->mod & SDL_KMOD_CTRL) != 0U;
    const bool alt = (event->mod & SDL_KMOD_ALT) != 0U;
    const bool shift = (event->mod & SDL_KMOD_SHIFT) != 0U;
    const float nudge = (event->mod & SDL_KMOD_SHIFT) != 0U ? 10.0F : 1.0F;
    if (!event->down) {
        return;
    }
    if (editor->context_menu != AF_EDITOR_CONTEXT_NONE && event->key == SDLK_ESCAPE) {
        editor->context_menu = AF_EDITOR_CONTEXT_NONE;
        return;
    }
    if (editor->rename_active) {
        if (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER) {
            af_editor_finish_rename(editor, true);
        } else if (event->key == SDLK_ESCAPE) {
            af_editor_finish_rename(editor, false);
        } else if (event->key == SDLK_BACKSPACE) {
            af_editor_backspace_utf8(editor->rename_buffer);
        }
        return;
    }
    if (control &&
        (event->key == SDLK_Y || ((event->mod & SDL_KMOD_SHIFT) != 0U && event->key == SDLK_Z))) {
        if (af_editor_history_redo(&editor->history, &editor->project) == AF_RESULT_SUCCESS) {
            editor->dirty = true;
            af_editor_reconcile_selection(editor);
            af_editor_status(editor, "Redo");
        }
    } else if (control && event->key == SDLK_Z) {
        if (af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS) {
            editor->dirty = true;
            af_editor_reconcile_selection(editor);
            af_editor_status(editor, "Undo");
        }
    } else if (control && event->key == SDLK_S) {
        (void)af_editor_save(editor);
    } else if (control && event->key == SDLK_C && editor->timeline_focus) {
        af_editor_copy_keyframes(editor);
    } else if (control && event->key == SDLK_V && editor->timeline_focus) {
        (void)af_editor_paste_keyframes(editor, false);
    } else if (control && event->key == SDLK_D) {
        if (editor->timeline_focus && editor->selected_keyframe_count != 0U) {
            (void)af_editor_paste_keyframes(editor, true);
        } else {
            (void)af_editor_duplicate_selection(editor);
        }
    } else if (event->key == SDLK_F5) {
        (void)af_editor_compile(editor);
    } else if (alt && event->key == SDLK_C) {
        (void)af_editor_new_clip(editor);
    } else if (alt && event->key == SDLK_DELETE) {
        (void)af_editor_delete_clip(editor);
    } else if (event->key == SDLK_DELETE || event->key == SDLK_BACKSPACE) {
        if (editor->timeline_focus && editor->selected_keyframe_count != 0U) {
            (void)af_editor_delete_keyframes(editor);
        } else if (!shift && af_editor_selected_path(editor) != NULL &&
                   editor->selected_path_command != UINT32_MAX) {
            (void)af_editor_delete_path_point(editor);
        } else {
            (void)af_editor_delete_selection(editor);
        }
    } else if (event->key == SDLK_F2) {
        af_editor_begin_rename(editor);
    } else if (event->key == SDLK_1) {
        (void)af_editor_create_primitive(editor, AF_NODE_RECTANGLE, width, height);
    } else if (event->key == SDLK_2) {
        (void)af_editor_create_primitive(editor, AF_NODE_ROUNDED_RECTANGLE, width, height);
    } else if (event->key == SDLK_3) {
        (void)af_editor_create_primitive(editor, AF_NODE_ELLIPSE, width, height);
    } else if (event->key == SDLK_4) {
        (void)af_editor_create_path(editor, width, height);
    } else if (!control && !alt && event->key == SDLK_I && !editor->timeline_focus &&
               af_editor_selected_path(editor) != NULL) {
        (void)af_editor_insert_path_point(editor);
    } else if (!control && !alt && event->key == SDLK_C && !editor->timeline_focus &&
               af_editor_selected_path(editor) != NULL) {
        (void)af_editor_convert_path_point(editor);
    } else if (!control && !alt && event->key == SDLK_O && !editor->timeline_focus &&
               af_editor_selected_path(editor) != NULL) {
        (void)af_editor_toggle_path_closed(editor);
    } else if (!control && !alt && event->key == SDLK_F && !editor->timeline_focus &&
               af_editor_selected_path(editor) != NULL) {
        (void)af_editor_toggle_path_fill_rule(editor);
    } else if (!control && !alt && shift && event->key == SDLK_M && !editor->timeline_focus &&
               af_editor_selected_path(editor) != NULL) {
        (void)af_editor_add_path_contour(editor);
    } else if (!control && !alt && shift && event->key == SDLK_R && !editor->timeline_focus &&
               af_editor_selected_path(editor) != NULL) {
        (void)af_editor_reverse_path_contour(editor);
    } else if (event->key == SDLK_LEFT) {
        af_editor_nudge(editor, -nudge, 0.0F);
    } else if (event->key == SDLK_RIGHT) {
        af_editor_nudge(editor, nudge, 0.0F);
    } else if (event->key == SDLK_UP) {
        af_editor_nudge(editor, 0.0F, -nudge);
    } else if (event->key == SDLK_DOWN) {
        af_editor_nudge(editor, 0.0F, nudge);
    } else if (event->key == SDLK_LEFTBRACKET) {
        if (control && editor->timeline_focus) {
            (void)af_editor_modify_selected_keys(editor, 2, AF_INTERPOLATION_LINEAR);
        } else {
            af_editor_modify_scalar(editor, AF_EDITOR_MERGE_LAYER, -1.0F, 2);
        }
    } else if (event->key == SDLK_RIGHTBRACKET) {
        if (control && editor->timeline_focus) {
            (void)af_editor_modify_selected_keys(editor, 1, AF_INTERPOLATION_LINEAR);
        } else {
            af_editor_modify_scalar(editor, AF_EDITOR_MERGE_LAYER, 1.0F, 2);
        }
    } else if (event->key == SDLK_Q) {
        af_editor_modify_scalar(editor, AF_EDITOR_MERGE_TRANSFORM, -0.05F, 3);
    } else if (event->key == SDLK_E) {
        af_editor_modify_scalar(editor, AF_EDITOR_MERGE_TRANSFORM, 0.05F, 3);
    } else if (event->key == SDLK_MINUS) {
        af_editor_modify_scalar(editor, AF_EDITOR_MERGE_TRANSFORM, -0.05F, 4);
    } else if (event->key == SDLK_EQUALS || event->key == SDLK_PLUS) {
        af_editor_modify_scalar(editor, AF_EDITOR_MERGE_TRANSFORM, 0.05F, 4);
    } else if (event->key == SDLK_G) {
        editor->grid_visible = !editor->grid_visible;
    } else if (event->key == SDLK_N) {
        editor->snapping = !editor->snapping;
    } else if (event->key == SDLK_U) {
        af_editor_reparent(editor, AF_INVALID_NODE_ID);
    } else if (event->key == SDLK_SPACE) {
        editor->preview_playing = !editor->preview_playing;
    } else if (event->key == SDLK_K) {
        (void)af_editor_add_keyframe(editor);
    } else if (event->key == SDLK_S && editor->timeline_focus) {
        (void)af_editor_modify_selected_keys(editor, 0, AF_INTERPOLATION_STEP);
    } else if (event->key == SDLK_L && editor->timeline_focus) {
        (void)af_editor_modify_selected_keys(editor, 0, AF_INTERPOLATION_LINEAR);
    } else if (event->key == SDLK_B && editor->timeline_focus) {
        (void)af_editor_modify_selected_keys(editor, 0, AF_INTERPOLATION_CUBIC_BEZIER);
    } else if ((event->mod & SDL_KMOD_SHIFT) != 0U && event->key == SDLK_R &&
               editor->timeline_focus) {
        (void)af_editor_modify_selected_keys(editor, 3, AF_INTERPOLATION_LINEAR);
    } else if (event->key == SDLK_F6) {
        editor->curve_visible = !editor->curve_visible;
        editor->timeline_focus = true;
    } else if (event->key == SDLK_F7 && editor->timeline_focus) {
        (void)af_editor_modify_selected_keys(editor, 4, AF_INTERPOLATION_CUBIC_BEZIER);
    } else if (event->key == SDLK_F8 && editor->timeline_focus) {
        (void)af_editor_modify_selected_keys(editor, 5, AF_INTERPOLATION_CUBIC_BEZIER);
    } else if (event->key == SDLK_F9 && editor->timeline_focus) {
        (void)af_editor_modify_selected_keys(editor, 6, AF_INTERPOLATION_CUBIC_BEZIER);
    } else if (event->key == SDLK_HOME && editor->timeline_focus) {
        editor->playhead = 0.0F;
    } else if (event->key == SDLK_END && editor->timeline_focus) {
        const AfProjectClip *clip =
            af_project_find_clip_const(&editor->project, editor->selected_clip_id);
        if (clip != NULL) {
            editor->playhead = clip->duration;
        }
    } else if (event->key == SDLK_ESCAPE) {
        if (editor->timeline_focus) {
            af_editor_clear_key_selection(editor);
        } else {
            af_editor_select_only(editor, 0U);
            editor->selected_path_command = UINT32_MAX;
        }
    }
}

static void af_editor_mouse_scale(const AfEditorState *editor, float *x, float *y) {
    int logical_width = 0;
    int logical_height = 0;
    int pixel_width = 0;
    int pixel_height = 0;
    if (SDL_GetWindowSize(editor->window.window, &logical_width, &logical_height) &&
        SDL_GetWindowSizeInPixels(editor->window.window, &pixel_width, &pixel_height) &&
        logical_width > 0 && logical_height > 0) {
        *x *= (float)pixel_width / (float)logical_width;
        *y *= (float)pixel_height / (float)logical_height;
    }
}

static void af_editor_handle_event(AfEditorState *editor, SDL_Event *event, int width, int height) {
    if (event->type == SDL_EVENT_QUIT) {
        editor->running = false;
    } else if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
               event->type == SDL_EVENT_WINDOW_RESIZED) {
        af_vk_renderer_request_resize(editor->renderer);
    } else if (event->type == SDL_EVENT_KEY_DOWN) {
        af_editor_key(editor, width, height, &event->key);
    } else if (event->type == SDL_EVENT_TEXT_INPUT) {
        af_editor_append_text(editor, event->text.text);
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        SDL_MouseButtonEvent button = event->button;
        af_editor_mouse_scale(editor, &button.x, &button.y);
        af_editor_mouse_down(editor, width, height, &button, SDL_GetModState());
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        af_editor_mouse_up(editor, width, height);
    } else if (event->type == SDL_EVENT_MOUSE_MOTION) {
        SDL_MouseMotionEvent motion = event->motion;
        af_editor_mouse_scale(editor, &motion.x, &motion.y);
        af_editor_mouse_scale(editor, &motion.xrel, &motion.yrel);
        af_editor_mouse_motion(editor, width, height, &motion);
    } else if (event->type == SDL_EVENT_MOUSE_WHEEL) {
        SDL_MouseWheelEvent wheel = event->wheel;
        af_editor_mouse_scale(editor, &wheel.mouse_x, &wheel.mouse_y);
        af_editor_mouse_wheel(editor, width, height, &wheel);
    }
}

static void af_editor_parse_arguments(AfEditorState *editor, int argument_count, char **arguments) {
    int index = 1;
    (void)snprintf(editor->project_path, sizeof(editor->project_path), "arcflow_project.afproj");
    for (index = 1; index < argument_count; ++index) {
        if (strcmp(arguments[index], "--frames") == 0 && index + 1 < argument_count) {
            editor->maximum_frames = (uint32_t)strtoul(arguments[++index], NULL, 10);
        } else if (strcmp(arguments[index], "--resize-smoke") == 0) {
            editor->resize_smoke = true;
        } else if (strcmp(arguments[index], "--context-smoke") == 0) {
            editor->context_smoke = true;
        } else if (strcmp(arguments[index], "--tooltip-smoke") == 0) {
            editor->tooltip_smoke = true;
        } else if (strcmp(arguments[index], "--selection-smoke") == 0) {
            editor->selection_smoke = true;
        } else if (strcmp(arguments[index], "--vfx-smoke") == 0) {
            editor->vfx_smoke = true;
        } else if (strcmp(arguments[index], "--particle-view-smoke") == 0) {
            editor->particle_view_smoke = true;
        } else if (strcmp(arguments[index], "--vector-animation-smoke") == 0) {
            editor->vector_animation_smoke = true;
        } else if (strcmp(arguments[index], "--sequence") == 0 && index + 1 < argument_count) {
            (void)snprintf(editor->sequence_prefix, sizeof(editor->sequence_prefix), "%s",
                           arguments[++index]);
        } else if (strcmp(arguments[index], "--output") == 0 && index + 1 < argument_count) {
            (void)snprintf(editor->output_path, sizeof(editor->output_path), "%s",
                           arguments[++index]);
        } else {
            (void)snprintf(editor->project_path, sizeof(editor->project_path), "%s",
                           arguments[index]);
        }
    }
    if (editor->output_path[0] != '\0' && editor->maximum_frames == 0U) {
        editor->maximum_frames =
            editor->particle_view_smoke
                ? 24U
                : (editor->vector_animation_smoke ? 18U : (editor->tooltip_smoke ? 2U : 1U));
    } else if (editor->sequence_prefix[0] != '\0' && editor->maximum_frames == 0U) {
        editor->maximum_frames =
            editor->particle_view_smoke
                ? 24U
                : (editor->vector_animation_smoke
                       ? 18U
                       : (editor->vfx_smoke ? AF_EDITOR_PARTICLE_PAGE_COUNT : 60U));
    }
    af_editor_derive_package_path(editor);
}
