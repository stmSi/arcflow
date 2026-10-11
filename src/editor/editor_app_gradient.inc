#define AF_EDITOR_PAINT(style)                                                                     \
    (*(editor->color_target_outline ? &(style)->stroke_paint : &(style)->fill))
/* Paint editing uses the existing selection snapshot transaction and color inspector. */
static uint32_t af_editor_gradient_stop(const AfEditorState *editor, const AfPaint *paint) {
    if (paint == NULL)
        return 0;
    return editor->gradient_target == editor->selected_id &&
                   editor->gradient_outline == editor->color_target_outline &&
                   editor->gradient_stop < paint->stop_count
               ? editor->gradient_stop
               : 0;
}
static void af_editor_gradient_select_stop(AfEditorState *editor, const AfPaint *paint,
                                           uint32_t index) {
    if (index >= paint->stop_count)
        return;
    editor->gradient_target = editor->selected_id;
    editor->gradient_outline = editor->color_target_outline;
    editor->gradient_stop = index;
    editor->gradient_stop_focus = true;
    editor->timeline_focus = false;
    float saturation, value;
    af_editor_color_hsv(paint->stops[index].color, &editor->color_hue, &saturation, &value);
    af_editor_status(editor, "Gradient stop selected · drag to move · edit color in inspector");
}
static AfColor af_editor_gradient_color_at(const AfPaint *paint, float offset) {
    AfPaint ramp = *paint;
    ramp.type = AF_PAINT_LINEAR_GRADIENT;
    ramp.start = (AfVec2){0, 0};
    ramp.end = (AfVec2){1, 0};
    ramp.transform = af_affine_identity();
    AfColor color = {0};
    (void)af_paint_sample(&ramp, (AfVec2){offset, 0}, 1, &color);
    return color;
}
static void af_editor_gradient_add_stop(AfEditorState *editor, float offset) {
    if (af_editor_gradient_has_tracks(editor, true)) {
        af_editor_status(editor, "Remove stop animation tracks before changing the stop list");
        return;
    }
    AfStyle *style = af_project_find_node_style(&editor->project, editor->selected_id);
    if (!style || AF_EDITOR_PAINT(style).type == AF_PAINT_SOLID)
        return;
    AfPaint *paint = &AF_EDITOR_PAINT(style);
    if (paint->stop_count >= AF_MAX_GRADIENT_STOPS) {
        af_editor_status(editor, "Gradient supports up to 16 stops");
        return;
    }
    offset = af_editor_clamp(offset, 0, 1);
    const AfColor color = af_editor_gradient_color_at(paint, offset);
    uint32_t at = 0;
    while (at < paint->stop_count && paint->stops[at].offset <= offset)
        ++at;
    af_editor_begin_selection_modify(editor, false);
    memmove(paint->stops + at + 1, paint->stops + at,
            (paint->stop_count - at) * sizeof(AfGradientStop));
    paint->stops[at] = (AfGradientStop){.offset = offset, .color = color};
    ++paint->stop_count;
    style->inherit_flags &= ~(uint32_t)(editor->color_target_outline ? AF_STYLE_INHERIT_STROKE_PAINT
                                                                     : AF_STYLE_INHERIT_FILL);
    (void)af_style_mark_paint_changed(style);
    (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_STYLE, false);
    af_editor_gradient_select_stop(editor, paint, at);
}
static void af_editor_gradient_set_type(AfEditorState *editor, AfPaintType type) {
    if (!editor->selected_id || type > AF_PAINT_CONIC_GRADIENT)
        return;
    if (type == AF_PAINT_SOLID && af_editor_gradient_has_tracks(editor, false)) {
        af_editor_status(editor, "Remove gradient tracks before switching to Solid");
        return;
    }
    af_editor_begin_selection_modify(editor, false);
    for (uint32_t i = 0; i < editor->selection_drag_count; ++i) {
        const uint64_t id = editor->selection_drag_before[i].node.id;
        AfSceneNode *node = af_scene_find(&editor->project.scene, id);
        AfStyle *style = af_project_find_node_style(&editor->project, id);
        if (!node || !style || type == AF_EDITOR_PAINT(style).type)
            continue;
        if (type == AF_PAINT_SOLID)
            AF_EDITOR_PAINT(style) = af_paint_solid(
                editor->color_target_outline ? AF_EDITOR_PAINT(style).solid : node->fill);
        else {
            if (AF_EDITOR_PAINT(style).type == AF_PAINT_SOLID) {
                AfPaint *target = &AF_EDITOR_PAINT(style);
                const AfColor color = editor->color_target_outline ? target->solid : node->fill;
                *target = af_paint_solid(color);
                target->type = AF_PAINT_LINEAR_GRADIENT;
                target->start = (AfVec2){0, 0};
                target->end = node->bounds;
                target->stop_count = 2;
                target->stops[0] = (AfGradientStop){0, color};
                target->stops[1] =
                    (AfGradientStop){1, {1 - color.r, 1 - color.g, 1 - color.b, color.a}};
            }
            AF_EDITOR_PAINT(style).type = type;
            if (type == AF_PAINT_RADIAL_GRADIENT || type == AF_PAINT_CONIC_GRADIENT) {
                AF_EDITOR_PAINT(style).start = (AfVec2){node->bounds.x * .5F, node->bounds.y * .5F};
                AF_EDITOR_PAINT(style).end_radius =
                    af_editor_clamp(fmaxf(node->bounds.x, node->bounds.y) * .5F, 1, 100000);
            } else {
                AF_EDITOR_PAINT(style).start = (AfVec2){0, 0};
                AF_EDITOR_PAINT(style).end = node->bounds;
            }
        }
        style->inherit_flags &=
            ~(uint32_t)(editor->color_target_outline ? AF_STYLE_INHERIT_STROKE_PAINT
                                                     : AF_STYLE_INHERIT_FILL);
        (void)af_style_mark_paint_changed(style);
    }
    editor->gradient_target = editor->selected_id;
    editor->gradient_outline = editor->color_target_outline;
    editor->gradient_stop = 0;
    editor->gradient_stop_focus = false;
    (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_STYLE, false);
}
static void af_editor_gradient_action(AfEditorState *editor, uint32_t action) {
    if (editor->selected_count != 1)
        return;
    AfSceneNode *node = af_scene_find(&editor->project.scene, editor->selected_id);
    AfStyle *style = af_project_find_node_style(&editor->project, editor->selected_id);
    if (!node || !style)
        return;
    if (((action == 3 || action == 4 || action == 5) &&
         af_editor_gradient_has_tracks(editor, true)) ||
        (action == 0 && AF_EDITOR_PAINT(style).type == AF_PAINT_CONIC_GRADIENT &&
         af_editor_gradient_has_tracks(editor, false))) {
        af_editor_status(
            editor, "Remove gradient tracks before changing the stop list or switching to Solid");
        return;
    }
    AfPaint *paint = &AF_EDITOR_PAINT(style);
    uint32_t selected = af_editor_gradient_stop(editor, paint);
    editor->gradient_target = editor->selected_id;
    editor->gradient_outline = editor->color_target_outline;
    if ((action == 1 || action == 2) && paint->stop_count) {
        af_editor_gradient_select_stop(editor, paint,
                                       (selected + (action == 1 ? paint->stop_count - 1 : 1)) %
                                           paint->stop_count);
        af_editor_status(
            editor,
            "Gradient stop selected · use the color picker or hex field to change its color");
        return;
    }
    if (action != 0 && paint->type == AF_PAINT_SOLID)
        return;
    if (action == 3 && paint->stop_count >= AF_MAX_GRADIENT_STOPS)
        return;
    if (action == 4 && paint->stop_count <= 2) {
        af_editor_status(editor, "Keep at least two gradient stops");
        return;
    }
    af_editor_begin_selection_modify(editor, false);
    if (action == 0) {
        AfPaintType next = (AfPaintType)(((uint32_t)paint->type + 1) % 4);
        if (paint->type == AF_PAINT_SOLID) {
            {
                AfPaint *target = &AF_EDITOR_PAINT(style);
                const AfColor color = editor->color_target_outline ? target->solid : node->fill;
                *target = af_paint_solid(color);
                target->type = AF_PAINT_LINEAR_GRADIENT;
                target->start = (AfVec2){0, 0};
                target->end = node->bounds;
                target->stop_count = 2;
                target->stops[0] = (AfGradientStop){0, color};
                target->stops[1] =
                    (AfGradientStop){1, {1 - color.r, 1 - color.g, 1 - color.b, color.a}};
            }
            selected = 0;
        } else if (next == AF_PAINT_SOLID)
            *paint = af_paint_solid(editor->color_target_outline ? AF_EDITOR_PAINT(style).solid
                                                                 : node->fill);
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
    style->inherit_flags &= ~(uint32_t)(editor->color_target_outline ? AF_STYLE_INHERIT_STROKE_PAINT
                                                                     : AF_STYLE_INHERIT_FILL);
    (void)af_style_mark_paint_changed(style);
    af_editor_gradient_select_stop(editor, paint, selected);
    (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_STYLE, false);
    af_editor_status(editor,
                     "Gradient updated · select a stop, then edit its color in the inspector");
}
static void af_editor_key_gradient(AfEditorState *editor) {
    const AfStyle *style =
        af_editor_gradient_has_tracks(editor, false)
            ? af_style_find_node_const(&editor->preview_style_data, editor->selected_id)
            : af_project_find_node_style(&editor->project, editor->selected_id);
    const AfPaint *paint = af_editor_target_paint(editor, style);
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    if (!paint || paint->type == AF_PAINT_SOLID || !clip)
        return;
    AfEditorCommand command = {0};
    if (af_editor_command_animation(AF_EDITOR_MERGE_NONE, editor->selected_id, &command) !=
        AF_RESULT_SUCCESS)
        return;
    const uint32_t base = editor->color_target_outline ? AF_ANIMATION_OUTLINE_ELEMENT : 0U;
    const float time = af_editor_key_time(editor, clip, editor->playhead);
    AfResult result = AF_RESULT_SUCCESS;
    for (uint32_t property = AF_PROPERTY_GRADIENT_START;
         property <= AF_PROPERTY_GRADIENT_STOP_OFFSET; ++property) {
        if ((property == AF_PROPERTY_GRADIENT_END && paint->type != AF_PAINT_LINEAR_GRADIENT) ||
            (property == AF_PROPERTY_GRADIENT_RADII && paint->type == AF_PAINT_LINEAR_GRADIENT) ||
            (property == AF_PROPERTY_GRADIENT_ANGLE && paint->type == AF_PAINT_LINEAR_GRADIENT))
            continue;
        const uint32_t stops = property >= AF_PROPERTY_GRADIENT_STOP_COLOR ? paint->stop_count : 1U;
        for (uint32_t stop = 0; result == AF_RESULT_SUCCESS && stop < stops; ++stop) {
            const AfProjectTrack *track = af_editor_find_track(
                editor, editor->selected_id, (AfPropertyType)property, base + stop);
            uint64_t track_id = track ? track->id : 0;
            if (!track) {
                if (command.track_after_count >= AF_EDITOR_COMMAND_MAX_TRACKS) {
                    result = AF_RESULT_LIMIT_EXCEEDED;
                    break;
                }
                result = af_project_allocate_animation_id(&editor->project, &track_id);
                const uint32_t n = command.track_after_count++;
                command.track_after[n] =
                    (AfProjectTrackSnapshot){.track = {.id = track_id,
                                                       .clip_id = clip->id,
                                                       .node_id = editor->selected_id,
                                                       .property = (AfPropertyType)property,
                                                       .element_index = base + stop},
                                             .index = editor->project.track_count + n};
            }
            const AfProjectKeyframe *current =
                track ? af_editor_track_key_at(editor, track_id, time) : NULL;
            AfProjectKeyframeSnapshot key = {0};
            if (command.keyframe_after_count >= AF_EDITOR_COMMAND_MAX_KEYFRAMES) {
                result = AF_RESULT_LIMIT_EXCEEDED;
                break;
            }
            if (current) {
                result = af_project_keyframe_snapshot(&editor->project, current->id, &key);
                command.keyframe_before[command.keyframe_before_count++] = key;
            } else {
                result = af_project_allocate_animation_id(&editor->project, &key.keyframe.id);
                key.index = editor->project.keyframe_count + command.keyframe_after_count -
                            command.keyframe_before_count;
                key.keyframe.track_id = track_id;
                key.keyframe.keyframe = (AfKeyframe){.time = time,
                                                     .interpolation = AF_INTERPOLATION_LINEAR,
                                                     .easing = {.42F, 0, .58F, 1}};
            }
            af_editor_gradient_value(paint, (AfPropertyType)property, stop,
                                     key.keyframe.keyframe.value);
            command.keyframe_after[command.keyframe_after_count++] = key;
        }
    }
    if (result == AF_RESULT_SUCCESS)
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    if (result == AF_RESULT_SUCCESS) {
        editor->selected_property = AF_PROPERTY_GRADIENT_STOP_COLOR;
        af_editor_mark_dirty(editor);
        af_editor_status(editor, "Gradient keyed · move the playhead, then edit colors or handles");
    } else
        af_editor_status(editor, "Gradient keyframes could not be added");
}

static uint32_t af_editor_choice_count(const AfEditorState *editor) {
    return editor->choice_menu == 3   ? 2U
           : editor->choice_menu == 1 ? 4U
                                      : (uint32_t)AF_PROPERTY_LAST + 1U;
}
static bool af_editor_choice_enabled(const AfEditorState *editor, uint32_t row) {
    return editor->selected_id == editor->choice_target &&
           (editor->choice_menu == 3
                ? editor->selected_path_command_count == 2U
                : editor->choice_menu == 1 ||
                      af_editor_property_available(editor, (AfPropertyType)row));
}
static void af_editor_choice_open(AfEditorState *editor, uint32_t menu, int width, int height,
                                  float x, float y) {
    editor->choice_menu = menu;
    editor->top_menu = AF_EDITOR_MENU_NONE;
    editor->context_menu = AF_EDITOR_CONTEXT_NONE;
    editor->choice_target = editor->selected_id;
    const AfStyle *style = af_project_find_node_style(&editor->project, editor->selected_id);
    editor->choice_index = menu == 3            ? 0U
                           : menu == 1 && style ? (uint32_t)AF_EDITOR_PAINT(style).type
                                                : (uint32_t)editor->selected_property;
    const float h = (float)af_editor_choice_count(editor) * 28;
    editor->choice_origin = (AfVec2){
        af_editor_clamp(x, 0, fmaxf(0, (float)width - 220)),
        af_editor_clamp(y + h > (float)height - editor->status_height ? y - h - 36 : y,
                        AF_EDITOR_MENU_HEIGHT,
                        fmaxf(AF_EDITOR_MENU_HEIGHT, (float)height - h - editor->status_height))};
    editor->gradient_stop_focus = false;
}
static void af_editor_choice_apply(AfEditorState *editor, uint32_t row) {
    if (!af_editor_choice_enabled(editor, row))
        return;
    if (editor->choice_menu == 3)
        af_editor_combine_points(editor, row == 1U);
    else if (editor->choice_menu == 1)
        af_editor_gradient_set_type(editor, (AfPaintType)row);
    else {
        editor->selected_property = (AfPropertyType)row;
        editor->selected_keyframe_count = 0;
        editor->timeline_focus = true;
        af_editor_status(editor, af_editor_property_name(editor->selected_property));
    }
    editor->choice_menu = 0;
}
static bool af_editor_choice_mouse(AfEditorState *editor, float x, float y) {
    if (!editor->choice_menu)
        return false;
    const AfVec2 origin = editor->choice_origin;
    if (x >= origin.x && x < origin.x + 220 && y >= origin.y &&
        y < origin.y + (float)af_editor_choice_count(editor) * 28) {
        af_editor_choice_apply(editor, (uint32_t)((y - origin.y) / 28));
        return true;
    }
    editor->choice_menu = 0;
    return true;
}
static bool af_editor_choice_key(AfEditorState *editor, const SDL_KeyboardEvent *event) {
    if (!editor->choice_menu)
        return false;
    if (event->key == SDLK_ESCAPE)
        editor->choice_menu = 0;
    else if (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER)
        af_editor_choice_apply(editor, editor->choice_index);
    else if (event->key == SDLK_UP || event->key == SDLK_DOWN) {
        const uint32_t count = af_editor_choice_count(editor);
        for (uint32_t i = 0; i < count; ++i) {
            editor->choice_index =
                (editor->choice_index + (event->key == SDLK_UP ? count - 1 : 1)) % count;
            if (af_editor_choice_enabled(editor, editor->choice_index))
                break;
        }
    }
    return true;
}
static void af_editor_render_choices(AfEditorState *editor) {
    if (!editor->choice_menu)
        return;
    if (editor->selected_id != editor->choice_target) {
        editor->choice_menu = 0;
        return;
    }
    const char *fills[] = {"Solid", "Linear gradient", "Radial gradient", "Conic gradient"};
    const AfVec2 p = editor->choice_origin;
    af_editor_add_rect(editor, p.x - 2, p.y - 2, 224,
                       (float)af_editor_choice_count(editor) * 28 + 4,
                       (AfColor){.08F, .1F, .14F, 1}, 205000);
    for (uint32_t i = 0; i < af_editor_choice_count(editor); ++i) {
        const bool enabled = af_editor_choice_enabled(editor, i);
        const bool hover = af_editor_hovered_rect(editor, p.x, p.y + (float)i * 28, 220, 28);
        if (hover || i == editor->choice_index)
            af_editor_add_rect(editor, p.x, p.y + (float)i * 28, 220, 28,
                               (AfColor){.18F, .35F, .52F, 1}, 205001);
        af_editor_add_text(
            editor, p.x + 10, p.y + (float)i * 28 + 9,
            editor->choice_menu == 3   ? (i == 0 ? "Add line" : "Merge points")
            : editor->choice_menu == 1 ? fills[i]
                                       : af_editor_property_name((AfPropertyType)i),
            .9F, 200, enabled ? (AfColor){.92F, .95F, 1, 1} : (AfColor){.4F, .43F, .5F, 1}, 205002);
    }
}
static bool af_editor_gradient_geometry(AfEditorState *editor, int width, int height,
                                        AfAffine2 *frame, AfVec2 *start, AfVec2 *end) {
    if (editor->text_panel_open || editor->selected_count != 1 ||
        editor->active_tool != AF_EDITOR_TOOL_SELECT)
        return false;
    const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    const AfStyle *style =
        af_editor_gradient_has_tracks(editor, false)
            ? af_style_find_node_const(&editor->preview_style_data, editor->selected_id)
            : af_project_find_node_style(&editor->project, editor->selected_id);
    if (!node || !style || AF_EDITOR_PAINT(style).type == AF_PAINT_SOLID)
        return false;
    const AfPaint *paint = &AF_EDITOR_PAINT(style);
    *frame = af_affine_multiply(af_editor_camera(editor, width, height),
                                af_affine_multiply(node->world_transform, paint->transform));
    *start = paint->start;
    *end = paint->type == AF_PAINT_LINEAR_GRADIENT
               ? paint->end
               : (AfVec2){start->x + cosf(paint->angle) * fmaxf(1, paint->end_radius),
                          start->y + sinf(paint->angle) * fmaxf(1, paint->end_radius)};
    return true;
}
static bool af_editor_gradient_begin_drag(AfEditorState *editor, int width, int height,
                                          AfEditorGradientDrag drag, float x, float y) {
    const AfStyle *style = af_project_find_node_style(&editor->project, editor->selected_id);
    if (!style || AF_EDITOR_PAINT(style).type == AF_PAINT_SOLID || editor->gradient_drag)
        return false;
    AfVec2 origin = {x, y};
    if (drag == AF_EDITOR_GRADIENT_DRAG_START || drag == AF_EDITOR_GRADIENT_DRAG_END) {
        AfAffine2 frame, inverse;
        AfVec2 start, end;
        if (!af_editor_gradient_geometry(editor, width, height, &frame, &start, &end) ||
            !af_affine_inverse(frame, &inverse))
            return false;
        const AfVec2 pointer = af_affine_transform_point(inverse, origin);
        const AfVec2 handle = drag == AF_EDITOR_GRADIENT_DRAG_START ? start : end;
        /* Keep the grab offset: clicking the edge of a handle must not move it. */
        origin = (AfVec2){handle.x - pointer.x, handle.y - pointer.y};
    }
    af_editor_begin_selection_modify(editor, false);
    if (!editor->selection_drag_count)
        return false;
    editor->preview_playing = false;
    editor->gradient_drag = drag;
    editor->gradient_drag_origin = origin;
    editor->gradient_drag_pointer = (AfVec2){x, y};
    editor->gradient_drag_stop_before = af_editor_gradient_stop(editor, &AF_EDITOR_PAINT(style));
    editor->gradient_drag_stop_offset =
        AF_EDITOR_PAINT(style).stops[editor->gradient_drag_stop_before].offset;
    editor->gradient_drag_changed = false;
    editor->gradient_drag_was_dirty = editor->dirty;
    const AfPropertyType property =
        drag == AF_EDITOR_GRADIENT_DRAG_START ? AF_PROPERTY_GRADIENT_START
        : drag == AF_EDITOR_GRADIENT_DRAG_END
            ? (AF_EDITOR_PAINT(style).type == AF_PAINT_LINEAR_GRADIENT ? AF_PROPERTY_GRADIENT_END
                                                                       : AF_PROPERTY_GRADIENT_RADII)
            : AF_PROPERTY_GRADIENT_STOP_OFFSET;
    af_editor_begin_transform_key_drag(editor, property);
    if (drag == AF_EDITOR_GRADIENT_DRAG_END &&
        AF_EDITOR_PAINT(style).type != AF_PAINT_LINEAR_GRADIENT)
        af_editor_append_transform_key_drag(editor, AF_PROPERTY_GRADIENT_ANGLE);
    af_editor_status(editor, drag == AF_EDITOR_GRADIENT_DRAG_START ? "Drag gradient start point"
                             : drag == AF_EDITOR_GRADIENT_DRAG_END ? "Drag gradient end / radius"
                                                                   : "Drag gradient stop position");
    return true;
}
static void af_editor_gradient_move_stop(AfEditorState *editor, AfPaint *paint, float offset) {
    uint32_t index = af_editor_gradient_stop(editor, paint);
    if (af_editor_gradient_has_tracks(editor, true)) {
        paint->stops[index].offset =
            af_editor_clamp(offset, index ? paint->stops[index - 1].offset : 0,
                            index + 1 < paint->stop_count ? paint->stops[index + 1].offset : 1);
        return;
    }
    const AfGradientStop stop = {.offset = af_editor_clamp(offset, 0, 1),
                                 .color = paint->stops[index].color};
    /* A stop keeps its color and selection when it crosses its neighbors. */
    while (index > 0 && paint->stops[index - 1].offset > stop.offset) {
        paint->stops[index] = paint->stops[index - 1];
        --index;
    }
    while (index + 1 < paint->stop_count && paint->stops[index + 1].offset < stop.offset) {
        paint->stops[index] = paint->stops[index + 1];
        ++index;
    }
    paint->stops[index] = stop;
    editor->gradient_stop = index;
}
static void af_editor_gradient_motion(AfEditorState *editor, int width, int height, float x,
                                      float y) {
    if (!editor->gradient_drag || !isfinite(x) || !isfinite(y))
        return;
    if (!editor->gradient_drag_changed && x == editor->gradient_drag_pointer.x &&
        y == editor->gradient_drag_pointer.y)
        return;
    AfStyle *style = af_project_find_node_style(&editor->project, editor->selected_id);
    if (!style || AF_EDITOR_PAINT(style).type == AF_PAINT_SOLID)
        return;
    const AfPaint previous = AF_EDITOR_PAINT(style);
    if (editor->gradient_drag == AF_EDITOR_GRADIENT_DRAG_TOOLBAR_STOP) {
        const float ramp_width =
            fmaxf(1, (float)width - editor->left_width - editor->right_width - 32);
        af_editor_gradient_move_stop(editor, &AF_EDITOR_PAINT(style),
                                     editor->gradient_drag_stop_offset +
                                         (x - editor->gradient_drag_origin.x) / ramp_width);
    } else {
        AfAffine2 frame, inverse;
        AfVec2 start, end;
        if (!af_editor_gradient_geometry(editor, width, height, &frame, &start, &end))
            return;
        if (editor->gradient_drag == AF_EDITOR_GRADIENT_DRAG_CANVAS_STOP) {
            start = af_affine_transform_point(frame, start);
            end = af_affine_transform_point(frame, end);
            const AfVec2 delta = {end.x - start.x, end.y - start.y};
            const float length_squared = delta.x * delta.x + delta.y * delta.y;
            if (length_squared <= 0.000001F)
                return;
            af_editor_gradient_move_stop(editor, &AF_EDITOR_PAINT(style),
                                         editor->gradient_drag_stop_offset +
                                             ((x - editor->gradient_drag_origin.x) * delta.x +
                                              (y - editor->gradient_drag_origin.y) * delta.y) /
                                                 length_squared);
        } else {
            if (!af_affine_inverse(frame, &inverse))
                return;
            AfVec2 point = af_affine_transform_point(inverse, (AfVec2){x, y});
            point.x += editor->gradient_drag_origin.x;
            point.y += editor->gradient_drag_origin.y;
            if (!isfinite(point.x) || !isfinite(point.y))
                return;
            if (editor->gradient_drag == AF_EDITOR_GRADIENT_DRAG_START)
                AF_EDITOR_PAINT(style).start = point;
            else if (AF_EDITOR_PAINT(style).type == AF_PAINT_LINEAR_GRADIENT)
                AF_EDITOR_PAINT(style).end = point;
            else {
                AF_EDITOR_PAINT(style).end_radius =
                    af_editor_clamp(hypotf(point.x - start.x, point.y - start.y), 1, 100000);
                AF_EDITOR_PAINT(style).angle = atan2f(point.y - start.y, point.x - start.x);
            }
        }
    }
    if (memcmp(&previous, &AF_EDITOR_PAINT(style), sizeof(previous)) == 0)
        return;
    editor->gradient_drag_changed = true;
    for (uint32_t i = 0; i < editor->transform_drag_key_count; ++i) {
        AfProjectKeyframe *key =
            af_project_find_keyframe(&editor->project, editor->transform_drag_key_ids[i]);
        const AfProjectTrack *track =
            key ? af_project_find_track_const(&editor->project, key->track_id) : NULL;
        if (track)
            af_editor_gradient_value(&AF_EDITOR_PAINT(style), track->property,
                                     track->element_index % AF_MAX_GRADIENT_STOPS,
                                     key->keyframe.value);
    }
    style->inherit_flags &= ~(uint32_t)(editor->color_target_outline ? AF_STYLE_INHERIT_STROKE_PAINT
                                                                     : AF_STYLE_INHERIT_FILL);
    (void)af_style_mark_paint_changed(style);
    af_editor_mark_dirty(editor);
}
static void af_editor_gradient_finish_drag(AfEditorState *editor, bool cancel) {
    if (!editor->gradient_drag)
        return;
    if (cancel || !editor->gradient_drag_changed) {
        for (uint32_t i = 0; i < editor->transform_drag_key_count; ++i) {
            if (editor->transform_drag_key_created[i])
                (void)af_project_remove_keyframe_exact(&editor->project,
                                                       editor->transform_drag_key_ids[i]);
            else {
                AfProjectKeyframe *key =
                    af_project_find_keyframe(&editor->project, editor->transform_drag_key_ids[i]);
                if (key)
                    *key = editor->transform_drag_key_before[i].keyframe;
            }
        }
        editor->transform_drag_key_count = 0;
        for (uint32_t i = 0; i < editor->selection_drag_count; ++i) {
            AfStyle *style = af_project_find_node_style(&editor->project,
                                                        editor->selection_drag_before[i].node.id);
            if (style)
                *style = editor->selection_drag_before[i].style;
        }
        editor->dirty = editor->gradient_drag_was_dirty;
        editor->gradient_stop = editor->gradient_drag_stop_before;
        af_editor_update_preview(editor);
        if (cancel)
            af_editor_status(editor, "Gradient drag canceled");
    } else if (editor->transform_drag_key_count) {
        AfStyle *style = af_project_find_node_style(&editor->project, editor->selected_id);
        const AfStyle before = editor->selection_drag_before[0].style;
        const AfPaint *original = af_editor_target_paint(editor, &before);
        bool all_keyed = af_editor_transform_key_drag_covers_selection(editor);
        if (editor->gradient_drag == AF_EDITOR_GRADIENT_DRAG_END &&
            AF_EDITOR_PAINT(style).type != AF_PAINT_LINEAR_GRADIENT)
            all_keyed =
                all_keyed && af_editor_transform_drag_key_property_index(
                                 editor, editor->selected_id, AF_PROPERTY_GRADIENT_ANGLE) >= 0;
        for (uint32_t i = 0; i < editor->transform_drag_key_count; ++i) {
            const AfProjectKeyframe *key =
                af_project_find_keyframe(&editor->project, editor->transform_drag_key_ids[i]);
            const AfProjectTrack *track =
                af_project_find_track_const(&editor->project, key->track_id);
            float value[4];
            af_editor_gradient_value(original, track->property,
                                     track->element_index % AF_MAX_GRADIENT_STOPS, value);
            (void)af_animation_apply_value(&editor->project.scene, track->node_id, track->property,
                                           track->element_index, value);
        }
        if (all_keyed)
            *style = before;
        else
            (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_STYLE, false);
        (void)af_editor_commit_transform_key_drag(editor, false);
    } else {
        (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_STYLE, false);
    }
    editor->selection_drag_count = 0;
    editor->gradient_drag = AF_EDITOR_GRADIENT_DRAG_NONE;
    editor->gradient_drag_changed = false;
    af_editor_update_preview(editor);
}
static void af_editor_render_gradient_handles(AfEditorState *editor, int width, int height) {
    AfAffine2 frame;
    AfVec2 start, end;
    if (!af_editor_gradient_geometry(editor, width, height, &frame, &start, &end))
        return;
    start = af_affine_transform_point(frame, start);
    end = af_affine_transform_point(frame, end);
    af_editor_add_line(editor, start, end, 1.5F, (AfColor){.4F, .8F, 1, 1}, 90080);
    const AfPaint *paint = af_editor_target_paint(
        editor, af_project_find_node_style(&editor->project, editor->selected_id));
    for (uint32_t i = 0; i < paint->stop_count; ++i) {
        const float t = paint->stops[i].offset;
        const AfVec2 point = {start.x + (end.x - start.x) * t, start.y + (end.y - start.y) * t};
        af_editor_add_rect(editor, point.x - 5, point.y - 5, 10, 10,
                           i == af_editor_gradient_stop(editor, paint) ? (AfColor){1, .85F, .25F, 1}
                                                                       : (AfColor){1, 1, 1, 1},
                           90082);
        af_editor_add_rect(editor, point.x - 3, point.y - 3, 6, 6, paint->stops[i].color, 90083);
    }
    /* Geometry handles own the endpoints, including where a color stop overlaps them. */
    af_editor_add_rect(editor, start.x - 6, start.y - 6, 12, 12, (AfColor){.4F, .8F, 1, 1}, 90084);
    af_editor_add_rect(editor, end.x - 6, end.y - 6, 12, 12, (AfColor){1, .8F, .3F, 1}, 90084);
    af_editor_add_rect(editor, start.x - 3, start.y - 3, 6, 6, (AfColor){.08F, .1F, .14F, 1},
                       90085);
    af_editor_add_rect(editor, end.x - 3, end.y - 3, 6, 6, (AfColor){.08F, .1F, .14F, 1}, 90085);
}
static bool af_editor_gradient_mouse(AfEditorState *editor, int width, int height, float x, float y,
                                     bool double_click) {
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
        y >= top + (AF_EDITOR_PAINT(style).type == AF_PAINT_SOLID ? 36 : 116)) {
        AfAffine2 frame;
        AfVec2 start, end;
        if (x < left || x >= left + available || y < top + 116 ||
            y >= af_editor_timeline_top(editor, height) ||
            !af_editor_gradient_geometry(editor, width, height, &frame, &start, &end))
            return false;
        start = af_affine_transform_point(frame, start);
        end = af_affine_transform_point(frame, end);
        /* Hit endpoints before stops: the first/last stops commonly occupy the same pixels. */
        const float start_distance = hypotf(x - start.x, y - start.y);
        const float end_distance = hypotf(x - end.x, y - end.y);
        if (start_distance < 9 || end_distance < 9) {
            const AfEditorGradientDrag drag =
                start_distance < 9 && (end_distance >= 9 || start_distance <= end_distance)
                    ? AF_EDITOR_GRADIENT_DRAG_START
                    : AF_EDITOR_GRADIENT_DRAG_END;
            const uint32_t stop =
                drag == AF_EDITOR_GRADIENT_DRAG_START ? 0 : AF_EDITOR_PAINT(style).stop_count - 1;
            af_editor_gradient_select_stop(editor, &AF_EDITOR_PAINT(style), stop);
            (void)af_editor_gradient_begin_drag(editor, width, height, drag, x, y);
            return true;
        }
        const AfVec2 delta = {end.x - start.x, end.y - start.y};
        const float length = hypotf(delta.x, delta.y);
        const float t =
            length > 1 ? ((x - start.x) * delta.x + (y - start.y) * delta.y) / (length * length)
                       : 0;
        const float perpendicular =
            length > 1 ? fabsf((x - start.x) * delta.y - (y - start.y) * delta.x) / length
                       : INFINITY;
        if (length > 1 && t * length >= -8 && (t - 1) * length <= 8 && perpendicular < 9) {
            const uint32_t selected = af_editor_gradient_stop(editor, &AF_EDITOR_PAINT(style));
            uint32_t hit = UINT32_MAX;
            float distance = 8;
            for (uint32_t i = 0; i < AF_EDITOR_PAINT(style).stop_count; ++i) {
                const float d = fabsf(t - AF_EDITOR_PAINT(style).stops[i].offset) * length;
                if (d < distance || (d == distance && i == selected)) {
                    hit = i;
                    distance = d;
                }
            }
            if (hit != UINT32_MAX) {
                af_editor_gradient_select_stop(editor, &AF_EDITOR_PAINT(style), hit);
                if (!double_click)
                    (void)af_editor_gradient_begin_drag(editor, width, height,
                                                        AF_EDITOR_GRADIENT_DRAG_CANVAS_STOP, x, y);
                return true;
            } else if (double_click && t >= 0 && t <= 1) {
                af_editor_gradient_add_stop(editor, t);
                return true;
            }
        }
        return false;
    }
    if (AF_EDITOR_PAINT(style).type != AF_PAINT_SOLID && y >= top + 72) {
        const float ramp_x = left + 10, ramp_width = fmaxf(1, available - 20);
        const float t = af_editor_clamp((x - ramp_x) / ramp_width, 0, 1);
        uint32_t hit = UINT32_MAX;
        float distance = 9;
        const uint32_t selected = af_editor_gradient_stop(editor, &AF_EDITOR_PAINT(style));
        for (uint32_t i = 0; i < AF_EDITOR_PAINT(style).stop_count; ++i) {
            float d = fabsf(x - ramp_x - AF_EDITOR_PAINT(style).stops[i].offset * ramp_width);
            if (d < distance || (d == distance && i == selected)) {
                hit = i;
                distance = d;
            }
        }
        if (hit != UINT32_MAX) {
            af_editor_gradient_select_stop(editor, &AF_EDITOR_PAINT(style), hit);
            if (!double_click)
                (void)af_editor_gradient_begin_drag(editor, width, height,
                                                    AF_EDITOR_GRADIENT_DRAG_TOOLBAR_STOP, x, y);
        } else if (double_click)
            af_editor_gradient_add_stop(editor, t);
        return true;
    }
    const uint32_t col = (uint32_t)((x - left) / (available / 6)), row = (uint32_t)((y - top) / 36);
    if (row == 0 && col == 0)
        af_editor_choice_open(editor, 1, width, height, left, top + 36);
    else if (row == 0)
        af_editor_gradient_action(editor, col);
    else if (row == 1 && col == 0)
        af_editor_key_gradient(editor);
    return true;
}
static void af_editor_render_gradient(AfEditorState *editor, int width) {
    if (editor->text_panel_open || editor->selected_count != 1 ||
        (editor->active_tool != AF_EDITOR_TOOL_SELECT &&
         editor->active_tool != AF_EDITOR_TOOL_TEXT))
        return;
    const AfStyle *style =
        af_editor_gradient_has_tracks(editor, false)
            ? af_style_find_node_const(&editor->preview_style_data, editor->selected_id)
            : af_project_find_node_style(&editor->project, editor->selected_id);
    if (!style)
        return;
    const AfPaint *paint = &AF_EDITOR_PAINT(style);
    const float x = editor->left_width + 6,
                y = editor->toolbar_height +
                    (af_editor_is_text(editor, editor->selected_id) ? 36 : 0);
    const float available = (float)width - editor->left_width - editor->right_width - 12,
                cell = available / 6;
    const AfColor white = {.9F, .94F, 1, 1}, idle = {.13F, .17F, .23F, 1};
    const char *types[] = {"Solid v", "Linear v", "Radial v", "Conic v"};
    const char *labels[] = {types[paint->type], "< Stop",      "Stop >",
                            "Add stop",         "Delete stop", "Reverse"};
    const uint32_t count = paint->type == AF_PAINT_SOLID ? 1 : 6;
    af_editor_add_rect(editor, x - 6, y, available + 12, count == 1 ? 36 : 116,
                       (AfColor){.08F, .1F, .14F, 1}, 100150);
    for (uint32_t i = 0; i < count; ++i) {
        const float bx = x + (float)(i % 6) * cell, by = y + (float)(i / 6) * 36 + 3;
        af_editor_add_rect(editor, bx, by, cell - 4, 30, idle, 100151);
        af_editor_add_text(editor, bx + 6, by + 10, labels[i], .8F, cell - 16, white, 100153);
    }
    if (count != 1) {
        af_editor_add_button_rect(editor, x, y + 39, cell - 4, 30, idle, 100151);
        af_editor_add_text(editor, x + 6, y + 49, "Key gradient", .8F, cell - 16, white, 100153);
    }
    if (count == 1) {
        af_editor_add_text(editor, x + cell + 6, y + 13,
                           editor->color_target_outline
                               ? "Outline type · Solid, Linear, Radial or Conic"
                               : "Fill type · Solid, Linear, Radial or Conic",
                           .85F, available - cell - 12, white, 100153);
    } else {
        const float ramp_x = x + 10, ramp_width = fmaxf(1, available - 20);
        for (uint32_t i = 0; i < 64; ++i) {
            float bx = ramp_x + (float)i * ramp_width / 64;
            af_editor_add_rect(editor, bx, y + 74, ramp_width / 64 + 1, 18,
                               (AfColor){.55F, .55F, .55F, 1}, 100151);
            af_editor_add_rect(editor, bx, y + 74, ramp_width / 64 + 1, 18,
                               af_editor_gradient_color_at(paint, ((float)i + .5F) / 64), 100152);
        }
        const uint32_t selected = af_editor_gradient_stop(editor, paint);
        for (uint32_t i = 0; i < paint->stop_count; ++i) {
            float sx = ramp_x + paint->stops[i].offset * ramp_width;
            af_editor_add_rect(editor, sx - 6, y + 93, 12, 16,
                               i == selected ? (AfColor){1, .85F, .25F, 1} : white, 100153);
            af_editor_add_rect(editor, sx - 4, y + 96, 8, 10, paint->stops[i].color, 100154);
        }
        uint32_t stop = af_editor_gradient_stop(editor, paint);
        char label[64];
        (void)snprintf(label, sizeof(label), "Stop %u/%u  %.0f%%", stop + 1, paint->stop_count,
                       (double)paint->stops[stop].offset * 100);
        af_editor_add_rect(editor, x + cell * 4, y + 43, 18, 18, paint->stops[stop].color, 100153);
        af_editor_add_text(editor, x + cell * 4 + 24, y + 49, label, .8F, cell * 2 - 30, white,
                           100153);
    }
}

#undef AF_EDITOR_PAINT
