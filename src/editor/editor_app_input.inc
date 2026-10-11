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
        af_editor_mark_dirty(editor);
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
        af_editor_mark_dirty(editor);
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
    if (event->y < top + 42.0F) {
        const float local_x = (event->x - editor->left_width) /
                              af_editor_timeline_controls_scale(editor, width);
        if (local_x >= 10.0F && local_x < 56.0F) {
            editor->preview_playing = !editor->preview_playing;
            af_editor_status(editor,
                             editor->preview_playing ? "Animation playing" : "Animation paused");
        } else if (local_x >= 62.0F && local_x < 114.0F) {
            editor->preview_playing = false;
            editor->playhead = 0.0F;
            af_editor_status(editor, "Preview reset to time zero");
        } else if (local_x >= 120.0F && local_x < 178.0F) {
            editor->curve_visible = !editor->curve_visible;
            af_editor_status(editor, editor->curve_visible ? "Animation graph opened"
                                                           : "Dope sheet opened");
        } else if (local_x >= 188.0F && local_x < 220.0F) {
            af_editor_select_adjacent_property(editor, -1);
        } else if (local_x >= 226.0F && local_x < 352.0F) {
            af_editor_choice_open(editor, 2, width, height,
                                  editor->left_width +
                                      226 * af_editor_timeline_controls_scale(editor, width),
                                  top + 42);
        } else if (local_x >= 358.0F && local_x < 390.0F) {
            af_editor_select_adjacent_property(editor, 1);
        } else if (local_x >= 398.0F && local_x < 514.0F) {
            if (af_editor_add_keyframe(editor) != AF_RESULT_SUCCESS) {
                af_editor_status(editor, "Select an object and an animatable property first");
            }
        } else if (af_editor_timeline_compact_clips(editor, width)) {
            if (event->x >= center_right - 74.0F && event->x < center_right - 6.0F &&
                editor->project.clip_count != 0U) {
                for (index = 0U; index < editor->project.clip_count; ++index) {
                    if (editor->project.clips[index].id == editor->selected_clip_id)
                        break;
                }
                editor->selected_clip_id =
                    editor->project.clips[(index + 1U) % editor->project.clip_count].id;
                editor->playhead = 0.0F;
                af_editor_clear_key_selection(editor);
                af_editor_status(editor, "Next animation clip selected");
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
        const bool in_loop_lane = event->y >= top + 58.0F;
        if (clip->loop && in_loop_lane && fabsf(event->x - start_x) <= 7.0F) {
            editor->drag_mode = AF_EDITOR_DRAG_LOOP_START;
            (void)af_project_clip_snapshot(&editor->project, clip->id, &editor->clip_drag_before);
            af_editor_status(editor, "Drag to set the loop start");
        } else if (clip->loop && in_loop_lane && fabsf(event->x - end_x) <= 7.0F) {
            editor->drag_mode = AF_EDITOR_DRAG_LOOP_END;
            (void)af_project_clip_snapshot(&editor->project, clip->id, &editor->clip_drag_before);
            af_editor_status(editor, "Drag to set the loop end");
        } else if (clip->loop && in_loop_lane && event->x > start_x && event->x < end_x) {
            editor->drag_mode = AF_EDITOR_DRAG_LOOP_RANGE;
            editor->drag_start_mouse = (AfVec2){event->x, event->y};
            (void)af_project_clip_snapshot(&editor->project, clip->id, &editor->clip_drag_before);
            af_editor_status(editor, "Drag to move the loop range");
        } else if (event->x >= ruler_x) {
            editor->drag_mode = AF_EDITOR_DRAG_PLAYHEAD;
            editor->preview_playing = false;
            editor->playhead =
                af_editor_clamp(af_editor_x_to_time(editor, event->x), 0.0F, clip->duration);
        }
        return true;
    }
    if (event->y >= top + AF_EDITOR_TIMELINE_RULER_HEIGHT + 3.0F && event->x >= ruler_x &&
        fabsf(event->x - af_editor_time_to_x(editor, editor->playhead)) <= 8.0F) {
        editor->drag_mode = AF_EDITOR_DRAG_PLAYHEAD;
        editor->preview_playing = false;
        af_editor_status(editor, "Drag the playhead to scrub animation time");
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
            const float bottom = (float)height - editor->status_height - 12.0F;
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
        const uint32_t clicked_row = (uint32_t)((event->y - top - AF_EDITOR_TIMELINE_RULER_HEIGHT) /
                                                AF_EDITOR_TIMELINE_TRACK_HEIGHT);
        const AfProjectTrack *clicked_track = af_editor_track_at_row(editor, clicked_row);
        uint64_t hit_id = 0U;
        float hit_distance = 10.0F;
        if (clicked_track != NULL) {
            af_editor_select_only(editor, clicked_track->node_id);
            editor->selected_property = clicked_track->property;
            if (clicked_track->property >= AF_PROPERTY_PATH_POINT) {
                editor->selected_path_command = clicked_track->element_index;
                af_editor_path_selection_only(editor, editor->selected_path_command);
            }
            af_editor_status(editor, "Animation track selected");
        }
        if (event->x < ruler_x) {
            af_editor_clear_key_selection(editor);
            return true;
        }
        for (index = 0U; index < editor->project.keyframe_count; ++index) {
            const AfProjectKeyframe *key = &editor->project.keyframes[index];
            const uint32_t row = af_editor_track_row(editor, key->track_id);
            const float x = af_editor_time_to_x(editor, key->keyframe.time);
            const float y = top + AF_EDITOR_TIMELINE_RULER_HEIGHT +
                            (float)row * AF_EDITOR_TIMELINE_TRACK_HEIGHT + 17.0F;
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
                        (float)row * AF_EDITOR_TIMELINE_TRACK_HEIGHT + 17.0F;
        if (row != UINT32_MAX && x >= left && x <= right && y >= box_top && y <= bottom &&
            editor->selected_keyframe_count < AF_EDITOR_COMMAND_MAX_KEYFRAMES) {
            editor->selected_keyframes[editor->selected_keyframe_count++] = key->id;
        }
    }
}

static bool af_editor_begin_path_drag(AfEditorState *editor, AfPath *path, AfEditorDragMode mode) {
    af_editor_prepare_snapping(editor, true);
    if (af_editor_is_text(editor, editor->selected_id))
        return false;
    const AfProjectTrack *track = af_editor_selected_track(editor);
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    float key_time = af_editor_key_time(editor, clip, editor->playhead);
    AfProjectKeyframe *key = NULL;
    editor->path_key_drag = false;
    editor->path_drag_key_id = 0U;
    if (mode == AF_EDITOR_DRAG_PATH_SEGMENT && af_editor_selected_path_has_animation(editor)) {
        af_editor_status(editor, "Segment shaping requires a static path");
        return false;
    }
    if (mode == AF_EDITOR_DRAG_PATH_POINT && editor->selected_path_command_count > 1U &&
        af_editor_selected_path_has_animation(editor)) {
        af_editor_status(editor, "Multi-node dragging requires a static path");
        return false;
    }
    if (mode != AF_EDITOR_DRAG_PATH_SEGMENT && track != NULL &&
        track->property >= AF_PROPERTY_PATH_POINT) {
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

static AfVec2 af_editor_path_segment_point(AfVec2 start, const AfPathCommand *command, float t) {
    if (command->type == AF_PATH_CUBIC) {
        const float inverse = 1.0F - t;
        const float inverse_squared = inverse * inverse;
        const float t_squared = t * t;
        return (AfVec2){
            inverse_squared * inverse * start.x + 3.0F * inverse_squared * t * command->control1.x +
                3.0F * inverse * t_squared * command->control2.x + t_squared * t * command->point.x,
            inverse_squared * inverse * start.y + 3.0F * inverse_squared * t * command->control1.y +
                3.0F * inverse * t_squared * command->control2.y +
                t_squared * t * command->point.y};
    }
    return (AfVec2){start.x + (command->point.x - start.x) * t,
                    start.y + (command->point.y - start.y) * t};
}

static bool af_editor_path_segment_hit(const AfEditorState *editor, int width, int height,
                                       AfVec2 mouse, uint32_t *out_command, float *out_t) {
    const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    const AfPath *path = af_path_find_node_const(&editor->render_path_data, editor->selected_id);
    uint32_t nearest_command = UINT32_MAX;
    float nearest_t = 0.5F;
    float nearest_distance = 64.0F;
    AfVec2 previous = {0};
    bool have_previous = false;
    AfAffine2 transform = {0};
    if (node == NULL || path == NULL || node->polygon_sides != 0U || out_command == NULL ||
        out_t == NULL) {
        return false;
    }
    transform = af_affine_multiply(af_editor_camera(editor, width, height), node->world_transform);
    for (uint32_t command_index = 0U; command_index < path->command_count; ++command_index) {
        const AfPathCommand *command =
            &editor->render_path_data.commands[path->command_start + command_index];
        if (command->type == AF_PATH_MOVE) {
            previous = command->point;
            have_previous = true;
            continue;
        }
        if ((command->type == AF_PATH_LINE || command->type == AF_PATH_CUBIC) && have_previous) {
            const uint32_t samples = command->type == AF_PATH_CUBIC ? 32U : 1U;
            AfVec2 a = af_affine_transform_point(transform, previous);
            for (uint32_t sample = 1U; sample <= samples; ++sample) {
                const AfVec2 b = af_affine_transform_point(
                    transform, af_editor_path_segment_point(previous, command,
                                                            (float)sample / (float)samples));
                const float vx = b.x - a.x, vy = b.y - a.y;
                const float length2 = vx * vx + vy * vy;
                const float u =
                    length2 > 0.0001F
                        ? af_editor_clamp(((mouse.x - a.x) * vx + (mouse.y - a.y) * vy) / length2,
                                          0.0F, 1.0F)
                        : 0.0F;
                const float t = ((float)(sample - 1U) + u) / (float)samples;
                const AfVec2 screen = {a.x + vx * u, a.y + vy * u};
                const float dx = mouse.x - screen.x;
                const float dy = mouse.y - screen.y;
                const float distance = dx * dx + dy * dy;
                if (t > 0.015F && t < 0.985F && distance < nearest_distance) {
                    nearest_distance = distance;
                    nearest_command = command_index;
                    nearest_t = t;
                }
                a = b;
            }
            previous = command->point;
        } else if (command->type == AF_PATH_CLOSE) {
            have_previous = false;
        }
    }
    if (nearest_command == UINT32_MAX) {
        return false;
    }
    *out_command = nearest_command;
    *out_t = nearest_t;
    return true;
}

static bool af_editor_path_mouse_down(AfEditorState *editor, int width, int height, AfVec2 mouse,
                                      SDL_Keymod modifiers) {
    const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    AfPath *path = af_editor_selected_path(editor);
    AfAffine2 transform = {0};
    const float threshold_squared = 100.0F;
    if (node == NULL || path == NULL || node->polygon_sides != 0U) {
        return false;
    }
    transform = af_affine_multiply(af_editor_camera(editor, width, height), node->world_transform);
    if ((modifiers & (SDL_KMOD_SHIFT | SDL_KMOD_CTRL)) == 0U &&
        editor->selected_path_command < path->command_count) {
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
            editor->selected_property = AF_PROPERTY_PATH_POINT;
            if ((modifiers & SDL_KMOD_CTRL) != 0U) {
                af_editor_path_selection_toggle(editor, command_index);
                af_editor_status(editor, "Node selection toggled");
                return true;
            }
            if ((modifiers & SDL_KMOD_SHIFT) != 0U) {
                (void)af_editor_path_selection_add(editor, command_index);
                af_editor_status(editor, "Node added to selection");
                return true;
            }
            if (!af_editor_path_selection_contains(editor, command_index)) {
                af_editor_path_selection_only(editor, command_index);
            } else {
                editor->selected_path_command = command_index;
            }
            return af_editor_begin_path_drag(editor, path, AF_EDITOR_DRAG_PATH_POINT);
        }
    }
    {
        uint32_t nearest_command = UINT32_MAX;
        float nearest_t = 0.5F;
        if (af_editor_path_segment_hit(editor, width, height, mouse, &nearest_command,
                                       &nearest_t)) {
            AfAffine2 inverse = {0};
            if (!af_affine_inverse(node->world_transform, &inverse)) {
                return false;
            }
            af_editor_path_selection_only(editor, nearest_command);
            editor->selected_property = AF_PROPERTY_PATH_POINT;
            editor->path_segment_t = nearest_t;
            editor->path_segment_start = af_affine_transform_point(
                inverse, af_editor_screen_to_project(editor, width, height, mouse));
            if (af_editor_begin_path_drag(editor, path, AF_EDITOR_DRAG_PATH_SEGMENT)) {
                af_editor_status(editor, "Drag the segment to shape its curvature");
                return true;
            }
        }
    }
    return false;
}

static bool af_editor_path_action_mouse_down(AfEditorState *editor, int width, int height,
                                             AfVec2 mouse, SDL_Keymod modifiers) {
    float canvas_x = 0.0F;
    float canvas_y = 0.0F;
    float canvas_width = 0.0F;
    float canvas_height = 0.0F;
    const AfPath *path = af_editor_selected_path(editor);
    const AfPathCommand *command = NULL;
    const bool multi = editor->selected_path_command_count > 1U;
    float x = 0.0F;
    float y = 0.0F;
    float right = 0.0F;
    if (path == NULL || editor->selected_path_command >= path->command_count) {
        return false;
    }
    command =
        &editor->project.path_data.commands[path->command_start + editor->selected_path_command];
    if (command->type == AF_PATH_CLOSE) {
        return false;
    }
    af_editor_layout(editor, width, height, &canvas_x, &canvas_y, &canvas_width, &canvas_height);
    (void)canvas_height;
    x = canvas_x + 12.0F;
    y = canvas_y + AF_EDITOR_PATH_ACTION_Y;
    right = canvas_x + canvas_width;
    if (mouse.y < y || mouse.y > y + AF_EDITOR_PATH_ACTION_HEIGHT || mouse.x < x ||
        mouse.x > right) {
        return false;
    }
    if (mouse.x <= x + 116.0F) {
        return true;
    }
    if (af_editor_path_action_fits(x + 122.0F, 96.0F, right) && mouse.x >= x + 122.0F &&
        mouse.x <= x + 218.0F) {
        if (multi) {
            af_editor_status(editor, "Key one node at a time; choose the primary node");
            return true;
        }
        editor->selected_property = AF_PROPERTY_PATH_POINT;
        if (af_editor_add_keyframe(editor) != AF_RESULT_SUCCESS) {
            af_editor_status(editor, "Point keyframe could not be added");
        }
        return true;
    }
    if (af_editor_path_action_fits(x + 224.0F, 82.0F, right) && mouse.x >= x + 224.0F &&
        mouse.x <= x + 306.0F) {
        if (multi) {
            af_editor_status(editor, "Convert one segment at a time");
        } else if (editor->selected_path_command == 0U ||
                   (command->type != AF_PATH_LINE && command->type != AF_PATH_CUBIC)) {
            af_editor_status(editor, "Select a segment point to change line or curve type");
        } else {
            (void)af_editor_convert_path_point(editor);
        }
        return true;
    }
    if (af_editor_path_action_fits(x + 312.0F, 62.0F, right) && mouse.x >= x + 312.0F &&
        mouse.x <= x + 374.0F) {
        if (multi) {
            af_editor_status(editor, "Insert beside one selected segment at a time");
        } else if (editor->selected_path_command == 0U ||
                   (command->type != AF_PATH_LINE && command->type != AF_PATH_CUBIC)) {
            af_editor_status(editor, "Select a segment point before inserting a point");
        } else {
            (void)af_editor_insert_path_point(editor);
        }
        return true;
    }
    if (af_editor_path_action_fits(x + 380.0F, 80.0F, right) && mouse.x >= x + 380.0F &&
        mouse.x <= x + 460.0F) {
        (void)af_editor_toggle_path_closed(editor);
        return true;
    }
    if (af_editor_path_action_fits(x + 466.0F, 68.0F, right) && mouse.x >= x + 466.0F &&
        mouse.x <= x + 534.0F) {
        if (!multi && (editor->selected_path_command == 0U ||
                       (command->type != AF_PATH_LINE && command->type != AF_PATH_CUBIC))) {
            af_editor_status(editor, "The contour start point cannot be deleted");
        } else {
            (void)af_editor_delete_path_point(editor);
        }
        return true;
    }
    if (af_editor_path_action_fits(x + 540.0F, 70.0F, right) && mouse.x >= x + 540.0F &&
        mouse.x <= x + 610.0F) {
        (void)af_editor_arrange_path_points(editor, true, false);
        return true;
    }
    if (af_editor_path_action_fits(x + 616.0F, 70.0F, right) && mouse.x >= x + 616.0F &&
        mouse.x <= x + 686.0F) {
        (void)af_editor_arrange_path_points(editor, false, false);
        return true;
    }
    if (af_editor_path_action_fits(x + 692.0F, 70.0F, right) && mouse.x >= x + 692.0F &&
        mouse.x <= x + 762.0F) {
        (void)af_editor_arrange_path_points(editor, true, true);
        return true;
    }
    if (af_editor_path_action_fits(x + 768.0F, 70.0F, right) && mouse.x >= x + 768.0F &&
        mouse.x <= x + 838.0F) {
        (void)af_editor_arrange_path_points(editor, false, true);
        return true;
    }
    if (af_editor_path_action_fits(x + 844.0F, 78.0F, right) && mouse.x >= x + 844.0F &&
        mouse.x <= x + 922.0F) {
        (void)af_editor_smooth_path_points(editor, (modifiers & SDL_KMOD_SHIFT) != 0U);
        return true;
    }
    return false;
}

static void af_editor_drag_path_handle(AfEditorState *editor, int width, int height, AfVec2 mouse,
                                       SDL_Keymod modifiers) {
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
    point = af_affine_transform_point(
        inverse,
        af_editor_snap_point(editor, af_editor_screen_to_project(editor, width, height, mouse)));
    if (!isfinite(point.x) || !isfinite(point.y)) {
        return;
    }
    point.x = af_editor_clamp(point.x, -1000000.0F, 1000000.0F);
    point.y = af_editor_clamp(point.y, -1000000.0F, 1000000.0F);
    if (editor->path_key_drag) {
        AfProjectKeyframe *key =
            af_project_find_keyframe(&editor->project, editor->path_drag_key_id);
        if (key != NULL) {
            key->keyframe.value[0] = point.x;
            key->keyframe.value[1] = point.y;
            af_editor_update_preview(editor);
        }
        return;
    }
    if (path->command_count != editor->path_drag_before.path.command_count) {
        return;
    }
    command = &editor->project.path_data.commands[path->command_start + selected];
    before = &editor->path_drag_commands[selected];
    if (editor->drag_mode == AF_EDITOR_DRAG_PATH_SEGMENT && selected > 0U &&
        (before->type == AF_PATH_LINE || before->type == AF_PATH_CUBIC)) {
        AfPathCommand *commands = &editor->project.path_data.commands[path->command_start];
        const AfVec2 start = editor->path_drag_commands[selected - 1U].point;
        const AfVec2 delta = {point.x - editor->path_segment_start.x,
                              point.y - editor->path_segment_start.y};
        const float t = af_editor_clamp(editor->path_segment_t, 0.08F, 0.92F);
        const float weight = fmaxf(0.12F, 3.0F * t * (1.0F - t));
        memcpy(commands, editor->path_drag_commands,
               (size_t)path->command_count * sizeof(*commands));
        command = &commands[selected];
        if (before->type == AF_PATH_LINE) {
            command->type = AF_PATH_CUBIC;
            command->control1 = (AfVec2){start.x + (before->point.x - start.x) / 3.0F,
                                         start.y + (before->point.y - start.y) / 3.0F};
            command->control2 = (AfVec2){start.x + (before->point.x - start.x) * 2.0F / 3.0F,
                                         start.y + (before->point.y - start.y) * 2.0F / 3.0F};
        }
        command->control1 = af_editor_path_clamp_point((AfVec2){
            command->control1.x + delta.x / weight, command->control1.y + delta.y / weight});
        command->control2 = af_editor_path_clamp_point((AfVec2){
            command->control2.x + delta.x / weight, command->control2.y + delta.y / weight});
        (void)af_path_mark_geometry_changed(path);
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_PATH_POINT && command->type != AF_PATH_CLOSE) {
        const AfVec2 delta = {point.x - before->point.x, point.y - before->point.y};
        if (editor->selected_path_command_count > 1U) {
            AfPathCommand *commands = &editor->project.path_data.commands[path->command_start];
            memcpy(commands, editor->path_drag_commands,
                   (size_t)path->command_count * sizeof(*commands));
            for (uint32_t selection = 0U; selection < editor->selected_path_command_count;
                 ++selection) {
                const uint32_t command_index = editor->selected_path_commands[selection];
                if (command_index < path->command_count &&
                    commands[command_index].type != AF_PATH_CLOSE) {
                    af_editor_path_shift_anchor(commands, path->command_count, command_index,
                                                delta);
                }
            }
            (void)af_path_mark_geometry_changed(path);
            return;
        }
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
        if ((modifiers & (SDL_KMOD_SHIFT | SDL_KMOD_CTRL)) != 0U && selected > 0U &&
            editor->path_drag_commands[selected - 1U].type == AF_PATH_CUBIC) {
            AfPathCommand *opposite = command - 1U;
            const AfPathCommand *opposite_before = before - 1U;
            const AfVec2 anchor = opposite_before->point;
            const AfVec2 direction = {point.x - anchor.x, point.y - anchor.y};
            const float direction_length =
                sqrtf(direction.x * direction.x + direction.y * direction.y);
            const AfVec2 old = {opposite_before->control2.x - anchor.x,
                                opposite_before->control2.y - anchor.y};
            const float old_length = sqrtf(old.x * old.x + old.y * old.y);
            const float length = (modifiers & SDL_KMOD_CTRL) != 0U ? direction_length : old_length;
            if (direction_length > 0.0001F) {
                opposite->control2 = af_editor_path_clamp_point(
                    (AfVec2){anchor.x - direction.x / direction_length * length,
                             anchor.y - direction.y / direction_length * length});
            }
        }
    } else if (editor->drag_mode == AF_EDITOR_DRAG_PATH_CONTROL2 &&
               command->type == AF_PATH_CUBIC) {
        command->control2 = point;
        if ((modifiers & (SDL_KMOD_SHIFT | SDL_KMOD_CTRL)) != 0U &&
            selected + 1U < path->command_count &&
            editor->path_drag_commands[selected + 1U].type == AF_PATH_CUBIC) {
            AfPathCommand *opposite = command + 1U;
            const AfPathCommand *opposite_before = before + 1U;
            const AfVec2 anchor = before->point;
            const AfVec2 direction = {point.x - anchor.x, point.y - anchor.y};
            const float direction_length =
                sqrtf(direction.x * direction.x + direction.y * direction.y);
            const AfVec2 old = {opposite_before->control1.x - anchor.x,
                                opposite_before->control1.y - anchor.y};
            const float old_length = sqrtf(old.x * old.x + old.y * old.y);
            const float length = (modifiers & SDL_KMOD_CTRL) != 0U ? direction_length : old_length;
            if (direction_length > 0.0001F) {
                opposite->control1 = af_editor_path_clamp_point(
                    (AfVec2){anchor.x - direction.x / direction_length * length,
                             anchor.y - direction.y / direction_length * length});
            }
        }
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
            af_editor_mark_dirty(editor);
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
        af_editor_mark_dirty(editor);
        af_editor_status(editor, editor->drag_mode == AF_EDITOR_DRAG_PATH_SEGMENT
                                     ? "Path segment reshaped"
                                     : "Path point edited");
    }
}

static void af_editor_canvas_box_select(AfEditorState *editor, int width, int height) {
    const AfAffine2 camera = af_editor_camera(editor, width, height);
    const float box_minimum_x = fminf(editor->canvas_box_start.x, editor->canvas_box_current.x);
    const float box_minimum_y = fminf(editor->canvas_box_start.y, editor->canvas_box_current.y);
    const float box_maximum_x = fmaxf(editor->canvas_box_start.x, editor->canvas_box_current.x);
    const float box_maximum_y = fmaxf(editor->canvas_box_start.y, editor->canvas_box_current.y);
    if (editor->active_tool == AF_EDITOR_TOOL_NODE && editor->selected_count == 1U) {
        const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->selected_id);
        const AfPath *path =
            af_path_find_node_const(&editor->render_path_data, editor->selected_id);
        if (node != NULL && path != NULL) {
            const AfAffine2 transform = af_affine_multiply(camera, node->world_transform);
            if (!editor->canvas_box_additive) {
                af_editor_path_selection_clear(editor);
                editor->selected_path_command = UINT32_MAX;
            }
            for (uint32_t index = 0U; index < path->command_count; ++index) {
                const AfPathCommand *command =
                    &editor->render_path_data.commands[path->command_start + index];
                const AfVec2 point = af_affine_transform_point(transform, command->point);
                if (command->type != AF_PATH_CLOSE && point.x >= box_minimum_x &&
                    point.x <= box_maximum_x && point.y >= box_minimum_y &&
                    point.y <= box_maximum_y) {
                    (void)af_editor_path_selection_add(editor, index);
                }
            }
            if (editor->selected_path_command_count == 0U) {
                af_editor_status(editor, "Node selection cleared");
            } else {
                char status[64];
                (void)snprintf(status, sizeof(status), "%u path nodes selected",
                               editor->selected_path_command_count);
                af_editor_status(editor, status);
            }
            return;
        }
    }
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
    if (editor->selected_path_command != UINT32_MAX) {
        af_editor_path_selection_only(editor, editor->selected_path_command);
    } else {
        af_editor_path_selection_clear(editor);
    }
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

static void af_editor_prepare_outline_smoke(AfEditorState *editor) {
    for (uint32_t index = 0U; index < editor->project.scene.count; ++index) {
        const AfSceneNode *node = &editor->project.scene.nodes[index];
        AfStyle *style = af_project_find_node_style(&editor->project, node->id);
        if (style == NULL || node->type < AF_NODE_RECTANGLE || node->type > AF_NODE_PATH) {
            continue;
        }
        style->stroke.enabled = true;
        style->stroke.width = node->type == AF_NODE_PATH ? 7.0F : 10.0F;
        style->stroke.end_width = style->stroke.width;
        style->stroke_paint = af_paint_solid((AfColor){0.98F, 0.76F, 0.16F, 1.0F});
        (void)af_style_mark_paint_changed(style);
        (void)af_style_mark_stroke_geometry_changed(style);
        if (editor->selected_id == 0U) {
            af_editor_select_only(editor, node->id);
        }
    }
    editor->color_target_outline = true;
}

static AfVec2 af_editor_gizmo_parent_point(const AfEditorState *editor, const AfSceneNode *node,
                                           int width, int height, AfVec2 screen) {
    AfVec2 point = af_editor_screen_to_project(editor, width, height, screen);
    if (node != NULL && node->parent_id != AF_INVALID_NODE_ID) {
        const AfSceneNode *parent = af_scene_find_const(&editor->preview_scene, node->parent_id);
        AfAffine2 inverse = af_affine_identity();
        if (parent != NULL && af_affine_inverse(parent->world_transform, &inverse)) {
            point = af_affine_transform_point(inverse, point);
        }
    }
    return point;
}

static bool af_editor_gizmo_parent_affine(const AfEditorState *editor, const AfSceneNode *node,
                                          const AfEditorGizmoGeometry *geometry, int width,
                                          int height, AfAffine2 *out_affine) {
    AfVec2 origin;
    AfVec2 horizontal;
    AfVec2 vertical;
    if (node == NULL || geometry == NULL || out_affine == NULL || node->resolved_bounds.x <= 0.0F ||
        node->resolved_bounds.y <= 0.0F) {
        return false;
    }
    origin = af_editor_gizmo_parent_point(editor, node, width, height, geometry->corners[0]);
    horizontal = af_editor_gizmo_parent_point(editor, node, width, height, geometry->corners[1]);
    vertical = af_editor_gizmo_parent_point(editor, node, width, height, geometry->corners[3]);
    *out_affine = (AfAffine2){.m00 = (horizontal.x - origin.x) / node->resolved_bounds.x,
                              .m01 = (vertical.x - origin.x) / node->resolved_bounds.y,
                              .m10 = (horizontal.y - origin.y) / node->resolved_bounds.x,
                              .m11 = (vertical.y - origin.y) / node->resolved_bounds.y,
                              .tx = origin.x,
                              .ty = origin.y};
    return true;
}

static float af_editor_gizmo_angle(AfVec2 point, AfVec2 origin) {
    return atan2f(point.y - origin.y, point.x - origin.x);
}

static float af_editor_gizmo_normalize_angle(float angle) {
    const float turn = 6.28318530718F;
    while (angle > 3.14159265359F) {
        angle -= turn;
    }
    while (angle < -3.14159265359F) {
        angle += turn;
    }
    return angle;
}

static void af_editor_update_gizmo_drag(AfEditorState *editor, int width, int height,
                                        AfVec2 screen) {
    AfSceneNode *node = af_scene_find(&editor->project.scene, editor->selected_id);
    const AfSceneNode *preview_node =
        af_scene_find_const(&editor->preview_scene, editor->selected_id);
    const int32_t key_index = af_editor_transform_drag_key_index(editor, editor->selected_id);
    const int32_t position_key_index = af_editor_transform_drag_key_property_index(
        editor, editor->selected_id, AF_PROPERTY_POSITION);
    AfProjectKeyframe *key =
        key_index < 0 ? NULL
                      : af_project_find_keyframe(
                            &editor->project, editor->transform_drag_key_ids[(uint32_t)key_index]);
    AfProjectKeyframe *position_key =
        position_key_index < 0
            ? NULL
            : af_project_find_keyframe(
                  &editor->project, editor->transform_drag_key_ids[(uint32_t)position_key_index]);
    AfTransform2D transform = editor->gizmo_start_transform;
    const SDL_Keymod modifiers = SDL_GetModState();
    AfVec2 current = {0};
    AfVec2 start = {0};
    if (node == NULL || preview_node == NULL) {
        return;
    }
    current = af_editor_gizmo_parent_point(editor, preview_node, width, height, screen);
    start =
        af_editor_gizmo_parent_point(editor, preview_node, width, height, editor->drag_start_mouse);
    if (editor->gizmo_handle == AF_EDITOR_GIZMO_PIVOT) {
        AfAffine2 inverse = {0};
        AfTransform2D linear_transform = editor->gizmo_start_transform;
        AfAffine2 linear;
        AfAffine2 original;
        AfVec2 pivot = editor->gizmo_start_pivot;
        AfLayoutNode layout = editor->gizmo_start_has_layout ? editor->gizmo_start_layout
                                                             : af_layout_node_default(node->id);
        if (!af_affine_inverse(editor->gizmo_start_parent_affine, &inverse)) {
            return;
        }
        pivot = af_affine_transform_point(inverse, current);
        pivot.x = af_editor_clamp(pivot.x, 0.0F, preview_node->resolved_bounds.x);
        pivot.y = af_editor_clamp(pivot.y, 0.0F, preview_node->resolved_bounds.y);
        layout.offset.x += pivot.x - editor->gizmo_start_pivot.x;
        layout.offset.y += pivot.y - editor->gizmo_start_pivot.y;
        layout.pivot = (AfVec2){pivot.x / fmaxf(preview_node->resolved_bounds.x, 0.0001F),
                                pivot.y / fmaxf(preview_node->resolved_bounds.y, 0.0001F)};
        original = af_affine_from_transform_pivot(editor->gizmo_start_transform,
                                                  editor->gizmo_start_pivot);
        linear_transform.position = (AfVec2){0};
        linear = af_affine_from_transform(linear_transform);
        transform.position =
            (AfVec2){original.tx - pivot.x + linear.m00 * pivot.x + linear.m01 * pivot.y,
                     original.ty - pivot.y + linear.m10 * pivot.x + linear.m11 * pivot.y};
        if (af_project_set_layout(&editor->project, &layout) != AF_RESULT_SUCCESS) {
            return;
        }
        node->local_transform.position = transform.position;
        editor->gizmo_position_changed = true;
        (void)af_scene_evaluate_world(&editor->project.scene);
        af_editor_update_preview(editor);
        {
            char status[96];
            (void)snprintf(status, sizeof(status), "Pivot %.0f%%, %.0f%%",
                           (double)(layout.pivot.x * 100.0F), (double)(layout.pivot.y * 100.0F));
            af_editor_status(editor, status);
        }
        return;
    }
    if (editor->gizmo_handle == AF_EDITOR_GIZMO_ROTATE) {
        float rotation = transform.rotation +
                         af_editor_gizmo_normalize_angle(
                             af_editor_gizmo_angle(current, editor->gizmo_start_pivot_parent) -
                             editor->gizmo_start_angle);
        if (editor->snapping || (modifiers & SDL_KMOD_SHIFT) != 0U) {
            const float step = 0.2617993878F;
            rotation = roundf(rotation / step) * step;
        }
        transform.rotation = af_editor_gizmo_normalize_angle(rotation);
    } else if (editor->gizmo_handle == AF_EDITOR_GIZMO_SCALE_X ||
               editor->gizmo_handle == AF_EDITOR_GIZMO_SCALE_Y ||
               editor->gizmo_handle == AF_EDITOR_GIZMO_SCALE_XY) {
        AfAffine2 inverse = {0};
        AfVec2 local = {0};
        const bool proportional = (modifiers & SDL_KMOD_SHIFT) != 0U;
        const bool from_center = (modifiers & SDL_KMOD_CTRL) != 0U;
        if (!af_affine_inverse(editor->gizmo_start_parent_affine, &inverse)) {
            return;
        }
        local = af_affine_transform_point(inverse, current);
        if (!af_editor_gizmo_scale_transform(editor->gizmo_start_transform,
                                             editor->gizmo_start_pivot,
                                             preview_node->resolved_bounds, editor->gizmo_handle,
                                             local, proportional, from_center, &transform)) {
            return;
        }
        editor->gizmo_position_changed =
            fabsf(transform.position.x - editor->gizmo_start_transform.position.x) > 0.0001F ||
            fabsf(transform.position.y - editor->gizmo_start_transform.position.y) > 0.0001F;
        {
            char status[128];
            (void)snprintf(status, sizeof(status), "Scale %.2f, %.2f%s%s",
                           (double)transform.scale.x, (double)transform.scale.y,
                           proportional ? " · ratio locked" : "",
                           from_center ? " · from center" : "");
            af_editor_status(editor, status);
        }
    } else if (editor->gizmo_handle == AF_EDITOR_GIZMO_SKEW_X ||
               editor->gizmo_handle == AF_EDITOR_GIZMO_SKEW_Y) {
        AfTransform2D rotation = {.rotation = transform.rotation, .scale = {1.0F, 1.0F}};
        AfAffine2 inverse = {0};
        AfVec2 delta = {current.x - start.x, current.y - start.y};
        if (af_affine_inverse(af_affine_from_transform(rotation), &inverse)) {
            delta = af_affine_transform_point(inverse, delta);
        }
        if (editor->gizmo_handle == AF_EDITOR_GIZMO_SKEW_X) {
            const float denominator = fmaxf(1.0F, fabsf(node->bounds.y * transform.scale.y));
            transform.skew.x =
                atanf(tanf(editor->gizmo_start_transform.skew.x) + delta.x / denominator);
        } else {
            const float denominator = fmaxf(1.0F, fabsf(node->bounds.x * transform.scale.x));
            transform.skew.y =
                atanf(tanf(editor->gizmo_start_transform.skew.y) + delta.y / denominator);
        }
        transform.skew.x = af_editor_clamp(transform.skew.x, -AF_TRANSFORM_MAX_SKEW_RADIANS,
                                           AF_TRANSFORM_MAX_SKEW_RADIANS);
        transform.skew.y = af_editor_clamp(transform.skew.y, -AF_TRANSFORM_MAX_SKEW_RADIANS,
                                           AF_TRANSFORM_MAX_SKEW_RADIANS);
    }
    if (editor->transform_drag_property == AF_PROPERTY_SCALE) {
        if (key != NULL) {
            key->keyframe.value[0] = transform.scale.x;
            key->keyframe.value[1] = transform.scale.y;
        } else {
            node->local_transform.scale = transform.scale;
        }
        if (position_key != NULL) {
            position_key->keyframe.value[0] = transform.position.x;
            position_key->keyframe.value[1] = transform.position.y;
        } else {
            node->local_transform.position = transform.position;
        }
    } else if (key != NULL) {
        if (editor->transform_drag_property == AF_PROPERTY_ROTATION) {
            key->keyframe.value[0] = transform.rotation;
        } else if (editor->transform_drag_property == AF_PROPERTY_SKEW) {
            key->keyframe.value[0] = transform.skew.x;
            key->keyframe.value[1] = transform.skew.y;
        }
    } else if (editor->transform_drag_property == AF_PROPERTY_ROTATION) {
        node->local_transform.rotation = transform.rotation;
    } else if (editor->transform_drag_property == AF_PROPERTY_SKEW) {
        node->local_transform.skew = transform.skew;
    }
    (void)af_scene_evaluate_world(&editor->project.scene);
    af_editor_update_preview(editor);
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
    if (editor->gradient_drag)
        return;
    if (editor->inspector_field != AF_EDITOR_INSPECTOR_NONE &&
        !af_editor_finish_inspector_edit(editor, true)) {
        return;
    }
    if (af_editor_workflow_mouse_down(editor, width, height, x, y)) {
        return;
    }
    if (event->button == SDL_BUTTON_LEFT && af_editor_choice_mouse(editor, x, y))
        return;
    if (event->button != SDL_BUTTON_LEFT)
        editor->choice_menu = 0;
    if (event->button == SDL_BUTTON_RIGHT) {
        editor->top_menu = AF_EDITOR_MENU_NONE;
        af_editor_open_context_menu(editor, width, height, x, y);
        return;
    }
    if (event->button == SDL_BUTTON_LEFT &&
        af_editor_top_menu_mouse_down(editor, width, height, x, y)) {
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
    if (event->button == SDL_BUTTON_LEFT) {
        const AfEditorDragMode splitter = af_editor_splitter_at(editor, width, height);
        if (splitter != AF_EDITOR_DRAG_NONE) {
            editor->drag_mode = splitter;
            return;
        }
    }
    if (event->button == SDL_BUTTON_LEFT &&
        af_editor_studio_mouse_down(editor, width, height, x, y)) {
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
    if (af_editor_gradient_mouse(editor, width, height, x, y, event->clicks >= 2))
        return;
    if (x < (float)width - editor->right_width)
        editor->gradient_stop_focus = false;
    if (x >= 12.0F && x <= 106.0F && y >= 30.0F && y <= 47.0F && x < editor->left_width) {
        af_editor_group_selection(editor);
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
    if (editor->asset_browser_visible && x < editor->left_width &&
        y >= af_editor_asset_browser_top(editor, height)) {
        const float row_start = af_editor_asset_browser_top(editor, height) + 48.0F;
        if (y >= row_start) {
            const uint32_t row = (uint32_t)((y - row_start) / 27.0F);
            const uint32_t asset_index = editor->asset_scroll + row;
            if (asset_index < editor->project.image_data.asset_count &&
                row < af_editor_asset_visible_rows(editor)) {
                const uint64_t asset_id = editor->project.image_data.assets[asset_index].id;
                editor->selected_asset_id = asset_id;
                for (uint32_t binding = 0U; binding < editor->project.image_data.binding_count;
                     ++binding) {
                    if (af_editor_binding_uses_asset(&editor->project.image_data,
                                                     &editor->project.image_data.bindings[binding],
                                                     asset_id)) {
                        af_editor_select_only(editor,
                                              editor->project.image_data.bindings[binding].node_id);
                        break;
                    }
                }
                af_editor_status(editor, "Image asset selected");
            }
        }
        return;
    }
    if (x < editor->left_width && y >= editor->toolbar_height + 8.0F) {
        const uint32_t row = (uint32_t)((y - editor->toolbar_height - 8.0F) / AF_EDITOR_ROW_HEIGHT);
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
                editor->hierarchy_drag_id = clicked;
                editor->hierarchy_drag_started = false;
                editor->drag_start_mouse = (AfVec2){x, y};
                editor->drag_mode = AF_EDITOR_DRAG_HIERARCHY;
            }
            editor->selected_path_command =
                af_path_find_node_const(&editor->project.path_data, editor->selected_id) != NULL
                    ? 0U
                    : UINT32_MAX;
            if (editor->selected_path_command != UINT32_MAX) {
                af_editor_path_selection_only(editor, editor->selected_path_command);
            } else {
                af_editor_path_selection_clear(editor);
            }
        }
        return;
    }
    if (y >= AF_EDITOR_MENU_HEIGHT && y < editor->toolbar_height && x >= editor->left_width &&
        x < (float)width - editor->right_width) {
        const float toolbar_x = x - af_editor_toolbar_origin(editor, width);
        const float tools_width = (float)AF_EDITOR_TOOL_COUNT * 42.0F;
        if (toolbar_x >= 0.0F && toolbar_x < tools_width) {
            const uint32_t tool = (uint32_t)(toolbar_x / 42.0F);
            const float button_x = (float)tool * 42.0F;
            const float screen_x = af_editor_toolbar_origin(editor, width) + button_x;
            if (tool < AF_EDITOR_TOOL_COUNT && toolbar_x < button_x + 38.0F &&
                screen_x >= editor->left_width + 4.0F &&
                screen_x + 38.0F <= (float)width - editor->right_width - 4.0F) {
                af_editor_set_vector_tool(editor, (AfEditorVectorTool)tool);
            }
        } else if (toolbar_x >= tools_width + 8.0F && toolbar_x < tools_width + 96.0F) {
            if (y < AF_EDITOR_MENU_HEIGHT + 22.0F) {
                editor->grid_visible = !editor->grid_visible;
                af_editor_status(editor, editor->grid_visible ? "Grid enabled" : "Grid disabled");
            } else {
                editor->snapping = !editor->snapping;
                af_editor_status(editor,
                                 editor->snapping ? "Snapping enabled" : "Snapping disabled");
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
            af_scene_find_const(&editor->preview_scene, editor->selected_id);
        const AfVec2 selected_pivot = af_editor_node_pivot(editor, selected);
        AfEditorGizmoGeometry geometry = {0};
        if (event->clicks >= 2U && editor->active_tool == AF_EDITOR_TOOL_SELECT) {
            const uint64_t hit = af_editor_hit_test(
                editor, af_editor_screen_to_project(editor, width, height, (AfVec2){x, y}));
            const AfSceneNode *hit_node = af_scene_find_const(&editor->project.scene, hit);
            if (hit_node != NULL && hit_node->polygon_sides == 0U &&
                af_path_find_node_const(&editor->project.path_data, hit) != NULL) {
                af_editor_select_only(editor, hit);
                if (af_editor_is_text(editor, hit)) {
                    af_editor_open_text_panel(editor, width, height);
                    af_editor_text_panel_mouse(editor, width, height, x, y);
                } else
                    af_editor_convert_selected_to_path(editor);
                return;
            }
        }
        if (editor->active_tool == AF_EDITOR_TOOL_KNIFE) {
            if (selected != NULL && selected->polygon_sides != 0U) {
                af_editor_status(editor, "Convert this shape to a path before using Knife");
                return;
            }
            if (editor->selected_count == 1U && af_editor_selected_path(editor) != NULL) {
                editor->tool_start = af_editor_tool_snap_point(
                    editor, af_editor_screen_to_project(editor, width, height, (AfVec2){x, y}));
                editor->tool_current = editor->tool_start;
                editor->drag_mode = AF_EDITOR_DRAG_TOOL_CREATE;
                af_editor_status(editor, "Drag a knife line across one or more path segments");
            } else {
                af_editor_status(editor, "Select one path before using the Knife tool");
            }
            return;
        }
        if (editor->active_tool == AF_EDITOR_TOOL_SCISSORS ||
            editor->active_tool == AF_EDITOR_TOOL_SEGMENT_DELETE) {
            uint32_t command_index = UINT32_MAX;
            float segment_t = 0.5F;
            if (editor->selected_count == 1U && af_editor_selected_path(editor) != NULL &&
                af_editor_path_segment_hit(editor, width, height, (AfVec2){x, y}, &command_index,
                                           &segment_t)) {
                if (editor->active_tool == AF_EDITOR_TOOL_SCISSORS) {
                    (void)af_editor_cut_path_segment(editor, command_index, segment_t);
                } else {
                    (void)af_editor_remove_path_segment(editor, command_index);
                }
                af_editor_update_preview(editor);
            } else {
                af_editor_status(editor, "Select a path, then click directly on a segment");
            }
            return;
        }
        if (af_editor_vector_tool_mouse_down(editor, width, height, event)) {
            return;
        }
        if (editor->active_tool == AF_EDITOR_TOOL_NODE && editor->selected_count == 1U &&
            af_editor_path_action_mouse_down(editor, width, height, (AfVec2){x, y}, modifiers)) {
            return;
        }
        if (editor->active_tool == AF_EDITOR_TOOL_NODE && editor->selected_count <= 1U &&
            af_editor_path_mouse_down(editor, width, height, (AfVec2){x, y}, modifiers)) {
            return;
        }
        if (editor->active_tool == AF_EDITOR_TOOL_SELECT && selected != NULL &&
            editor->selected_count == 1U &&
            af_editor_gizmo_geometry(selected, selected_pivot, camera, &geometry)) {
            editor->gizmo_handle = af_editor_gizmo_pick(&geometry, (AfVec2){x, y}, 10.0F);
            if (editor->gizmo_handle == AF_EDITOR_GIZMO_MOVE_X) {
                editor->drag_mode = AF_EDITOR_DRAG_X;
            } else if (editor->gizmo_handle == AF_EDITOR_GIZMO_MOVE_Y) {
                editor->drag_mode = AF_EDITOR_DRAG_Y;
            } else if (editor->gizmo_handle != AF_EDITOR_GIZMO_NONE) {
                editor->drag_mode = AF_EDITOR_DRAG_GIZMO;
            }
        }
        if (editor->drag_mode == AF_EDITOR_DRAG_NONE) {
            const uint64_t hit = af_editor_hit_test(
                editor, af_editor_screen_to_project(editor, width, height, (AfVec2){x, y}));
            const bool control = (modifiers & SDL_KMOD_CTRL) != 0U;
            const bool additive = (modifiers & (SDL_KMOD_CTRL | SDL_KMOD_SHIFT)) != 0U;
            const bool node_marquee = editor->active_tool == AF_EDITOR_TOOL_NODE &&
                                      af_editor_selected_path(editor) != NULL &&
                                      (hit == 0U || hit == editor->selected_id);
            if (node_marquee) {
                editor->canvas_box_start = (AfVec2){x, y};
                editor->canvas_box_current = editor->canvas_box_start;
                editor->canvas_box_additive = additive;
                editor->drag_mode = AF_EDITOR_DRAG_CANVAS_BOX;
            } else if (hit != 0U) {
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
                if (editor->selected_path_command != UINT32_MAX) {
                    af_editor_path_selection_only(editor, editor->selected_path_command);
                } else {
                    af_editor_path_selection_clear(editor);
                }
                if (editor->active_tool == AF_EDITOR_TOOL_SELECT &&
                    af_editor_selection_contains(editor, hit)) {
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
            editor->selected_property = AF_PROPERTY_POSITION;
            af_editor_begin_selection_modify(editor, true);
            af_editor_begin_transform_key_drag(editor, AF_PROPERTY_POSITION);
            af_editor_prepare_snapping(editor, true);
        } else if (editor->drag_mode == AF_EDITOR_DRAG_GIZMO && selected != NULL) {
            const AfVec2 parent_mouse =
                af_editor_gizmo_parent_point(editor, selected, width, height, (AfVec2){x, y});
            const AfLayoutNode *layout =
                af_layout_find_const(&editor->project.layout_data, selected->id);
            editor->drag_start_mouse = (AfVec2){x, y};
            editor->gizmo_start_transform = selected->local_transform;
            editor->gizmo_start_pivot = selected_pivot;
            editor->gizmo_start_pivot_parent =
                af_editor_gizmo_parent_point(editor, selected, width, height, geometry.pivot);
            editor->gizmo_start_has_layout = layout != NULL;
            editor->gizmo_start_layout =
                layout == NULL ? af_layout_node_default(selected->id) : *layout;
            editor->gizmo_position_changed = false;
            if (!af_editor_gizmo_parent_affine(editor, selected, &geometry, width, height,
                                               &editor->gizmo_start_parent_affine)) {
                editor->drag_mode = AF_EDITOR_DRAG_NONE;
                return;
            }
            editor->gizmo_start_angle =
                af_editor_gizmo_angle(parent_mouse, editor->gizmo_start_pivot_parent);
            af_editor_begin_selection_modify(editor, true);
            if (editor->gizmo_handle == AF_EDITOR_GIZMO_PIVOT) {
                editor->selected_property = AF_PROPERTY_ROTATION;
                editor->transform_drag_key_count = 0U;
                af_editor_status(editor, "Drag pivot · rotation and scale use this point");
            } else {
                editor->selected_property = af_editor_gizmo_property(editor->gizmo_handle);
                af_editor_begin_transform_key_drag(editor, editor->selected_property);
                if (editor->selected_property == AF_PROPERTY_SCALE) {
                    af_editor_append_transform_key_drag(editor, AF_PROPERTY_POSITION);
                }
            }
        }
    }
}

static void af_editor_mouse_motion(AfEditorState *editor, int width, int height,
                                   const SDL_MouseMotionEvent *event) {
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    editor->mouse_position = (AfVec2){event->x, event->y};
    af_editor_top_menu_hover(editor, width, event);
    if (editor->gradient_drag) {
        af_editor_gradient_motion(editor, width, height, event->x, event->y);
        return;
    }
    if (editor->text_panel_open) {
        if (editor->text_dragging && (event->state & SDL_BUTTON_LMASK) &&
            event->x >= editor->left_width && event->x < (float)width - editor->right_width &&
            event->y >= editor->toolbar_height + 108 &&
            event->y < af_editor_timeline_top(editor, height)) {
            size_t anchor = editor->text_anchor;
            af_editor_text_panel_mouse(editor, width, height, event->x, event->y);
            editor->text_anchor = anchor;
        }
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_HIERARCHY) {
        if (!editor->hierarchy_drag_started &&
            hypotf(event->x - editor->drag_start_mouse.x, event->y - editor->drag_start_mouse.y) >
                5.0F) {
            editor->hierarchy_drag_started = true;
            af_editor_status(editor,
                             "Drop on a group to move inside; use insertion lines to choose parent and order");
        }
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_ASSET_HEIGHT) {
        editor->asset_height = af_editor_clamp(
            (float)height - editor->status_height - editor->timeline_height - event->y, 80.0F,
            fmaxf(80.0F, (float)height - editor->status_height - editor->timeline_height -
                             editor->toolbar_height - 56.0F));
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_TOOLBAR_HEIGHT) {
        editor->toolbar_height =
            af_editor_clamp(event->y, 72.0F,
                            fmaxf(72.0F, fminf(160.0F, (float)height - editor->status_height -
                                                           editor->timeline_height - 150.0F)));
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_STATUS_HEIGHT) {
        editor->status_height = af_editor_clamp((float)height - event->y, 28.0F, 96.0F);
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_TIMELINE_HEADER) {
        editor->timeline_header_width = af_editor_clamp(
            event->x - editor->left_width, 100.0F,
            fmaxf(100.0F, (float)width - editor->left_width - editor->right_width - 140.0F));
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_COLOR_SV ||
        editor->drag_mode == AF_EDITOR_DRAG_COLOR_HUE ||
        editor->drag_mode == AF_EDITOR_DRAG_COLOR_ALPHA) {
        af_editor_update_color_picker(editor, width, event->x, event->y);
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_INSPECTOR_SCRUB) {
        af_editor_update_inspector_scrub(editor, event->x);
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_TOOL_CREATE) {
        af_editor_vector_tool_mouse_motion(editor, width, height, (AfVec2){event->x, event->y});
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_CANVAS_BOX) {
        editor->canvas_box_current = (AfVec2){event->x, event->y};
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_TIMELINE_HEIGHT) {
        editor->timeline_height = af_editor_clamp((float)height - editor->status_height - event->y,
                                                  AF_EDITOR_TIMELINE_MIN_HEIGHT,
                                                  (float)height - editor->toolbar_height - 150.0F);
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
         editor->drag_mode == AF_EDITOR_DRAG_LOOP_END ||
         editor->drag_mode == AF_EDITOR_DRAG_LOOP_RANGE) &&
        clip != NULL) {
        AfProjectClip *mutable_clip = af_project_find_clip(&editor->project, clip->id);
        const float time = af_editor_key_time(editor, clip, af_editor_x_to_time(editor, event->x));
        if (editor->drag_mode == AF_EDITOR_DRAG_LOOP_START) {
            mutable_clip->loop_start = fminf(time, mutable_clip->loop_end);
        } else if (editor->drag_mode == AF_EDITOR_DRAG_LOOP_END) {
            mutable_clip->loop_end = fmaxf(time, mutable_clip->loop_start);
        } else {
            const float length =
                editor->clip_drag_before.clip.loop_end - editor->clip_drag_before.clip.loop_start;
            float start = editor->clip_drag_before.clip.loop_start +
                          (event->x - editor->drag_start_mouse.x) / editor->timeline_zoom;
            if (editor->snapping) {
                start = roundf(start * 10.0F) * 0.1F;
            }
            start = af_editor_clamp(start, 0.0F, fmaxf(0.0F, clip->duration - length));
            mutable_clip->loop_start = start;
            mutable_clip->loop_end = start + length;
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
            time = af_editor_key_time(editor, clip, time);
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
        const float bottom = (float)height - editor->status_height - 12.0F;
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
        editor->drag_mode == AF_EDITOR_DRAG_PATH_CONTROL2 ||
        editor->drag_mode == AF_EDITOR_DRAG_PATH_SEGMENT) {
        af_editor_drag_path_handle(editor, width, height, (AfVec2){event->x, event->y},
                                   SDL_GetModState());
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
    if (editor->drag_mode == AF_EDITOR_DRAG_GIZMO) {
        af_editor_update_gizmo_drag(editor, width, height, (AfVec2){event->x, event->y});
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_MOVE || editor->drag_mode == AF_EDITOR_DRAG_X ||
        editor->drag_mode == AF_EDITOR_DRAG_Y) {
        const AfVec2 project_current =
            af_editor_screen_to_project(editor, width, height, (AfVec2){event->x, event->y});
        AfVec2 delta = {project_current.x - editor->drag_start_mouse.x,
                        project_current.y - editor->drag_start_mouse.y};
        AfVec2 axis = {0};
        if (editor->drag_mode == AF_EDITOR_DRAG_X || editor->drag_mode == AF_EDITOR_DRAG_Y) {
            const AfSceneNode *primary =
                af_scene_find_const(&editor->preview_scene, editor->selected_id);
            if (primary) {
                axis = editor->drag_mode == AF_EDITOR_DRAG_X
                           ? (AfVec2){primary->world_transform.m00, primary->world_transform.m10}
                           : (AfVec2){primary->world_transform.m01, primary->world_transform.m11};
                const float length = hypotf(axis.x, axis.y);
                if (length <= .0001F)
                    return;
                axis.x /= length;
                axis.y /= length;
                const float distance = delta.x * axis.x + delta.y * axis.y;
                delta = (AfVec2){axis.x * distance, axis.y * distance};
            }
        }
        delta = af_editor_snap_move(editor, delta, axis);
        for (uint32_t index = 0; index < editor->selection_drag_count; ++index) {
            const AfProjectNodeSnapshot *before = &editor->selection_drag_before[index];
            AfSceneNode *node = af_scene_find(&editor->project.scene, before->node.id);
            if (!node)
                continue;
            const int32_t key_index = af_editor_transform_drag_key_index(editor, before->node.id);
            AfProjectKeyframe *key =
                key_index < 0
                    ? NULL
                    : af_project_find_keyframe(&editor->project,
                                               editor->transform_drag_key_ids[(uint32_t)key_index]);
            AfVec2 position =
                key_index < 0 ? before->node.local_transform.position
                              : (AfVec2){editor->transform_drag_key_starts[(uint32_t)key_index][0],
                                         editor->transform_drag_key_starts[(uint32_t)key_index][1]};
            AfVec2 local_delta = delta;
            if (node->parent_id != AF_INVALID_NODE_ID) {
                const AfSceneNode *parent =
                    af_scene_find_const(&editor->preview_scene, node->parent_id);
                AfAffine2 inverse;
                if (!parent || !af_affine_inverse(parent->world_transform, &inverse))
                    continue;
                local_delta = (AfVec2){inverse.m00 * delta.x + inverse.m01 * delta.y,
                                       inverse.m10 * delta.x + inverse.m11 * delta.y};
            }
            position.x += local_delta.x;
            position.y += local_delta.y;
            if (key) {
                key->keyframe.value[0] = position.x;
                key->keyframe.value[1] = position.y;
            } else
                node->local_transform.position = position;
        }
        (void)af_scene_evaluate_world(&editor->project.scene);
        if (editor->transform_drag_key_count != 0U) {
            af_editor_update_preview(editor);
        }
    }
}

static void af_editor_mouse_up(AfEditorState *editor, int width, int height) {
    editor->text_dragging = false;
    editor->snap_result = (AfEditorSnapResult){0};
    if (editor->gradient_drag) {
        af_editor_gradient_finish_drag(editor, false);
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_HIERARCHY && editor->hierarchy_drag_started) {
        const float top = editor->toolbar_height + 8.0F;
        const float bottom = editor->asset_browser_visible
                                 ? af_editor_asset_browser_top(editor, height)
                                 : (float)height - editor->status_height - editor->timeline_height;
        const AfVec2 mouse = editor->mouse_position;
        if (mouse.x >= 0 && mouse.x < editor->left_width && mouse.y >= top && mouse.y < bottom) {
            const uint32_t row = (uint32_t)((mouse.y - top) / AF_EDITOR_ROW_HEIGHT);
            const uint32_t index = editor->hierarchy_scroll + row;
            if (index < editor->hierarchy_count) {
                const AfSceneNode *target_node =
                    &editor->project.scene.nodes[editor->hierarchy_order[index]];
                const uint64_t target = target_node->id;
                const float row_y = mouse.y - top - (float)row * AF_EDITOR_ROW_HEIGHT;
                const bool inside = af_editor_hierarchy_drop_inside(target_node, row_y);
                const bool after = row_y >= AF_EDITOR_ROW_HEIGHT * 0.5F;
                const AfSceneNode *source =
                    af_scene_find_const(&editor->project.scene, editor->hierarchy_drag_id);
                const bool already_inside = inside && source != NULL && source->parent_id == target;
                AfEditorCommand command = {0};
                AfResult result =
                    already_inside ? AF_RESULT_SUCCESS
                    : inside
                        ? af_editor_command_reparent(&editor->project, editor->hierarchy_drag_id,
                                                     target, &command)
                        : af_editor_command_reorder(&editor->project, editor->hierarchy_drag_id,
                                                    target, after, &command);
                if (result == AF_RESULT_SUCCESS && !already_inside)
                    result = af_editor_history_execute(&editor->history, &editor->project, &command,
                                                       false, false);
                if (already_inside) {
                    char message[AF_EDITOR_STATUS_CAPACITY];
                    (void)snprintf(message, sizeof(message), "Object is already inside %s",
                                   af_project_name(&editor->project, target));
                    af_editor_status(editor, message);
                } else if (result == AF_RESULT_SUCCESS) {
                    af_editor_mark_dirty(editor);
                    af_editor_status(editor, inside ? "Object moved into group · Ctrl+Z to undo"
                                                    : "Object moved to this tree position · Ctrl+Z to undo");
                } else if (target != editor->hierarchy_drag_id) {
                    af_editor_status(editor, inside ? "Cannot move object into this group"
                                                    : "Cannot move an object beside itself or into its own descendants");
                }
            }
        }
        editor->hierarchy_drag_started = false;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_COLOR_SV ||
        editor->drag_mode == AF_EDITOR_DRAG_COLOR_HUE ||
        editor->drag_mode == AF_EDITOR_DRAG_COLOR_ALPHA) {
        if (editor->color_target_outline ||
            !af_editor_transform_key_drag_covers_selection(editor)) {
            (void)af_editor_commit_selection_modify(
                editor, editor->color_target_outline ? AF_EDITOR_MERGE_STYLE : AF_EDITOR_MERGE_FILL,
                false);
        }
        if (!editor->color_target_outline) {
            (void)af_editor_commit_transform_key_drag(editor, false);
        }
    } else if (editor->drag_mode == AF_EDITOR_DRAG_INSPECTOR_SCRUB) {
        if (editor->inspector_scrub_changed) {
            af_editor_status(editor, "Inspector value adjusted");
        } else {
            af_editor_begin_inspector_edit(editor, editor->inspector_scrub_field);
        }
        editor->inspector_scrub_field = AF_EDITOR_INSPECTOR_NONE;
        editor->inspector_scrub_changed = false;
    } else if (editor->drag_mode == AF_EDITOR_DRAG_TOOL_CREATE) {
        af_editor_finish_vector_tool_drag(editor, width, height);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_MOVE || editor->drag_mode == AF_EDITOR_DRAG_X ||
               editor->drag_mode == AF_EDITOR_DRAG_Y) {
        if (!af_editor_transform_key_drag_covers_selection(editor)) {
            (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_TRANSFORM, false);
        }
        (void)af_editor_commit_transform_key_drag(editor, false);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_GIZMO) {
        const bool position_keyed = af_editor_transform_drag_key_property_index(
                                        editor, editor->selected_id, AF_PROPERTY_POSITION) >= 0;
        if (editor->gizmo_handle == AF_EDITOR_GIZMO_PIVOT ||
            (editor->gizmo_position_changed && !position_keyed) ||
            !af_editor_transform_key_drag_covers_selection(editor)) {
            (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_TRANSFORM, false);
        }
        if (editor->gizmo_handle != AF_EDITOR_GIZMO_PIVOT) {
            (void)af_editor_commit_transform_key_drag(editor, false);
        }
    } else if (editor->drag_mode == AF_EDITOR_DRAG_CANVAS_BOX) {
        af_editor_canvas_box_select(editor, width, height);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_KEYFRAMES) {
        af_editor_timeline_commit_keys(editor, AF_EDITOR_MERGE_KEYFRAME_TIME);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_LOOP_START ||
               editor->drag_mode == AF_EDITOR_DRAG_LOOP_END ||
               editor->drag_mode == AF_EDITOR_DRAG_LOOP_RANGE) {
        af_editor_timeline_commit_clip(editor);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_TANGENT_IN ||
               editor->drag_mode == AF_EDITOR_DRAG_TANGENT_OUT) {
        af_editor_timeline_commit_keys(editor, AF_EDITOR_MERGE_TANGENT);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_KEY_BOX) {
        af_editor_timeline_box_select(editor, height);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_PATH_POINT ||
               editor->drag_mode == AF_EDITOR_DRAG_PATH_CONTROL1 ||
               editor->drag_mode == AF_EDITOR_DRAG_PATH_CONTROL2 ||
               editor->drag_mode == AF_EDITOR_DRAG_PATH_SEGMENT) {
        af_editor_commit_path_drag(editor);
    }
    editor->drag_mode = AF_EDITOR_DRAG_NONE;
    editor->gizmo_handle = AF_EDITOR_GIZMO_NONE;
}

static void af_editor_mouse_wheel(AfEditorState *editor, int width, int height,
                                  const SDL_MouseWheelEvent *event) {
    if (editor->text_panel_open && editor->text_panel_field == 7) {
        if (event->y < 0) {
            uint32_t indices[11];
            if (af_editor_text_font_results(editor, indices, 11) > 10)
                ++editor->font_scroll;
        } else if (event->y > 0 && editor->font_scroll)
            --editor->font_scroll;
        return;
    }
    if (editor->command_palette.open) {
        const float amount = event->y * (event->direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1);
        editor->command_palette.page_size = af_editor_palette_layout(width, height).rows;
        if (isfinite(amount) && amount != 0)
            (void)af_editor_command_palette_handle_key(
                &editor->command_palette,
                amount > 0 ? AF_EDITOR_COMMAND_PALETTE_UP : AF_EDITOR_COMMAND_PALETTE_DOWN, NULL);
        return;
    }
    if (editor->gradient_drag || af_editor_menu_blocked(editor) ||
        editor->top_menu != AF_EDITOR_MENU_NONE || editor->choice_menu ||
        editor->context_menu != AF_EDITOR_CONTEXT_NONE)
        return;
    const float direction = event->direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0F : 1.0F;
    const float scroll_x = event->x * direction;
    const float scroll_y = event->y * direction;
    if (!isfinite(scroll_x) || !isfinite(scroll_y) || event->mouse_x < 0.0F ||
        event->mouse_x >= (float)width - editor->right_width ||
        event->mouse_y < AF_EDITOR_MENU_HEIGHT ||
        event->mouse_y >= (float)height - editor->status_height) {
        return;
    }
    if (event->mouse_x >= editor->left_width &&
        event->mouse_x < (float)width - editor->right_width &&
        event->mouse_y >= AF_EDITOR_MENU_HEIGHT && event->mouse_y < editor->toolbar_height) {
        editor->toolbar_scroll =
            af_editor_clamp(editor->toolbar_scroll - scroll_y * 42.0F - scroll_x * 42.0F, 0.0F,
                            af_editor_toolbar_scroll_limit(editor, width));
        af_editor_status(editor, "Scroll over the toolbar to reveal more tools");
        return;
    }
    if (scroll_y == 0.0F) {
        return;
    }
    if (event->mouse_x < editor->left_width) {
        if (editor->asset_browser_visible &&
            event->mouse_y >= af_editor_asset_browser_top(editor, height)) {
            const uint32_t visible = af_editor_asset_visible_rows(editor);
            const uint32_t maximum = editor->project.image_data.asset_count > visible
                                         ? editor->project.image_data.asset_count - visible
                                         : 0U;
            if (scroll_y > 0.0F && editor->asset_scroll > 0U) {
                --editor->asset_scroll;
            } else if (scroll_y < 0.0F && editor->asset_scroll < maximum) {
                ++editor->asset_scroll;
            }
        } else if (scroll_y > 0.0F && editor->hierarchy_scroll > 0U) {
            --editor->hierarchy_scroll;
        } else if (scroll_y < 0.0F && editor->hierarchy_scroll < editor->hierarchy_count) {
            ++editor->hierarchy_scroll;
        }
        return;
    }
    if (event->mouse_y >= af_editor_timeline_top(editor, height)) {
        const float anchor_time = af_editor_x_to_time(editor, event->mouse_x);
        if ((SDL_GetModState() & SDL_KMOD_SHIFT) != 0U) {
            editor->timeline_pan = fmaxf(0.0F, editor->timeline_pan - scroll_y * 0.25F);
        } else {
            editor->timeline_zoom = af_editor_clamp(
                editor->timeline_zoom * powf(1.12F, af_editor_clamp(scroll_y, -64.0F, 64.0F)),
                24.0F, 800.0F);
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
            af_editor_clamp(editor->zoom * powf(1.12F, af_editor_clamp(scroll_y, -64.0F, 64.0F)),
                            0.1F, 8.0F);
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
    if (editor->gradient_drag) {
        if (event->key == SDLK_ESCAPE)
            af_editor_gradient_finish_drag(editor, true);
        return;
    }
    if (editor->text_panel_open) {
        af_editor_text_panel_key(editor, event);
        return;
    }
    if (editor->offset_panel_open) {
        if (event->key == SDLK_ESCAPE)
            editor->offset_panel_open = false;
        else if (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER)
            af_editor_apply_offset_panel(editor);
        else if (event->key == SDLK_BACKSPACE) {
            if (editor->offset_replace)
                editor->offset_buffer[0] = '\0';
            else
                af_editor_backspace_utf8(editor->offset_buffer);
            editor->offset_replace = false;
        } else if (control && event->key == SDLK_A)
            editor->offset_replace = true;
        return;
    }
    if (editor->inspector_field != AF_EDITOR_INSPECTOR_NONE) {
        if (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER) {
            (void)af_editor_finish_inspector_edit(editor, true);
        } else if (event->key == SDLK_ESCAPE) {
            (void)af_editor_finish_inspector_edit(editor, false);
        } else if (event->key == SDLK_BACKSPACE) {
            if (editor->inspector_replace) {
                editor->inspector_buffer[0] = '\0';
                editor->inspector_replace = false;
            } else {
                af_editor_backspace_utf8(editor->inspector_buffer);
            }
        } else if (event->key == SDLK_TAB) {
            const AfEditorInspectorField next =
                af_editor_next_inspector_field(editor->inspector_field, shift);
            if (af_editor_finish_inspector_edit(editor, true)) {
                af_editor_begin_inspector_edit(editor, next);
            }
        } else if (control && event->key == SDLK_A) {
            editor->inspector_replace = true;
        } else if (control && event->key == SDLK_C) {
            (void)SDL_SetClipboardText(editor->inspector_buffer);
        } else if (control && event->key == SDLK_X) {
            (void)SDL_SetClipboardText(editor->inspector_buffer);
            editor->inspector_buffer[0] = '\0';
            editor->inspector_replace = false;
        } else if (control && event->key == SDLK_V) {
            char *clipboard = SDL_GetClipboardText();
            if (clipboard != NULL) {
                af_editor_append_inspector_text(editor, clipboard);
                SDL_free(clipboard);
            }
        }
        return;
    }
    if (editor->top_menu != AF_EDITOR_MENU_NONE && event->key == SDLK_ESCAPE) {
        editor->top_menu = AF_EDITOR_MENU_NONE;
        return;
    }
    if (editor->help_overlay && event->key == SDLK_ESCAPE) {
        editor->help_overlay = false;
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
    if (af_editor_workflow_key(editor, event)) {
        return;
    }
    if (control && event->key == SDLK_G) {
        af_editor_group_selection(editor);
        return;
    }
    if (control && event->key == SDLK_L) {
        af_editor_compound_selection(editor, (event->mod & SDL_KMOD_SHIFT) != 0U);
        return;
    }
    if (event->key == SDLK_ESCAPE && editor->drag_mode == AF_EDITOR_DRAG_HIERARCHY) {
        editor->drag_mode = AF_EDITOR_DRAG_NONE;
        editor->hierarchy_drag_started = false;
        af_editor_status(editor, "Hierarchy drag cancelled");
        return;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_TOOL_CREATE && event->key == SDLK_ESCAPE) {
        af_editor_cancel_vector_tool(editor, "Drawing cancelled");
        return;
    }
    if (editor->active_tool == AF_EDITOR_TOOL_PEN && editor->tool_command_count != 0U) {
        if (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER) {
            (void)af_editor_finish_pen_path(editor);
            return;
        }
        if (event->key == SDLK_ESCAPE) {
            af_editor_cancel_vector_tool(editor, "Pen path cancelled");
            return;
        }
        if (event->key == SDLK_BACKSPACE) {
            if (editor->tool_command_count <= editor->tool_pen_resume_count) {
                af_editor_cancel_vector_tool(editor, "Path continuation cancelled");
                return;
            }
            --editor->tool_command_count;
            editor->tool_pen_has_outgoing = false;
            af_editor_status(editor, editor->tool_command_count == 0U ? "Pen path cancelled"
                                                                      : "Last pen point removed");
            return;
        }
    }
    if (!control && !alt && !editor->timeline_focus) {
        AfEditorVectorTool shortcut_tool = AF_EDITOR_TOOL_COUNT;
        if (event->key == SDLK_V) {
            if (event->repeat)
                return;
            shortcut_tool = shift || editor->active_tool == AF_EDITOR_TOOL_SELECT
                                ? AF_EDITOR_TOOL_NODE
                                : AF_EDITOR_TOOL_SELECT;
        } else if (event->key == SDLK_X) {
            shortcut_tool = AF_EDITOR_TOOL_SCISSORS;
        } else if (event->key == SDLK_D) {
            shortcut_tool = AF_EDITOR_TOOL_SEGMENT_DELETE;
        } else if (event->key == SDLK_J) {
            shortcut_tool = AF_EDITOR_TOOL_KNIFE;
        } else if (event->key == SDLK_R && editor->active_tool != AF_EDITOR_TOOL_NODE) {
            shortcut_tool = shift ? AF_EDITOR_TOOL_ROUNDED_RECTANGLE : AF_EDITOR_TOOL_RECTANGLE;
        } else if (event->key == SDLK_O && editor->active_tool != AF_EDITOR_TOOL_NODE) {
            shortcut_tool = AF_EDITOR_TOOL_ELLIPSE;
        } else if (event->key == SDLK_L) {
            shortcut_tool = AF_EDITOR_TOOL_LINE;
        } else if (event->key == SDLK_T) {
            if (af_editor_is_text(editor, editor->selected_id))
                af_editor_open_text_panel(editor, width, height);
            else
                af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_TEXT);
            return;
        } else if (event->key == SDLK_P) {
            shortcut_tool = AF_EDITOR_TOOL_PEN;
        } else if (event->key == SDLK_B) {
            shortcut_tool = AF_EDITOR_TOOL_PENCIL;
        } else if (event->key == SDLK_Y) {
            shortcut_tool = AF_EDITOR_TOOL_POLYGON;
        } else if (event->key == SDLK_A) {
            shortcut_tool = AF_EDITOR_TOOL_STAR;
        } else if (event->key == SDLK_C && editor->active_tool != AF_EDITOR_TOOL_NODE) {
            shortcut_tool = AF_EDITOR_TOOL_ARC;
        } else if (event->key == SDLK_I && editor->active_tool != AF_EDITOR_TOOL_NODE) {
            shortcut_tool = AF_EDITOR_TOOL_SPIRAL;
        }
        if (shortcut_tool != AF_EDITOR_TOOL_COUNT) {
            af_editor_set_vector_tool(editor, shortcut_tool);
            return;
        }
    }
    if (alt && (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER)) {
        af_editor_execute_menu_action(editor, AF_EDITOR_MENU_ACTION_FULLSCREEN, width, height);
    } else if (control && (event->key == SDLK_Y ||
                           ((event->mod & SDL_KMOD_SHIFT) != 0U && event->key == SDLK_Z))) {
        if (af_editor_history_redo(&editor->history, &editor->project) == AF_RESULT_SUCCESS) {
            af_editor_mark_dirty(editor);
            af_editor_reconcile_selection(editor);
            af_editor_status(editor, "Redo");
        }
    } else if (control && event->key == SDLK_Z) {
        if (af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS) {
            af_editor_mark_dirty(editor);
            af_editor_reconcile_selection(editor);
            af_editor_status(editor, "Undo");
        }
    } else if (control && shift && event->key == SDLK_S) {
        af_editor_show_project_dialog(editor, AF_EDITOR_DIALOG_SAVE_PROJECT_AS);
    } else if (control && event->key == SDLK_S) {
        if (editor->project_path[0] == '\0') {
            af_editor_show_project_dialog(editor, AF_EDITOR_DIALOG_SAVE_PROJECT_AS);
        } else {
            (void)af_editor_save(editor);
        }
    } else if (control && event->key == SDLK_O) {
        af_editor_show_project_dialog(editor, AF_EDITOR_DIALOG_OPEN_PROJECT);
    } else if (control && event->key == SDLK_N) {
        (void)af_editor_new_project(editor);
    } else if (control && event->key == SDLK_I) {
        if (editor->project_path[0] == '\0') {
            af_editor_status(editor, "Save the project before importing images");
        } else {
            af_editor_show_project_dialog(editor, AF_EDITOR_DIALOG_IMPORT_IMAGE);
        }
    } else if (control && event->key == SDLK_A) {
        AfPath *path = editor->active_tool == AF_EDITOR_TOOL_NODE && !editor->timeline_focus &&
                               editor->selected_count == 1U
                           ? af_editor_selected_path(editor)
                           : NULL;
        if (path != NULL) {
            af_editor_path_selection_clear(editor);
            for (uint32_t command_index = 0U; command_index < path->command_count;
                 ++command_index) {
                if (editor->project.path_data.commands[path->command_start + command_index].type !=
                    AF_PATH_CLOSE) {
                    (void)af_editor_path_selection_add(editor, command_index);
                }
            }
            af_editor_status(editor, "All path nodes selected");
        } else {
            af_editor_select_all(editor);
        }
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
        } else if (editor->active_tool == AF_EDITOR_TOOL_NODE && !shift &&
                   af_editor_selected_path(editor) != NULL &&
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
    } else if (!control && !alt && event->key == SDLK_S && !editor->timeline_focus &&
               editor->active_tool == AF_EDITOR_TOOL_NODE &&
               af_editor_selected_path(editor) != NULL) {
        (void)af_editor_smooth_path_points(editor, shift);
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
    (void)af_platform_window_point_to_pixels(&editor->window, x, y);
}

static void af_editor_handle_event(AfEditorState *editor, SDL_Event *event, int width, int height) {
    if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST || event->type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
        if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST)
            af_editor_gradient_finish_drag(editor, false);
        editor->top_menu = AF_EDITOR_MENU_NONE;
        editor->choice_menu = 0;
        editor->context_menu = AF_EDITOR_CONTEXT_NONE;
    } else if (event->type == SDL_EVENT_QUIT) {
        if (editor->dialog_active) {
            af_editor_status(editor, "Close the file dialog before exiting");
        } else if (af_editor_confirm_discard(editor, "Exit Arcflow Editor?")) {
            editor->running = false;
        }
    } else if (event->type == editor->dialog_event_type) {
        af_editor_handle_dialog_event(editor, &event->user);
    } else if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
               event->type == SDL_EVENT_WINDOW_RESIZED ||
               event->type == SDL_EVENT_WINDOW_DISPLAY_CHANGED ||
               event->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
        af_vk_renderer_request_resize(editor->renderer);
    } else if (event->type == SDL_EVENT_KEY_DOWN) {
        af_editor_key(editor, width, height, &event->key);
    } else if (event->type == SDL_EVENT_TEXT_INPUT) {
        if (editor->text_panel_open) {
            af_editor_text_panel_append(editor, event->text.text);
        } else if (editor->inspector_field != AF_EDITOR_INSPECTOR_NONE) {
            af_editor_append_inspector_text(editor, event->text.text);
        } else if (!af_editor_workflow_text(editor, event->text.text)) {
            af_editor_append_text(editor, event->text.text);
        }
    } else if (event->type == SDL_EVENT_DROP_FILE) {
        af_editor_workflow_drop_file(editor, event->drop.data);
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        SDL_MouseButtonEvent button = event->button;
        af_editor_mouse_scale(editor, &button.x, &button.y);
        af_editor_mouse_down(editor, width, height, &button, SDL_GetModState());
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if ((editor->gradient_drag || editor->drag_mode == AF_EDITOR_DRAG_HIERARCHY) &&
            event->button.button != SDL_BUTTON_LEFT)
            return;
        float x = event->button.x, y = event->button.y;
        af_editor_mouse_scale(editor, &x, &y);
        editor->mouse_position = (AfVec2){x, y};
        if (editor->gradient_drag)
            af_editor_gradient_motion(editor, width, height, x, y);
        if (editor->drag_mode == AF_EDITOR_DRAG_HIERARCHY &&
            hypotf(x - editor->drag_start_mouse.x, y - editor->drag_start_mouse.y) > 5.0F)
            editor->hierarchy_drag_started = true;
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
    } else if (event->type == SDL_EVENT_PEN_DOWN) {
        SDL_MouseButtonEvent button = {
            .button = SDL_BUTTON_LEFT, .down = true, .x = event->ptouch.x, .y = event->ptouch.y};
        af_editor_mouse_scale(editor, &button.x, &button.y);
        af_editor_mouse_down(editor, width, height, &button, SDL_GetModState());
    } else if (event->type == SDL_EVENT_PEN_UP) {
        af_editor_mouse_up(editor, width, height);
    } else if (event->type == SDL_EVENT_PEN_MOTION) {
        SDL_MouseMotionEvent motion = {.x = event->pmotion.x, .y = event->pmotion.y};
        af_editor_mouse_scale(editor, &motion.x, &motion.y);
        motion.xrel = motion.x - editor->mouse_position.x;
        motion.yrel = motion.y - editor->mouse_position.y;
        af_editor_mouse_motion(editor, width, height, &motion);
    }
}

static AfResult af_editor_interaction_smoke_failure(const AfEditorState *editor, int line) {
    af_log(AF_LOG_ERROR, "Editor interaction check at line %d failed: %s", line, editor->status);
    return AF_RESULT_INVALID_ARGUMENT;
}

static AfResult af_editor_interaction_smoke(AfEditorState *editor, int width, int height) {
    /* Hierarchy drags below need several visible rows regardless of saved dock preferences. */
    const float saved_asset_height = editor->asset_height;
    editor->asset_height = fminf(editor->asset_height, fmaxf(0,
        af_editor_timeline_top(editor, height) - editor->toolbar_height - 6 * AF_EDITOR_ROW_HEIGHT - 16));
    /* Navigation must preserve the pointer anchor and keep dock input local. */
    const float saved_zoom = editor->zoom;
    const AfVec2 saved_pan = editor->pan;
    const float saved_timeline_zoom = editor->timeline_zoom;
    const float saved_timeline_pan = editor->timeline_pan;
    const bool saved_profiler = editor->profiler_visible;
    const SDL_Keymod saved_modifiers = SDL_GetModState();
    SDL_SetModState(SDL_KMOD_NONE);
    editor->timeline_focus = false;
    const SDL_KeyboardEvent node_key = {.key = SDLK_V, .mod = SDL_KMOD_SHIFT, .down = true};
    af_editor_key(editor, width, height, &node_key);
    if (editor->active_tool != AF_EDITOR_TOOL_NODE || editor->profiler_visible != saved_profiler)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    const SDL_KeyboardEvent toggle_key = {.key = SDLK_V, .down = true};
    for (uint32_t index = 0U; index < 4U; ++index) {
        const AfEditorVectorTool expected =
            (index & 1U) == 0U ? AF_EDITOR_TOOL_SELECT : AF_EDITOR_TOOL_NODE;
        af_editor_key(editor, width, height, &toggle_key);
        if (editor->active_tool != expected)
            return af_editor_interaction_smoke_failure(editor, __LINE__);
        af_editor_key(editor, width, height,
                      &(SDL_KeyboardEvent){.key = SDLK_V, .down = true, .repeat = true});
        if (editor->active_tool != expected)
            return af_editor_interaction_smoke_failure(editor, __LINE__);
    }
    af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_RECTANGLE);
    af_editor_key(editor, width, height, &toggle_key);
    if (editor->active_tool != AF_EDITOR_TOOL_SELECT)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    af_editor_key(editor, width, height, &node_key);
    if (editor->active_tool != AF_EDITOR_TOOL_NODE)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    const SDL_KeyboardEvent profiler_key = {.key = SDLK_F10, .down = true};
    af_editor_key(editor, width, height, &profiler_key);
    if (editor->profiler_visible == saved_profiler || editor->active_tool != AF_EDITOR_TOOL_NODE)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    editor->profiler_visible = saved_profiler;
    af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_SELECT);
    SDL_MouseWheelEvent wheel = {.x = 1.0F,
                                 .mouse_x = editor->left_width + 100.0F,
                                 .mouse_y = editor->toolbar_height + 100.0F};
    editor->zoom = 1.0F;
    const AfVec2 anchor = af_editor_screen_to_project(
        editor, width, height, (AfVec2){wheel.mouse_x, wheel.mouse_y});
    af_editor_mouse_wheel(editor, width, height, &wheel);
    if (editor->zoom != 1.0F || editor->pan.x != saved_pan.x || editor->pan.y != saved_pan.y)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    wheel.x = 0.0F;
    wheel.y = 0.5F;
    af_editor_mouse_wheel(editor, width, height, &wheel);
    af_editor_mouse_wheel(editor, width, height, &wheel);
    const AfVec2 after = af_editor_screen_to_project(
        editor, width, height, (AfVec2){wheel.mouse_x, wheel.mouse_y});
    if (fabsf(editor->zoom - 1.12F) > 0.0001F ||
        hypotf(after.x - anchor.x, after.y - anchor.y) > 0.001F)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    wheel.y = 1.0F;
    wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    af_editor_mouse_wheel(editor, width, height, &wheel);
    if (fabsf(editor->zoom - 1.0F) > 0.0001F)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    wheel.mouse_y = af_editor_timeline_top(editor, height) + 90.0F;
    wheel.mouse_x = (float)width - editor->right_width + 20.0F;
    af_editor_mouse_wheel(editor, width, height, &wheel);
    if (editor->timeline_zoom != saved_timeline_zoom || editor->timeline_pan != saved_timeline_pan)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    wheel.mouse_x = af_editor_timeline_ruler_x(editor) + 100.0F;
    wheel.x = 1.0F;
    wheel.y = 0.0F;
    af_editor_mouse_wheel(editor, width, height, &wheel);
    if (editor->timeline_zoom != saved_timeline_zoom || editor->timeline_pan != saved_timeline_pan)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    editor->zoom = saved_zoom;
    editor->pan = saved_pan;
    SDL_SetModState(saved_modifiers);
    /* Compact controls and clip cycling must use the same geometry as rendering. */
    const int compact_width = (int)(editor->left_width + editor->right_width + 500.0F);
    const float compact_right = (float)compact_width - editor->right_width;
    const float saved_playhead = editor->playhead;
    const bool saved_playing = editor->preview_playing;
    const uint64_t saved_clip = editor->selected_clip_id;
    const bool saved_timeline_focus = editor->timeline_focus;
    AfProjectClip extra_clip = {.id = editor->project.next_animation_id,
                                .name = "Compact clip", .duration = 4.0F};
    if (af_project_add_clip(&editor->project, &extra_clip) != AF_RESULT_SUCCESS)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    editor->selected_clip_id = extra_clip.id;
    SDL_MouseButtonEvent compact_click = {
        .button = SDL_BUTTON_LEFT, .down = true,
        .x = editor->left_width + 30.0F * af_editor_timeline_controls_scale(editor, compact_width),
        .y = af_editor_timeline_top(editor, height) + 20.0F};
    if (!af_editor_timeline_mouse_down(editor, compact_width, height, &compact_click, SDL_KMOD_NONE) ||
        editor->preview_playing == saved_playing)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    compact_click.x = compact_right - 40.0F;
    if (!af_editor_timeline_mouse_down(editor, compact_width, height, &compact_click, SDL_KMOD_NONE) ||
        editor->selected_clip_id != editor->project.clips[0].id || editor->playhead != 0.0F ||
        af_project_remove_clip_exact(&editor->project, extra_clip.id) != AF_RESULT_SUCCESS)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    editor->selected_clip_id = saved_clip;
    editor->playhead = saved_playhead;
    editor->preview_playing = saved_playing;
    editor->timeline_focus = saved_timeline_focus;
    const AfVec2 center = af_editor_visible_center(editor, width, height);
    const AfEditorToolRect rect = {{center.x - 50, center.y - 40}, {100, 80}};
    if (af_editor_create_primitive_at(editor, AF_NODE_ELLIPSE, rect) != AF_RESULT_SUCCESS)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    const uint64_t ellipse = editor->selected_id;
    const uint32_t original_count = editor->project.scene.count;
    const SDL_KeyboardEvent duplicate_key = {.key = SDLK_D, .mod = SDL_KMOD_CTRL, .down = true};
    editor->timeline_focus = false;
    af_editor_key(editor, width, height, &duplicate_key);
    if (editor->project.scene.count != original_count + 1U || editor->selected_id == ellipse ||
        af_editor_history_undo(&editor->history, &editor->project) != AF_RESULT_SUCCESS)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    af_editor_select_only(editor, ellipse);
    af_scene_find(&editor->project.scene, ellipse)->layer_order = 1000;
    af_editor_update_preview(editor);
    const uint32_t cursor = editor->history.cursor;
    SDL_MouseButtonEvent click = {.button = SDL_BUTTON_LEFT,
                                  .down = true,
                                  .clicks = 1,
                                  .x = (float)width - editor->right_width + 30,
                                  .y = AF_EDITOR_CONVERT_PATH_Y + 12};
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    af_editor_mouse_up(editor, width, height);
    if (af_scene_find(&editor->project.scene, ellipse)->type != AF_NODE_PATH ||
        editor->active_tool != AF_EDITOR_TOOL_NODE || editor->history.cursor != cursor + 1U)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    if (af_editor_history_undo(&editor->history, &editor->project) != AF_RESULT_SUCCESS ||
        af_scene_find(&editor->project.scene, ellipse)->type != AF_NODE_ELLIPSE ||
        af_editor_history_redo(&editor->history, &editor->project) != AF_RESULT_SUCCESS)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_SELECT);
    af_editor_update_preview(editor);
    const AfVec2 screen =
        af_affine_transform_point(af_editor_camera(editor, width, height), center);
    click.x = screen.x;
    click.y = screen.y;
    click.clicks = 2;
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    af_editor_mouse_up(editor, width, height);
    if (editor->active_tool != AF_EDITOR_TOOL_NODE || editor->selected_id != ellipse)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    AfPathCommand commands[16];
    const uint32_t count = af_editor_vector_tool_polygon(rect.size, 5, 0.45F, commands, 16);
    if (af_editor_create_path_geometry_with_shape(editor, rect.position, rect.size, commands, count,
                                                  af_editor_vector_tool_name(AF_EDITOR_TOOL_STAR),
                                                  true, 5U, 0.45F) != AF_RESULT_SUCCESS)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    const uint64_t star = editor->selected_id;
    af_editor_update_preview(editor);
    click.clicks = 1;
    click.x = (float)width - editor->right_width + 18 + (editor->right_width - 36) * 0.375F;
    click.y = AF_EDITOR_SHAPE_SETTINGS_Y + 12;
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    af_editor_mouse_up(editor, width, height);
    if (af_scene_find(&editor->project.scene, star)->polygon_sides != 6U)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    click.y = AF_EDITOR_CONVERT_PATH_Y + 12;
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    af_editor_mouse_up(editor, width, height);
    if (af_scene_find(&editor->project.scene, star)->polygon_sides != 0U ||
        af_editor_history_undo(&editor->history, &editor->project) != AF_RESULT_SUCCESS ||
        af_scene_find(&editor->project.scene, star)->polygon_sides != 6U)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    af_editor_select_only(editor, ellipse);
    af_editor_selection_add(editor, star);
    click.x = 30;
    click.y = 38;
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    af_editor_mouse_up(editor, width, height);
    const uint64_t group = editor->selected_id;
    if (af_scene_find(&editor->project.scene, ellipse)->parent_id != group ||
        af_scene_find(&editor->project.scene, star)->parent_id != group)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    af_editor_build_hierarchy(editor);
    uint32_t first = 0U;
    while (first < editor->hierarchy_count &&
           editor->project.scene.nodes[editor->hierarchy_order[first]].id != star)
        ++first;
    if (first + 1U >= editor->hierarchy_count)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    editor->hierarchy_scroll = first;
    const uint32_t reorder_cursor = editor->history.cursor;
    click.x = 100;
    click.y = editor->toolbar_height + 18;
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    SDL_MouseMotionEvent motion = {.x = 100,
                                   .y = editor->toolbar_height + 8 + AF_EDITOR_ROW_HEIGHT * 1.8F};
    af_editor_mouse_motion(editor, width, height, &motion);
    af_editor_mouse_up(editor, width, height);
    if (editor->history.cursor !=
            (reorder_cursor < editor->history.capacity ? reorder_cursor + 1U : reorder_cursor) ||
        af_scene_find(&editor->project.scene, star)->layer_order != 1000 ||
        af_editor_history_undo(&editor->history, &editor->project) != AF_RESULT_SUCCESS)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    const float old_toolbar = editor->toolbar_height;
    click.x = editor->left_width + 100;
    click.y = old_toolbar;
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    motion.x = click.x;
    motion.y = old_toolbar + 12;
    af_editor_mouse_motion(editor, width, height, &motion);
    af_editor_mouse_up(editor, width, height);
    if (editor->toolbar_height <= old_toolbar)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    editor->toolbar_height = old_toolbar;
    /* A newly created root can be dropped into the center of an existing group. */
    if (af_editor_create_primitive_at(editor, AF_NODE_RECTANGLE, rect) != AF_RESULT_SUCCESS)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    const uint64_t fresh = editor->selected_id;
    const uint64_t destination = af_scene_find(&editor->project.scene, star)->parent_id;
    af_editor_update_preview(editor);
    const AfVec2 fresh_position =
        (AfVec2){af_scene_find(&editor->preview_scene, fresh)->world_transform.tx,
                 af_scene_find(&editor->preview_scene, fresh)->world_transform.ty};
    AfPlatformWindowMetrics metrics = {0};
    if (af_platform_window_metrics(&editor->window, &metrics) != AF_RESULT_SUCCESS)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    af_editor_build_hierarchy(editor);
    for (uint32_t row = 0U; row < editor->hierarchy_count; ++row)
        if (editor->project.scene.nodes[editor->hierarchy_order[row]].id == fresh)
            editor->hierarchy_scroll = row;
    SDL_Event direct = {.button = {.type = SDL_EVENT_MOUSE_BUTTON_DOWN,
        .button = SDL_BUTTON_LEFT, .down = true, .clicks = 1,
        .x = 100 / metrics.pixel_scale_x,
        .y = (editor->toolbar_height + 8 + AF_EDITOR_ROW_HEIGHT * 0.5F) / metrics.pixel_scale_y}};
    af_editor_handle_event(editor, &direct, width, height);
    if (!af_editor_can_reorder(&editor->project, fresh, star))
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    for (uint32_t row = 0U; row < editor->hierarchy_count; ++row)
        if (editor->project.scene.nodes[editor->hierarchy_order[row]].id == star)
            editor->hierarchy_scroll = row;
    direct.button.type = SDL_EVENT_MOUSE_BUTTON_UP;
    direct.button.down = false;
    direct.button.x = 110 / metrics.pixel_scale_x;
    direct.button.y = (editor->toolbar_height + 10) / metrics.pixel_scale_y;
    af_editor_handle_event(editor, &direct, width, height);
    if (af_scene_find(&editor->project.scene, fresh)->parent_id != destination ||
        af_project_find_index(&editor->project, fresh) >= af_project_find_index(&editor->project, star) ||
        af_editor_history_undo(&editor->history, &editor->project) != AF_RESULT_SUCCESS ||
        af_scene_find(&editor->project.scene, fresh)->parent_id != AF_INVALID_NODE_ID)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    const float drop_offsets[] = {5.0F, AF_EDITOR_ROW_HEIGHT * 0.5F, AF_EDITOR_ROW_HEIGHT - 5.0F};
    for (uint32_t attempt = 0U; attempt < 3U; ++attempt) {
        af_editor_build_hierarchy(editor);
        for (uint32_t row = 0U; row < editor->hierarchy_count; ++row)
            if (editor->project.scene.nodes[editor->hierarchy_order[row]].id == fresh)
                editor->hierarchy_scroll = row;
        SDL_Event event = {
            .button = {.type = SDL_EVENT_MOUSE_BUTTON_DOWN,
                       .button = SDL_BUTTON_LEFT,
                       .down = true,
                       .clicks = 1,
                       .x = 100 / metrics.pixel_scale_x,
                       .y = (editor->toolbar_height + 8 + AF_EDITOR_ROW_HEIGHT * 0.5F) /
                            metrics.pixel_scale_y}};
        af_editor_handle_event(editor, &event, width, height);
        if (editor->drag_mode != AF_EDITOR_DRAG_HIERARCHY)
            return af_editor_interaction_smoke_failure(editor, __LINE__);
        /* Start the drag, then release at a different location without another motion. */
        event = (SDL_Event){.motion = {.type = SDL_EVENT_MOUSE_MOTION,
                                       .x = 110 / metrics.pixel_scale_x,
                                       .y = (editor->toolbar_height + 10) / metrics.pixel_scale_y}};
        af_editor_handle_event(editor, &event, width, height);
        const uint32_t scroll_before = editor->hierarchy_scroll;
        af_editor_hierarchy_autoscroll(editor, height,
                                       editor->hierarchy_scroll_ticks + UINT64_C(80000000));
        if (scroll_before > 0U && editor->hierarchy_scroll != scroll_before - 1U)
            return af_editor_interaction_smoke_failure(editor, __LINE__);
        for (uint32_t row = 0U; row < editor->hierarchy_count; ++row)
            if (editor->project.scene.nodes[editor->hierarchy_order[row]].id == destination)
                editor->hierarchy_scroll = row;
        /* Auxiliary button release must not finish the left-button drag. */
        event =
            (SDL_Event){.button = {.type = SDL_EVENT_MOUSE_BUTTON_UP, .button = SDL_BUTTON_RIGHT}};
        af_editor_handle_event(editor, &event, width, height);
        if (editor->drag_mode != AF_EDITOR_DRAG_HIERARCHY)
            return af_editor_interaction_smoke_failure(editor, __LINE__);
        event = (SDL_Event){.button = {.type = SDL_EVENT_MOUSE_BUTTON_UP,
                                       .button = SDL_BUTTON_LEFT,
                                       .x = 110 / metrics.pixel_scale_x,
                                       .y = (editor->toolbar_height + 8 + drop_offsets[attempt]) /
                                            metrics.pixel_scale_y}};
        af_editor_handle_event(editor, &event, width, height);
        af_editor_update_preview(editor);
        const AfSceneNode *moved = af_scene_find(&editor->preview_scene, fresh);
        if (af_scene_find(&editor->project.scene, fresh)->parent_id != destination ||
            fabsf(moved->world_transform.tx - fresh_position.x) > 0.01F ||
            fabsf(moved->world_transform.ty - fresh_position.y) > 0.01F ||
            af_editor_history_undo(&editor->history, &editor->project) != AF_RESULT_SUCCESS ||
            af_scene_find(&editor->project.scene, fresh)->parent_id != AF_INVALID_NODE_ID)
            return af_editor_interaction_smoke_failure(editor, __LINE__);
    }
    if (af_editor_history_redo(&editor->history, &editor->project) != AF_RESULT_SUCCESS)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    /* Dropping a child onto its current group is accepted without recording an edit. */
    af_editor_build_hierarchy(editor);
    for (uint32_t row = 0U; row < editor->hierarchy_count; ++row)
        if (editor->project.scene.nodes[editor->hierarchy_order[row]].id == fresh)
            editor->hierarchy_scroll = row;
    SDL_Event same_parent = {
        .button = {.type = SDL_EVENT_MOUSE_BUTTON_DOWN,
                   .button = SDL_BUTTON_LEFT,
                   .down = true,
                   .clicks = 1,
                   .x = 100 / metrics.pixel_scale_x,
                   .y = (editor->toolbar_height + 8 + AF_EDITOR_ROW_HEIGHT * 0.5F) /
                        metrics.pixel_scale_y}};
    af_editor_handle_event(editor, &same_parent, width, height);
    if (!af_editor_hierarchy_can_drop_inside(editor, destination))
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    for (uint32_t row = 0U; row < editor->hierarchy_count; ++row)
        if (editor->project.scene.nodes[editor->hierarchy_order[row]].id == destination)
            editor->hierarchy_scroll = row;
    const uint32_t no_op_cursor = editor->history.cursor;
    same_parent.button.type = SDL_EVENT_MOUSE_BUTTON_UP;
    same_parent.button.down = false;
    same_parent.button.x = 110 / metrics.pixel_scale_x;
    af_editor_handle_event(editor, &same_parent, width, height);
    if (editor->history.cursor != no_op_cursor ||
        strstr(editor->status, "already inside") == NULL ||
        af_scene_find(&editor->project.scene, fresh)->parent_id != destination)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    af_editor_select_only(editor, destination);
    const uint32_t group_nodes = editor->project.scene.count;
    const uint32_t group_paths = editor->project.path_data.path_count;
    if (af_editor_duplicate_selection(editor) != AF_RESULT_SUCCESS ||
        editor->project.scene.count != group_nodes + 4U ||
        editor->project.path_data.path_count != group_paths + 2U ||
        af_editor_history_undo(&editor->history, &editor->project) != AF_RESULT_SUCCESS)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    if (af_editor_prepare_image_smoke(editor) != AF_RESULT_SUCCESS ||
        af_image_build_atlases(&editor->project.image_data) != AF_RESULT_SUCCESS ||
        af_project_validate(&editor->project) != AF_RESULT_SUCCESS)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    for (uint32_t image_index = 0U; image_index < 2U; ++image_index) {
        const AfImageBinding binding = editor->project.image_data.bindings[image_index];
        af_editor_select_only(editor, binding.node_id);
        const uint32_t image_count = editor->project.scene.count;
        af_editor_execute_menu_action(editor, AF_EDITOR_MENU_ACTION_DUPLICATE, width, height);
        const uint64_t image_copy = editor->selected_id;
        const AfImageBinding *copied_binding =
            af_image_find_binding_const(&editor->project.image_data, image_copy);
        if (editor->project.scene.count != image_count + 1U || image_copy == binding.node_id ||
            copied_binding == NULL || copied_binding->asset_id != binding.asset_id ||
            copied_binding->sequence_id != binding.sequence_id ||
            af_editor_history_undo(&editor->history, &editor->project) != AF_RESULT_SUCCESS ||
            af_scene_find(&editor->project.scene, image_copy) != NULL ||
            af_editor_history_redo(&editor->history, &editor->project) != AF_RESULT_SUCCESS)
            return af_editor_interaction_smoke_failure(editor, __LINE__);
    }
    af_editor_select_only(editor, star);
    af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_SELECT);
    editor->asset_height = saved_asset_height;
    af_editor_update_preview(editor);
    return af_project_validate(&editor->project);
}

static AfResult af_editor_knife_smoke(AfEditorState *editor, int width, int height) {
    const AfPathCommand rectangle[] = {{.type = AF_PATH_MOVE},
                                       {.type = AF_PATH_LINE, .point = {100.0F, 0.0F}},
                                       {.type = AF_PATH_LINE, .point = {100.0F, 100.0F}},
                                       {.type = AF_PATH_LINE, .point = {0.0F, 100.0F}},
                                       {.type = AF_PATH_CLOSE}};
    AfPathCommand after[8];
    AfVec2 position = af_editor_visible_center(editor, width, height);
    position.x -= 50.0F;
    position.y -= 50.0F;
    if (af_editor_create_path_geometry(editor, position, (AfVec2){100.0F, 100.0F}, rectangle, 5U,
                                       "Knife closing edge", true) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    const uint64_t selected_id = editor->selected_id;
    const uint32_t original_selection = editor->selected_path_command;
    const uint32_t history_count = editor->history.count;
    const uint32_t history_cursor = editor->history.cursor;
    const bool snapping = editor->snapping;
    AfSceneNode *authored = af_scene_find(&editor->project.scene, selected_id);
    authored->local_transform.rotation = 0.4F;
    authored->local_transform.scale = (AfVec2){1.3F, 0.8F};
    authored->local_transform.skew = (AfVec2){0.2F, 0.1F};
    editor->snapping = false;
    af_editor_update_preview(editor);
    const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, selected_id);
    const AfAffine2 transform =
        af_affine_multiply(af_editor_camera(editor, width, height), node->world_transform);
    const AfVec2 start = af_affine_transform_point(transform, (AfVec2){-10.0F, 50.0F});
    const AfVec2 end = af_affine_transform_point(transform, (AfVec2){110.0F, 50.0F});
    SDL_MouseButtonEvent click = {
        .button = SDL_BUTTON_LEFT, .down = true, .clicks = 1U, .x = start.x, .y = start.y};
    SDL_MouseMotionEvent motion = {
        .x = end.x, .y = end.y, .xrel = end.x - start.x, .yrel = end.y - start.y};
    SDL_KeyboardEvent key = {.down = true, .key = SDLK_ESCAPE};
    af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_KNIFE);
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    af_editor_mouse_motion(editor, width, height, &motion);
    if (editor->drag_mode != AF_EDITOR_DRAG_TOOL_CREATE || editor->history.count != history_count) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    af_editor_key(editor, width, height, &key);
    af_editor_mouse_up(editor, width, height);
    AfPath *path = af_editor_selected_path(editor);
    if (path == NULL || path->command_count != 5U || editor->drag_mode != AF_EDITOR_DRAG_NONE ||
        editor->history.count != history_count || editor->history.cursor != history_cursor ||
        editor->selected_id != selected_id || editor->selected_path_command != original_selection ||
        memcmp(&editor->project.path_data.commands[path->command_start], rectangle,
               sizeof(rectangle)) != 0) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    af_editor_mouse_motion(editor, width, height, &motion);
    af_editor_mouse_up(editor, width, height);
    path = af_editor_selected_path(editor);
    if (path == NULL || path->command_count != 8U || editor->history.count != history_count + 1U ||
        editor->history.cursor != history_cursor + 1U || editor->selected_id != selected_id ||
        editor->selected_path_command != 0U || editor->selected_path_command_count != 1U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    memcpy(after, &editor->project.path_data.commands[path->command_start], sizeof(after));
    uint32_t moves = 0U;
    for (uint32_t index = 0U; index < 8U; ++index) {
        moves += after[index].type == AF_PATH_MOVE;
        if (after[index].type == AF_PATH_CLOSE) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    if (moves != 2U || fabsf(after[0].point.x - 100.0F) > 0.001F ||
        fabsf(after[0].point.y - 50.0F) > 0.001F || fabsf(after[4].point.x) > 0.001F ||
        fabsf(after[4].point.y - 50.0F) > 0.001F) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    key.key = SDLK_Z;
    key.mod = SDL_KMOD_CTRL;
    af_editor_key(editor, width, height, &key);
    path = af_editor_selected_path(editor);
    if (path == NULL || path->command_count != 5U || editor->history.cursor != history_cursor ||
        memcmp(&editor->project.path_data.commands[path->command_start], rectangle,
               sizeof(rectangle)) != 0) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    key.mod = SDL_KMOD_CTRL | SDL_KMOD_SHIFT;
    af_editor_key(editor, width, height, &key);
    path = af_editor_selected_path(editor);
    if (path == NULL || path->command_count != 8U ||
        editor->history.cursor != history_cursor + 1U ||
        memcmp(&editor->project.path_data.commands[path->command_start], after, sizeof(after)) !=
            0) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    /* Indexed point animation must still reject topology changes without touching the history. */
    editor->selected_property = AF_PROPERTY_PATH_POINT;
    if (af_editor_add_keyframe(editor) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    const uint32_t animated_history = editor->history.count;
    if (af_editor_knife_path(
            editor, af_affine_transform_point(node->world_transform, (AfVec2){50.0F, -10.0F}),
            af_affine_transform_point(node->world_transform, (AfVec2){50.0F, 110.0F})) !=
            AF_RESULT_INVALID_ARGUMENT ||
        editor->history.count != animated_history ||
        memcmp(&editor->project.path_data.commands[path->command_start], after, sizeof(after)) !=
            0) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    editor->snapping = snapping;
    af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_NODE);
    af_editor_update_preview(editor);
    return af_project_validate(&editor->project);
}

static AfResult af_editor_boolean_smoke(AfEditorState *editor, int width, int height) {
    const AfVec2 center = af_editor_visible_center(editor, width, height);
    const AfEditorMenuAction actions[] = {AF_EDITOR_MENU_ACTION_WELD, AF_EDITOR_MENU_ACTION_TRIM,
                                          AF_EDITOR_MENU_ACTION_INTERSECT};
    const SDL_KeyboardEvent undo = {.key = SDLK_Z, .mod = SDL_KMOD_CTRL, .down = true};
    const SDL_KeyboardEvent redo = {.key = SDLK_Y, .mod = SDL_KMOD_CTRL, .down = true};
    editor->timeline_focus = false;
    for (uint32_t op = 0; op < 3U; ++op) {
        const uint32_t initial_count = editor->project.scene.count;
        const AfEditorToolRect a = {{center.x - 440 + (float)op * 300, center.y - 85}, {180, 150}};
        const AfEditorToolRect b = {{a.position.x + 60, a.position.y + 35}, {150, 140}};
        if (af_editor_create_primitive_at(editor, AF_NODE_RECTANGLE, a) != AF_RESULT_SUCCESS)
            return af_editor_interaction_smoke_failure(editor, __LINE__);
        const uint64_t primary = editor->selected_id;
        if (af_editor_create_primitive_at(editor, AF_NODE_ELLIPSE, b) != AF_RESULT_SUCCESS)
            return af_editor_interaction_smoke_failure(editor, __LINE__);
        const uint64_t other = editor->selected_id;
        af_editor_select_only(editor, other);
        af_editor_selection_add(editor, primary);
        const uint64_t started = SDL_GetPerformanceCounter();
        if (af_editor_action_callback(editor, (AfEditorActionId)actions[op]) != AF_RESULT_SUCCESS)
            return af_editor_interaction_smoke_failure(editor, __LINE__);
        af_log(AF_LOG_INFO, "Boolean command %u: %.3f ms", op,
            (double)(SDL_GetPerformanceCounter() - started) * 1000.0 / (double)SDL_GetPerformanceFrequency());
        const AfPath *path = af_editor_selected_path(editor);
        if (editor->project.scene.count != initial_count + 1U || editor->selected_id != primary ||
            path == NULL || path->fill_rule != AF_PATH_FILL_NON_ZERO)
            return af_editor_interaction_smoke_failure(editor, __LINE__);
        af_editor_key(editor, width, height, &undo);
        if (editor->project.scene.count != initial_count + 2U ||
            af_scene_find_const(&editor->project.scene, primary)->type != AF_NODE_RECTANGLE)
            return af_editor_interaction_smoke_failure(editor, __LINE__);
        af_editor_key(editor, width, height, &redo);
        if (editor->project.scene.count != initial_count + 1U ||
            af_project_validate(&editor->project) != AF_RESULT_SUCCESS)
            return af_editor_interaction_smoke_failure(editor, __LINE__);
        af_editor_select_only(editor, primary);
    }
    editor->active_tool = AF_EDITOR_TOOL_SELECT;
    af_editor_update_preview(editor);
    af_editor_status(editor, "Left to right: Weld, Trim Primary, Intersect · curves approximated; undo verified");
    return AF_RESULT_SUCCESS;
}

static AfResult af_editor_vector_package_smoke(AfEditorState *editor, int width, int height) {
#define VECTOR_CHECK(condition)                                                                    \
    do {                                                                                           \
        if (!(condition))                                                                          \
            return af_editor_interaction_smoke_failure(editor, __LINE__);                          \
    } while (0)
    const AfVec2 center = af_editor_visible_center(editor, width, height);
    af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_TEXT);
    editor->tool_start = (AfVec2){center.x, center.y};
    editor->tool_current = (AfVec2){center.x + 200, center.y + 80};
    editor->drag_mode = AF_EDITOR_DRAG_TOOL_CREATE;
    af_editor_finish_vector_tool_drag(editor, width, height);
    VECTOR_CHECK(editor->text_panel_open &&
                 editor->text_panel_draft.mode == AF_PROJECT_TEXT_PARAGRAPH);
    VECTOR_CHECK(strcmp(editor->text_panel_numbers[1], "200") == 0);
    af_editor_text_panel_append(editor, "Esc confirms");
    af_editor_text_panel_key(editor, &(SDL_KeyboardEvent){.key = SDLK_ESCAPE, .down = true});
    VECTOR_CHECK(!editor->text_panel_open && !SDL_TextInputActive(editor->window.window));
    VECTOR_CHECK(
        strcmp(editor->project.texts[af_project_find_index(&editor->project, editor->selected_id)]
                   .utf8,
               "Esc confirms") == 0);
    af_editor_open_text_panel(editor, width, height);
    af_editor_text_panel_append(editor, "Toolbar confirms");
    const float text_bar_width = (float)width - editor->left_width - editor->right_width - 12;
    af_editor_text_panel_mouse(editor, width, height, editor->left_width + 6 + text_bar_width * .9F,
                               editor->toolbar_height + 90);
    VECTOR_CHECK(!editor->text_panel_open);
    VECTOR_CHECK(
        strcmp(editor->project.texts[af_project_find_index(&editor->project, editor->selected_id)]
                   .utf8,
               "Toolbar confirms") == 0);
    VECTOR_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    VECTOR_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    af_editor_select_only(editor, 0);
    AfPathCommand line[] = {{.type = AF_PATH_MOVE, .point = {center.x - 350, center.y - 180}},
                            {.type = AF_PATH_CUBIC,
                             .point = {center.x - 240, center.y - 180},
                             .control1 = {center.x - 320, center.y - 240},
                             .control2 = {center.x - 270, center.y - 120}}};
    VECTOR_CHECK(af_editor_create_path_commands(editor, line, 2, false, "Continue") ==
                 AF_RESULT_SUCCESS);
    const uint64_t curve_id = editor->selected_id;
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand original[2];
    memcpy(original, editor->project.path_data.commands + path->command_start, sizeof(original));
    af_editor_path_selection_only(editor, 1);
    af_editor_continue_path(editor);
    VECTOR_CHECK(editor->tool_pen_resume_id == curve_id && editor->tool_command_count == 2);
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_BACKSPACE, .down = true});
    VECTOR_CHECK(editor->tool_pen_resume_id == 0 && path->command_count == 2);
    af_editor_continue_path(editor);
    editor->tool_commands[editor->tool_command_count++] =
        (AfPathCommand){.type = AF_PATH_LINE, .point = {center.x - 140, center.y - 180}};
    VECTOR_CHECK(af_editor_commit_pen_path(editor, false));
    VECTOR_CHECK(af_editor_selected_path(editor)->command_count == 3);
    VECTOR_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    path = af_editor_selected_path(editor);
    VECTOR_CHECK(memcmp(original, editor->project.path_data.commands + path->command_start,
                        sizeof(original)) == 0);
    VECTOR_CHECK(af_editor_history_redo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    af_editor_path_selection_only(editor, 0);
    af_editor_path_selection_add(editor, 2);
    af_editor_join_endpoints(editor);
    VECTOR_CHECK(af_editor_selected_path(editor)->command_count == 4);
    VECTOR_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    VECTOR_CHECK(af_editor_create_primitive_at(
                     editor, AF_NODE_RECTANGLE,
                     (AfEditorToolRect){{center.x - 350, center.y - 70}, {150, 100}}) ==
                 AF_RESULT_SUCCESS);
    AfStyle *style = af_project_find_node_style(&editor->project, editor->selected_id);
    style->stroke.width = style->stroke.end_width = 12;
    const uint32_t before = editor->project.scene.count;
    VECTOR_CHECK(af_editor_expand_outline(editor, false, 0));
    VECTOR_CHECK(editor->project.scene.count == before + 1);
    VECTOR_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    VECTOR_CHECK(af_editor_history_redo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    VECTOR_CHECK(af_editor_expand_outline(editor, true, 5));
    VECTOR_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    VECTOR_CHECK(af_editor_history_redo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    for (uint32_t mode = 1; mode <= 3; ++mode) {
        af_editor_select_only(editor, 0);
        af_editor_open_text_panel(editor, width, height);
        editor->text_panel_position =
            (AfVec2){center.x - 100, center.y - 200 + (float)(mode - 1) * 125};
        editor->text_panel_draft.mode = (AfProjectTextMode)mode;
        editor->text_panel_draft.font_family = mode == 1 ? 1 : 0;
        editor->text_panel_draft.bold = mode == 1;
        editor->text_panel_draft.italic = mode == 3;
        af_editor_text_panel_append(editor, mode == 1   ? "Artistic text"
                                            : mode == 2 ? "Paragraph text wraps into a frame."
                                                        : "Text on a curve");
        (void)snprintf(editor->text_panel_numbers[0], 32, "28");
        (void)snprintf(editor->text_panel_numbers[1], 32, "280");
        (void)snprintf(editor->text_panel_numbers[3], 32, "0.5");
        (void)snprintf(editor->text_panel_numbers[4], 32, "1.4");
        (void)snprintf(editor->text_panel_numbers[5], 32, "#FFCC66FF");
        if (mode == 3) {
            editor->text_panel_draft.guide_count = 2;
            editor->text_panel_draft.guide[0] =
                (AfPathCommand){.type = AF_PATH_MOVE, .point = {0, 70}};
            editor->text_panel_draft.guide[1] = (AfPathCommand){.type = AF_PATH_CUBIC,
                                                                .point = {400, 70},
                                                                .control1 = {130, -50},
                                                                .control2 = {270, -50}};
        }
        af_editor_apply_text_panel(editor);
        VECTOR_CHECK(!editor->text_panel_open && af_editor_is_text(editor, editor->selected_id));
        const AfStyle *text_style =
            af_project_find_node_style(&editor->project, editor->selected_id);
        VECTOR_CHECK(text_style->fill.solid.r == 1 &&
                     fabsf(text_style->fill.solid.g - .8F) < .001F);
        const AfProjectText *authored =
            &editor->project.texts[af_project_find_index(&editor->project, editor->selected_id)];
        VECTOR_CHECK(authored->bold == (mode == 1) && authored->italic == (mode == 3) &&
                     authored->letter_spacing == .5F);
        VECTOR_CHECK(af_editor_history_undo(&editor->history, &editor->project) ==
                     AF_RESULT_SUCCESS);
        VECTOR_CHECK(af_editor_history_redo(&editor->history, &editor->project) ==
                     AF_RESULT_SUCCESS);
        af_editor_open_text_panel(editor, width, height);
        VECTOR_CHECK(editor->text_panel_id == editor->selected_id);
        const AfProjectText before_confirm = *authored;
        af_editor_text_panel_append(editor, "Confirmed edit");
        af_editor_text_focus(editor, 7, true);
        af_editor_text_panel_key(editor, &(SDL_KeyboardEvent){.key = SDLK_ESCAPE, .down = true});
        VECTOR_CHECK(!editor->text_panel_open);
        VECTOR_CHECK(strcmp(editor->project
                                .texts[af_project_find_index(&editor->project, editor->selected_id)]
                                .utf8,
                            "Confirmed edit") == 0);
        VECTOR_CHECK(af_editor_history_undo(&editor->history, &editor->project) ==
                     AF_RESULT_SUCCESS);
        VECTOR_CHECK(memcmp(&editor->project.texts[af_project_find_index(&editor->project,
                                                                         editor->selected_id)],
                            &before_confirm, sizeof(before_confirm)) == 0);
    }
    /* Exercise actual SDL text events, UTF-8 insertion, caret selection and font lookup. */
    af_editor_open_text_panel(editor, width, height);
    VECTOR_CHECK(SDL_TextInputActive(editor->window.window));
    af_editor_text_panel_append(editor, "AéB");
    af_editor_text_panel_key(editor, &(SDL_KeyboardEvent){.key = SDLK_LEFT, .down = true});
    af_editor_text_panel_key(editor, &(SDL_KeyboardEvent){.key = SDLK_BACKSPACE, .down = true});
    VECTOR_CHECK(strcmp(editor->text_panel_draft.utf8, "AB") == 0);
    SDL_Event typed = {.text = {.type = SDL_EVENT_TEXT_INPUT, .text = "é"}};
    af_editor_handle_event(editor, &typed, width, height);
    VECTOR_CHECK(strcmp(editor->text_panel_draft.utf8, "AéB") == 0);
    af_editor_text_panel_key(
        editor, &(SDL_KeyboardEvent){.key = SDLK_LEFT, .mod = SDL_KMOD_SHIFT, .down = true});
    af_editor_text_panel_append(editor, "x");
    VECTOR_CHECK(strcmp(editor->text_panel_draft.utf8, "AxB") == 0);
    af_editor_text_focus(editor, 7, true);
    af_editor_text_panel_append(editor, "Liberation Sans");
    af_editor_text_panel_key(editor, &(SDL_KeyboardEvent){.key = SDLK_RETURN, .down = true});
    VECTOR_CHECK(strcmp(editor->text_panel_draft.font_name, "Liberation Sans") == 0);
    af_editor_text_preview_update(editor);
    VECTOR_CHECK(editor->text_preview_valid && editor->system_font_engine);
    VECTOR_CHECK(editor->text_caret_tops[0].y < editor->text_carets[0].y);
    af_editor_text_focus(editor, 7, true);
    af_editor_text_panel_key(
        editor, &(SDL_KeyboardEvent){.key = SDLK_RETURN, .mod = SDL_KMOD_CTRL, .down = true});
    VECTOR_CHECK(!editor->text_panel_open && !SDL_TextInputActive(editor->window.window));
    uint32_t text_index = af_project_find_index(&editor->project, editor->selected_id);
    VECTOR_CHECK(strcmp(editor->project.texts[text_index].font_name, "Liberation Sans") == 0);
    VECTOR_CHECK(strcmp(editor->project.texts[text_index].utf8, "AxB") == 0);
    VECTOR_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    VECTOR_CHECK(strcmp(editor->project.texts[text_index].utf8, "Text on a curve") == 0);
    VECTOR_CHECK(af_editor_history_redo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    af_editor_open_text_panel(editor, width, height);
    (void)snprintf(editor->text_panel_draft.font_name, sizeof(editor->text_panel_draft.font_name),
                   "Arcflow Missing Font Test");
    VECTOR_CHECK(af_editor_document_font(editor, &editor->text_panel_draft) == 0);
    af_editor_text_panel_key(editor, &(SDL_KeyboardEvent){.key = SDLK_ESCAPE, .down = true});
    VECTOR_CHECK(editor->text_panel_open && SDL_TextInputActive(editor->window.window));
    VECTOR_CHECK(strcmp(editor->project.texts[text_index].font_name, "Liberation Sans") == 0);
    af_editor_text_close(editor);
    /* Gradient changes, color edits and geometry dragging remain one undoable transaction. */
    af_editor_gradient_action(editor, 0);
    style = af_project_find_node_style(&editor->project, editor->selected_id);
    VECTOR_CHECK(style->fill.type == AF_PAINT_LINEAR_GRADIENT && style->fill.stop_count == 2);
    af_editor_gradient_action(editor, 3);
    VECTOR_CHECK(style->fill.stop_count == 3);
    VECTOR_CHECK(af_editor_apply_inspector_color(editor, (AfColor){.2F, .4F, .8F, .6F}, false));
    VECTOR_CHECK(style->fill.type == AF_PAINT_LINEAR_GRADIENT &&
                 fabsf(style->fill.stops[1].color.a - .6F) < .001F);
    af_editor_gradient_action(editor, 7);
    VECTOR_CHECK(fabsf(style->fill.stops[1].offset - .55F) < .001F);
    af_editor_update_preview(editor);
    AfAffine2 gradient_frame;
    AfVec2 gradient_start, gradient_end;
    VECTOR_CHECK(af_editor_gradient_geometry(editor, width, height, &gradient_frame,
                                             &gradient_start, &gradient_end));
    const AfVec2 moved_end = {gradient_end.x + 30, gradient_end.y + 10};
    const AfVec2 screen_end = af_affine_transform_point(gradient_frame, moved_end);
    AfPaint before_drag = style->fill;
    const AfVec2 original_screen_end = af_affine_transform_point(gradient_frame, gradient_end);
    VECTOR_CHECK(af_editor_gradient_begin_drag(editor, width, height, AF_EDITOR_GRADIENT_DRAG_END,
                                               original_screen_end.x, original_screen_end.y));
    af_editor_gradient_motion(editor, width, height, screen_end.x, screen_end.y);
    af_editor_mouse_up(editor, width, height);
    VECTOR_CHECK(fabsf(style->fill.end.x - moved_end.x) < .001F &&
                 fabsf(style->fill.end.y - moved_end.y) < .001F);
    VECTOR_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    VECTOR_CHECK(memcmp(&before_drag, &style->fill, sizeof(AfPaint)) == 0);
    VECTOR_CHECK(af_editor_gradient_begin_drag(editor, width, height, AF_EDITOR_GRADIENT_DRAG_END,
                                               original_screen_end.x, original_screen_end.y));
    af_editor_gradient_motion(editor, width, height, screen_end.x, screen_end.y);
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_ESCAPE, .down = true});
    VECTOR_CHECK(memcmp(&before_drag, &style->fill, sizeof(AfPaint)) == 0 &&
                 !editor->gradient_drag);
    AfPaint original_paint = style->fill;
    af_editor_gradient_action(editor, 5);
    VECTOR_CHECK(fabsf(style->fill.stops[1].offset - .45F) < .001F);
    VECTOR_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    VECTOR_CHECK(memcmp(&style->fill, &original_paint, sizeof(AfPaint)) == 0);
    af_editor_open_text_panel(editor, width, height);
    af_editor_text_panel_append(editor, "Gradient text");
    af_editor_text_preview_update(editor);
    VECTOR_CHECK(editor->text_preview_valid);
    VECTOR_CHECK(editor->text_preview->after[0].style.fill.type == AF_PAINT_LINEAR_GRADIENT);
    af_editor_apply_text_panel(editor);
    style = af_project_find_node_style(&editor->project, editor->selected_id);
    VECTOR_CHECK(style->fill.type == AF_PAINT_LINEAR_GRADIENT);
    af_editor_gradient_action(editor, 0);
    VECTOR_CHECK(style->fill.type == AF_PAINT_RADIAL_GRADIENT && style->fill.stop_count == 3);
    af_editor_gradient_action(editor, 0);
    VECTOR_CHECK(style->fill.type == AF_PAINT_CONIC_GRADIENT && style->fill.stop_count == 3);
    af_editor_gradient_action(editor, 4);
    VECTOR_CHECK(style->fill.stop_count == 2);
    VECTOR_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    /* Exercise stop selection through mouse events and the actual inspector editor. */
    const uint32_t object_count = editor->project.scene.count;
    const float bar_top = editor->toolbar_height + 36;
    const float ramp_left = editor->left_width + 16;
    const float ramp_width = (float)width - editor->left_width - editor->right_width - 32;
    SDL_MouseButtonEvent stop_click = {.button = SDL_BUTTON_LEFT,
                                       .down = true,
                                       .clicks = 2,
                                       .x = ramp_left + ramp_width * .25F,
                                       .y = bar_top + 82};
    af_editor_mouse_down(editor, width, height, &stop_click, SDL_KMOD_NONE);
    VECTOR_CHECK(style->fill.stop_count == 4 && editor->gradient_stop_focus);
    VECTOR_CHECK(fabsf(style->fill.stops[editor->gradient_stop].offset - .25F) < .001F);
    af_editor_mouse_down(editor, width, height, &stop_click, SDL_KMOD_NONE);
    VECTOR_CHECK(style->fill.stop_count == 4); /* Double-clicking a stop selects it. */
    stop_click.clicks = 1;
    af_editor_mouse_down(editor, width, height, &stop_click, SDL_KMOD_NONE);
    af_editor_mouse_up(editor, width, height);
    af_editor_begin_inspector_edit(editor, AF_EDITOR_INSPECTOR_COLOR_HEX);
    af_editor_append_inspector_text(editor, "#33CC66FF");
    VECTOR_CHECK(af_editor_finish_inspector_edit(editor, true));
    VECTOR_CHECK(style->fill.type == AF_PAINT_CONIC_GRADIENT &&
                 fabsf(style->fill.stops[editor->gradient_stop].color.g - .8F) < .001F);
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_DELETE, .down = true});
    VECTOR_CHECK(style->fill.stop_count == 3 && editor->project.scene.count == object_count);
    VECTOR_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    VECTOR_CHECK(style->fill.stop_count == 4);
    for (uint32_t i = 0; i < 3; ++i)
        af_editor_key(editor, width, height,
                      &(SDL_KeyboardEvent){.key = SDLK_DELETE, .down = true});
    VECTOR_CHECK(style->fill.stop_count == 2 && editor->project.scene.count == object_count);
    VECTOR_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    VECTOR_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    VECTOR_CHECK(style->fill.stop_count == 4);
    /* The fill and timeline labels open menus; selecting an item does not cycle. */
    SDL_MouseButtonEvent choice_click = {.button = SDL_BUTTON_LEFT,
                                         .down = true,
                                         .clicks = 1,
                                         .x = editor->left_width + 20,
                                         .y = bar_top + 15};
    af_editor_mouse_down(editor, width, height, &choice_click, SDL_KMOD_NONE);
    VECTOR_CHECK(editor->choice_menu == 1);
    choice_click.x = editor->choice_origin.x + 20;
    choice_click.y = editor->choice_origin.y + 28 * 2 + 14;
    af_editor_mouse_down(editor, width, height, &choice_click, SDL_KMOD_NONE);
    VECTOR_CHECK(!editor->choice_menu && style->fill.type == AF_PAINT_RADIAL_GRADIENT &&
                 style->fill.stop_count == 4);
    choice_click.x = editor->left_width + 280 * af_editor_timeline_controls_scale(editor, width);
    choice_click.y = af_editor_timeline_top(editor, height) + 20;
    af_editor_mouse_down(editor, width, height, &choice_click, SDL_KMOD_NONE);
    VECTOR_CHECK(editor->choice_menu == 2);
    choice_click.x = editor->choice_origin.x + 20;
    choice_click.y = editor->choice_origin.y + 28 * AF_PROPERTY_ROTATION + 14;
    af_editor_mouse_down(editor, width, height, &choice_click, SDL_KMOD_NONE);
    VECTOR_CHECK(!editor->choice_menu && editor->selected_property == AF_PROPERTY_ROTATION);
    af_editor_choice_open(editor, 2, width, height, editor->left_width + 250,
                          af_editor_timeline_top(editor, height) + 42);
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_DOWN, .down = true});
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_RETURN, .down = true});
    VECTOR_CHECK(editor->selected_property == AF_PROPERTY_SCALE && !editor->choice_menu);
    af_editor_choice_open(editor, 2, width, height, editor->left_width + 250,
                          af_editor_timeline_top(editor, height) + 42);
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_DOWN, .down = true});
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_ESCAPE, .down = true});
    VECTOR_CHECK(editor->selected_property == AF_PROPERTY_SCALE && !editor->choice_menu);
    /* Inspector fill choices apply to the entire selection as one transaction. */
    const uint64_t primary = editor->selected_id;
    const uint64_t secondary = editor->project.scene.nodes[0].id;
    VECTOR_CHECK(primary != secondary);
    const AfPaint primary_paint = style->fill;
    const AfPaint secondary_paint = af_project_find_node_style(&editor->project, secondary)->fill;
    af_editor_selection_add(editor, secondary);
    af_editor_gradient_set_type(editor, AF_PAINT_LINEAR_GRADIENT);
    VECTOR_CHECK(style->fill.type == AF_PAINT_LINEAR_GRADIENT &&
                 af_project_find_node_style(&editor->project, secondary)->fill.type ==
                     AF_PAINT_LINEAR_GRADIENT);
    VECTOR_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    VECTOR_CHECK(memcmp(&primary_paint, &style->fill, sizeof(AfPaint)) == 0 &&
                 memcmp(&secondary_paint,
                        &af_project_find_node_style(&editor->project, secondary)->fill,
                        sizeof(AfPaint)) == 0);
    af_editor_select_only(editor, primary);
    editor->timeline_focus = false;
    VECTOR_CHECK(af_project_validate(&editor->project) == AF_RESULT_SUCCESS);
    af_editor_update_preview(editor);
    af_editor_status(editor, "Verified: join / continue, outline, offset, artistic / paragraph / "
                             "path text, undo / redo");
    if (editor->text_panel_smoke)
        af_editor_open_text_panel(editor, width, height);
#undef VECTOR_CHECK
    return AF_RESULT_SUCCESS;
}

static AfResult af_editor_compound_smoke(AfEditorState *editor, int width, int height) {
    const uint32_t initial_count = editor->project.scene.count;
    const AfVec2 center = af_editor_visible_center(editor, width, height);
    const AfEditorToolRect outer = {{center.x - 150, center.y - 120}, {300, 240}};
    const AfEditorToolRect inner = {{center.x - 70, center.y - 60}, {140, 120}};
    if (af_editor_create_primitive_at(editor, AF_NODE_ROUNDED_RECTANGLE, outer) != AF_RESULT_SUCCESS)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    const uint64_t outer_id = editor->selected_id;
    if (af_editor_create_primitive_at(editor, AF_NODE_ELLIPSE, inner) != AF_RESULT_SUCCESS)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    const uint64_t inner_id = editor->selected_id;
    af_editor_select_only(editor, outer_id);
    af_editor_selection_add(editor, inner_id);
    editor->timeline_focus = false;
    const SDL_KeyboardEvent combine = {.key = SDLK_L, .mod = SDL_KMOD_CTRL, .down = true};
    const SDL_KeyboardEvent undo = {.key = SDLK_Z, .mod = SDL_KMOD_CTRL, .down = true};
    const SDL_KeyboardEvent redo = {.key = SDLK_Y, .mod = SDL_KMOD_CTRL, .down = true};
    const uint32_t cursor = editor->history.cursor;
    editor->tool_command_count = 1U;
    af_editor_key(editor, width, height, &combine);
    if (editor->history.cursor != cursor || editor->tool_command_count != 1U ||
        editor->project.scene.count != initial_count + 2U)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    editor->tool_command_count = 0U;
    af_editor_key(editor, width, height, &combine);
    const AfPath *path = af_editor_selected_path(editor);
    if (editor->project.scene.count != initial_count + 1U || editor->selected_id != inner_id ||
        path == NULL || path->fill_rule != AF_PATH_FILL_EVEN_ODD)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    af_editor_key(editor, width, height, &undo);
    if (editor->project.scene.count != initial_count + 2U ||
        af_scene_find_const(&editor->project.scene, outer_id) == NULL)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    af_editor_key(editor, width, height, &redo);
    af_editor_select_only(editor, inner_id);
    af_editor_execute_menu_action(editor, AF_EDITOR_MENU_ACTION_BREAK_APART, width, height);
    if (editor->selected_count != 2U || editor->project.scene.count != initial_count + 2U)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    af_editor_key(editor, width, height, &undo);
    af_editor_key(editor, width, height, &redo);
    if (editor->project.scene.count != initial_count + 2U ||
        af_project_validate(&editor->project) != AF_RESULT_SUCCESS)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    af_editor_key(editor, width, height, &undo);
    af_editor_select_only(editor, inner_id);
    af_editor_update_preview(editor);
    af_editor_status(editor, "Combine / Break Apart: curves, selection, menu and undo verified");
    return AF_RESULT_SUCCESS;
}

static AfResult af_editor_pen_close_smoke(AfEditorState *editor, int width, int height) {
    const bool snapping = editor->snapping;
    editor->snapping = false;
    float x, y, w, h;
    af_editor_layout(editor, width, height, &x, &y, &w, &h);
    for (uint32_t mode = 0U; mode < 5U; ++mode) {
        const uint32_t anchors = mode == 2U ? 2U : 3U;
        const bool first_handle = mode == 1U || mode == 2U || mode == 3U;
        const bool last_handle = mode == 1U || mode == 2U || mode == 4U;
        const uint32_t scene_count = editor->project.scene.count;
        af_editor_cancel_vector_tool(editor, NULL);
        af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_PEN);
        SDL_MouseButtonEvent click = {.button = SDL_BUTTON_LEFT, .down = true, .clicks = 1U};
        for (uint32_t anchor = 0U; anchor < anchors; ++anchor) {
            click.x = x + w * (anchor == 0U ? 0.3F : 0.65F);
            click.y = y + h * (anchor == 2U ? 0.7F : 0.35F);
            af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
            if ((anchor == 0U && first_handle) || (anchor + 1U == anchors && last_handle)) {
                const SDL_MouseMotionEvent motion = {.x = click.x + 20.0F,
                                                      .y = click.y - 24.0F};
                af_editor_mouse_motion(editor, width, height, &motion);
            }
            af_editor_mouse_up(editor, width, height);
        }
        if (editor->tool_command_count != anchors)
            return af_editor_interaction_smoke_failure(editor, __LINE__);
        const AfVec2 start = editor->tool_commands[0].point;
        const AfVec2 last = editor->tool_commands[anchors - 1U].point;
        const AfVec2 outgoing = editor->tool_pen_outgoing;
        const AfVec2 incoming = first_handle
            ? (AfVec2){2.0F * start.x - editor->tool_commands[1].control1.x,
                       2.0F * start.y - editor->tool_commands[1].control1.y} : start;
        const AfVec2 first_screen = af_affine_transform_point(
            af_editor_camera(editor, width, height), start);
        click.x = first_screen.x;
        click.y = first_screen.y;
        af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
        af_editor_mouse_up(editor, width, height);
        const uint64_t node_id = editor->selected_id;
        const AfPath *path = af_editor_selected_path(editor);
        if (editor->tool_command_count != 0U || editor->drag_mode != AF_EDITOR_DRAG_NONE ||
            editor->project.scene.count != scene_count + 1U ||
            path == NULL ||
            path->command_count != anchors + (mode == 0U ? 1U : 2U)) {
            af_log(AF_LOG_ERROR, "Pen close case %u: pending=%u drag=%u nodes=%u/%u commands=%u",
                   mode, editor->tool_command_count, (unsigned)editor->drag_mode,
                   editor->project.scene.count, scene_count + 1U,
                   path == NULL ? 0U : path->command_count);
            return af_editor_interaction_smoke_failure(editor, __LINE__);
        }
        const AfPathCommand *commands = &editor->project.path_data.commands[path->command_start];
        const AfVec2 offset = {start.x - commands[0].point.x, start.y - commands[0].point.y};
        if (commands[path->command_count - 1U].type != AF_PATH_CLOSE)
            return af_editor_interaction_smoke_failure(editor, __LINE__);
        if (mode != 0U) {
            const AfPathCommand *closing = &commands[path->command_count - 2U];
            const AfVec2 expected_out = last_handle ? outgoing : last;
            if (closing->type != AF_PATH_CUBIC ||
                hypotf(closing->point.x - commands[0].point.x,
                       closing->point.y - commands[0].point.y) > 0.001F ||
                hypotf(closing->control1.x + offset.x - expected_out.x,
                       closing->control1.y + offset.y - expected_out.y) > 0.001F ||
                hypotf(closing->control2.x + offset.x - incoming.x,
                       closing->control2.y + offset.y - incoming.y) > 0.001F)
                return af_editor_interaction_smoke_failure(editor, __LINE__);
        }
        af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_NODE);
        af_editor_update_preview(editor);
        af_editor_prepare_render_paths(editor);
        editor->render_scene.count = 0U;
        af_editor_render_path_points(editor, af_editor_camera(editor, width, height));
        uint32_t connections = 0U;
        for (uint32_t index = 0U; index < editor->render_scene.count; ++index) {
            const AfSceneNode *node = &editor->render_scene.nodes[index];
            if (node->layer_order == 90025 && node->bounds.x > 0.01F)
                ++connections;
        }
        if (connections < anchors ||
            af_editor_history_undo(&editor->history, &editor->project) != AF_RESULT_SUCCESS ||
            af_scene_find(&editor->project.scene, node_id) != NULL ||
            af_editor_history_redo(&editor->history, &editor->project) != AF_RESULT_SUCCESS ||
            af_path_find_node_const(&editor->project.path_data, node_id) == NULL)
            return af_editor_interaction_smoke_failure(editor, __LINE__);
        if (mode != 4U &&
            af_editor_history_undo(&editor->history, &editor->project) != AF_RESULT_SUCCESS)
            return af_editor_interaction_smoke_failure(editor, __LINE__);
    }
    /* A full command buffer cannot accept CLOSE: keep all input available for correction. */
    const uint32_t scene_count = editor->project.scene.count;
    af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_PEN);
    editor->tool_command_count = AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY;
    for (uint32_t index = 0U; index < editor->tool_command_count; ++index) {
        editor->tool_commands[index] = (AfPathCommand){
            .type = index == 0U ? AF_PATH_MOVE : AF_PATH_LINE,
            .point = {(float)index, (float)(index % 3U)}};
    }
    if (af_editor_commit_pen_path(editor, true) ||
        editor->tool_command_count != AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY ||
        editor->project.scene.count != scene_count)
        return af_editor_interaction_smoke_failure(editor, __LINE__);
    af_editor_cancel_vector_tool(editor, NULL);
    af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_NODE);
    af_editor_update_preview(editor);
    editor->snapping = snapping;
    return af_project_validate(&editor->project);
}

static AfResult af_editor_vector_preview_smoke(AfEditorState *editor, int width, int height) {
    const uint32_t stage = editor->rendered_frames;
    const uint32_t scene_count = editor->project.scene.count;
    const uint32_t path_count = editor->project.path_data.path_count;
    const uint32_t history_count = editor->history.count;
    const uint32_t render_count = editor->render_scene.count;
    const uint32_t capacity = editor->render_scene.capacity;
    SDL_MouseButtonEvent click = {.button = SDL_BUTTON_LEFT,
                                  .down = true,
                                  .clicks = 1U,
                                  .x = editor->left_width + 300.0F,
                                  .y = editor->toolbar_height + 90.0F};
    SDL_MouseMotionEvent motion = {.x = click.x + 220.0F, .y = click.y + 170.0F};
    if (stage >= 9U) {
        return AF_RESULT_SUCCESS;
    }
    if (stage < 4U) {
        static const AfEditorVectorTool tools[] = {AF_EDITOR_TOOL_STAR, AF_EDITOR_TOOL_POLYGON,
                                                   AF_EDITOR_TOOL_ARC, AF_EDITOR_TOOL_SPIRAL};
        af_editor_cancel_vector_tool(editor, NULL);
        af_editor_set_vector_tool(editor, tools[stage]);
        af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
        af_editor_mouse_motion(editor, width, height, &motion);
    } else if (stage == 4U) {
        af_editor_cancel_vector_tool(editor, NULL);
        af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_PEN);
        af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
        af_editor_mouse_up(editor, width, height);
        click.x += 200.0F;
        click.y += 110.0F;
        af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
        motion.x = click.x + 50.0F;
        motion.y = click.y - 65.0F;
        af_editor_mouse_motion(editor, width, height, &motion);
    } else if (stage == 5U) {
        af_editor_mouse_up(editor, width, height);
        motion.x = click.x + 300.0F;
        motion.y = click.y + 180.0F;
        af_editor_mouse_motion(editor, width, height, &motion);
    } else if (stage == 6U) {
        click.x += 300.0F;
        click.y += 180.0F;
        af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
        af_editor_mouse_up(editor, width, height);
        const AfVec2 first = af_affine_transform_point(af_editor_camera(editor, width, height),
                                                       editor->tool_commands[0].point);
        motion.x = first.x + 3.0F;
        motion.y = first.y + 2.0F;
        af_editor_mouse_motion(editor, width, height, &motion);
        if (!af_editor_pen_closes_at(editor, af_editor_camera(editor, width, height),
                                     editor->mouse_position)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    } else if (stage == 7U) {
        const AfVec2 start = editor->tool_commands[0].point;
        // Exercise the maximum supported Pen length, including the final live segment.
        editor->tool_command_count = AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY;
        for (uint32_t index = 1U; index < editor->tool_command_count; ++index) {
            const AfVec2 previous = editor->tool_commands[index - 1U].point;
            const AfVec2 point = {start.x + (float)index * 0.8F,
                                  start.y + 110.0F + sinf((float)index * 0.025F) * 70.0F};
            editor->tool_commands[index] =
                (AfPathCommand){.type = AF_PATH_CUBIC,
                                .point = point,
                                .control1 = {previous.x + 0.25F, previous.y - 8.0F},
                                .control2 = {point.x - 0.25F, point.y + 8.0F}};
        }
        editor->tool_pen_has_outgoing = false;
        editor->drag_mode = AF_EDITOR_DRAG_TOOL_CREATE;
    } else {
        const SDL_KeyboardEvent escape = {.key = SDLK_ESCAPE, .down = true};
        af_editor_key(editor, width, height, &escape);
    }
    editor->render_scene.count = 0U;
    af_editor_render_vector_tool_preview(editor, width, height);
    const uint32_t preview_count = editor->render_scene.count;
    const bool valid = editor->render_scene.capacity == capacity &&
                       preview_count < capacity - AF_EDITOR_UI_RESERVE &&
                       (stage == 8U ? (preview_count == 0U && editor->tool_command_count == 0U &&
                                       editor->drag_mode == AF_EDITOR_DRAG_NONE)
                                    : preview_count >= 10U) &&
                       editor->project.scene.count == scene_count &&
                       editor->project.path_data.path_count == path_count &&
                       editor->history.count == history_count;
    editor->render_scene.count = render_count;
    if (!valid) {
        af_log(AF_LOG_ERROR, "Vector preview smoke stage %u failed (%u preview nodes)", stage,
               preview_count);
    }
    af_editor_status(editor, stage == 8U
                                 ? "Vector preview smoke: previews and cancellation accepted"
                                 : "Vector preview smoke: live construction in progress");
    return valid ? AF_RESULT_SUCCESS : AF_RESULT_INVALID_ARGUMENT;
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
        } else if (strcmp(arguments[index], "--outline-smoke") == 0) {
            editor->outline_smoke = true;
        } else if (strcmp(arguments[index], "--inspector-smoke") == 0) {
            editor->inspector_smoke = true;
        } else if (strcmp(arguments[index], "--studio-smoke") == 0) {
            editor->studio_smoke = true;
            if (editor->maximum_frames == 0U)
                editor->maximum_frames = 4U;
        } else if (strcmp(arguments[index], "--template-smoke") == 0) {
            editor->template_smoke = true;
            if (editor->maximum_frames == 0U)
                editor->maximum_frames = 2U;
        } else if (strcmp(arguments[index], "--vector-tools-smoke") == 0) {
            editor->vector_tools_smoke = true;
        } else if (strcmp(arguments[index], "--pan-smoke") == 0) {
            editor->pan_smoke = true;
            editor->preferences_read_only = true;
            if (editor->maximum_frames == 0U)
                editor->maximum_frames = 180U;
        } else if (strcmp(arguments[index], "--vector-preview-smoke") == 0) {
            editor->vector_preview_smoke = true;
        } else if (strcmp(arguments[index], "--pen-close-smoke") == 0) {
            editor->pen_close_smoke = true;
            if (editor->maximum_frames == 0U)
                editor->maximum_frames = 4U;
        } else if (strcmp(arguments[index], "--compound-smoke") == 0) {
            editor->compound_smoke = true;
            if (editor->maximum_frames == 0U)
                editor->maximum_frames = 4U;
        } else if (strcmp(arguments[index], "--text-panel-smoke") == 0) {
            editor->text_panel_smoke = editor->vector_package_smoke = true;
            if (editor->maximum_frames == 0U)
                editor->maximum_frames = 4U;
        } else if (strcmp(arguments[index], "--vector-package-smoke") == 0) {
            editor->vector_package_smoke = true;
            if (editor->maximum_frames == 0U)
                editor->maximum_frames = 4U;
        } else if (strcmp(arguments[index], "--gradient-drag-smoke") == 0) {
            editor->gradient_drag_smoke = true;
            if (editor->maximum_frames == 0U)
                editor->maximum_frames = 4U;
        } else if (strcmp(arguments[index], "--boolean-smoke") == 0) {
            editor->boolean_smoke = true;
            if (editor->maximum_frames == 0U)
                editor->maximum_frames = 4U;
        } else if (strcmp(arguments[index], "--vfx-smoke") == 0) {
            editor->vfx_smoke = true;
        } else if (strcmp(arguments[index], "--particle-view-smoke") == 0) {
            editor->particle_view_smoke = true;
        } else if (strcmp(arguments[index], "--vector-animation-smoke") == 0) {
            editor->vector_animation_smoke = true;
        } else if (strcmp(arguments[index], "--image-smoke") == 0) {
            editor->image_smoke = true;
        } else if (strcmp(arguments[index], "--export-smoke") == 0) {
            editor->export_smoke = true;
        } else if (strcmp(arguments[index], "--snapping-smoke") == 0) {
            editor->snapping_smoke = true;
        } else if (strcmp(arguments[index], "--menu-smoke") == 0) {
            editor->menu_smoke = true;
        } else if (strcmp(arguments[index], "--workflow-smoke") == 0) {
            editor->workflow_smoke = true;
        } else if (strcmp(arguments[index], "--platform-smoke") == 0) {
            editor->platform_smoke = true;
        } else if (strcmp(arguments[index], "--release-smoke") == 0) {
            editor->release_smoke = true;
        } else if (strcmp(arguments[index], "--onboarding-smoke") == 0) {
            editor->onboarding_smoke = true;
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
        editor->maximum_frames = editor->particle_view_smoke      ? 24U
                                 : editor->vector_animation_smoke ? 18U
                                 : editor->image_smoke            ? 16U
                                 : editor->workflow_smoke         ? 18U
                                 : editor->platform_smoke         ? 4U
                                 : editor->release_smoke          ? 4U
                                 : editor->onboarding_smoke       ? 18U
                                 : editor->menu_smoke             ? 12U
                                 : editor->tooltip_smoke          ? 2U
                                 : editor->inspector_smoke        ? 8U
                                 : editor->vector_tools_smoke     ? 8U
                                 : editor->vector_preview_smoke   ? 9U
                                                                  : 1U;
    } else if (editor->sequence_prefix[0] != '\0' && editor->maximum_frames == 0U) {
        editor->maximum_frames = editor->particle_view_smoke      ? 24U
                                 : editor->vector_animation_smoke ? 18U
                                 : editor->image_smoke            ? 16U
                                 : editor->workflow_smoke         ? 18U
                                 : editor->platform_smoke         ? 4U
                                 : editor->release_smoke          ? 4U
                                 : editor->onboarding_smoke       ? 18U
                                 : editor->menu_smoke             ? 12U
                                 : editor->vfx_smoke              ? AF_EDITOR_PARTICLE_PAGE_COUNT
                                 : editor->vector_preview_smoke   ? 9U
                                                                  : 60U;
    }
    editor->preferences_read_only =
        editor->output_path[0] != '\0' || editor->sequence_prefix[0] != '\0' ||
        editor->resize_smoke || editor->context_smoke || editor->tooltip_smoke ||
        editor->selection_smoke || editor->outline_smoke || editor->vector_tools_smoke ||
        editor->vector_preview_smoke || editor->pen_close_smoke || editor->compound_smoke ||
        editor->boolean_smoke || editor->vector_package_smoke || editor->gradient_drag_smoke ||
        editor->vfx_smoke || editor->particle_view_smoke || editor->vector_animation_smoke ||
        editor->image_smoke || editor->export_smoke || editor->snapping_smoke ||
        editor->menu_smoke || editor->studio_smoke || editor->template_smoke || editor->workflow_smoke ||
        editor->platform_smoke || editor->release_smoke || editor->onboarding_smoke;
    af_editor_derive_package_path(editor);
}
