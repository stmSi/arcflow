static uint32_t af_editor_current_frame(const AfEditorState *editor) {
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    return (uint32_t)floorf(fmaxf(0, editor->playhead) * (float)af_editor_frame_rate(clip) +
                            .0001F);
}

static bool af_editor_frame_node_active(const AfEditorState *editor, uint64_t node_id) {
    const AfProjectTrack *owner = af_editor_frame_owner(&editor->project, node_id);
    if (!owner)
        return true;
    const uint32_t frame = af_editor_current_frame(editor);
    return owner->clip_id == editor->selected_clip_id && frame >= owner->cel_frame &&
           frame - owner->cel_frame < owner->cel_length;
}

static void af_editor_frame_seek(AfEditorState *editor, uint32_t frame) {
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    if (!clip)
        return;
    const uint32_t fps = af_editor_frame_rate(clip);
    const uint32_t last = (uint32_t)ceilf(clip->duration * (float)fps);
    frame = frame < last ? frame : last; /* The final empty cell allows extending the clip. */
    af_editor_cancel_vector_tool(editor, NULL);
    editor->playhead = (float)frame / (float)fps;
    editor->preview_playing = false;
    if (editor->frames_visible && editor->window_width > 0) {
        const float x = af_editor_time_to_x(editor, editor->playhead);
        const float right = (float)editor->window_width - editor->right_width;
        const float cell = editor->timeline_zoom / (float)fps;
        if (x < af_editor_timeline_origin_x(editor))
            editor->timeline_pan = editor->playhead;
        else if (x + cell > right)
            editor->timeline_pan =
                fmaxf(0, editor->playhead - (right - af_editor_timeline_origin_x(editor) - cell) /
                                                editor->timeline_zoom);
    }
    af_editor_select_only(editor, 0);
    af_editor_clear_key_selection(editor);
    editor->selected_path_command = UINT32_MAX;
    af_editor_update_preview(editor);
}

static void af_editor_toggle_frames(AfEditorState *editor) {
    editor->frames_visible = !editor->frames_visible;
    editor->curve_visible = false;
    editor->timeline_focus = true;
    if (editor->frames_visible) {
        const AfProjectClip *clip =
            af_project_find_clip_const(&editor->project, editor->selected_clip_id);
        editor->timeline_zoom = (float)af_editor_frame_rate(clip) * 24;
        editor->timeline_pan = editor->playhead;
        af_editor_frame_seek(editor, af_editor_current_frame(editor));
    } else {
        editor->timeline_zoom = 120;
    }
    af_editor_status(editor,
                     editor->frames_visible
                         ? "Vector frames · [ / ] previous / next · draw with any vector tool"
                         : "Dope sheet opened");
}

static AfResult af_editor_frame_execute(AfEditorState *editor, AfEditorCommand *command,
                                        AfResult result) {
    if (result == AF_RESULT_SUCCESS)
        result =
            af_editor_history_execute(&editor->history, &editor->project, command, false, false);
    if (result == AF_RESULT_SUCCESS) {
        af_editor_mark_dirty(editor);
        af_editor_update_preview(editor);
    } else {
        af_editor_status(editor, result == AF_RESULT_LIMIT_EXCEEDED
                                     ? "Drawing exceeds project or undo capacity"
                                     : "Frame unavailable · leave space before the next drawing");
    }
    return result;
}

static AfResult af_editor_frame_parent(AfEditorState *editor, uint64_t *parent) {
    *parent = AF_INVALID_NODE_ID;
    if (!editor->frames_visible)
        return AF_RESULT_SUCCESS;
    editor->preview_playing = false;
    const uint32_t frame = af_editor_current_frame(editor);
    const AfProjectTrack *track =
        af_editor_frame_at(&editor->project, editor->selected_clip_id, frame);
    if (!track) {
        AfEditorCommand command;
        AfResult result =
            af_editor_command_frame(&editor->project, editor->selected_clip_id, frame, 0, &command);
        result = af_editor_frame_execute(editor, &command, result);
        if (result != AF_RESULT_SUCCESS)
            return result;
        track = af_editor_frame_at(&editor->project, editor->selected_clip_id, frame);
    }
    *parent = track->node_id;
    return AF_RESULT_SUCCESS;
}

static void af_editor_frame_new(AfEditorState *editor, bool duplicate) {
    uint32_t frame = af_editor_current_frame(editor);
    const AfProjectTrack *track =
        af_editor_frame_at(&editor->project, editor->selected_clip_id, frame);
    const uint64_t source = duplicate && track ? track->node_id : 0;
    if (duplicate && !source) {
        af_editor_status(editor, "Select a drawing frame to duplicate");
        return;
    }
    if (track)
        frame = track->cel_frame + track->cel_length;
    while ((track = af_editor_frame_at(&editor->project, editor->selected_clip_id, frame)) != NULL)
        frame = track->cel_frame + track->cel_length;
    AfEditorCommand command;
    AfResult result = af_editor_command_frame(&editor->project, editor->selected_clip_id, frame,
                                              source, &command);
    if (af_editor_frame_execute(editor, &command, result) == AF_RESULT_SUCCESS) {
        af_editor_frame_seek(editor, frame);
        af_editor_status(editor,
                         duplicate ? "Drawing duplicated · edit this frame independently"
                                   : "Blank drawing ready · use the shape, pen or freehand tools");
    }
}

static void af_editor_frame_delete(AfEditorState *editor) {
    const uint32_t frame = af_editor_current_frame(editor);
    const AfProjectTrack *cel =
        af_editor_frame_at(&editor->project, editor->selected_clip_id, frame);
    if (!cel)
        return;
    AfEditorCommand command;
    AfResult result = af_editor_command_delete(&editor->project, cel->node_id, &command);
    if (af_editor_frame_execute(editor, &command, result) == AF_RESULT_SUCCESS) {
        af_editor_frame_seek(editor, frame);
        af_editor_status(editor, "Drawing deleted · undo restores it");
    }
}

static void af_editor_frame_fit_length(AfEditorState *editor) {
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    if (!clip)
        return;
    float duration = 1.0F / (float)af_editor_frame_rate(clip);
    for (uint32_t i = 0; i < editor->project.keyframe_count; ++i) {
        const AfProjectKeyframe *key = &editor->project.keyframes[i];
        const AfProjectTrack *track = af_project_find_track_const(&editor->project, key->track_id);
        if (track && track->clip_id == clip->id)
            duration = fmaxf(duration, key->keyframe.time);
    }
    AfEditorCommand command = {0};
    command.type = AF_EDITOR_COMMAND_ANIMATION;
    command.clip_before_count = command.clip_after_count = 1;
    (void)af_project_clip_snapshot(&editor->project, clip->id, &command.clip_before[0]);
    command.clip_after[0] = command.clip_before[0];
    command.clip_after[0].clip.duration = duration;
    command.clip_after[0].clip.loop_start = 0;
    command.clip_after[0].clip.loop_end = duration;
    if (af_editor_frame_execute(editor, &command, AF_RESULT_SUCCESS) == AF_RESULT_SUCCESS) {
        af_editor_frame_seek(editor, 0);
        af_editor_status(editor, "Clip length fits its drawings and animation keys");
    }
}

static void af_editor_frame_button(AfEditorState *editor, float x, float y, float width,
                                   const char *label, bool active) {
    af_editor_add_button_rect(editor, x, y, width, 24, active ? AF_UI_SELECTION : AF_UI_FIELD,
                              100140);
    af_editor_add_text(editor, x + 7, y + 8, label, .58F, width - 12, AF_UI_TEXT, 100145);
}

static void af_editor_render_frames(AfEditorState *editor, int width, int height) {
    const float left = editor->left_width + 8;
    const float top = af_editor_timeline_top(editor, height);
    const float ruler = af_editor_timeline_ruler_x(editor);
    const float right = (float)width - editor->right_width;
    const float half = (editor->timeline_header_width - 20) / 2;
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    const uint32_t fps = af_editor_frame_rate(clip), current = af_editor_current_frame(editor);
    const AfProjectTrack *active =
        af_editor_frame_at(&editor->project, editor->selected_clip_id, current);
    af_editor_frame_button(editor, left, top + 76, half, "< Previous", false);
    af_editor_frame_button(editor, left + half + 4, top + 76, half, "Next >", false);
    af_editor_frame_button(editor, left, top + 104, half, "+ Blank", false);
    af_editor_frame_button(editor, left + half + 4, top + 104, half, "Duplicate", false);
    af_editor_frame_button(editor, left, top + 132, half, "Onion skin",
                           !editor->onion_skin_disabled);
    af_editor_frame_button(editor, left + half + 4, top + 132, half, "Delete", false);
    char label[48];
    snprintf(label, sizeof(label), "Hold %u", active ? active->cel_length : 1);
    af_editor_add_text(editor, left + 22, top + 169, label, .52F, half - 42, AF_UI_TEXT, 100145);
    snprintf(label, sizeof(label), "%u fps", fps);
    af_editor_add_text(editor, left + half + 26, top + 169, label, .52F, half - 42, AF_UI_TEXT,
                       100145);
    for (uint32_t i = 0; i < 2; ++i) {
        const float x = left + (float)i * (half + 4);
        af_editor_frame_button(editor, x, top + 160, 20, "-", false);
        af_editor_frame_button(editor, x + half - 20, top + 160, 20, "+", false);
    }
    if (editor->timeline_height >= 218)
        af_editor_frame_button(editor, left, top + 188, half * 2 + 4, "Fit clip length", false);
    const float cell = editor->timeline_zoom / (float)fps;
    const uint32_t last = (uint32_t)ceilf(clip->duration * (float)fps);
    uint32_t first = (uint32_t)fmaxf(0, floorf(af_editor_x_to_time(editor, ruler) * (float)fps));
    /* Bounded work even if a project has a very long clip. */
    for (uint32_t frame = first; frame <= last && frame - first < 600U; ++frame) {
        const float x = af_editor_time_to_x(editor, (float)frame / (float)fps);
        if (x >= right)
            break;
        const float a = fmaxf(ruler + 1, x), b = fminf(right - 1, x + cell - 1);
        if (b <= a)
            continue;
        const AfProjectTrack *cel =
            af_editor_frame_at(&editor->project, editor->selected_clip_id, frame);
        const AfColor color = frame == current ? AF_UI_SELECTION
                              : cel            ? (AfColor){.23F, .39F, .48F, 1}
                                               : AF_UI_FIELD;
        af_editor_add_rect(editor, a, top + 78, b - a, 40, color, 100130);
        if (frame == current || (cell >= 22 || frame % 5 == 0)) {
            snprintf(label, sizeof(label), "%u", frame + 1);
            af_editor_add_text(editor, a + 3, top + 53, label, .58F, fminf(40, right - a - 3),
                               AF_UI_TEXT, 100145);
        }
        if (cel && cel->cel_frame == frame && b - a > 8)
            af_editor_add_shape(editor, AF_NODE_ELLIPSE,
                                (AfAffine2){.m00 = 1, .m11 = 1, .tx = a + 5, .ty = top + 93},
                                (AfVec2){7, 7}, AF_UI_TEXT, 0, 100140);
        else if (cel && b - a > 4)
            af_editor_add_rect(editor, a + 2, top + 97, b - a - 4, 2, AF_UI_TEXT, 100140);
    }
    snprintf(label, sizeof(label), "Frame %u  ·  %u fps%s", current + 1, fps,
             active ? "" : "  ·  Empty");
    af_editor_add_text(editor, ruler + 24, top + 134, label, .7F, right - ruler - 32, AF_UI_TEXT,
                       100145);
    af_editor_add_text(editor, ruler + 24, top + 159,
                       "Click a cell to draw · [ / ] to step · middle-drag to pan", .6F,
                       right - ruler - 32, AF_UI_TEXT, 100145);
    const float play_x = af_editor_time_to_x(editor, editor->playhead);
    if (play_x >= ruler && play_x < right)
        af_editor_add_rect(editor, play_x, top + 73, 2, 49, (AfColor){1, .65F, .2F, 1}, 100150);
}

static bool af_editor_frames_mouse(AfEditorState *editor, int width, int height, float x, float y) {
    (void)width;
    const float top = af_editor_timeline_top(editor, height);
    const float left = editor->left_width + 8;
    const float half = (editor->timeline_header_width - 20) / 2;
    const float ruler = af_editor_timeline_ruler_x(editor);
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    if (!clip)
        return true;
    if (x >= ruler && y >= top + 44 && y <= top + 122) {
        const float frame =
            floorf(af_editor_x_to_time(editor, x) * (float)af_editor_frame_rate(clip) + .0001F);
        af_editor_frame_seek(editor, (uint32_t)fmaxf(0, frame));
        return true;
    }
    if (x < left || x >= ruler - 8 || y < top + 76)
        return true;
    const bool second = x >= left + half + 4;
    const uint32_t frame = af_editor_current_frame(editor);
    if (y < top + 100)
        af_editor_frame_seek(editor, second ? frame + 1 : frame ? frame - 1 : 0);
    else if (y >= top + 104 && y < top + 128)
        af_editor_frame_new(editor, second);
    else if (y >= top + 132 && y < top + 156) {
        if (!second)
            editor->onion_skin_disabled = !editor->onion_skin_disabled;
        else
            af_editor_frame_delete(editor);
    } else if (y >= top + 188 && y < top + 212 && editor->timeline_height >= 218) {
        af_editor_frame_fit_length(editor);
    } else if (y >= top + 160 && y < top + 184) {
        const float local = x - left - (second ? half + 4 : 0);
        if (local > 20 && local < half - 20)
            return true;
        const int delta = local >= half - 20 ? 1 : -1;
        AfEditorCommand command;
        AfResult result;
        if (second) {
            int fps = (int)af_editor_frame_rate(clip) + delta;
            if (fps < 1 || fps > 120)
                return true;
            result =
                af_editor_command_frame_rate(&editor->project, clip->id, (uint32_t)fps, &command);
            if (af_editor_frame_execute(editor, &command, result) == AF_RESULT_SUCCESS) {
                editor->timeline_zoom = (float)fps * 24;
                af_editor_frame_seek(editor, frame);
            }
        } else {
            const AfProjectTrack *cel = af_editor_frame_at(&editor->project, clip->id, frame);
            if (!cel || (cel->cel_length == 1 && delta < 0))
                return true;
            result = af_editor_command_frame_hold(
                &editor->project, cel->id, (uint32_t)((int)cel->cel_length + delta), &command);
            (void)af_editor_frame_execute(editor, &command, result);
        }
    }
    return true;
}

static void af_editor_render_onion(AfEditorState *editor, AfAffine2 camera) {
    if (!editor->frames_visible || editor->onion_skin_disabled || editor->preview_playing ||
        editor->export_panel_visible)
        return;
    const uint32_t frame = af_editor_current_frame(editor);
    const AfProjectTrack *previous = NULL, *next = NULL;
    for (uint32_t i = 0; i < editor->project.track_count; ++i) {
        const AfProjectTrack *track = &editor->project.tracks[i];
        if (track->clip_id != editor->selected_clip_id || !track->cel_length)
            continue;
        if (track->cel_frame + track->cel_length <= frame &&
            (!previous || track->cel_frame > previous->cel_frame))
            previous = track;
        if (track->cel_frame > frame && (!next || track->cel_frame < next->cel_frame))
            next = track;
    }
    const AfProjectTrack *neighbors[2] = {previous, next};
    const float saved = editor->playhead;
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    uint32_t segments = 0;
    for (uint32_t side = 0; side < 2; ++side) {
        if (!neighbors[side])
            continue;
        editor->playhead = (float)neighbors[side]->cel_frame / (float)af_editor_frame_rate(clip);
        af_editor_update_preview(editor);
        AfColor color = side ? (AfColor){1, .4F, .62F, .32F} : (AfColor){.25F, .8F, 1, .32F};
        for (uint32_t i = 0; i < editor->preview_scene.count; ++i) {
            const AfSceneNode *node = &editor->preview_scene.nodes[i];
            const AfProjectTrack *owner = af_editor_frame_owner(&editor->project, node->id);
            if (!owner || owner->id != neighbors[side]->id || !node->world_visible ||
                node->world_opacity <= 0)
                continue;
            const AfAffine2 transform = af_affine_multiply(camera, node->world_transform);
            if (node->type >= AF_NODE_RECTANGLE && node->type <= AF_NODE_ELLIPSE) {
                af_editor_add_shape(editor, node->type, transform, node->resolved_bounds, color,
                                    node->corner_radius, -1000);
            } else if (node->type == AF_NODE_PATH && editor->path_scratch) {
                const AfPath *path = af_path_find_node_const(&editor->render_path_data, node->id);
                AfFlattenedPath flat = {.points = editor->path_scratch->points,
                                        .point_capacity = AF_MAX_PATH_FLATTENED_POINTS,
                                        .contours = editor->path_scratch->contours,
                                        .contour_capacity = AF_MAX_PATH_CONTOURS};
                if (!path || af_path_flatten(&editor->render_path_data, path, .7F, &flat) !=
                                 AF_RESULT_SUCCESS)
                    continue;
                for (uint32_t c = 0; c < flat.contour_count; ++c) {
                    const AfPathContour *contour = &flat.contours[c];
                    if (contour->point_count < 2)
                        continue;
                    const uint32_t count = contour->point_count - (contour->closed ? 0U : 1U);
                    for (uint32_t p = 0; p < count && segments < 1000; ++p, ++segments) {
                        const AfVec2 a = flat.points[contour->point_start + p];
                        const AfVec2 b =
                            flat.points[contour->point_start + (p + 1) % contour->point_count];
                        af_editor_add_line(editor, af_affine_transform_point(transform, a),
                                           af_affine_transform_point(transform, b), 1.5F, color,
                                           -1000);
                    }
                }
            }
        }
    }
    editor->playhead = saved;
    af_editor_update_preview(editor);
}
