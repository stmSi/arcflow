/* Paint editing uses the existing selection snapshot transaction and color inspector. */
static uint32_t af_editor_gradient_stop(const AfEditorState *editor, const AfPaint *paint) {
    return editor->gradient_target == editor->selected_id &&
                   editor->gradient_stop < paint->stop_count
               ? editor->gradient_stop
               : 0;
}
static void af_editor_gradient_action(AfEditorState *editor, uint32_t action) {
    if (editor->selected_count != 1)
        return;
    AfSceneNode *node = af_scene_find(&editor->project.scene, editor->selected_id);
    AfStyle *style = af_project_find_node_style(&editor->project, editor->selected_id);
    if (!node || !style)
        return;
    AfPaint *paint = &style->fill;
    uint32_t selected = af_editor_gradient_stop(editor, paint);
    editor->color_target_outline = false;
    editor->gradient_target = editor->selected_id;
    if ((action == 1 || action == 2) && paint->stop_count) {
        editor->gradient_stop =
            (selected + (action == 1 ? paint->stop_count - 1 : 1)) % paint->stop_count;
        af_editor_status(
            editor,
            "Gradient stop selected · use the color picker or hex field to change its color");
        return;
    }
    if (action != 0 && paint->type == AF_PAINT_SOLID)
        return;
    if (action == 3 && paint->stop_count >= AF_MAX_GRADIENT_STOPS)
        return;
    if (action == 4 && paint->stop_count <= 2)
        return;
    af_editor_begin_selection_modify(editor, false);
    if (action == 0) {
        AfPaintType next = (AfPaintType)(((uint32_t)paint->type + 1) % 4);
        if (paint->type == AF_PAINT_SOLID) {
            af_editor_apply_style_action(node, style, 3);
            selected = 0;
        } else if (next == AF_PAINT_SOLID)
            *paint = af_paint_solid(node->fill);
        else
            paint->type = next;
        if (next == AF_PAINT_RADIAL_GRADIENT || next == AF_PAINT_CONIC_GRADIENT) {
            paint->start = (AfVec2){node->bounds.x * .5F, node->bounds.y * .5F};
            paint->end_radius =
                af_editor_clamp(fmaxf(node->bounds.x, node->bounds.y) * .5F, 1, 100000);
        }
    } else if (action == 3) {
        uint32_t gap = 0;
        for (uint32_t i = 1; i + 1 < paint->stop_count; ++i)
            if (paint->stops[i + 1].offset - paint->stops[i].offset >
                paint->stops[gap + 1].offset - paint->stops[gap].offset)
                gap = i;
        AfGradientStop a = paint->stops[gap], b = paint->stops[gap + 1];
        memmove(paint->stops + gap + 2, paint->stops + gap + 1,
                (paint->stop_count - gap - 1) * sizeof(AfGradientStop));
        paint->stops[gap + 1] = (AfGradientStop){
            .offset = (a.offset + b.offset) * .5F,
            .color = {(a.color.r + b.color.r) * .5F, (a.color.g + b.color.g) * .5F,
                      (a.color.b + b.color.b) * .5F, (a.color.a + b.color.a) * .5F}};
        ++paint->stop_count;
        selected = gap + 1;
    } else if (action == 4) {
        memmove(paint->stops + selected, paint->stops + selected + 1,
                (paint->stop_count - selected - 1) * sizeof(AfGradientStop));
        --paint->stop_count;
        if (selected >= paint->stop_count)
            selected = paint->stop_count - 1;
    } else if (action == 5) {
        for (uint32_t i = 0; i < paint->stop_count / 2; ++i) {
            AfGradientStop a = paint->stops[i], b = paint->stops[paint->stop_count - 1 - i];
            a.offset = 1 - a.offset;
            b.offset = 1 - b.offset;
            paint->stops[i] = b;
            paint->stops[paint->stop_count - 1 - i] = a;
        }
        if (paint->stop_count % 2)
            paint->stops[paint->stop_count / 2].offset =
                1 - paint->stops[paint->stop_count / 2].offset;
        selected = paint->stop_count - 1 - selected;
    } else if (action == 6 || action == 7) {
        paint->stops[selected].offset = af_editor_clamp(
            paint->stops[selected].offset + (action == 6 ? -.05F : .05F),
            selected ? paint->stops[selected - 1].offset : 0,
            selected + 1 < paint->stop_count ? paint->stops[selected + 1].offset : 1);
    } else if (action == 8 || action == 9) {
        const float angle = (action == 8 ? -1 : 1) * .261799388F;
        if (paint->type == AF_PAINT_RADIAL_GRADIENT)
            paint->end_radius =
                af_editor_clamp(paint->end_radius * (action == 8 ? .9F : 1.1F), 1, 100000);
        else if (paint->type == AF_PAINT_CONIC_GRADIENT)
            paint->angle = remainderf(paint->angle + angle, 6.283185307F);
        else {
            AfVec2 delta = {paint->end.x - paint->start.x, paint->end.y - paint->start.y};
            paint->end = (AfVec2){paint->start.x + delta.x * cosf(angle) - delta.y * sinf(angle),
                                  paint->start.y + delta.x * sinf(angle) + delta.y * cosf(angle)};
        }
    }
    style->inherit_flags &= ~(uint32_t)AF_STYLE_INHERIT_FILL;
    (void)af_style_mark_paint_changed(style);
    editor->gradient_stop = selected;
    (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_STYLE, false);
    af_editor_status(editor,
                     "Gradient updated · select a stop, then edit its color in the inspector");
}
static bool af_editor_gradient_geometry(AfEditorState *editor, int width, int height,
                                        AfAffine2 *frame, AfVec2 *start, AfVec2 *end) {
    if (editor->text_panel_open || editor->selected_count != 1 ||
        editor->active_tool != AF_EDITOR_TOOL_SELECT)
        return false;
    const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    const AfStyle *style = af_project_find_node_style(&editor->project, editor->selected_id);
    if (!node || !style || style->fill.type == AF_PAINT_SOLID)
        return false;
    const AfPaint *paint = &style->fill;
    *frame = af_affine_multiply(af_editor_camera(editor, width, height),
                                af_affine_multiply(node->world_transform, paint->transform));
    *start = paint->start;
    *end = paint->type == AF_PAINT_LINEAR_GRADIENT
               ? paint->end
               : (AfVec2){start->x + cosf(paint->angle) * fmaxf(1, paint->end_radius),
                          start->y + sinf(paint->angle) * fmaxf(1, paint->end_radius)};
    return true;
}
static void af_editor_gradient_motion(AfEditorState *editor, int width, int height, float x,
                                      float y) {
    AfAffine2 frame, inverse;
    AfVec2 start, end;
    if (!editor->gradient_drag ||
        !af_editor_gradient_geometry(editor, width, height, &frame, &start, &end) ||
        !af_affine_inverse(frame, &inverse))
        return;
    AfStyle *style = af_project_find_node_style(&editor->project, editor->selected_id);
    AfVec2 point = af_affine_transform_point(inverse, (AfVec2){x, y});
    if (!isfinite(point.x) || !isfinite(point.y))
        return;
    if (editor->gradient_drag == 1)
        style->fill.start = point;
    else if (style->fill.type == AF_PAINT_LINEAR_GRADIENT)
        style->fill.end = point;
    else {
        style->fill.end_radius =
            af_editor_clamp(hypotf(point.x - start.x, point.y - start.y), 1, 100000);
        style->fill.angle = atan2f(point.y - start.y, point.x - start.x);
    }
    style->inherit_flags &= ~(uint32_t)AF_STYLE_INHERIT_FILL;
    (void)af_style_mark_paint_changed(style);
    af_editor_mark_dirty(editor);
}
static void af_editor_render_gradient_handles(AfEditorState *editor, int width, int height) {
    AfAffine2 frame;
    AfVec2 start, end;
    if (!af_editor_gradient_geometry(editor, width, height, &frame, &start, &end))
        return;
    start = af_affine_transform_point(frame, start);
    end = af_affine_transform_point(frame, end);
    af_editor_add_line(editor, start, end, 1.5F, (AfColor){.4F, .8F, 1, 1}, 90080);
    af_editor_add_rect(editor, start.x - 5, start.y - 5, 10, 10, (AfColor){.4F, .8F, 1, 1}, 90081);
    af_editor_add_rect(editor, end.x - 5, end.y - 5, 10, 10, (AfColor){1, .8F, .3F, 1}, 90081);
}
static bool af_editor_gradient_mouse(AfEditorState *editor, int width, int height, float x,
                                     float y) {
    if (editor->text_panel_open || editor->selected_count != 1 ||
        (editor->active_tool != AF_EDITOR_TOOL_SELECT &&
         editor->active_tool != AF_EDITOR_TOOL_TEXT))
        return false;
    const float left = editor->left_width + 6,
                available = (float)width - editor->left_width - editor->right_width - 12;
    float top = editor->toolbar_height;
    if (af_editor_is_text(editor, editor->selected_id)) {
        if (x >= left && x < left + available && y >= top && y < top + 36) {
            af_editor_open_text_panel(editor, width, height);
            return true;
        }
        top += 36;
    }
    const AfStyle *style = af_project_find_node_style(&editor->project, editor->selected_id);
    if (!style)
        return false;
    if (x < left || x >= left + available || y < top ||
        y >= top + (style->fill.type == AF_PAINT_SOLID ? 36 : 72)) {
        AfAffine2 frame;
        AfVec2 start, end;
        if (x < left || x >= left + available || y < top + 72 ||
            y >= af_editor_timeline_top(editor, height) ||
            !af_editor_gradient_geometry(editor, width, height, &frame, &start, &end))
            return false;
        start = af_affine_transform_point(frame, start);
        end = af_affine_transform_point(frame, end);
        uint32_t handle = hypotf(x - start.x, y - start.y) < 9 ? 1
                          : hypotf(x - end.x, y - end.y) < 9   ? 2
                                                               : 0;
        if (!handle)
            return false;
        af_editor_begin_selection_modify(editor, false);
        editor->gradient_drag = handle;
        return true;
    }
    const uint32_t col = (uint32_t)((x - left) / (available / 6)), row = (uint32_t)((y - top) / 36);
    if (row == 0)
        af_editor_gradient_action(editor, col);
    else if (col < 4)
        af_editor_gradient_action(editor, col + 6);
    return true;
}
static void af_editor_render_gradient(AfEditorState *editor, int width) {
    if (editor->text_panel_open || editor->selected_count != 1 ||
        (editor->active_tool != AF_EDITOR_TOOL_SELECT &&
         editor->active_tool != AF_EDITOR_TOOL_TEXT))
        return;
    const AfStyle *style = af_project_find_node_style(&editor->project, editor->selected_id);
    if (!style)
        return;
    const AfPaint *paint = &style->fill;
    const float x = editor->left_width + 6,
                y = editor->toolbar_height +
                    (af_editor_is_text(editor, editor->selected_id) ? 36 : 0);
    const float available = (float)width - editor->left_width - editor->right_width - 12,
                cell = available / 6;
    const AfColor white = {.9F, .94F, 1, 1}, idle = {.13F, .17F, .23F, 1};
    const char *types[] = {"Solid >", "Linear >", "Radial >", "Conic >"};
    const char *labels[] = {types[paint->type],
                            "< Stop",
                            "Stop >",
                            "Add stop",
                            "Delete stop",
                            "Reverse",
                            "Position -",
                            "Position +",
                            paint->type == AF_PAINT_RADIAL_GRADIENT ? "Radius -" : "Angle -",
                            paint->type == AF_PAINT_RADIAL_GRADIENT ? "Radius +" : "Angle +"};
    const uint32_t count = paint->type == AF_PAINT_SOLID ? 1 : 10;
    af_editor_add_rect(editor, x - 6, y, available + 12, count == 1 ? 36 : 72,
                       (AfColor){.08F, .1F, .14F, 1}, 100150);
    for (uint32_t i = 0; i < count; ++i) {
        const float bx = x + (float)(i % 6) * cell, by = y + (float)(i / 6) * 36 + 3;
        af_editor_add_rect(editor, bx, by, cell - 4, 30, idle, 100151);
        af_editor_add_text(editor, bx + 6, by + 10, labels[i], .8F, cell - 16, white, 100153);
    }
    if (count == 1) {
        af_editor_add_text(editor, x + cell + 6, y + 13,
                           "Fill type · click Solid to choose a gradient", .85F,
                           available - cell - 12, white, 100153);
    } else {
        uint32_t stop = af_editor_gradient_stop(editor, paint);
        char label[64];
        (void)snprintf(label, sizeof(label), "Stop %u/%u  %.0f%%", stop + 1, paint->stop_count,
                       (double)paint->stops[stop].offset * 100);
        af_editor_add_rect(editor, x + cell * 4, y + 43, 18, 18, paint->stops[stop].color, 100153);
        af_editor_add_text(editor, x + cell * 4 + 24, y + 49, label, .8F, cell * 2 - 30, white,
                           100153);
    }
}
