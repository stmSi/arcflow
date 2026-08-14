static AfResult af_editor_commit_modify(AfEditorState *editor, const AfProjectNodeSnapshot *before,
                                        AfEditorMergeKey merge_key, bool merge) {
    AfProjectNodeSnapshot after = {0};
    AfEditorCommand command = {0};
    AfResult result = af_project_snapshot(&editor->project, before->node.id, &after);
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_command_modify(before, &after, merge_key, &command);
    }
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, true, merge);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_editor_mark_dirty(editor);
    }
    return result;
}

static void af_editor_clear_key_selection(AfEditorState *editor) {
    editor->selected_keyframe_count = 0U;
}

static void af_editor_select_keyframe(AfEditorState *editor, uint64_t id, bool toggle) {
    uint32_t index = 0U;
    for (index = 0U; index < editor->selected_keyframe_count; ++index) {
        if (editor->selected_keyframes[index] == id) {
            if (toggle) {
                memmove(&editor->selected_keyframes[index], &editor->selected_keyframes[index + 1U],
                        (size_t)(editor->selected_keyframe_count - index - 1U) *
                            sizeof(editor->selected_keyframes[0]));
                --editor->selected_keyframe_count;
            }
            return;
        }
    }
    if (!toggle) {
        editor->selected_keyframe_count = 0U;
    }
    if (editor->selected_keyframe_count < AF_EDITOR_COMMAND_MAX_KEYFRAMES) {
        editor->selected_keyframes[editor->selected_keyframe_count++] = id;
    }
}

static bool af_editor_property_value(const AfEditorState *editor, const AfSceneNode *node,
                                     AfPropertyType property, float value[4]) {
    switch (property) {
    case AF_PROPERTY_POSITION:
        value[0] = node->local_transform.position.x;
        value[1] = node->local_transform.position.y;
        break;
    case AF_PROPERTY_ROTATION:
        value[0] = node->local_transform.rotation;
        break;
    case AF_PROPERTY_SCALE:
        value[0] = node->local_transform.scale.x;
        value[1] = node->local_transform.scale.y;
        break;
    case AF_PROPERTY_OPACITY:
        value[0] = node->opacity;
        break;
    case AF_PROPERTY_FILL_COLOR:
        value[0] = node->fill.r;
        value[1] = node->fill.g;
        value[2] = node->fill.b;
        value[3] = node->fill.a;
        break;
    case AF_PROPERTY_CORNER_RADIUS:
        value[0] = node->corner_radius;
        break;
    case AF_PROPERTY_PATH_POINT:
    case AF_PROPERTY_PATH_CONTROL_IN:
    case AF_PROPERTY_PATH_CONTROL_OUT: {
        const AfPath *path = af_path_find_node_const(&editor->render_path_data, node->id);
        const AfPathCommand *path_command = NULL;
        const AfVec2 *point = NULL;
        if (path == NULL || editor->selected_path_command >= path->command_count) {
            return false;
        }
        path_command =
            &editor->render_path_data.commands[path->command_start + editor->selected_path_command];
        if (path_command->type == AF_PATH_CLOSE ||
            (property != AF_PROPERTY_PATH_POINT && path_command->type != AF_PATH_CUBIC)) {
            return false;
        }
        point = property == AF_PROPERTY_PATH_POINT
                    ? &path_command->point
                    : (property == AF_PROPERTY_PATH_CONTROL_IN ? &path_command->control2
                                                               : &path_command->control1);
        value[0] = point->x;
        value[1] = point->y;
        break;
    }
    default:
        return false;
    }
    return true;
}

static AfResult af_editor_add_keyframe(AfEditorState *editor) {
    const AfSceneNode *node = af_scene_find_const(&editor->project.scene, editor->selected_id);
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    const AfProjectTrack *existing = af_editor_selected_track(editor);
    AfProjectTrack new_track = {0};
    AfProjectKeyframe new_key = {0};
    AfEditorCommand command = {0};
    uint64_t track_id = 0U;
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    if (node == NULL || clip == NULL) {
        return result;
    }
    result = af_editor_command_animation(AF_EDITOR_MERGE_NONE, editor->selected_id, &command);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    if (existing == NULL) {
        result = af_project_allocate_animation_id(&editor->project, &track_id);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
        new_track =
            (AfProjectTrack){.id = track_id,
                             .clip_id = clip->id,
                             .node_id = node->id,
                             .property = editor->selected_property,
                             .element_index = editor->selected_property >= AF_PROPERTY_PATH_POINT
                                                  ? editor->selected_path_command
                                                  : 0U};
        command.track_after[0] =
            (AfProjectTrackSnapshot){.track = new_track, .index = editor->project.track_count};
        command.track_after_count = 1U;
    } else {
        track_id = existing->id;
    }
    result = af_project_allocate_animation_id(&editor->project, &new_key.id);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    new_key.track_id = track_id;
    new_key.keyframe.time = af_editor_clamp(editor->playhead, 0.0F, clip->duration);
    if (editor->snapping) {
        new_key.keyframe.time = roundf(new_key.keyframe.time * 10.0F) * 0.1F;
    }
    new_key.keyframe.interpolation = AF_INTERPOLATION_LINEAR;
    new_key.keyframe.easing[0] = 0.42F;
    new_key.keyframe.easing[2] = 0.58F;
    new_key.keyframe.easing[3] = 1.0F;
    if (!af_editor_property_value(editor, node, editor->selected_property,
                                  new_key.keyframe.value)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (existing != NULL) {
        AfProjectKeyframe *current =
            af_editor_track_key_at(editor, existing->id, new_key.keyframe.time);
        if (current != NULL) {
            AfProjectKeyframeSnapshot before = {0};
            if (af_project_keyframe_snapshot(&editor->project, current->id, &before) !=
                AF_RESULT_SUCCESS) {
                return AF_RESULT_NOT_FOUND;
            }
            command.keyframe_before_count = 1U;
            command.keyframe_before[0] = before;
            command.keyframe_after_count = 1U;
            command.keyframe_after[0] = before;
            memcpy(command.keyframe_after[0].keyframe.keyframe.value, new_key.keyframe.value,
                   sizeof(new_key.keyframe.value));
            result = af_editor_history_execute(&editor->history, &editor->project, &command, false,
                                               false);
            if (result == AF_RESULT_SUCCESS) {
                editor->selected_keyframe_count = 1U;
                editor->selected_keyframes[0] = current->id;
                editor->timeline_focus = true;
                af_editor_mark_dirty(editor);
                af_editor_status(editor, "Keyframe updated");
            }
            return result;
        }
    }
    command.keyframe_after[0] =
        (AfProjectKeyframeSnapshot){.keyframe = new_key, .index = editor->project.keyframe_count};
    command.keyframe_after_count = 1U;
    result = af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    if (result == AF_RESULT_SUCCESS) {
        editor->selected_keyframe_count = 1U;
        editor->selected_keyframes[0] = new_key.id;
        editor->timeline_focus = true;
        af_editor_mark_dirty(editor);
        af_editor_status(editor, "Keyframe added");
    }
    return result;
}

static AfResult af_editor_delete_keyframes(AfEditorState *editor) {
    AfEditorCommand command = {0};
    uint32_t index = 0U;
    AfResult result = AF_RESULT_NOT_FOUND;
    if (editor->selected_keyframe_count == 0U) {
        return result;
    }
    result =
        af_editor_command_animation(AF_EDITOR_MERGE_NONE, editor->selected_keyframes[0], &command);
    for (index = 0U; result == AF_RESULT_SUCCESS && index < editor->selected_keyframe_count;
         ++index) {
        result = af_project_keyframe_snapshot(&editor->project, editor->selected_keyframes[index],
                                              &command.keyframe_before[index]);
    }
    if (result == AF_RESULT_SUCCESS) {
        command.keyframe_before_count = editor->selected_keyframe_count;
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_editor_clear_key_selection(editor);
        af_editor_mark_dirty(editor);
        af_editor_status(editor, "Keyframes deleted");
    }
    return result;
}

static void af_editor_copy_keyframes(AfEditorState *editor) {
    uint32_t index = 0U;
    editor->key_clipboard_count = 0U;
    for (index = 0U; index < editor->selected_keyframe_count; ++index) {
        const AfProjectKeyframe *key =
            af_project_find_keyframe_const(&editor->project, editor->selected_keyframes[index]);
        if (key != NULL) {
            editor->key_clipboard[editor->key_clipboard_count++] = *key;
        }
    }
    af_editor_status(editor, "Keyframes copied");
}

static AfResult af_editor_paste_keyframes(AfEditorState *editor, bool duplicate) {
    AfEditorCommand command = {0};
    float earliest = INFINITY;
    uint32_t index = 0U;
    AfResult result = AF_RESULT_NOT_FOUND;
    if (duplicate) {
        af_editor_copy_keyframes(editor);
    }
    if (editor->key_clipboard_count == 0U) {
        return result;
    }
    result = af_editor_command_animation(AF_EDITOR_MERGE_NONE, editor->selected_clip_id, &command);
    for (index = 0U; index < editor->key_clipboard_count; ++index) {
        earliest = fminf(earliest, editor->key_clipboard[index].keyframe.time);
    }
    editor->selected_keyframe_count = 0U;
    for (index = 0U; result == AF_RESULT_SUCCESS && index < editor->key_clipboard_count; ++index) {
        AfProjectKeyframe copy = editor->key_clipboard[index];
        const AfProjectTrack *track = af_project_find_track_const(&editor->project, copy.track_id);
        const AfProjectClip *clip =
            track == NULL ? NULL : af_project_find_clip_const(&editor->project, track->clip_id);
        if (clip == NULL || clip->id != editor->selected_clip_id) {
            result = AF_RESULT_INVALID_ARGUMENT;
            break;
        }
        result = af_project_allocate_animation_id(&editor->project, &copy.id);
        copy.keyframe.time =
            af_editor_clamp(editor->playhead + copy.keyframe.time - earliest, 0.0F, clip->duration);
        if (duplicate) {
            copy.keyframe.time = af_editor_clamp(copy.keyframe.time + 0.1F, 0.0F, clip->duration);
        }
        command.keyframe_after[index] = (AfProjectKeyframeSnapshot){
            .keyframe = copy, .index = editor->project.keyframe_count + index};
        editor->selected_keyframes[editor->selected_keyframe_count++] = copy.id;
    }
    if (result == AF_RESULT_SUCCESS) {
        command.keyframe_after_count = editor->key_clipboard_count;
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_editor_mark_dirty(editor);
        af_editor_status(editor, duplicate ? "Keyframes duplicated" : "Keyframes pasted");
    }
    return result;
}

static AfResult af_editor_modify_selected_keys(AfEditorState *editor, int operation,
                                               AfInterpolation interpolation) {
    AfEditorCommand command = {0};
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    float minimum = INFINITY;
    float maximum = -INFINITY;
    uint32_t index = 0U;
    AfResult result = AF_RESULT_NOT_FOUND;
    if (clip == NULL || editor->selected_keyframe_count == 0U) {
        return result;
    }
    result =
        af_editor_command_animation(AF_EDITOR_MERGE_NONE, editor->selected_keyframes[0], &command);
    for (index = 0U; result == AF_RESULT_SUCCESS && index < editor->selected_keyframe_count;
         ++index) {
        result = af_project_keyframe_snapshot(&editor->project, editor->selected_keyframes[index],
                                              &command.keyframe_before[index]);
        if (result == AF_RESULT_SUCCESS) {
            const float time = command.keyframe_before[index].keyframe.keyframe.time;
            minimum = fminf(minimum, time);
            maximum = fmaxf(maximum, time);
        }
    }
    command.keyframe_before_count = editor->selected_keyframe_count;
    for (index = 0U; result == AF_RESULT_SUCCESS && index < editor->selected_keyframe_count;
         ++index) {
        AfProjectKeyframeSnapshot snapshot = command.keyframe_before[index];
        if (operation == 0) {
            snapshot.keyframe.keyframe.interpolation = interpolation;
        } else if (operation == 1) {
            snapshot.keyframe.keyframe.time =
                minimum + (snapshot.keyframe.keyframe.time - minimum) * 1.1F;
        } else if (operation == 2) {
            snapshot.keyframe.keyframe.time =
                minimum + (snapshot.keyframe.keyframe.time - minimum) * 0.9F;
        } else if (operation == 3) {
            snapshot.keyframe.keyframe.time = minimum + maximum - snapshot.keyframe.keyframe.time;
        } else if (operation >= 4 && operation <= 6) {
            snapshot.keyframe.keyframe.interpolation = AF_INTERPOLATION_CUBIC_BEZIER;
            snapshot.keyframe.keyframe.easing[0] = operation == 6 ? 0.0F : 0.42F;
            snapshot.keyframe.keyframe.easing[1] = 0.0F;
            snapshot.keyframe.keyframe.easing[2] = operation == 5 ? 1.0F : 0.58F;
            snapshot.keyframe.keyframe.easing[3] = 1.0F;
        }
        snapshot.keyframe.keyframe.time =
            af_editor_clamp(snapshot.keyframe.keyframe.time, 0.0F, clip->duration);
        command.keyframe_after[index] = snapshot;
    }
    command.keyframe_after_count = editor->selected_keyframe_count;
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_editor_mark_dirty(editor);
        af_editor_status(editor, "Keyframes edited");
    }
    return result;
}

static AfResult af_editor_new_clip(AfEditorState *editor) {
    AfEditorCommand command = {0};
    AfProjectClip clip = {.duration = 4.0F, .loop_start = 0.0F, .loop_end = 4.0F, .loop = true};
    AfResult result = af_project_allocate_animation_id(&editor->project, &clip.id);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    (void)snprintf(clip.name, sizeof(clip.name), "Clip %llu", (unsigned long long)clip.id);
    result = af_editor_command_animation(AF_EDITOR_MERGE_NONE, clip.id, &command);
    command.clip_after[0] =
        (AfProjectClipSnapshot){.clip = clip, .index = editor->project.clip_count};
    command.clip_after_count = 1U;
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        editor->selected_clip_id = clip.id;
        editor->playhead = 0.0F;
        af_editor_mark_dirty(editor);
        af_editor_status(editor, "Animation clip created");
    }
    return result;
}

static AfResult af_editor_delete_clip(AfEditorState *editor) {
    AfEditorCommand command = {0};
    uint32_t index = 0U;
    AfResult result = AF_RESULT_NOT_FOUND;
    if (editor->project.clip_count <= 1U || editor->selected_clip_id == 0U) {
        return result;
    }
    result = af_editor_command_animation(AF_EDITOR_MERGE_NONE, editor->selected_clip_id, &command);
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_clip_snapshot(&editor->project, editor->selected_clip_id,
                                          &command.clip_before[0]);
        command.clip_before_count = 1U;
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < editor->project.track_count; ++index) {
        const AfProjectTrack *track = &editor->project.tracks[index];
        uint32_t key_index = 0U;
        if (track->clip_id != editor->selected_clip_id) {
            continue;
        }
        if (command.track_before_count >= AF_EDITOR_COMMAND_MAX_TRACKS) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        command.track_before[command.track_before_count++] =
            (AfProjectTrackSnapshot){.track = *track, .index = index};
        for (key_index = 0U; key_index < editor->project.keyframe_count; ++key_index) {
            if (editor->project.keyframes[key_index].track_id == track->id) {
                if (command.keyframe_before_count >= AF_EDITOR_COMMAND_MAX_KEYFRAMES) {
                    return AF_RESULT_LIMIT_EXCEEDED;
                }
                command.keyframe_before[command.keyframe_before_count++] =
                    (AfProjectKeyframeSnapshot){.keyframe = editor->project.keyframes[key_index],
                                                .index = key_index};
            }
        }
    }
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        editor->selected_clip_id = editor->project.clips[0].id;
        editor->playhead = 0.0F;
        af_editor_clear_key_selection(editor);
        af_editor_mark_dirty(editor);
        af_editor_status(editor, "Animation clip deleted");
    }
    return result;
}
