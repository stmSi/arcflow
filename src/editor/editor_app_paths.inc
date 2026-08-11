static AfVec2 af_editor_visible_center(const AfEditorState *editor, int width, int height) {
    float canvas_x = 0.0F;
    float canvas_y = 0.0F;
    float canvas_width = 0.0F;
    float canvas_height = 0.0F;
    af_editor_layout(editor, width, height, &canvas_x, &canvas_y, &canvas_width, &canvas_height);
    return af_editor_screen_to_project(
        editor, width, height,
        (AfVec2){canvas_x + canvas_width * 0.5F, canvas_y + canvas_height * 0.5F});
}

static AfResult af_editor_create_primitive(AfEditorState *editor, AfNodeType type, int width,
                                           int height) {
    AfProjectNodeSnapshot snapshot = {0};
    AfEditorCommand command = {0};
    uint64_t id = 0U;
    const char *name = type == AF_NODE_RECTANGLE
                           ? "Rectangle"
                           : (type == AF_NODE_ROUNDED_RECTANGLE ? "Rounded Rectangle" : "Ellipse");
    AfResult result = af_project_allocate_id(&editor->project, &id);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    snapshot = (AfProjectNodeSnapshot){
        .node = af_editor_default_node(type, id, af_editor_visible_center(editor, width, height)),
        .index = editor->project.scene.count};
    (void)snprintf(snapshot.name, sizeof(snapshot.name), "%s %llu", name, (unsigned long long)id);
    result = af_editor_command_create(&snapshot, &command);
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_editor_select_only(editor, id);
        editor->selected_path_command = UINT32_MAX;
        editor->dirty = true;
        af_editor_status(editor, "Primitive created");
    }
    return result;
}

static AfPath *af_editor_selected_path(AfEditorState *editor) {
    const AfSceneNode *node = af_scene_find_const(&editor->project.scene, editor->selected_id);
    return node != NULL && node->type == AF_NODE_PATH
               ? af_project_find_node_path(&editor->project, node->id)
               : NULL;
}

static bool af_editor_selected_path_has_animation(const AfEditorState *editor) {
    for (uint32_t index = 0U; index < editor->project.track_count; ++index) {
        const AfProjectTrack *track = &editor->project.tracks[index];
        if (track->node_id == editor->selected_id && track->property >= AF_PROPERTY_PATH_POINT) {
            return true;
        }
    }
    return false;
}

static bool af_editor_require_static_path_topology(AfEditorState *editor) {
    if (!af_editor_selected_path_has_animation(editor)) {
        return true;
    }
    af_editor_status(editor, "Remove vector point tracks before changing path topology");
    return false;
}

static AfResult af_editor_create_path(AfEditorState *editor, int width, int height) {
    static const AfPathCommand commands[] = {{.type = AF_PATH_MOVE, .point = {0.0F, 90.0F}},
                                             {.type = AF_PATH_CUBIC,
                                              .point = {110.0F, 0.0F},
                                              .control1 = {20.0F, 25.0F},
                                              .control2 = {72.0F, 0.0F}},
                                             {.type = AF_PATH_CUBIC,
                                              .point = {220.0F, 90.0F},
                                              .control1 = {148.0F, 0.0F},
                                              .control2 = {200.0F, 25.0F}},
                                             {.type = AF_PATH_LINE, .point = {110.0F, 180.0F}},
                                             {.type = AF_PATH_CLOSE}};
    AfProjectNodeSnapshot snapshot = {0};
    AfEditorCommand command = {0};
    uint64_t id = 0U;
    AfResult result = af_project_allocate_id(&editor->project, &id);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    snapshot = (AfProjectNodeSnapshot){
        .node = af_editor_default_node(AF_NODE_PATH, id,
                                       af_editor_visible_center(editor, width, height)),
        .index = editor->project.scene.count};
    snapshot.node.bounds = (AfVec2){220.0F, 180.0F};
    snapshot.node.fill = (AfColor){0.78F, 0.42F, 0.96F, 1.0F};
    (void)snprintf(snapshot.name, sizeof(snapshot.name), "Path %llu", (unsigned long long)id);
    result = af_editor_command_create(&snapshot, &command);
    if (result == AF_RESULT_SUCCESS) {
        command.path_after_count = 1U;
        command.path_command_after_count = (uint32_t)(sizeof(commands) / sizeof(commands[0]));
        command.path_after[0] = (AfProjectPathSnapshot){
            .path = {.id = id,
                     .node_id = id,
                     .command_start = editor->project.path_data.command_count,
                     .command_count = command.path_command_after_count,
                     .fill_rule = AF_PATH_FILL_EVEN_ODD,
                     .geometry_generation = 1U},
            .index = editor->project.path_data.path_count};
        memcpy(command.path_command_after, commands, sizeof(commands));
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_editor_select_only(editor, id);
        editor->selected_path_command = 1U;
        editor->dirty = true;
        af_editor_status(editor, "Editable path created");
    }
    return result;
}

static AfResult af_editor_add_vector_smoke_track(AfEditorState *editor, uint64_t node_id,
                                                 AfPropertyType property, uint32_t command_index,
                                                 float duration, AfVec2 start, AfVec2 end) {
    AfProjectTrack track = {.clip_id = editor->selected_clip_id,
                            .node_id = node_id,
                            .property = property,
                            .element_index = command_index};
    AfProjectKeyframe keys[2] = {0};
    AfResult result = af_project_allocate_animation_id(&editor->project, &track.id);
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_track(&editor->project, &track);
    }
    for (uint32_t index = 0U; result == AF_RESULT_SUCCESS && index < 2U; ++index) {
        result = af_project_allocate_animation_id(&editor->project, &keys[index].id);
        keys[index].track_id = track.id;
        keys[index].keyframe.time = index == 0U ? 0.0F : duration;
        keys[index].keyframe.interpolation = AF_INTERPOLATION_LINEAR;
        keys[index].keyframe.value[0] = index == 0U ? start.x : end.x;
        keys[index].keyframe.value[1] = index == 0U ? start.y : end.y;
        if (result == AF_RESULT_SUCCESS) {
            result = af_project_add_keyframe(&editor->project, &keys[index]);
        }
    }
    return result;
}

static AfResult af_editor_prepare_vector_animation_smoke(AfEditorState *editor, int width,
                                                         int height) {
    const AfProjectClip *clip = NULL;
    AfPath *path = NULL;
    AfPathCommand *command = NULL;
    AfResult result = af_editor_create_path(editor, width, height);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    clip = af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    path = af_editor_selected_path(editor);
    if (clip == NULL || path == NULL || path->command_count <= 1U) {
        return AF_RESULT_NOT_FOUND;
    }
    command = &editor->project.path_data.commands[path->command_start + 1U];
    if (command->type != AF_PATH_CUBIC) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result =
        af_editor_add_vector_smoke_track(editor, path->node_id, AF_PROPERTY_PATH_POINT, 1U,
                                         clip->duration, command->point, (AfVec2){90.0F, 85.0F});
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_add_vector_smoke_track(editor, path->node_id,
                                                  AF_PROPERTY_PATH_CONTROL_OUT, 1U, clip->duration,
                                                  command->control1, (AfVec2){10.0F, 145.0F});
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_add_vector_smoke_track(editor, path->node_id,
                                                  AF_PROPERTY_PATH_CONTROL_IN, 1U, clip->duration,
                                                  command->control2, (AfVec2){140.0F, 145.0F});
    }
    editor->selected_path_command = 1U;
    return result;
}

static AfResult af_editor_apply_path_edit(AfEditorState *editor, const AfPathCommand *new_commands,
                                          uint32_t new_command_count, AfPathFillRule fill_rule,
                                          const char *status) {
    AfPath *path = af_editor_selected_path(editor);
    AfEditorCommand command = {0};
    AfResult result = AF_RESULT_NOT_FOUND;
    if (path == NULL || new_commands == NULL || new_command_count == 0U) {
        return result;
    }
    if (path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS ||
        new_command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS) {
        af_editor_status(editor, "Path edit exceeds bounded undo capacity");
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    if (path->geometry_generation == UINT64_MAX) {
        af_editor_status(editor, "Path geometry generation exhausted");
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    result = af_editor_command_path(AF_EDITOR_MERGE_NONE, path->id, &command);
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_path_snapshot(&editor->project, path->id, &command.path_before[0],
                                          command.path_command_before,
                                          AF_EDITOR_COMMAND_MAX_PATH_COMMANDS);
    }
    if (result == AF_RESULT_SUCCESS) {
        command.path_before_count = 1U;
        command.path_command_before_count = path->command_count;
        command.path_before[0].command_offset = 0U;
        command.path_after_count = 1U;
        command.path_after[0] = command.path_before[0];
        command.path_after[0].path.command_count = new_command_count;
        command.path_after[0].path.fill_rule = fill_rule;
        command.path_after[0].path.geometry_generation =
            command.path_before[0].path.geometry_generation + 1U;
        command.path_after[0].command_offset = 0U;
        command.path_command_after_count = new_command_count;
        memcpy(command.path_command_after, new_commands,
               (size_t)new_command_count * sizeof(*new_commands));
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        editor->dirty = true;
        af_editor_status(editor, status);
    } else {
        af_editor_status(editor, "Path edit rejected");
    }
    return result;
}

static bool af_editor_path_contour(const AfPathCommand *commands, uint32_t count, uint32_t selected,
                                   uint32_t *out_start, uint32_t *out_end) {
    uint32_t start = selected;
    uint32_t end = selected;
    if (commands == NULL || selected >= count || out_start == NULL || out_end == NULL) {
        return false;
    }
    while (start > 0U && commands[start].type != AF_PATH_MOVE) {
        --start;
    }
    if (commands[start].type != AF_PATH_MOVE) {
        return false;
    }
    end = start + 1U;
    while (end < count && commands[end].type != AF_PATH_MOVE) {
        ++end;
    }
    *out_start = start;
    *out_end = end;
    return true;
}

static AfVec2 af_editor_vec_midpoint(AfVec2 first, AfVec2 second) {
    return (AfVec2){(first.x + second.x) * 0.5F, (first.y + second.y) * 0.5F};
}

static AfResult af_editor_insert_path_point(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    uint32_t selected = editor->selected_path_command;
    if (!af_editor_require_static_path_topology(editor) || path == NULL ||
        path->command_count >= AF_EDITOR_COMMAND_MAX_PATH_COMMANDS || selected == 0U ||
        selected >= path->command_count) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memcpy(commands, &editor->project.path_data.commands[path->command_start],
           (size_t)path->command_count * sizeof(*commands));
    if (commands[selected].type != AF_PATH_LINE && commands[selected].type != AF_PATH_CUBIC) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    memmove(&commands[selected + 1U], &commands[selected],
            (size_t)(path->command_count - selected) * sizeof(*commands));
    if (commands[selected + 1U].type == AF_PATH_LINE) {
        commands[selected] =
            (AfPathCommand){.type = AF_PATH_LINE,
                            .point = af_editor_vec_midpoint(commands[selected - 1U].point,
                                                            commands[selected + 1U].point)};
    } else {
        const AfVec2 start = commands[selected - 1U].point;
        const AfPathCommand original = commands[selected + 1U];
        const AfVec2 first = af_editor_vec_midpoint(start, original.control1);
        const AfVec2 middle = af_editor_vec_midpoint(original.control1, original.control2);
        const AfVec2 last = af_editor_vec_midpoint(original.control2, original.point);
        const AfVec2 left = af_editor_vec_midpoint(first, middle);
        const AfVec2 right = af_editor_vec_midpoint(middle, last);
        const AfVec2 point = af_editor_vec_midpoint(left, right);
        commands[selected] = (AfPathCommand){
            .type = AF_PATH_CUBIC, .point = point, .control1 = first, .control2 = left};
        commands[selected + 1U] = (AfPathCommand){
            .type = AF_PATH_CUBIC, .point = original.point, .control1 = right, .control2 = last};
    }
    if (af_editor_apply_path_edit(editor, commands, path->command_count + 1U, path->fill_rule,
                                  "Path point inserted") == AF_RESULT_SUCCESS) {
        editor->selected_path_command = selected;
        return AF_RESULT_SUCCESS;
    }
    return AF_RESULT_INVALID_ARGUMENT;
}

static AfResult af_editor_delete_path_point(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    uint32_t start = 0U;
    uint32_t end = 0U;
    uint32_t segments = 0U;
    const uint32_t selected = editor->selected_path_command;
    if (!af_editor_require_static_path_topology(editor) || path == NULL ||
        path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS ||
        path->geometry_generation == UINT64_MAX || selected >= path->command_count) {
        return AF_RESULT_NOT_FOUND;
    }
    memcpy(commands, &editor->project.path_data.commands[path->command_start],
           (size_t)path->command_count * sizeof(*commands));
    if ((commands[selected].type != AF_PATH_LINE && commands[selected].type != AF_PATH_CUBIC) ||
        !af_editor_path_contour(commands, path->command_count, selected, &start, &end)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = start + 1U; index < end; ++index) {
        segments += commands[index].type == AF_PATH_LINE || commands[index].type == AF_PATH_CUBIC;
    }
    if (segments <= 1U) {
        af_editor_status(editor, "A contour must retain a segment");
        return AF_RESULT_INVALID_ARGUMENT;
    }
    memmove(&commands[selected], &commands[selected + 1U],
            (size_t)(path->command_count - selected - 1U) * sizeof(*commands));
    if (af_editor_apply_path_edit(editor, commands, path->command_count - 1U, path->fill_rule,
                                  "Path point deleted") == AF_RESULT_SUCCESS) {
        editor->selected_path_command = selected > 0U ? selected - 1U : 0U;
        return AF_RESULT_SUCCESS;
    }
    return AF_RESULT_INVALID_ARGUMENT;
}

static AfResult af_editor_convert_path_point(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    const uint32_t selected = editor->selected_path_command;
    if (!af_editor_require_static_path_topology(editor) || path == NULL ||
        path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS || selected == 0U ||
        selected >= path->command_count) {
        return AF_RESULT_NOT_FOUND;
    }
    memcpy(commands, &editor->project.path_data.commands[path->command_start],
           (size_t)path->command_count * sizeof(*commands));
    if (commands[selected].type == AF_PATH_LINE) {
        const AfVec2 first = commands[selected - 1U].point;
        const AfVec2 last = commands[selected].point;
        commands[selected].type = AF_PATH_CUBIC;
        commands[selected].control1 =
            (AfVec2){first.x + (last.x - first.x) / 3.0F, first.y + (last.y - first.y) / 3.0F};
        commands[selected].control2 = (AfVec2){first.x + (last.x - first.x) * 2.0F / 3.0F,
                                               first.y + (last.y - first.y) * 2.0F / 3.0F};
    } else if (commands[selected].type == AF_PATH_CUBIC) {
        commands[selected].type = AF_PATH_LINE;
        commands[selected].control1 = (AfVec2){0};
        commands[selected].control2 = (AfVec2){0};
    } else {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return af_editor_apply_path_edit(editor, commands, path->command_count, path->fill_rule,
                                     "Path point converted");
}

static AfResult af_editor_toggle_path_closed(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    uint32_t start = 0U;
    uint32_t end = 0U;
    const uint32_t selected = editor->selected_path_command;
    if (!af_editor_require_static_path_topology(editor) || path == NULL ||
        path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS ||
        !af_editor_path_contour(&editor->project.path_data.commands[path->command_start],
                                path->command_count, selected < path->command_count ? selected : 0U,
                                &start, &end)) {
        return AF_RESULT_NOT_FOUND;
    }
    memcpy(commands, &editor->project.path_data.commands[path->command_start],
           (size_t)path->command_count * sizeof(*commands));
    if (end > start && commands[end - 1U].type == AF_PATH_CLOSE) {
        memmove(&commands[end - 1U], &commands[end],
                (size_t)(path->command_count - end) * sizeof(*commands));
        editor->selected_path_command = end > start + 1U ? end - 2U : start;
        return af_editor_apply_path_edit(editor, commands, path->command_count - 1U,
                                         path->fill_rule, "Path contour opened");
    }
    if (path->command_count >= AF_EDITOR_COMMAND_MAX_PATH_COMMANDS) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memmove(&commands[end + 1U], &commands[end],
            (size_t)(path->command_count - end) * sizeof(*commands));
    commands[end] = (AfPathCommand){.type = AF_PATH_CLOSE};
    editor->selected_path_command = end;
    return af_editor_apply_path_edit(editor, commands, path->command_count + 1U, path->fill_rule,
                                     "Path contour closed");
}

static AfResult af_editor_reverse_path_contour(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    AfPathCommand reversed[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    uint32_t start = 0U;
    uint32_t end = 0U;
    uint32_t write = 0U;
    uint32_t segment_end = 0U;
    bool closed = false;
    const uint32_t selected = editor->selected_path_command;
    if (!af_editor_require_static_path_topology(editor) || path == NULL ||
        path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS) {
        return AF_RESULT_NOT_FOUND;
    }
    memcpy(commands, &editor->project.path_data.commands[path->command_start],
           (size_t)path->command_count * sizeof(*commands));
    if (!af_editor_path_contour(commands, path->command_count,
                                selected < path->command_count ? selected : 0U, &start, &end)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    closed = end > start && commands[end - 1U].type == AF_PATH_CLOSE;
    segment_end = closed ? end - 1U : end;
    if (segment_end <= start + 1U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    reversed[write++] =
        (AfPathCommand){.type = AF_PATH_MOVE, .point = commands[segment_end - 1U].point};
    for (uint32_t index = segment_end; index > start + 1U;) {
        AfPathCommand segment = commands[--index];
        segment.point = index == start + 1U ? commands[start].point : commands[index - 1U].point;
        if (segment.type == AF_PATH_CUBIC) {
            const AfVec2 control = segment.control1;
            segment.control1 = segment.control2;
            segment.control2 = control;
        }
        reversed[write++] = segment;
    }
    if (closed) {
        reversed[write++] = (AfPathCommand){.type = AF_PATH_CLOSE};
    }
    memcpy(&commands[start], reversed, (size_t)write * sizeof(*reversed));
    editor->selected_path_command = start + (write > 1U ? 1U : 0U);
    return af_editor_apply_path_edit(editor, commands, path->command_count, path->fill_rule,
                                     "Path contour reversed");
}

static AfResult af_editor_add_path_contour(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    uint32_t count = 0U;
    if (!af_editor_require_static_path_topology(editor) || path == NULL ||
        path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS - 3U) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    count = path->command_count;
    memcpy(commands, &editor->project.path_data.commands[path->command_start],
           (size_t)count * sizeof(*commands));
    commands[count] = (AfPathCommand){.type = AF_PATH_MOVE, .point = {55.0F, 65.0F}};
    commands[count + 1U] = (AfPathCommand){.type = AF_PATH_LINE, .point = {165.0F, 65.0F}};
    commands[count + 2U] = (AfPathCommand){.type = AF_PATH_LINE, .point = {110.0F, 130.0F}};
    if (af_editor_apply_path_edit(editor, commands, count + 3U, path->fill_rule,
                                  "Open contour added") == AF_RESULT_SUCCESS) {
        editor->selected_path_command = count + 1U;
        return AF_RESULT_SUCCESS;
    }
    return AF_RESULT_INVALID_ARGUMENT;
}

static AfResult af_editor_toggle_path_fill_rule(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    if (path == NULL || path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS) {
        return AF_RESULT_NOT_FOUND;
    }
    return af_editor_apply_path_edit(
        editor, &editor->project.path_data.commands[path->command_start], path->command_count,
        path->fill_rule == AF_PATH_FILL_EVEN_ODD ? AF_PATH_FILL_NON_ZERO : AF_PATH_FILL_EVEN_ODD,
        path->fill_rule == AF_PATH_FILL_EVEN_ODD ? "Non-zero fill rule" : "Even-odd fill rule");
}

static AfResult af_editor_delete_selection(AfEditorState *editor) {
    uint64_t roots[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint32_t root_count = 0U;
    AfEditorCommand command = {0};
    AfResult result = AF_RESULT_NOT_FOUND;
    if (editor->selected_id == 0U) {
        return result;
    }
    for (uint32_t index = 0U; index < editor->selected_count; ++index) {
        const uint64_t id = editor->selected_ids[index];
        if (!af_editor_selection_has_selected_ancestor(editor, id)) {
            roots[root_count++] = id;
        }
    }
    if (root_count == 0U) {
        roots[root_count++] = editor->selected_id;
    }
    result = af_editor_command_delete_many(&editor->project, roots, root_count, &command);
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_editor_select_only(editor, 0U);
        editor->selected_path_command = UINT32_MAX;
        editor->dirty = true;
        af_editor_status(editor, root_count == 1U ? "Object deleted" : "Objects deleted");
    }
    return result;
}

static void af_editor_reconcile_selection(AfEditorState *editor) {
    uint32_t selection_write = 0U;
    uint32_t write_index = 0U;
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    for (uint32_t index = 0U; index < editor->selected_count; ++index) {
        if (af_scene_find_const(&editor->project.scene, editor->selected_ids[index]) != NULL) {
            editor->selected_ids[selection_write++] = editor->selected_ids[index];
        }
    }
    editor->selected_count = selection_write;
    if (af_scene_find_const(&editor->project.scene, editor->selected_id) == NULL) {
        editor->selected_id =
            editor->selected_count == 0U ? 0U : editor->selected_ids[editor->selected_count - 1U];
        editor->selected_path_command = UINT32_MAX;
    } else if (!af_editor_selection_contains(editor, editor->selected_id)) {
        af_editor_selection_add(editor, editor->selected_id);
    }
    if (editor->selected_id != 0U) {
        const AfPath *path =
            af_path_find_node_const(&editor->project.path_data, editor->selected_id);
        if (path == NULL) {
            editor->selected_path_command = UINT32_MAX;
        } else if (editor->selected_path_command >= path->command_count) {
            editor->selected_path_command = 0U;
        }
    } else {
        editor->selected_path_command = UINT32_MAX;
    }
    if (clip == NULL) {
        editor->selected_clip_id =
            editor->project.clip_count == 0U ? 0U : editor->project.clips[0].id;
        clip = af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    }
    if (clip != NULL) {
        editor->playhead = af_editor_clamp(editor->playhead, 0.0F, clip->duration);
    }
    for (uint32_t index = 0U; index < editor->selected_keyframe_count; ++index) {
        const AfProjectKeyframe *key =
            af_project_find_keyframe_const(&editor->project, editor->selected_keyframes[index]);
        const AfProjectTrack *track =
            key == NULL ? NULL : af_project_find_track_const(&editor->project, key->track_id);
        if (track != NULL && track->clip_id == editor->selected_clip_id) {
            editor->selected_keyframes[write_index++] = key->id;
        }
    }
    editor->selected_keyframe_count = write_index;
}

static AfResult af_editor_duplicate_selection(AfEditorState *editor) {
    AfProjectNodeSnapshot originals[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    AfProjectNodeSnapshot copies[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint64_t roots[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint64_t new_roots[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint64_t old_ids[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint64_t new_ids[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint32_t root_count = 0U;
    uint32_t count = 0U;
    uint32_t index = 0U;
    AfEditorCommand command = {0};
    AfResult result = AF_RESULT_NOT_FOUND;
    if (editor->selected_id == 0U) {
        return result;
    }
    for (index = 0U; index < editor->selected_count; ++index) {
        const uint64_t id = editor->selected_ids[index];
        if (!af_editor_selection_has_selected_ancestor(editor, id)) {
            roots[root_count++] = id;
        }
    }
    if (root_count == 0U) {
        roots[root_count++] = editor->selected_id;
    }
    result = AF_RESULT_SUCCESS;
    for (index = 0U; result == AF_RESULT_SUCCESS && index < root_count; ++index) {
        uint32_t subtree_count = 0U;
        result =
            af_project_collect_subtree(&editor->project, roots[index], &originals[count],
                                       AF_EDITOR_COMMAND_MAX_SNAPSHOTS - count, &subtree_count);
        count += subtree_count;
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < count; ++index) {
        old_ids[index] = originals[index].node.id;
        result = af_project_allocate_id(&editor->project, &new_ids[index]);
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < count; ++index) {
        uint32_t parent_index = 0U;
        copies[index] = originals[index];
        copies[index].node.id = new_ids[index];
        copies[index].style.id = new_ids[index];
        copies[index].style.node_id = new_ids[index];
        copies[index].layout.node_id = new_ids[index];
        copies[index].index = editor->project.scene.count + index;
        bool is_root = false;
        for (uint32_t root_index = 0U; root_index < root_count; ++root_index) {
            if (originals[index].node.id == roots[root_index]) {
                new_roots[root_index] = new_ids[index];
                is_root = true;
                break;
            }
        }
        if (is_root) {
            copies[index].node.local_transform.position.x += 16.0F;
            copies[index].node.local_transform.position.y += 16.0F;
        }
        for (parent_index = 0U; parent_index < count; ++parent_index) {
            if (copies[index].node.parent_id == old_ids[parent_index]) {
                copies[index].node.parent_id = new_ids[parent_index];
                break;
            }
        }
        (void)snprintf(copies[index].name, sizeof(copies[index].name), "Copy %.56s",
                       originals[index].name);
        if (copies[index].has_emitter) {
            result = af_project_allocate_animation_id(&editor->project, &copies[index].emitter.id);
            copies[index].emitter.node_id = new_ids[index];
            if (copies[index].emitter.type == AF_PARTICLE_EMITTER_VECTOR_PATH) {
                copies[index].emitter.path_id = new_ids[index];
            }
        }
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_command_duplicate(copies, count, &command);
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < count; ++index) {
        const AfPath *source =
            af_path_find_node_const(&editor->project.path_data, originals[index].node.id);
        AfProjectPathSnapshot *path_copy = NULL;
        if (source == NULL) {
            continue;
        }
        if (command.path_after_count >= AF_EDITOR_COMMAND_MAX_PATHS ||
            source->command_count >
                AF_EDITOR_COMMAND_MAX_PATH_COMMANDS - command.path_command_after_count) {
            result = AF_RESULT_LIMIT_EXCEEDED;
            break;
        }
        path_copy = &command.path_after[command.path_after_count];
        *path_copy = (AfProjectPathSnapshot){.path = *source,
                                             .index = editor->project.path_data.path_count +
                                                      command.path_after_count,
                                             .command_offset = command.path_command_after_count};
        path_copy->path.id = new_ids[index];
        path_copy->path.node_id = new_ids[index];
        path_copy->path.command_start =
            editor->project.path_data.command_count + command.path_command_after_count;
        path_copy->path.geometry_generation = 1U;
        memcpy(&command.path_command_after[command.path_command_after_count],
               &editor->project.path_data.commands[source->command_start],
               (size_t)source->command_count * sizeof(*command.path_command_after));
        command.path_command_after_count += source->command_count;
        ++command.path_after_count;
    }
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_editor_select_only(editor, 0U);
        for (index = 0U; index < root_count; ++index) {
            af_editor_selection_add(editor, new_roots[index]);
        }
        editor->selected_path_command =
            af_path_find_node_const(&editor->project.path_data, editor->selected_id) != NULL
                ? 0U
                : UINT32_MAX;
        editor->dirty = true;
        af_editor_status(editor, root_count == 1U ? "Object duplicated" : "Objects duplicated");
    }
    return result;
}

static void af_editor_begin_rename(AfEditorState *editor) {
    const char *name = af_project_name(&editor->project, editor->selected_id);
    if (name == NULL || af_project_snapshot(&editor->project, editor->selected_id,
                                            &editor->rename_before) != AF_RESULT_SUCCESS) {
        return;
    }
    (void)snprintf(editor->rename_buffer, sizeof(editor->rename_buffer), "%s", name);
    editor->rename_active = true;
    (void)SDL_StartTextInput(editor->window.window);
}

static void af_editor_finish_rename(AfEditorState *editor, bool commit) {
    if (!editor->rename_active) {
        return;
    }
    if (commit && editor->rename_buffer[0] != '\0' &&
        af_project_set_name(&editor->project, editor->selected_id, editor->rename_buffer) ==
            AF_RESULT_SUCCESS) {
        (void)af_editor_commit_modify(editor, &editor->rename_before, AF_EDITOR_MERGE_RENAME,
                                      false);
        af_editor_status(editor, "Node renamed");
    }
    editor->rename_active = false;
    SDL_StopTextInput(editor->window.window);
}

static void af_editor_backspace_utf8(char *text) {
    size_t length = strlen(text);
    if (length == 0U) {
        return;
    }
    --length;
    while (length > 0U && ((unsigned char)text[length] & 0xC0U) == 0x80U) {
        --length;
    }
    text[length] = '\0';
}

static void af_editor_append_text(AfEditorState *editor, const char *text) {
    const size_t current = strlen(editor->rename_buffer);
    const size_t incoming = text == NULL ? 0U : strlen(text);
    if (incoming != 0U && current + incoming < sizeof(editor->rename_buffer) &&
        strchr(text, '\n') == NULL && strchr(text, '\r') == NULL) {
        memcpy(editor->rename_buffer + current, text, incoming + 1U);
    }
}
