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

static void af_editor_group_selection(AfEditorState *editor) {
    uint64_t ids[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint32_t count = 0U;
    for (uint32_t i = 0U; i < editor->selected_count; ++i) {
        if (!af_editor_selection_has_selected_ancestor(editor, editor->selected_ids[i]))
            ids[count++] = editor->selected_ids[i];
    }
    if (count == 0U) {
        af_editor_status(editor, "Select objects to group");
        return;
    }
    uint64_t id = 0U;
    AfEditorCommand command = {0};
    AfResult result = af_project_allocate_id(&editor->project, &id);
    if (result == AF_RESULT_SUCCESS)
        result = af_editor_command_group(&editor->project, ids, count, id, &command);
    if (result == AF_RESULT_SUCCESS)
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    if (result == AF_RESULT_SUCCESS) {
        af_editor_select_only(editor, id);
        af_editor_mark_dirty(editor);
        af_editor_status(editor, "Objects grouped · Ctrl+Z to undo");
    } else {
        af_editor_status(editor, "Group up to 63 objects that share the same parent");
    }
}

static void af_editor_compound_selection(AfEditorState *editor, bool break_apart) {
    if (editor->drag_mode != AF_EDITOR_DRAG_NONE || editor->tool_command_count != 0U) {
        af_editor_status(editor, "Finish or cancel the current drawing or drag first");
        return;
    }
    if ((break_apart && editor->selected_count != 1U) ||
        (!break_apart && editor->selected_count < 2U)) {
        af_editor_status(editor, break_apart ? "Select one compound path to break apart"
                                             : "Select two or more shapes to combine");
        return;
    }
    AfEditorCommand command = {0};
    AfResult result =
        break_apart
            ? af_editor_command_break_apart(&editor->project, editor->selected_id, &command)
            : af_editor_command_combine(&editor->project, editor->selected_ids,
                                        editor->selected_count, editor->selected_id, &command);
    if (result == AF_RESULT_SUCCESS)
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    if (result != AF_RESULT_SUCCESS) {
        af_editor_status(
            editor,
            result == AF_RESULT_LIMIT_EXCEEDED
                ? "Compound edit exceeds document or undo capacity (8 paths / 4096 commands)"
            : result == AF_RESULT_UNSUPPORTED_FEATURE
                ? "Use static sibling shapes with solid paint, no bindings, and no flow layout"
            : break_apart ? "Break Apart needs a path with at least two contours"
                          : "Cannot combine this selection; check shapes and transforms");
        return;
    }
    af_editor_cancel_vector_tool(editor, NULL);
    editor->active_tool = AF_EDITOR_TOOL_SELECT;
    af_editor_select_only(editor, command.after[0].node.id);
    for (uint32_t i = 1U; i < command.after_count; ++i)
        editor->selected_ids[editor->selected_count++] = command.after[i].node.id;
    editor->selected_path_command = UINT32_MAX;
    editor->selected_path_command_count = 0U;
    af_editor_mark_dirty(editor);
    af_editor_status(
        editor, break_apart
                    ? "Contours separated; holes become filled objects · Ctrl+Z to undo"
                    : "Combined with primary appearance; overlaps become holes · Ctrl+Z to undo");
}

static void af_editor_boolean_selection(AfEditorState *editor, AfPathBooleanOperation operation) {
    if (editor->drag_mode != AF_EDITOR_DRAG_NONE || editor->tool_command_count != 0U) {
        af_editor_status(editor, "Finish or cancel the current drawing or drag first");
        return;
    }
    if (editor->selected_count != 2U) {
        af_editor_status(editor, "Select two closed shapes; Trim cuts the last-selected shape");
        return;
    }
    const uint64_t other = editor->selected_ids[0] == editor->selected_id ? editor->selected_ids[1]
                                                                          : editor->selected_ids[0];
    AfEditorCommand command = {0};
    AfResult result = af_editor_command_boolean(&editor->project, editor->selected_id, other,
                                                operation, &command);
    if (result == AF_RESULT_SUCCESS)
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    if (result != AF_RESULT_SUCCESS) {
        af_editor_status(
            editor, result == AF_RESULT_NOT_FOUND ? "No filled result; original shapes kept"
                    : result == AF_RESULT_LIMIT_EXCEEDED
                        ? "Shape operation exceeds geometry or undo capacity; simplify the inputs"
                        : "Use closed static sibling shapes with solid paint; complex or tiny "
                          "details may need simplification");
        return;
    }
    af_editor_cancel_vector_tool(editor, NULL);
    editor->active_tool = AF_EDITOR_TOOL_NODE;
    af_editor_select_only(editor, command.target_id);
    editor->selected_path_command = 0U;
    af_editor_path_selection_only(editor, 0U);
    af_editor_mark_dirty(editor);
    af_editor_status(
        editor,
        operation == AF_PATH_BOOLEAN_UNION
            ? "Welded · curves approximated as editable lines · Ctrl+Z restores originals"
        : operation == AF_PATH_BOOLEAN_SUBTRACT
            ? "Primary trimmed · curves approximated as editable lines · Ctrl+Z restores originals"
            : "Intersection created · curves approximated as editable lines · Ctrl+Z restores "
              "originals");
}

static void af_editor_convert_selected_to_path(AfEditorState *editor) {
    const AfSceneNode *node = af_scene_find_const(&editor->project.scene, editor->selected_id);
    if (editor->selected_count != 1U || node == NULL) {
        af_editor_status(editor, "Select one shape to convert to a path");
        return;
    }
    if (af_editor_is_text(editor, node->id)) {
        AfProjectNodeSnapshot before, after;
        AfEditorCommand command;
        if (af_project_snapshot(&editor->project, node->id, &before) != AF_RESULT_SUCCESS)
            return;
        after = before;
        after.text = (AfProjectText){0};
        if (af_editor_command_modify(&before, &after, AF_EDITOR_MERGE_NONE, &command) !=
                AF_RESULT_SUCCESS ||
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false) !=
                AF_RESULT_SUCCESS)
            return;
        af_editor_mark_dirty(editor);
    }
    if (node->type != AF_NODE_PATH || node->polygon_sides != 0U) {
        for (uint32_t index = 0U; index < editor->project.track_count; ++index) {
            const AfProjectTrack *track = &editor->project.tracks[index];
            if (track->node_id == node->id && track->property == AF_PROPERTY_CORNER_RADIUS) {
                af_editor_status(editor,
                                 "Remove corner-radius animation before converting to a path");
                return;
            }
        }
        AfEditorCommand command = {0};
        AfResult result = af_editor_command_convert_path(&editor->project, node->id, &command);
        if (result == AF_RESULT_SUCCESS) {
            result = af_editor_history_execute(&editor->history, &editor->project, &command, false,
                                               false);
        }
        if (result != AF_RESULT_SUCCESS) {
            af_editor_status(editor, "Could not convert this shape to a path");
            return;
        }
        af_editor_mark_dirty(editor);
    }
    af_editor_cancel_vector_tool(editor, NULL);
    editor->active_tool = AF_EDITOR_TOOL_NODE;
    editor->selected_path_command = 0U;
    af_editor_path_selection_only(editor, 0U);
    af_editor_status(editor,
                     "Vector points ready · drag points or handles · undo restores the shape");
}

static AfVec2 af_editor_tool_snap_point(AfEditorState *editor, AfVec2 point) {
    return af_editor_snap_point(editor, point);
}

static void af_editor_cancel_vector_tool(AfEditorState *editor, const char *status) {
    editor->tool_point_count = 0U;
    editor->tool_command_count = 0U;
    editor->tool_pen_has_outgoing = false;
    editor->tool_pen_resume_id = 0U;
    editor->tool_pen_resume_count = 0U;
    if (editor->drag_mode == AF_EDITOR_DRAG_TOOL_CREATE) {
        editor->drag_mode = AF_EDITOR_DRAG_NONE;
    }
    if (status != NULL) {
        af_editor_status(editor, status);
    }
}

static void af_editor_set_vector_tool(AfEditorState *editor, AfEditorVectorTool tool) {
    char status[96];
    if (tool >= AF_EDITOR_TOOL_COUNT) {
        return;
    }
    if (tool != editor->active_tool && editor->active_tool == AF_EDITOR_TOOL_PEN &&
        editor->tool_command_count != 0U) {
        af_editor_cancel_vector_tool(editor, NULL);
    }
    editor->active_tool = tool;
    if (tool == AF_EDITOR_TOOL_GRADIENT) {
        const AfStyle *style = af_project_find_node_style(&editor->project, editor->selected_id);
        const AfPaint *paint = af_editor_target_paint(editor, style);
        if (paint && paint->type != AF_PAINT_SOLID)
            editor->gradient_tool_type = paint->type;
        else if (editor->gradient_tool_type == AF_PAINT_SOLID)
            editor->gradient_tool_type = AF_PAINT_LINEAR_GRADIENT;
    }
    editor->drag_mode = AF_EDITOR_DRAG_NONE;
    (void)snprintf(status, sizeof(status), "%s tool · %s", af_editor_vector_tool_name(tool),
                   af_editor_vector_tool_is_construction(tool)
                       ? "draw directly on the canvas"
                       : (tool == AF_EDITOR_TOOL_GRADIENT
                              ? "choose Fill or Outline, then drag a gradient line"
                              : (tool == AF_EDITOR_TOOL_NODE
                                     ? "edit animated path nodes"
                                     : (tool == AF_EDITOR_TOOL_SCISSORS
                                            ? "click a selected path segment to cut it"
                                            : (tool == AF_EDITOR_TOOL_SEGMENT_DELETE
                                                   ? "click a selected path segment to remove it"
                                                   : (tool == AF_EDITOR_TOOL_KNIFE
                                                          ? "drag across selected path segments"
                                                          : "select and transform objects"))))));
    af_editor_status(editor, status);
}

static AfResult af_editor_create_primitive_at(AfEditorState *editor, AfNodeType type,
                                              AfEditorToolRect rect) {
    AfProjectNodeSnapshot snapshot = {0};
    AfEditorCommand command = {0};
    uint64_t id = 0U;
    const char *name = type == AF_NODE_RECTANGLE
                           ? "Rectangle"
                           : (type == AF_NODE_ROUNDED_RECTANGLE ? "Rounded Rectangle" : "Ellipse");
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    if (rect.size.x < 0.1F || rect.size.y < 0.1F) {
        return result;
    }
    uint64_t frame_parent = 0;
    result = af_editor_frame_parent(editor, &frame_parent);
    if (result != AF_RESULT_SUCCESS)
        return result;
    result = af_project_allocate_id(&editor->project, &id);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    snapshot = (AfProjectNodeSnapshot){.node = af_editor_default_node(type, id, rect.position),
                                       .index = editor->project.scene.count};
    snapshot.node.bounds = rect.size;
    if (type == AF_NODE_ROUNDED_RECTANGLE) {
        snapshot.node.corner_radius = fminf(rect.size.x, rect.size.y) * 0.18F;
    }
    snapshot.node.parent_id = frame_parent;
    snapshot.style = af_editor_default_style(id, &snapshot.node);
    snapshot.layout = af_editor_centered_pivot_layout(&snapshot.node);
    snapshot.has_layout = true;
    (void)snprintf(snapshot.name, sizeof(snapshot.name), "%s %llu", name, (unsigned long long)id);
    result = af_editor_command_create(&snapshot, &command);
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_editor_select_only(editor, id);
        editor->selected_path_command = UINT32_MAX;
        af_editor_mark_dirty(editor);
        af_editor_status(editor, "Shape drawn · transform and appearance are animatable");
    }
    return result;
}

static AfResult af_editor_create_path_geometry_with_shape(
    AfEditorState *editor, AfVec2 position, AfVec2 size, const AfPathCommand *commands,
    uint32_t command_count, const char *name, bool closed, uint32_t sides, float inset) {
    AfProjectNodeSnapshot snapshot = {0};
    AfEditorCommand command = {0};
    uint64_t id = 0U;
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    if (commands == NULL || command_count < 2U ||
        command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS || size.x <= 0.0F || size.y <= 0.0F) {
        return result;
    }
    uint64_t frame_parent = 0;
    result = af_editor_frame_parent(editor, &frame_parent);
    if (result != AF_RESULT_SUCCESS)
        return result;
    result = af_project_allocate_id(&editor->project, &id);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    snapshot = (AfProjectNodeSnapshot){.node = af_editor_default_node(AF_NODE_PATH, id, position),
                                       .index = editor->project.scene.count};
    snapshot.node.bounds = size;
    snapshot.node.polygon_sides = sides;
    snapshot.node.polygon_inset = inset;
    snapshot.node.fill =
        closed ? (AfColor){0.58F, 0.34F, 0.94F, 1.0F} : (AfColor){0.25F, 0.72F, 1.0F, 0.0F};
    snapshot.node.parent_id = frame_parent;
    snapshot.style = af_editor_default_style(id, &snapshot.node);
    if (!closed) {
        snapshot.style.stroke.width = 3.0F;
        snapshot.style.stroke.end_width = 3.0F;
        snapshot.style.stroke.cap = AF_STROKE_CAP_ROUND;
        snapshot.style.stroke.join = AF_STROKE_JOIN_ROUND;
        snapshot.style.stroke_paint = af_paint_solid((AfColor){0.25F, 0.72F, 1.0F, 1.0F});
    }
    snapshot.layout = af_editor_centered_pivot_layout(&snapshot.node);
    snapshot.has_layout = true;
    (void)snprintf(snapshot.name, sizeof(snapshot.name), "%s %llu", name, (unsigned long long)id);
    result = af_editor_command_create(&snapshot, &command);
    if (result == AF_RESULT_SUCCESS) {
        command.path_after_count = 1U;
        command.path_command_after_count = command_count;
        command.path_after[0] = (AfProjectPathSnapshot){
            .path = {.id = id,
                     .node_id = id,
                     .command_start = editor->project.path_data.command_count,
                     .command_count = command_count,
                     .fill_rule = AF_PATH_FILL_EVEN_ODD,
                     .geometry_generation = 1U},
            .index = editor->project.path_data.path_count};
        memcpy(command.path_command_after, commands, (size_t)command_count * sizeof(*commands));
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_editor_select_only(editor, id);
        editor->selected_path_command = command_count > 1U ? command_count - 1U : 0U;
        if (commands[editor->selected_path_command].type == AF_PATH_CLOSE) {
            --editor->selected_path_command;
        }
        af_editor_path_selection_only(editor, editor->selected_path_command);
        af_editor_mark_dirty(editor);
        af_editor_status(editor, "Editable path drawn · nodes and handles can be animated");
    }
    return result;
}

static AfResult af_editor_create_path_geometry(AfEditorState *editor, AfVec2 position, AfVec2 size,
                                               const AfPathCommand *commands,
                                               uint32_t command_count, const char *name,
                                               bool closed) {
    return af_editor_create_path_geometry_with_shape(editor, position, size, commands,
                                                     command_count, name, closed, 0U, 0.0F);
}

static AfResult af_editor_create_path_points(AfEditorState *editor, const AfVec2 *points,
                                             uint32_t point_count, bool closed, const char *name) {
    AfPathCommand commands[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 1U];
    AfVec2 minimum = points == NULL || point_count == 0U ? (AfVec2){0} : points[0];
    AfVec2 maximum = minimum;
    if (points == NULL || point_count < 2U ||
        point_count + (closed ? 1U : 0U) > AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 1U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 1U; index < point_count; ++index) {
        minimum.x = fminf(minimum.x, points[index].x);
        minimum.y = fminf(minimum.y, points[index].y);
        maximum.x = fmaxf(maximum.x, points[index].x);
        maximum.y = fmaxf(maximum.y, points[index].y);
    }
    for (uint32_t index = 0U; index < point_count; ++index) {
        commands[index] =
            (AfPathCommand){.type = index == 0U ? AF_PATH_MOVE : AF_PATH_LINE,
                            .point = {points[index].x - minimum.x, points[index].y - minimum.y}};
    }
    if (closed) {
        commands[point_count] = (AfPathCommand){.type = AF_PATH_CLOSE};
    }
    return af_editor_create_path_geometry(
        editor, minimum,
        (AfVec2){fmaxf(0.1F, maximum.x - minimum.x), fmaxf(0.1F, maximum.y - minimum.y)}, commands,
        point_count + (closed ? 1U : 0U), name, closed);
}

static AfResult af_editor_create_path_commands(AfEditorState *editor, const AfPathCommand *source,
                                               uint32_t command_count, bool closed,
                                               const char *name) {
    AfPathCommand commands[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 1U];
    AfVec2 minimum = source == NULL || command_count == 0U ? (AfVec2){0} : source[0].point;
    AfVec2 maximum = minimum;
    if (source == NULL || command_count < 2U ||
        command_count + (closed ? 1U : 0U) > AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 1U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < command_count; ++index) {
        const AfPathCommand *command = &source[index];
        minimum.x = fminf(minimum.x, command->point.x);
        minimum.y = fminf(minimum.y, command->point.y);
        maximum.x = fmaxf(maximum.x, command->point.x);
        maximum.y = fmaxf(maximum.y, command->point.y);
        if (command->type == AF_PATH_CUBIC) {
            minimum.x = fminf(minimum.x, fminf(command->control1.x, command->control2.x));
            minimum.y = fminf(minimum.y, fminf(command->control1.y, command->control2.y));
            maximum.x = fmaxf(maximum.x, fmaxf(command->control1.x, command->control2.x));
            maximum.y = fmaxf(maximum.y, fmaxf(command->control1.y, command->control2.y));
        }
    }
    for (uint32_t index = 0U; index < command_count; ++index) {
        commands[index] = source[index];
        commands[index].point.x -= minimum.x;
        commands[index].point.y -= minimum.y;
        if (commands[index].type == AF_PATH_CUBIC) {
            commands[index].control1.x -= minimum.x;
            commands[index].control1.y -= minimum.y;
            commands[index].control2.x -= minimum.x;
            commands[index].control2.y -= minimum.y;
        }
    }
    if (closed) {
        commands[command_count] = (AfPathCommand){.type = AF_PATH_CLOSE};
    }
    return af_editor_create_path_geometry(
        editor, minimum,
        (AfVec2){fmaxf(0.1F, maximum.x - minimum.x), fmaxf(0.1F, maximum.y - minimum.y)}, commands,
        command_count + (closed ? 1U : 0U), name, closed);
}

static AfVec2 af_editor_tool_line_end(AfVec2 start, AfVec2 end, SDL_Keymod modifiers) {
    if ((modifiers & SDL_KMOD_SHIFT) != 0U) {
        const float dx = end.x - start.x;
        const float dy = end.y - start.y;
        const float length = sqrtf(dx * dx + dy * dy);
        const float step = 0.261799387799F;
        const float angle = roundf(atan2f(dy, dx) / step) * step;
        end = (AfVec2){start.x + cosf(angle) * length, start.y + sinf(angle) * length};
    }
    return end;
}

static bool af_editor_pen_closes_at(const AfEditorState *editor, AfAffine2 camera, AfVec2 screen) {
    if (editor->tool_command_count < 2U ||
        (editor->tool_command_count == 2U && editor->tool_commands[1].type != AF_PATH_CUBIC &&
         !editor->tool_pen_has_outgoing)) {
        return false;
    }
    const AfVec2 first = af_affine_transform_point(camera, editor->tool_commands[0].point);
    const float dx = screen.x - first.x;
    const float dy = screen.y - first.y;
    return dx * dx + dy * dy <= 100.0F;
}

static AfPathCommand af_editor_pen_closing_segment(const AfEditorState *editor) {
    const AfVec2 first = editor->tool_commands[0].point;
    const AfVec2 last = editor->tool_commands[editor->tool_command_count - 1U].point;
    const AfPathCommand *next = &editor->tool_commands[1];
    const AfVec2 incoming =
        next->type == AF_PATH_CUBIC
            ? (AfVec2){2.0F * first.x - next->control1.x, 2.0F * first.y - next->control1.y}
            : first;
    return (AfPathCommand){
        .type = editor->tool_pen_has_outgoing || incoming.x != first.x || incoming.y != first.y
                    ? AF_PATH_CUBIC
                    : AF_PATH_LINE,
        .point = first,
        .control1 = editor->tool_pen_has_outgoing ? editor->tool_pen_outgoing : last,
        .control2 = incoming};
}

static bool af_editor_commit_pen_path(AfEditorState *editor, bool closed) {
    uint32_t count = editor->tool_command_count;
    if (count < 2U || count > AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY) {
        return false;
    }
    if (closed) {
        const AfPathCommand closing = af_editor_pen_closing_segment(editor);
        if (count + (closing.type == AF_PATH_CUBIC ? 2U : 1U) >
            AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY) {
            af_editor_status(editor,
                             "Pen path limit reached · drawing kept · Backspace removes a point");
            return false;
        }
        if (closing.type == AF_PATH_CUBIC) {
            editor->tool_commands[count++] = closing;
        }
    }
    const AfResult result =
        editor->tool_pen_resume_id != 0U
            ? af_editor_commit_pen_continuation(editor, count, closed)
            : af_editor_create_path_commands(editor, editor->tool_commands, count, closed, "Pen");
    if (result != AF_RESULT_SUCCESS) {
        af_editor_status(editor,
                         "Cannot finish Pen path · drawing kept · Backspace removes a point");
        return false;
    }
    af_editor_cancel_vector_tool(editor, closed ? "Pen path closed · endpoints connected"
                                                : "Pen path finished · open endpoints");
    return true;
}

static bool af_editor_vector_tool_mouse_down(AfEditorState *editor, int width, int height,
                                             const SDL_MouseButtonEvent *event) {
    if (!af_editor_vector_tool_is_construction(editor->active_tool))
        return false;
    af_editor_prepare_snapping(editor, false);
    const AfVec2 raw_point =
        af_editor_screen_to_project(editor, width, height, (AfVec2){event->x, event->y});
    AfVec2 point = editor->active_tool == AF_EDITOR_TOOL_PENCIL
                       ? raw_point
                       : af_editor_tool_snap_point(editor, raw_point);
    if (!af_editor_vector_tool_is_construction(editor->active_tool)) {
        return false;
    }
    if (editor->active_tool == AF_EDITOR_TOOL_TEXT) {
        const uint64_t hit = af_editor_hit_test(editor, raw_point);
        if (af_editor_is_text(editor, hit)) {
            af_editor_select_only(editor, hit);
            af_editor_open_text_panel(editor, width, height);
            af_editor_text_panel_mouse(editor, width, height, event->x, event->y);
            return true;
        }
    }
    if (editor->active_tool == AF_EDITOR_TOOL_PEN) {
        const bool close_path = af_editor_pen_closes_at(
            editor, af_editor_camera(editor, width, height), (AfVec2){event->x, event->y});
        if (close_path) {
            (void)af_editor_commit_pen_path(editor, true);
        } else {
            bool append = editor->tool_command_count == 0U;
            if (!append) {
                const AfVec2 previous =
                    editor->tool_commands[editor->tool_command_count - 1U].point;
                const float dx = point.x - previous.x;
                const float dy = point.y - previous.y;
                append = dx * dx + dy * dy > 0.01F;
            }
            if (append && editor->tool_command_count < AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY) {
                const uint32_t index = editor->tool_command_count++;
                if (index == 0U) {
                    editor->tool_commands[index] =
                        (AfPathCommand){.type = AF_PATH_MOVE, .point = point};
                } else {
                    const AfVec2 previous = editor->tool_commands[index - 1U].point;
                    editor->tool_commands[index] = (AfPathCommand){
                        .type = editor->tool_pen_has_outgoing ? AF_PATH_CUBIC : AF_PATH_LINE,
                        .point = point,
                        .control1 =
                            editor->tool_pen_has_outgoing ? editor->tool_pen_outgoing : previous,
                        .control2 = point};
                }
                editor->tool_pen_has_outgoing = false;
            }
            if (event->clicks >= 2U && editor->tool_command_count >= 2U) {
                (void)af_editor_commit_pen_path(editor, false);
            } else {
                editor->tool_start = point;
                editor->tool_current = point;
                editor->drag_mode = AF_EDITOR_DRAG_TOOL_CREATE;
                af_editor_status(editor, editor->tool_command_count == 1U
                                             ? "Pen started · click or drag nodes · Enter finishes"
                                             : "Pen node added · drag for symmetric handles");
            }
        }
        return true;
    }
    editor->tool_start = point;
    editor->tool_current = point;
    editor->tool_point_count = 0U;
    if (editor->active_tool == AF_EDITOR_TOOL_PENCIL) {
        editor->tool_points[editor->tool_point_count++] = point;
    }
    editor->drag_mode = AF_EDITOR_DRAG_TOOL_CREATE;
    af_editor_status(editor, "Drawing · Shift constrains · Ctrl draws from center · Esc cancels");
    return true;
}

static void af_editor_vector_tool_mouse_motion(AfEditorState *editor, int width, int height,
                                               AfVec2 screen) {
    const AfVec2 raw_point = af_editor_screen_to_project(editor, width, height, screen);
    AfVec2 point =
        editor->active_tool == AF_EDITOR_TOOL_PENCIL || editor->active_tool == AF_EDITOR_TOOL_PEN
            ? raw_point
            : af_editor_tool_snap_point(editor, raw_point);
    editor->tool_current = point;
    if (editor->active_tool == AF_EDITOR_TOOL_PEN && editor->tool_command_count != 0U) {
        const float dx = point.x - editor->tool_start.x;
        const float dy = point.y - editor->tool_start.y;
        if ((dx * dx + dy * dy) * editor->zoom * editor->zoom >= 9.0F) {
            const uint32_t index = editor->tool_command_count - 1U;
            if (index != 0U) {
                AfPathCommand *command = &editor->tool_commands[index];
                if (command->type == AF_PATH_LINE) {
                    command->type = AF_PATH_CUBIC;
                    command->control1 = editor->tool_commands[index - 1U].point;
                }
                command->control2 = (AfVec2){editor->tool_start.x - dx, editor->tool_start.y - dy};
            }
            editor->tool_pen_outgoing =
                (AfVec2){editor->tool_start.x + dx, editor->tool_start.y + dy};
            editor->tool_pen_has_outgoing = true;
        }
    } else if (editor->active_tool == AF_EDITOR_TOOL_PENCIL &&
               editor->tool_point_count < AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY) {
        const AfVec2 previous = editor->tool_points[editor->tool_point_count - 1U];
        const float dx = point.x - previous.x;
        const float dy = point.y - previous.y;
        const float minimum = 2.0F / fmaxf(editor->zoom, 0.1F);
        if (dx * dx + dy * dy >= minimum * minimum) {
            editor->tool_points[editor->tool_point_count++] = point;
        }
    }
}

static void af_editor_finish_vector_tool_drag(AfEditorState *editor, int width, int height) {
    if (editor->active_tool == AF_EDITOR_TOOL_TEXT) {
        af_editor_finish_text_tool(editor, width, height);
        return;
    }
    const SDL_Keymod modifiers = SDL_GetModState();
    AfEditorToolRect rect = {0};
    AfPathCommand commands[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 1U];
    uint32_t command_count = 0U;
    if (editor->active_tool == AF_EDITOR_TOOL_KNIFE) {
        (void)af_editor_knife_path(editor, editor->tool_start, editor->tool_current);
        return;
    }
    if (editor->active_tool == AF_EDITOR_TOOL_PEN) {
        return;
    }
    if (editor->active_tool == AF_EDITOR_TOOL_PENCIL) {
        AfVec2 simplified[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY];
        const uint32_t count = af_editor_vector_tool_simplify(
            editor->tool_points, editor->tool_point_count, 1.5F / fmaxf(editor->zoom, 0.1F),
            simplified, AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY);
        if (count >= 2U) {
            (void)af_editor_create_path_points(editor, simplified, count, false, "Pencil");
        } else {
            af_editor_status(editor, "Pencil stroke was too short");
        }
        editor->tool_point_count = 0U;
        return;
    }
    if (editor->active_tool == AF_EDITOR_TOOL_LINE) {
        AfVec2 points[2] = {
            editor->tool_start,
            af_editor_tool_line_end(editor->tool_start, editor->tool_current, modifiers)};
        const float dx = points[1].x - points[0].x;
        const float dy = points[1].y - points[0].y;
        if (dx * dx + dy * dy < 0.25F) {
            af_editor_status(editor, "Draw cancelled · line was too short");
            return;
        }
        (void)af_editor_create_path_points(editor, points, 2U, false, "Line");
        return;
    }
    if (!af_editor_vector_tool_rect(editor->tool_start, editor->tool_current,
                                    (modifiers & SDL_KMOD_CTRL) != 0U,
                                    (modifiers & SDL_KMOD_SHIFT) != 0U, &rect) ||
        rect.size.x < 0.5F || rect.size.y < 0.5F) {
        af_editor_status(editor, "Draw cancelled · shape was too small");
        return;
    }
    if (editor->active_tool == AF_EDITOR_TOOL_RECTANGLE ||
        editor->active_tool == AF_EDITOR_TOOL_ROUNDED_RECTANGLE ||
        editor->active_tool == AF_EDITOR_TOOL_ELLIPSE) {
        const AfNodeType type = editor->active_tool == AF_EDITOR_TOOL_RECTANGLE ? AF_NODE_RECTANGLE
                                : editor->active_tool == AF_EDITOR_TOOL_ROUNDED_RECTANGLE
                                    ? AF_NODE_ROUNDED_RECTANGLE
                                    : AF_NODE_ELLIPSE;
        (void)af_editor_create_primitive_at(editor, type, rect);
        return;
    }
    if (editor->active_tool == AF_EDITOR_TOOL_POLYGON) {
        command_count = af_editor_vector_tool_polygon(rect.size, 6U, 0.0F, commands,
                                                      AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 1U);
    } else if (editor->active_tool == AF_EDITOR_TOOL_STAR) {
        command_count = af_editor_vector_tool_polygon(rect.size, 5U, 0.45F, commands,
                                                      AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 1U);
    } else if (editor->active_tool == AF_EDITOR_TOOL_ARC) {
        command_count = af_editor_vector_tool_arc(rect.size, commands,
                                                  AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 1U);
    } else if (editor->active_tool == AF_EDITOR_TOOL_SPIRAL) {
        command_count = af_editor_vector_tool_spiral(rect.size, 3U, commands,
                                                     AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY + 1U);
    }
    if (command_count >= 2U) {
        const bool closed = editor->active_tool == AF_EDITOR_TOOL_POLYGON ||
                            editor->active_tool == AF_EDITOR_TOOL_STAR;
        (void)af_editor_create_path_geometry_with_shape(
            editor, rect.position, rect.size, commands, command_count,
            af_editor_vector_tool_name(editor->active_tool), closed,
            editor->active_tool == AF_EDITOR_TOOL_STAR      ? 5U
            : editor->active_tool == AF_EDITOR_TOOL_POLYGON ? 6U
                                                            : 0U,
            editor->active_tool == AF_EDITOR_TOOL_STAR ? 0.45F : 0.0F);
    }
}

static bool af_editor_finish_pen_path(AfEditorState *editor) {
    if (editor->active_tool != AF_EDITOR_TOOL_PEN || editor->tool_command_count < 2U) {
        if (editor->active_tool == AF_EDITOR_TOOL_PEN && editor->tool_command_count != 0U) {
            af_editor_status(editor, "Pen needs at least two points · Esc cancels");
        }
        return false;
    }
    return af_editor_commit_pen_path(editor, false);
}

static AfResult af_editor_create_primitive(AfEditorState *editor, AfNodeType type, int width,
                                           int height) {
    AfProjectNodeSnapshot snapshot = {0};
    AfEditorCommand command = {0};
    uint64_t id = 0U;
    const char *name = type == AF_NODE_RECTANGLE
                           ? "Rectangle"
                           : (type == AF_NODE_ROUNDED_RECTANGLE ? "Rounded Rectangle" : "Ellipse");
    uint64_t frame_parent = 0;
    AfResult result = af_editor_frame_parent(editor, &frame_parent);
    if (result != AF_RESULT_SUCCESS)
        return result;
    result = af_project_allocate_id(&editor->project, &id);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    snapshot = (AfProjectNodeSnapshot){
        .node = af_editor_default_node(type, id, af_editor_visible_center(editor, width, height)),
        .index = editor->project.scene.count};
    snapshot.node.parent_id = frame_parent;
    snapshot.style = af_editor_default_style(id, &snapshot.node);
    snapshot.layout = af_editor_centered_pivot_layout(&snapshot.node);
    snapshot.has_layout = true;
    (void)snprintf(snapshot.name, sizeof(snapshot.name), "%s %llu", name, (unsigned long long)id);
    result = af_editor_command_create(&snapshot, &command);
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_editor_select_only(editor, id);
        editor->selected_path_command = UINT32_MAX;
        af_editor_mark_dirty(editor);
        af_editor_status(editor, "Primitive created");
    }
    return result;
}

static AfPath *af_editor_selected_path(AfEditorState *editor) {
    const AfSceneNode *node = af_scene_find_const(&editor->project.scene, editor->selected_id);
    return node != NULL && node->type == AF_NODE_PATH && !af_editor_is_text(editor, node->id)
               ? af_project_find_node_path(&editor->project, node->id)
               : NULL;
}

static bool af_editor_selected_path_has_animation(const AfEditorState *editor) {
    for (uint32_t index = 0U; index < editor->project.track_count; ++index) {
        const AfProjectTrack *track = &editor->project.tracks[index];
        if (track->node_id == editor->selected_id && af_property_is_path(track->property)) {
            return true;
        }
    }
    return false;
}

static bool af_editor_require_static_path_topology(AfEditorState *editor) {
    if (af_editor_is_text(editor, editor->selected_id)) {
        af_editor_status(editor, "Edit text with T, or convert to path before editing glyph nodes");
        return false;
    }
    const AfSceneNode *node = af_scene_find_const(&editor->project.scene, editor->selected_id);
    if (node != NULL && node->polygon_sides != 0U) {
        af_editor_status(editor, "Convert this shape to a path before editing vector points");
        return false;
    }
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
    uint64_t frame_parent = 0;
    AfResult result = af_editor_frame_parent(editor, &frame_parent);
    if (result != AF_RESULT_SUCCESS)
        return result;
    result = af_project_allocate_id(&editor->project, &id);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    snapshot = (AfProjectNodeSnapshot){
        .node = af_editor_default_node(AF_NODE_PATH, id,
                                       af_editor_visible_center(editor, width, height)),
        .index = editor->project.scene.count};
    snapshot.node.bounds = (AfVec2){220.0F, 180.0F};
    snapshot.node.fill = (AfColor){0.78F, 0.42F, 0.96F, 1.0F};
    snapshot.node.parent_id = frame_parent;
    snapshot.style = af_editor_default_style(id, &snapshot.node);
    snapshot.layout = af_editor_centered_pivot_layout(&snapshot.node);
    snapshot.has_layout = true;
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
        af_editor_path_selection_only(editor, editor->selected_path_command);
        af_editor_mark_dirty(editor);
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
    af_editor_path_selection_only(editor, editor->selected_path_command);
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
    const AfSceneNode *node = af_scene_find_const(&editor->project.scene, editor->selected_id);
    if (node != NULL && node->polygon_sides != 0U) {
        af_editor_status(editor, "Convert this shape to a path before editing vector points");
        return AF_RESULT_INVALID_ARGUMENT;
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
        af_editor_mark_dirty(editor);
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

static AfVec2 af_editor_vec_lerp(AfVec2 first, AfVec2 second, float amount) {
    return (AfVec2){first.x + (second.x - first.x) * amount,
                    first.y + (second.y - first.y) * amount};
}

static AfVec2 af_editor_path_clamp_point(AfVec2 point) {
    point.x = af_editor_clamp(point.x, -1000000.0F, 1000000.0F);
    point.y = af_editor_clamp(point.y, -1000000.0F, 1000000.0F);
    return point;
}

static void af_editor_path_shift_anchor(AfPathCommand *commands, uint32_t command_count,
                                        uint32_t index, AfVec2 delta) {
    AfPathCommand *command = NULL;
    if (commands == NULL || index >= command_count || commands[index].type == AF_PATH_CLOSE) {
        return;
    }
    command = &commands[index];
    command->point = af_editor_path_clamp_point(
        (AfVec2){command->point.x + delta.x, command->point.y + delta.y});
    if (command->type == AF_PATH_CUBIC) {
        command->control2 = af_editor_path_clamp_point(
            (AfVec2){command->control2.x + delta.x, command->control2.y + delta.y});
    }
    if (index + 1U < command_count && commands[index + 1U].type == AF_PATH_CUBIC) {
        commands[index + 1U].control1 = af_editor_path_clamp_point((AfVec2){
            commands[index + 1U].control1.x + delta.x, commands[index + 1U].control1.y + delta.y});
    }
}

static AfResult af_editor_arrange_path_points(AfEditorState *editor, bool horizontal,
                                              bool distribute) {
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    uint32_t ordered[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY];
    const uint32_t required = distribute ? 3U : 2U;
    float target = 0.0F;
    if (path == NULL || path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS ||
        editor->path_command_selection_node_id != editor->selected_id ||
        editor->selected_path_command_count < required) {
        af_editor_status(editor, distribute ? "Select at least three nodes to distribute"
                                            : "Select at least two nodes to align");
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (af_editor_selected_path_has_animation(editor)) {
        af_editor_status(editor, "Multi-node geometry edits require a static path");
        return AF_RESULT_INVALID_ARGUMENT;
    }
    memcpy(commands, &editor->project.path_data.commands[path->command_start],
           (size_t)path->command_count * sizeof(*commands));
    for (uint32_t selection = 0U; selection < editor->selected_path_command_count; ++selection) {
        const uint32_t command_index = editor->selected_path_commands[selection];
        if (command_index >= path->command_count || commands[command_index].type == AF_PATH_CLOSE) {
            af_editor_status(editor, "Node selection changed; select the nodes again");
            return AF_RESULT_INVALID_ARGUMENT;
        }
        ordered[selection] = command_index;
        target += horizontal ? commands[command_index].point.x : commands[command_index].point.y;
    }
    if (!distribute) {
        target /= (float)editor->selected_path_command_count;
        for (uint32_t selection = 0U; selection < editor->selected_path_command_count;
             ++selection) {
            const uint32_t command_index = ordered[selection];
            const AfVec2 point = commands[command_index].point;
            const AfVec2 delta =
                horizontal ? (AfVec2){target - point.x, 0.0F} : (AfVec2){0.0F, target - point.y};
            af_editor_path_shift_anchor(commands, path->command_count, command_index, delta);
        }
    } else {
        for (uint32_t selection = 1U; selection < editor->selected_path_command_count;
             ++selection) {
            const uint32_t value = ordered[selection];
            const float coordinate = horizontal ? commands[value].point.x : commands[value].point.y;
            uint32_t insert = selection;
            while (insert > 0U) {
                const float previous = horizontal ? commands[ordered[insert - 1U]].point.x
                                                  : commands[ordered[insert - 1U]].point.y;
                if (previous <= coordinate) {
                    break;
                }
                ordered[insert] = ordered[insert - 1U];
                --insert;
            }
            ordered[insert] = value;
        }
        {
            const float first =
                horizontal ? commands[ordered[0]].point.x : commands[ordered[0]].point.y;
            const float last =
                horizontal ? commands[ordered[editor->selected_path_command_count - 1U]].point.x
                           : commands[ordered[editor->selected_path_command_count - 1U]].point.y;
            const float step = (last - first) / (float)(editor->selected_path_command_count - 1U);
            for (uint32_t selection = 1U; selection + 1U < editor->selected_path_command_count;
                 ++selection) {
                const uint32_t command_index = ordered[selection];
                const float coordinate =
                    horizontal ? commands[command_index].point.x : commands[command_index].point.y;
                target = first + step * (float)selection;
                af_editor_path_shift_anchor(commands, path->command_count, command_index,
                                            horizontal ? (AfVec2){target - coordinate, 0.0F}
                                                       : (AfVec2){0.0F, target - coordinate});
            }
        }
    }
    return af_editor_apply_path_edit(
        editor, commands, path->command_count, path->fill_rule,
        distribute
            ? (horizontal ? "Nodes distributed horizontally" : "Nodes distributed vertically")
            : (horizontal ? "Nodes aligned horizontally" : "Nodes aligned vertically"));
}

static AfResult af_editor_smooth_path_points(AfEditorState *editor, bool symmetric) {
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    uint32_t changed = 0U;
    if (path == NULL || path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS ||
        editor->selected_path_command >= path->command_count) {
        return AF_RESULT_NOT_FOUND;
    }
    if (af_editor_selected_path_has_animation(editor)) {
        af_editor_status(editor, "Handle smoothing requires a static path");
        return AF_RESULT_INVALID_ARGUMENT;
    }
    memcpy(commands, &editor->project.path_data.commands[path->command_start],
           (size_t)path->command_count * sizeof(*commands));
    for (uint32_t selection = 0U;
         selection <
         (editor->selected_path_command_count == 0U ? 1U : editor->selected_path_command_count);
         ++selection) {
        const uint32_t index = editor->selected_path_command_count == 0U
                                   ? editor->selected_path_command
                                   : editor->selected_path_commands[selection];
        AfVec2 direction = {0};
        float incoming_length = 0.0F;
        float outgoing_length = 0.0F;
        float direction_length = 0.0F;
        if (index >= path->command_count || index + 1U >= path->command_count ||
            commands[index].type != AF_PATH_CUBIC || commands[index + 1U].type != AF_PATH_CUBIC) {
            continue;
        }
        {
            const AfVec2 anchor = commands[index].point;
            const AfVec2 incoming = {anchor.x - commands[index].control2.x,
                                     anchor.y - commands[index].control2.y};
            const AfVec2 outgoing = {commands[index + 1U].control1.x - anchor.x,
                                     commands[index + 1U].control1.y - anchor.y};
            incoming_length = sqrtf(incoming.x * incoming.x + incoming.y * incoming.y);
            outgoing_length = sqrtf(outgoing.x * outgoing.x + outgoing.y * outgoing.y);
            if (incoming_length > 0.0001F) {
                direction.x += incoming.x / incoming_length;
                direction.y += incoming.y / incoming_length;
            }
            if (outgoing_length > 0.0001F) {
                direction.x += outgoing.x / outgoing_length;
                direction.y += outgoing.y / outgoing_length;
            }
            direction_length = sqrtf(direction.x * direction.x + direction.y * direction.y);
            if (direction_length <= 0.0001F) {
                direction = (AfVec2){commands[index + 1U].point.x - commands[index - 1U].point.x,
                                     commands[index + 1U].point.y - commands[index - 1U].point.y};
                direction_length = sqrtf(direction.x * direction.x + direction.y * direction.y);
            }
            if (direction_length <= 0.0001F) {
                continue;
            }
            direction.x /= direction_length;
            direction.y /= direction_length;
            if (symmetric) {
                incoming_length = outgoing_length = (incoming_length + outgoing_length) * 0.5F;
            }
            commands[index].handle_mode =
                symmetric ? AF_PATH_HANDLE_SYMMETRIC : AF_PATH_HANDLE_SMOOTH;
            commands[index].control2 =
                af_editor_path_clamp_point((AfVec2){anchor.x - direction.x * incoming_length,
                                                    anchor.y - direction.y * incoming_length});
            commands[index + 1U].control1 =
                af_editor_path_clamp_point((AfVec2){anchor.x + direction.x * outgoing_length,
                                                    anchor.y + direction.y * outgoing_length});
            ++changed;
        }
    }
    if (changed == 0U) {
        af_editor_status(editor, "Select a node with incoming and outgoing curve handles");
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return af_editor_apply_path_edit(editor, commands, path->command_count, path->fill_rule,
                                     symmetric ? "Selected handles made symmetric"
                                               : "Selected handles smoothed");
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
        af_editor_path_selection_only(editor, editor->selected_path_command);
        return AF_RESULT_SUCCESS;
    }
    return AF_RESULT_INVALID_ARGUMENT;
}

static AfResult af_editor_cut_path_segment(AfEditorState *editor, uint32_t selected, float amount) {
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    AfPathCommand cut[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    uint32_t start = 0U;
    uint32_t end = 0U;
    uint32_t write = 0U;
    uint32_t cut_node = 0U;
    if (!af_editor_require_static_path_topology(editor) || path == NULL || selected == 0U ||
        selected >= path->command_count ||
        path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS - 2U) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memcpy(commands, &editor->project.path_data.commands[path->command_start],
           (size_t)path->command_count * sizeof(*commands));
    if ((commands[selected].type != AF_PATH_LINE && commands[selected].type != AF_PATH_CUBIC) ||
        !af_editor_path_contour(commands, path->command_count, selected, &start, &end)) {
        af_editor_status(editor, "Click a visible line or curve segment to cut it");
        return AF_RESULT_INVALID_ARGUMENT;
    }
    amount = af_editor_clamp(amount, 0.02F, 0.98F);
    for (uint32_t index = 0U; index < path->command_count; ++index) {
        if (index == selected) {
            const AfVec2 segment_start = commands[index - 1U].point;
            AfPathCommand left = commands[index];
            AfPathCommand right = commands[index];
            AfVec2 point = {0};
            if (commands[index].type == AF_PATH_CUBIC) {
                const AfVec2 first =
                    af_editor_vec_lerp(segment_start, commands[index].control1, amount);
                const AfVec2 middle =
                    af_editor_vec_lerp(commands[index].control1, commands[index].control2, amount);
                const AfVec2 last =
                    af_editor_vec_lerp(commands[index].control2, commands[index].point, amount);
                const AfVec2 left_control = af_editor_vec_lerp(first, middle, amount);
                const AfVec2 right_control = af_editor_vec_lerp(middle, last, amount);
                point = af_editor_vec_lerp(left_control, right_control, amount);
                left.point = point;
                left.control1 = first;
                left.control2 = left_control;
                right.control1 = right_control;
                right.control2 = last;
            } else {
                point = af_editor_vec_lerp(segment_start, commands[index].point, amount);
                left.point = point;
            }
            cut_node = write;
            cut[write++] = left;
            cut[write++] = (AfPathCommand){.type = AF_PATH_MOVE, .point = point};
            cut[write++] = right;
        } else if (index == end - 1U && commands[index].type == AF_PATH_CLOSE) {
            cut[write++] = (AfPathCommand){.type = AF_PATH_LINE, .point = commands[start].point};
        } else {
            cut[write++] = commands[index];
        }
    }
    if (af_editor_apply_path_edit(editor, cut, write, path->fill_rule,
                                  "Path cut into editable contours") == AF_RESULT_SUCCESS) {
        editor->selected_path_command = cut_node;
        af_editor_path_selection_only(editor, editor->selected_path_command);
        return AF_RESULT_SUCCESS;
    }
    return AF_RESULT_INVALID_ARGUMENT;
}

static AfResult af_editor_remove_path_segment(AfEditorState *editor, uint32_t selected) {
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    AfPathCommand compacted[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    uint32_t start = 0U;
    uint32_t end = 0U;
    uint32_t segment_count = 0U;
    uint32_t write = 0U;
    if (!af_editor_require_static_path_topology(editor) || path == NULL || selected == 0U ||
        selected >= path->command_count ||
        path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memcpy(commands, &editor->project.path_data.commands[path->command_start],
           (size_t)path->command_count * sizeof(*commands));
    if ((commands[selected].type != AF_PATH_LINE && commands[selected].type != AF_PATH_CUBIC) ||
        !af_editor_path_contour(commands, path->command_count, selected, &start, &end)) {
        af_editor_status(editor, "Click a line or curve segment to remove it");
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = start + 1U; index < end; ++index) {
        segment_count +=
            commands[index].type == AF_PATH_LINE || commands[index].type == AF_PATH_CUBIC;
    }
    if (segment_count <= 1U) {
        af_editor_status(editor, "A contour must retain at least one segment");
        return AF_RESULT_INVALID_ARGUMENT;
    }
    commands[selected].type = AF_PATH_MOVE;
    commands[selected].control1 = (AfVec2){0};
    commands[selected].control2 = (AfVec2){0};
    if (end > start && commands[end - 1U].type == AF_PATH_CLOSE) {
        commands[end - 1U] = (AfPathCommand){.type = AF_PATH_LINE, .point = commands[start].point};
    }
    for (uint32_t index = 0U; index < path->command_count; ++index) {
        if (commands[index].type == AF_PATH_MOVE &&
            (index + 1U >= path->command_count || commands[index + 1U].type == AF_PATH_MOVE ||
             commands[index + 1U].type == AF_PATH_CLOSE)) {
            continue;
        }
        compacted[write++] = commands[index];
    }
    if (write < 2U || compacted[0].type != AF_PATH_MOVE) {
        af_editor_status(editor, "Segment removal would create an invalid contour");
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (af_editor_apply_path_edit(editor, compacted, write, path->fill_rule,
                                  "Virtual segment removed") == AF_RESULT_SUCCESS) {
        editor->selected_path_command = 0U;
        af_editor_path_selection_only(editor, editor->selected_path_command);
        return AF_RESULT_SUCCESS;
    }
    return AF_RESULT_INVALID_ARGUMENT;
}

static AfResult af_editor_knife_path(AfEditorState *editor, AfVec2 project_start,
                                     AfVec2 project_end) {
    const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand cut[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    AfEditorKnifeResult knife = {0};
    AfAffine2 inverse = {0};
    if (!af_editor_require_static_path_topology(editor) || node == NULL || path == NULL ||
        !af_affine_inverse(node->world_transform, &inverse)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    AfResult result = af_editor_vector_tool_knife(
        &editor->project.path_data.commands[path->command_start], path->command_count,
        af_affine_transform_point(inverse, project_start),
        af_affine_transform_point(inverse, project_end), cut, AF_EDITOR_COMMAND_MAX_PATH_COMMANDS,
        &knife);
    if (result != AF_RESULT_SUCCESS) {
        af_editor_status(editor, result == AF_RESULT_NOT_FOUND
                                     ? "Knife did not cross a segment away from its endpoints"
                                     : (result == AF_RESULT_LIMIT_EXCEEDED
                                            ? "Knife cuts exceed bounded path capacity"
                                            : "Drag a longer knife line across the selected path"));
        return result;
    }
    char status[64];
    (void)snprintf(status, sizeof(status), "%u knife cut%s applied", knife.cut_count,
                   knife.cut_count == 1U ? "" : "s");
    result = af_editor_apply_path_edit(editor, cut, knife.command_count, path->fill_rule, status);
    if (result == AF_RESULT_SUCCESS) {
        editor->selected_path_command = knife.first_cut_command;
        af_editor_path_selection_only(editor, editor->selected_path_command);
    }
    return result;
}

static AfResult af_editor_delete_path_point(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    AfPathCommand compacted[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
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
    if (editor->selected_path_command_count > 1U) {
        uint32_t deleted = 0U;
        uint32_t write = 0U;
        uint32_t fallback = 0U;
        uint32_t fallback_after = 0U;
        for (uint32_t contour = 0U; contour < path->command_count;) {
            uint32_t contour_end = contour + 1U;
            uint32_t contour_segments = 0U;
            uint32_t removed_segments = 0U;
            while (contour_end < path->command_count &&
                   commands[contour_end].type != AF_PATH_MOVE) {
                ++contour_end;
            }
            for (uint32_t index = contour + 1U; index < contour_end; ++index) {
                if (commands[index].type == AF_PATH_LINE || commands[index].type == AF_PATH_CUBIC) {
                    ++contour_segments;
                    if (af_editor_path_selection_contains(editor, index)) {
                        ++removed_segments;
                    }
                }
            }
            if (removed_segments != 0U && contour_segments <= removed_segments) {
                af_editor_status(editor, "Each contour must retain at least one segment");
                return AF_RESULT_INVALID_ARGUMENT;
            }
            contour = contour_end;
        }
        for (uint32_t index = selected; index > 0U; --index) {
            if (commands[index].type == AF_PATH_MOVE) {
                fallback = index;
                break;
            }
        }
        for (uint32_t index = 0U; index < path->command_count; ++index) {
            const bool segment =
                commands[index].type == AF_PATH_LINE || commands[index].type == AF_PATH_CUBIC;
            if (segment && af_editor_path_selection_contains(editor, index)) {
                ++deleted;
                continue;
            }
            if (index == fallback) {
                fallback_after = write;
            }
            compacted[write++] = commands[index];
        }
        if (deleted == 0U) {
            af_editor_status(editor, "Select segment nodes to delete");
            return AF_RESULT_INVALID_ARGUMENT;
        }
        {
            char status[64];
            (void)snprintf(status, sizeof(status), "%u path nodes deleted", deleted);
            if (af_editor_apply_path_edit(editor, compacted, write, path->fill_rule, status) ==
                AF_RESULT_SUCCESS) {
                editor->selected_path_command = fallback_after;
                af_editor_path_selection_only(editor, editor->selected_path_command);
                return AF_RESULT_SUCCESS;
            }
        }
        return AF_RESULT_INVALID_ARGUMENT;
    }
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
        af_editor_path_selection_only(editor, editor->selected_path_command);
        return AF_RESULT_SUCCESS;
    }
    return AF_RESULT_INVALID_ARGUMENT;
}

static AfResult af_editor_convert_path_point(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    const uint32_t selected = editor->selected_path_command;
    if (editor->selected_path_command_count > 1U) {
        af_editor_status(editor, "Convert one segment at a time");
        return AF_RESULT_INVALID_ARGUMENT;
    }
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
        AfResult result = AF_RESULT_INVALID_ARGUMENT;
        memmove(&commands[end - 1U], &commands[end],
                (size_t)(path->command_count - end) * sizeof(*commands));
        editor->selected_path_command = end > start + 1U ? end - 2U : start;
        result = af_editor_apply_path_edit(editor, commands, path->command_count - 1U,
                                           path->fill_rule, "Path contour opened");
        if (result == AF_RESULT_SUCCESS) {
            af_editor_path_selection_only(editor, editor->selected_path_command);
        }
        return result;
    }
    if (path->command_count >= AF_EDITOR_COMMAND_MAX_PATH_COMMANDS) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memmove(&commands[end + 1U], &commands[end],
            (size_t)(path->command_count - end) * sizeof(*commands));
    commands[end] = (AfPathCommand){.type = AF_PATH_CLOSE};
    editor->selected_path_command = end > start + 1U ? end - 1U : start;
    {
        const AfResult result = af_editor_apply_path_edit(
            editor, commands, path->command_count + 1U, path->fill_rule, "Path contour closed");
        if (result == AF_RESULT_SUCCESS) {
            af_editor_path_selection_only(editor, editor->selected_path_command);
        }
        return result;
    }
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
    reversed[write++] = (AfPathCommand){.type = AF_PATH_MOVE,
                                        .point = commands[segment_end - 1U].point,
                                        .handle_mode = commands[segment_end - 1U].handle_mode};
    for (uint32_t index = segment_end; index > start + 1U;) {
        AfPathCommand segment = commands[--index];
        segment.point = index == start + 1U ? commands[start].point : commands[index - 1U].point;
        segment.handle_mode = commands[index - 1U].handle_mode;
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
    {
        const AfResult result = af_editor_apply_path_edit(editor, commands, path->command_count,
                                                          path->fill_rule, "Path contour reversed");
        if (result == AF_RESULT_SUCCESS) {
            af_editor_path_selection_only(editor, editor->selected_path_command);
        }
        return result;
    }
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
        af_editor_path_selection_only(editor, editor->selected_path_command);
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
        af_editor_mark_dirty(editor);
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
            af_editor_path_selection_clear(editor);
        } else {
            uint32_t path_selection_write = 0U;
            if (editor->selected_path_command >= path->command_count ||
                editor->project.path_data
                        .commands[path->command_start + editor->selected_path_command]
                        .type == AF_PATH_CLOSE) {
                editor->selected_path_command = 0U;
            }
            if (editor->path_command_selection_node_id != editor->selected_id) {
                af_editor_path_selection_clear(editor);
            }
            for (uint32_t index = 0U; index < editor->selected_path_command_count; ++index) {
                const uint32_t command_index = editor->selected_path_commands[index];
                if (command_index < path->command_count &&
                    editor->project.path_data.commands[path->command_start + command_index].type !=
                        AF_PATH_CLOSE) {
                    editor->selected_path_commands[path_selection_write++] = command_index;
                }
            }
            editor->selected_path_command_count = path_selection_write;
            if (!af_editor_path_selection_contains(editor, editor->selected_path_command)) {
                const uint32_t primary = editor->selected_path_command;
                (void)af_editor_path_selection_add(editor, primary);
                editor->selected_path_command = primary;
            }
        }
    } else {
        editor->selected_path_command = UINT32_MAX;
        af_editor_path_selection_clear(editor);
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
    const AfProjectTrack *cel = af_editor_frame_owner(&editor->project, editor->selected_id);
    if (cel && cel->node_id == editor->selected_id) {
        af_editor_frame_new(editor, true);
        return AF_RESULT_SUCCESS;
    }
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
        if (copies[index].has_image_binding) {
            copies[index].image_binding.node_id = new_ids[index];
        }
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
        af_editor_mark_dirty(editor);
        af_editor_build_hierarchy(editor);
        for (uint32_t row = 0U; row < editor->hierarchy_count; ++row) {
            if (editor->project.scene.nodes[editor->hierarchy_order[row]].id ==
                editor->selected_id) {
                editor->hierarchy_scroll = row;
                break;
            }
        }
        af_editor_status(editor, root_count == 1U ? "Object duplicated" : "Objects duplicated");
    } else {
        char message[128];
        (void)snprintf(message, sizeof(message), "Duplicate failed: %s", af_result_string(result));
        af_editor_status(editor, message);
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
    (void)af_platform_text_input_set(&editor->window, true);
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
    (void)af_platform_text_input_set(&editor->window, false);
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

static void af_editor_combine_points(AfEditorState *editor, bool merge) {
    AfPath *path = af_editor_selected_path(editor);
    if (!path || editor->selected_path_command_count != 2U ||
        !af_editor_require_static_path_topology(editor)) {
        af_editor_status(editor, "Select two path points with the Node tool");
        return;
    }
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    uint32_t count = 0, selected = 0;
    AfResult result = af_editor_vector_combine(
        editor->project.path_data.commands + path->command_start, path->command_count,
        editor->selected_path_commands[0], editor->selected_path_commands[1], merge, commands,
        AF_EDITOR_COMMAND_MAX_PATH_COMMANDS, &count, &selected);
    if (result == AF_RESULT_SUCCESS)
        result = af_editor_apply_path_edit(editor, commands, count, path->fill_rule,
                                           merge ? "Points merged · Ctrl+Z to undo"
                                                 : "Line added · use To Curve to shape it");
    if (result == AF_RESULT_SUCCESS)
        af_editor_path_selection_only(editor, selected);
    else
        af_editor_status(editor,
                         "Points could not be combined; each contour must retain a segment");
}

static void af_editor_cusp_path_points(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    if (!path || path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS)
        return;
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    memcpy(commands, editor->project.path_data.commands + path->command_start,
           (size_t)path->command_count * sizeof(*commands));
    for (uint32_t i = 0; i < path->command_count; ++i)
        if (i == editor->selected_path_command || af_editor_path_selection_contains(editor, i))
            commands[i].handle_mode = AF_PATH_HANDLE_CUSP;
    (void)af_editor_apply_path_edit(editor, commands, path->command_count, path->fill_rule,
                                    "Cusp handles · drag each handle independently");
}

static void af_editor_join_endpoints(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    if (editor->tool_command_count != 0U || editor->drag_mode != AF_EDITOR_DRAG_NONE) {
        af_editor_status(editor, "Finish or cancel drawing first");
        return;
    }
    if (!af_editor_require_static_path_topology(editor))
        return;
    if (path == NULL || editor->selected_path_command_count != 2U) {
        af_editor_status(
            editor,
            "Node tool: select two open endpoints · combine separate objects first (Ctrl+L)");
        return;
    }
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    uint32_t count = 0U;
    AfResult result = af_editor_vector_join(
        editor->project.path_data.commands + path->command_start, path->command_count,
        editor->selected_path_commands[0], editor->selected_path_commands[1], commands,
        AF_EDITOR_COMMAND_MAX_PATH_COMMANDS, &count);
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_apply_path_edit(editor, commands, count, path->fill_rule,
                                           "Endpoints joined · Ctrl+Z to undo");
        if (result == AF_RESULT_SUCCESS)
            af_editor_path_selection_only(editor, 0U);
    } else {
        af_editor_status(editor, "Select two endpoints of open contours · bounded to 512 commands");
    }
}

static void af_editor_continue_path(AfEditorState *editor) {
    AfPath *path = af_editor_selected_path(editor);
    if (editor->tool_command_count != 0U || editor->drag_mode != AF_EDITOR_DRAG_NONE) {
        af_editor_status(editor, "Finish or cancel drawing first");
        return;
    }
    if (!af_editor_require_static_path_topology(editor))
        return;
    uint32_t start, end;
    if (!path || path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS ||
        !af_editor_path_contour(editor->project.path_data.commands + path->command_start,
                                path->command_count, editor->selected_path_command, &start, &end)) {
        af_editor_status(editor, "Node tool: select an open endpoint to continue with Pen");
        return;
    }
    const AfPathCommand *commands = editor->project.path_data.commands + path->command_start;
    if (end <= start + 1U || end - start >= AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY ||
        commands[end - 1U].type == AF_PATH_CLOSE ||
        (editor->selected_path_command != start && editor->selected_path_command != end - 1U)) {
        af_editor_status(editor, "Continue requires the first or last node of an open contour");
        return;
    }
    uint64_t ancestor = editor->selected_id;
    for (uint32_t depth = 0; ancestor != AF_INVALID_NODE_ID && depth <= AF_MAX_HIERARCHY_DEPTH;
         ++depth) {
        for (uint32_t i = 0; i < editor->project.track_count; ++i) {
            if (editor->project.tracks[i].node_id == ancestor) {
                af_editor_status(editor,
                                 "Continue requires a static path and static parent transforms");
                return;
            }
        }
        const AfSceneNode *parent = af_scene_find_const(&editor->project.scene, ancestor);
        if (!parent)
            return;
        ancestor = parent->parent_id;
    }
    (void)af_layout_evaluate_scene(&editor->project.scene);
    const AfSceneNode *node = af_scene_find_const(&editor->project.scene, editor->selected_id);
    if (!node || !af_affine_inverse(node->world_transform, &editor->tool_pen_resume_inverse))
        return;
    af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_PEN);
    if (editor->selected_path_command == start)
        af_editor_vector_reverse_open(commands + start, end - start, editor->tool_commands);
    else
        memcpy(editor->tool_commands, commands + start, (size_t)(end - start) * sizeof(*commands));
    for (uint32_t i = 0U; i < end - start; ++i) {
        AfPathCommand *command = &editor->tool_commands[i];
        command->point = af_affine_transform_point(node->world_transform, command->point);
        if (command->type == AF_PATH_CUBIC) {
            command->control1 = af_affine_transform_point(node->world_transform, command->control1);
            command->control2 = af_affine_transform_point(node->world_transform, command->control2);
        }
    }
    editor->tool_command_count = end - start;
    editor->tool_pen_resume_count = end - start;
    editor->tool_pen_resume_start = start;
    editor->tool_pen_resume_end = end;
    editor->tool_pen_resume_id = editor->selected_id;
    editor->tool_pen_resume_generation = path->geometry_generation;
    editor->preview_playing = false;
    const AfPathCommand *last = &editor->tool_commands[end - start - 1U];
    editor->tool_pen_has_outgoing = last->type == AF_PATH_CUBIC;
    editor->tool_pen_outgoing =
        (AfVec2){2.0F * last->point.x - last->control2.x, 2.0F * last->point.y - last->control2.y};
    af_editor_status(
        editor, "Continue Pen · click to extend · click opposite endpoint to close · Esc cancels");
}

static AfResult af_editor_commit_pen_continuation(AfEditorState *editor, uint32_t count,
                                                  bool closed) {
    AfPath *path = af_editor_selected_path(editor);
    if (!path || editor->selected_id != editor->tool_pen_resume_id ||
        path->geometry_generation != editor->tool_pen_resume_generation ||
        !af_editor_require_static_path_topology(editor))
        return AF_RESULT_INVALID_ARGUMENT;
    if (!closed && count == editor->tool_pen_resume_count)
        return AF_RESULT_SUCCESS;
    const uint32_t start = editor->tool_pen_resume_start;
    const uint32_t end = editor->tool_pen_resume_end;
    const uint32_t total = path->command_count - (end - start) + count + (closed ? 1U : 0U);
    if (total > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS)
        return AF_RESULT_LIMIT_EXCEEDED;
    (void)af_layout_evaluate_scene(&editor->project.scene);
    const AfSceneNode *node = af_scene_find_const(&editor->project.scene, editor->selected_id);
    AfAffine2 inverse;
    if (!node || !af_affine_inverse(node->world_transform, &inverse) ||
        memcmp(&inverse, &editor->tool_pen_resume_inverse, sizeof(inverse)) != 0)
        return AF_RESULT_INVALID_ARGUMENT;
    AfPathCommand commands[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    const AfPathCommand *original = editor->project.path_data.commands + path->command_start;
    memcpy(commands, original, (size_t)start * sizeof(*commands));
    for (uint32_t i = 0; i < count; ++i) {
        AfPathCommand command = editor->tool_commands[i];
        command.point = af_affine_transform_point(inverse, command.point);
        if (command.type == AF_PATH_CUBIC) {
            command.control1 = af_affine_transform_point(inverse, command.control1);
            command.control2 = af_affine_transform_point(inverse, command.control2);
        }
        commands[start + i] = command;
    }
    uint32_t write = start + count;
    if (closed)
        commands[write++] = (AfPathCommand){.type = AF_PATH_CLOSE};
    memcpy(commands + write, original + end,
           (size_t)(path->command_count - end) * sizeof(*commands));
    return af_editor_apply_path_edit(editor, commands, total, path->fill_rule,
                                     "Path continued · Ctrl+Z to undo");
}

static bool af_editor_expand_outline(AfEditorState *editor, bool offset, float distance) {
    if (editor->selected_count != 1U || editor->tool_command_count != 0U ||
        editor->drag_mode != AF_EDITOR_DRAG_NONE) {
        af_editor_status(editor, "Select one shape and finish drawing first");
        return false;
    }
    AfEditorCommand command = {0};
    AfResult result = af_editor_command_outline(&editor->project, editor->selected_id, offset,
                                                distance, &command);
    if (result == AF_RESULT_SUCCESS)
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    if (result != AF_RESULT_SUCCESS) {
        af_editor_status(editor, result == AF_RESULT_NOT_FOUND ? "Result is empty · original kept"
                                 : result == AF_RESULT_LIMIT_EXCEEDED
                                     ? "Geometry exceeds expansion or undo capacity · original kept"
                                     : "Use a static solid shape; offset needs a closed fill, "
                                       "outline needs a visible stroke");
        return false;
    }
    af_editor_select_only(editor, command.after[command.after_count - 1U].node.id);
    af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_NODE);
    af_editor_path_selection_only(editor, 0U);
    af_editor_mark_dirty(editor);
    af_editor_status(
        editor, offset
                    ? "Offset applied · curves approximated · Ctrl+Z to undo"
                    : "Outline converted to filled path · source fill retained · Ctrl+Z to undo");
    return true;
}

static void af_editor_apply_offset_panel(AfEditorState *editor) {
    char *end = NULL;
    const float distance = strtof(editor->offset_buffer, &end);
    if (end == editor->offset_buffer || *end != '\0' || !isfinite(distance) || distance == 0) {
        af_editor_status(editor, "Enter a nonzero offset: positive expands, negative insets");
        return;
    }
    if (af_editor_expand_outline(editor, true, distance))
        editor->offset_panel_open = false;
}
