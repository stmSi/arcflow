/* Cache stationary world bounds once per gesture; pointer motion does no allocation. */
static void af_editor_prepare_snapping(AfEditorState *editor, bool exclude_selection) {
    editor->snap_target_count = 0;
    editor->snap_result = (AfEditorSnapResult){0};
    bool first = true;
    for (uint32_t i = 0; i < editor->preview_scene.count; ++i) {
        AfSceneNode node = editor->preview_scene.nodes[i];
        if (!af_editor_frame_node_active(editor, node.id))
            continue;
        AfEditorSnapBounds bounds;
        if (!af_editor_scene_node_bounds(&editor->preview_scene, &node, false, &bounds))
            continue;
        const bool selected = af_editor_selection_contains(editor, node.id) ||
                              af_editor_selection_has_selected_ancestor(editor, node.id);
        if (selected) {
            if (first) {
                editor->snap_drag_bounds = bounds;
                first = false;
            } else {
                editor->snap_drag_bounds.minimum.x =
                    fminf(editor->snap_drag_bounds.minimum.x, bounds.minimum.x);
                editor->snap_drag_bounds.minimum.y =
                    fminf(editor->snap_drag_bounds.minimum.y, bounds.minimum.y);
                editor->snap_drag_bounds.maximum.x =
                    fmaxf(editor->snap_drag_bounds.maximum.x, bounds.maximum.x);
                editor->snap_drag_bounds.maximum.y =
                    fmaxf(editor->snap_drag_bounds.maximum.y, bounds.maximum.y);
            }
        }
        if ((exclude_selection && selected) || node.id == editor->tool_pen_resume_id)
            continue;
        if (editor->snap_target_count < AF_MAX_NODES)
            editor->snap_targets[editor->snap_target_count++] =
                (AfEditorSnapTarget){bounds, node.id};
    }
    if (first)
        editor->snap_drag_bounds = (AfEditorSnapBounds){0};
}
static AfVec2 af_editor_snap_point(AfEditorState *editor, AfVec2 point) {
    editor->snap_result = (AfEditorSnapResult){0};
    if (editor->snapping) {
        editor->snap_result =
            af_editor_snap_bounds((AfEditorSnapBounds){point, point}, editor->snap_targets,
                                  editor->snap_target_count, editor->zoom, (AfVec2){0});
        point.x += editor->snap_result.correction.x;
        point.y += editor->snap_result.correction.y;
    }
    return point;
}
static AfVec2 af_editor_snap_move(AfEditorState *editor, AfVec2 delta, AfVec2 constraint) {
    editor->snap_result = (AfEditorSnapResult){0};
    if (editor->snapping) {
        AfEditorSnapBounds bounds = editor->snap_drag_bounds;
        bounds.minimum.x += delta.x;
        bounds.maximum.x += delta.x;
        bounds.minimum.y += delta.y;
        bounds.maximum.y += delta.y;
        editor->snap_result = af_editor_snap_bounds(
            bounds, editor->snap_targets, editor->snap_target_count, editor->zoom, constraint);
        delta.x += editor->snap_result.correction.x;
        delta.y += editor->snap_result.correction.y;
    }
    return delta;
}
static void af_editor_render_snap_guides(AfEditorState *editor, AfAffine2 camera, float x, float y,
                                         float width, float height) {
    if (!editor->smart_guides_visible || !editor->snapping ||
        (editor->drag_mode == AF_EDITOR_DRAG_NONE && !editor->tool_command_count))
        return;
    const AfColor color = {.2F, .95F, .78F, 1};
    for (uint32_t axis = 0; axis < 2; ++axis) {
        const AfEditorSnapMatch *match = &editor->snap_result.axes[axis];
        if (!match->active)
            continue;
        AfVec2 a =
            af_affine_transform_point(camera, axis ? (AfVec2){match->minimum, match->coordinate}
                                                   : (AfVec2){match->coordinate, match->minimum});
        AfVec2 b =
            af_affine_transform_point(camera, axis ? (AfVec2){match->maximum, match->coordinate}
                                                   : (AfVec2){match->coordinate, match->maximum});
        if ((axis && (a.y < y || a.y >= y + height)) || (!axis && (a.x < x || a.x >= x + width)))
            continue;
        if (axis) {
            a.x = af_editor_clamp(a.x - 12, x, x + width);
            b.x = af_editor_clamp(b.x + 12, x, x + width);
        } else {
            a.y = af_editor_clamp(a.y - 12, y, y + height);
            b.y = af_editor_clamp(b.y + 12, y, y + height);
        }
        af_editor_add_line(editor, a, b, 1.5F, color, 90055);
        const char *labels[2][3] = {{"Left edge", "Center X", "Right edge"},
                                    {"Top edge", "Center Y", "Bottom edge"}};
        const float lx = af_editor_clamp(b.x + 7, x, x + fmaxf(0, width - 90)),
                    ly = af_editor_clamp(b.y + 7, y, y + fmaxf(0, height - 18));
        af_editor_add_rect(editor, lx - 3, ly - 3, 88, 17, (AfColor){.06F, .12F, .15F, .96F},
                           90056);
        af_editor_add_text(editor, lx, ly, labels[axis][match->feature], .65F, 82, color, 90057);
    }
}
