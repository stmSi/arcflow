static void af_editor_layout(const AfEditorState *editor, int width, int height, float *canvas_x,
                             float *canvas_y, float *canvas_width, float *canvas_height) {
    *canvas_x = editor->left_width;
    *canvas_y = AF_EDITOR_TOOLBAR_HEIGHT;
    *canvas_width = (float)width - editor->left_width - editor->right_width;
    *canvas_height = (float)height - AF_EDITOR_TOOLBAR_HEIGHT - AF_EDITOR_STATUS_HEIGHT -
                     editor->timeline_height;
}

static float af_editor_timeline_top(const AfEditorState *editor, int height) {
    return (float)height - AF_EDITOR_STATUS_HEIGHT - editor->timeline_height;
}

static float af_editor_timeline_ruler_x(const AfEditorState *editor) {
    return editor->left_width + AF_EDITOR_TIMELINE_HEADER_WIDTH;
}

static float af_editor_time_to_x(const AfEditorState *editor, float time) {
    return af_editor_timeline_ruler_x(editor) +
           (time - editor->timeline_pan) * editor->timeline_zoom;
}

static float af_editor_x_to_time(const AfEditorState *editor, float x) {
    return editor->timeline_pan + (x - af_editor_timeline_ruler_x(editor)) / editor->timeline_zoom;
}

static const AfProjectTrack *af_editor_selected_track(const AfEditorState *editor) {
    uint32_t index = 0U;
    for (index = 0U; index < editor->project.track_count; ++index) {
        const AfProjectTrack *track = &editor->project.tracks[index];
        if (track->clip_id == editor->selected_clip_id && track->node_id == editor->selected_id &&
            track->property == editor->selected_property &&
            (track->property < AF_PROPERTY_PATH_POINT ||
             track->element_index == editor->selected_path_command)) {
            return track;
        }
    }
    return NULL;
}

static AfProjectKeyframe *af_editor_track_key_at(AfEditorState *editor, uint64_t track_id,
                                                 float time) {
    for (uint32_t index = 0U; index < editor->project.keyframe_count; ++index) {
        AfProjectKeyframe *key = &editor->project.keyframes[index];
        if (key->track_id == track_id && fabsf(key->keyframe.time - time) <= 0.0001F) {
            return key;
        }
    }
    return NULL;
}

static bool af_editor_keyframe_selected(const AfEditorState *editor, uint64_t id) {
    uint32_t index = 0U;
    for (index = 0U; index < editor->selected_keyframe_count; ++index) {
        if (editor->selected_keyframes[index] == id) {
            return true;
        }
    }
    return false;
}

static AfColor af_editor_property_color(AfPropertyType property) {
    static const AfColor colors[] = {
        {0.3F, 0.72F, 1.0F, 1.0F},  {1.0F, 0.56F, 0.3F, 1.0F},  {0.45F, 0.9F, 0.46F, 1.0F},
        {0.78F, 0.58F, 1.0F, 1.0F}, {1.0F, 0.35F, 0.55F, 1.0F}, {0.96F, 0.8F, 0.28F, 1.0F},
        {0.3F, 0.9F, 0.88F, 1.0F},  {0.55F, 0.7F, 1.0F, 1.0F},  {0.75F, 0.48F, 1.0F, 1.0F}};
    return property <= AF_PROPERTY_PATH_CONTROL_OUT ? colors[property]
                                                    : (AfColor){1.0F, 1.0F, 1.0F, 1.0F};
}

static const char *af_editor_property_name(AfPropertyType property) {
    static const char *const names[] = {"POSITION", "ROTATION", "SCALE",     "OPACITY",   "FILL",
                                        "RADIUS",   "ANCHOR",   "HANDLE IN", "HANDLE OUT"};
    return property <= AF_PROPERTY_PATH_CONTROL_OUT ? names[property] : "UNKNOWN";
}

static uint32_t af_editor_track_row(const AfEditorState *editor, uint64_t track_id) {
    uint32_t index = 0U;
    uint32_t row = 0U;
    for (index = 0U; index < editor->project.track_count; ++index) {
        if (editor->project.tracks[index].clip_id != editor->selected_clip_id) {
            continue;
        }
        if (editor->project.tracks[index].id == track_id) {
            return row;
        }
        ++row;
    }
    return UINT32_MAX;
}

static float af_editor_curve_value(const AfProjectTrack *track, const AfProjectKeyframe *key) {
    float value = key->keyframe.value[0];
    switch (track->property) {
    case AF_PROPERTY_POSITION:
        value /= 1280.0F;
        break;
    case AF_PROPERTY_ROTATION:
        value = 0.5F + value / 6.28318530718F;
        break;
    case AF_PROPERTY_SCALE:
        value *= 0.5F;
        break;
    case AF_PROPERTY_CORNER_RADIUS:
        value *= 0.01F;
        break;
    case AF_PROPERTY_PATH_POINT:
    case AF_PROPERTY_PATH_CONTROL_IN:
    case AF_PROPERTY_PATH_CONTROL_OUT:
        value = 0.5F + value / 2560.0F;
        break;
    case AF_PROPERTY_OPACITY:
    case AF_PROPERTY_FILL_COLOR:
    default:
        break;
    }
    return af_editor_clamp(value, 0.0F, 1.0F);
}

static bool af_editor_curve_key_less(const AfProject *project, uint32_t left, uint32_t right) {
    const AfProjectKeyframe *left_key = &project->keyframes[left];
    const AfProjectKeyframe *right_key = &project->keyframes[right];
    return left_key->keyframe.time < right_key->keyframe.time ||
           (left_key->keyframe.time == right_key->keyframe.time && left_key->id < right_key->id);
}

static void af_editor_curve_sift(const AfProject *project, uint32_t *order, uint32_t root,
                                 uint32_t count) {
    for (;;) {
        uint32_t child = root * 2U + 1U;
        uint32_t largest = root;
        if (child < count && af_editor_curve_key_less(project, order[largest], order[child])) {
            largest = child;
        }
        if (child + 1U < count &&
            af_editor_curve_key_less(project, order[largest], order[child + 1U])) {
            largest = child + 1U;
        }
        if (largest == root) {
            return;
        }
        {
            const uint32_t swap = order[root];
            order[root] = order[largest];
            order[largest] = swap;
        }
        root = largest;
    }
}

static void af_editor_curve_sort(const AfProject *project, uint32_t *order, uint32_t count) {
    uint32_t index = count / 2U;
    while (index > 0U) {
        --index;
        af_editor_curve_sift(project, order, index, count);
    }
    index = count;
    while (index > 1U) {
        const uint32_t swap = order[0];
        order[0] = order[index - 1U];
        order[index - 1U] = swap;
        --index;
        af_editor_curve_sift(project, order, 0U, index);
    }
}

static float af_editor_curve_bezier(float first, float second, float parameter) {
    const float inverse = 1.0F - parameter;
    return 3.0F * inverse * inverse * parameter * first +
           3.0F * inverse * parameter * parameter * second + parameter * parameter * parameter;
}

static void af_editor_render_curve(AfEditorState *editor, int width, int height) {
    const AfProjectTrack *track = af_editor_selected_track(editor);
    const float top = af_editor_timeline_top(editor, height) + AF_EDITOR_TIMELINE_RULER_HEIGHT;
    const float bottom = (float)height - AF_EDITOR_STATUS_HEIGHT - 12.0F;
    const float left = af_editor_timeline_ruler_x(editor);
    const float right = (float)width - editor->right_width;
    uint32_t curve_count = 0U;
    uint32_t index = 0U;
    if (track == NULL || bottom <= top || editor->curve_order == NULL) {
        return;
    }
    for (index = 0U; index <= 4U; ++index) {
        const float y = top + (bottom - top) * (float)index * 0.25F;
        af_editor_add_rect(editor, left, y, right - left, 1.0F, (AfColor){0.2F, 0.22F, 0.27F, 1.0F},
                           100115);
    }
    for (index = 0U; index < editor->project.keyframe_count; ++index) {
        if (editor->project.keyframes[index].track_id == track->id) {
            editor->curve_order[curve_count++] = index;
        }
    }
    af_editor_curve_sort(&editor->project, editor->curve_order, curve_count);
    for (index = 1U; index < curve_count; ++index) {
        const AfProjectKeyframe *previous =
            &editor->project.keyframes[editor->curve_order[index - 1U]];
        const AfProjectKeyframe *key = &editor->project.keyframes[editor->curve_order[index]];
        const float first_value = af_editor_curve_value(track, previous);
        const float second_value = af_editor_curve_value(track, key);
        AfVec2 start = {af_editor_time_to_x(editor, previous->keyframe.time),
                        bottom - first_value * (bottom - top)};
        if (previous->keyframe.interpolation == AF_INTERPOLATION_STEP) {
            const AfVec2 corner = {af_editor_time_to_x(editor, key->keyframe.time), start.y};
            const AfVec2 end = {corner.x, bottom - second_value * (bottom - top)};
            af_editor_add_line(editor, start, corner, 2.0F,
                               af_editor_property_color(track->property), 100130);
            af_editor_add_line(editor, corner, end, 2.0F, af_editor_property_color(track->property),
                               100130);
        } else if (previous->keyframe.interpolation == AF_INTERPOLATION_CUBIC_BEZIER) {
            for (uint32_t sample = 1U; sample <= 16U; ++sample) {
                const float parameter = (float)sample / 16.0F;
                const float time_progress = af_editor_curve_bezier(
                    previous->keyframe.easing[0], previous->keyframe.easing[2], parameter);
                const float value_progress = af_editor_curve_bezier(
                    previous->keyframe.easing[1], previous->keyframe.easing[3], parameter);
                const AfVec2 end = {
                    af_editor_time_to_x(editor, previous->keyframe.time +
                                                    (key->keyframe.time - previous->keyframe.time) *
                                                        time_progress),
                    bottom - (first_value + (second_value - first_value) * value_progress) *
                                 (bottom - top)};
                af_editor_add_line(editor, start, end, 2.0F,
                                   af_editor_property_color(track->property), 100130);
                start = end;
            }
        } else {
            const AfVec2 end = {af_editor_time_to_x(editor, key->keyframe.time),
                                bottom - second_value * (bottom - top)};
            af_editor_add_line(editor, start, end, 2.0F, af_editor_property_color(track->property),
                               100130);
        }
    }
    if (editor->selected_keyframe_count == 1U) {
        const AfProjectKeyframe *key =
            af_project_find_keyframe_const(&editor->project, editor->selected_keyframes[0]);
        if (key != NULL && key->track_id == track->id &&
            key->keyframe.interpolation == AF_INTERPOLATION_CUBIC_BEZIER) {
            const AfVec2 origin = {left + 30.0F, bottom - 30.0F};
            const AfVec2 out = {origin.x + key->keyframe.easing[0] * 100.0F,
                                origin.y - key->keyframe.easing[1] * 80.0F};
            const AfVec2 in = {right - 130.0F + key->keyframe.easing[2] * 100.0F,
                               bottom - key->keyframe.easing[3] * 80.0F};
            const AfVec2 in_origin = {right - 30.0F, top + 30.0F};
            af_editor_add_line(editor, origin, out, 2.0F, (AfColor){0.96F, 0.58F, 0.25F, 1.0F},
                               100140);
            af_editor_add_line(editor, in_origin, in, 2.0F, (AfColor){0.35F, 0.74F, 1.0F, 1.0F},
                               100140);
            af_editor_add_rect(editor, out.x - 5.0F, out.y - 5.0F, 10.0F, 10.0F,
                               (AfColor){0.96F, 0.58F, 0.25F, 1.0F}, 100150);
            af_editor_add_rect(editor, in.x - 5.0F, in.y - 5.0F, 10.0F, 10.0F,
                               (AfColor){0.35F, 0.74F, 1.0F, 1.0F}, 100150);
        }
    }
}

static void af_editor_render_timeline(AfEditorState *editor, int width, int height) {
    const float top = af_editor_timeline_top(editor, height);
    const float center_left = editor->left_width;
    const float center_right = (float)width - editor->right_width;
    const float ruler_x = af_editor_timeline_ruler_x(editor);
    const float bottom = (float)height - AF_EDITOR_STATUS_HEIGHT;
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    float tick_time = 0.0F;
    uint32_t index = 0U;
    uint32_t row = 0U;
    af_editor_add_rect(editor, center_left, top, center_right - center_left,
                       editor->timeline_height, (AfColor){0.075F, 0.082F, 0.102F, 1.0F}, 100000);
    af_editor_add_rect(editor, center_left, top - 2.0F, center_right - center_left, 4.0F,
                       (AfColor){0.27F, 0.29F, 0.35F, 1.0F}, 100110);
    af_editor_add_rect(editor, center_left + 10.0F, top + 9.0F, 26.0F, 26.0F,
                       editor->preview_playing ? (AfColor){0.3F, 0.82F, 0.46F, 1.0F}
                                               : (AfColor){0.24F, 0.27F, 0.33F, 1.0F},
                       100120);
    af_editor_add_shape(
        editor, AF_NODE_ELLIPSE,
        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = center_left + 47.0F, .ty = top + 14.0F},
        (AfVec2){16.0F, 16.0F}, (AfColor){0.9F, 0.34F, 0.3F, 1.0F}, 0.0F, 100120);
    af_editor_add_rect(editor, center_left + 73.0F, top + 9.0F, 34.0F, 26.0F,
                       editor->curve_visible ? (AfColor){0.48F, 0.4F, 0.82F, 1.0F}
                                             : (AfColor){0.24F, 0.27F, 0.33F, 1.0F},
                       100120);
    af_editor_add_text(editor, center_left + 16.0F, top + 19.0F, "PLAY", 0.5F, 20.0F,
                       (AfColor){0.9F, 0.92F, 0.96F, 1.0F}, 100135);
    af_editor_add_text(editor, center_left + 47.0F, top + 19.0F, "STOP", 0.5F, 20.0F,
                       (AfColor){0.9F, 0.92F, 0.96F, 1.0F}, 100135);
    af_editor_add_text(editor, center_left + 76.0F, top + 19.0F, "CURVE", 0.45F, 31.0F,
                       (AfColor){0.9F, 0.92F, 0.96F, 1.0F}, 100135);
    for (index = 0U; index < 9U; ++index) {
        const float x = center_left + 116.0F + (float)index * 20.0F;
        static const char *const property_icons[] = {"P", "R", "S", "O", "F", "C", "A", "I", "H"};
        af_editor_add_rect(editor, x, top + 12.0F, 15.0F, 18.0F,
                           index == (uint32_t)editor->selected_property
                               ? af_editor_property_color((AfPropertyType)index)
                               : (AfColor){0.22F, 0.24F, 0.29F, 1.0F},
                           100120);
        af_editor_add_text(editor, x + 5.0F, top + 18.0F, property_icons[index], 0.85F, 6.0F,
                           (AfColor){0.94F, 0.95F, 0.98F, 1.0F}, 100135);
    }
    af_editor_add_text(editor, center_left + 302.0F, top + 18.0F, "TIMELINE", 0.9F, 58.0F,
                       (AfColor){0.62F, 0.65F, 0.72F, 1.0F}, 100130);
    for (index = 0U; index < editor->project.clip_count && index < 8U; ++index) {
        const float x = center_right - 74.0F * (float)(index + 1U);
        af_editor_add_rect(editor, x, top + 10.0F, 68.0F, 22.0F,
                           editor->project.clips[index].id == editor->selected_clip_id
                               ? (AfColor){0.2F, 0.42F, 0.68F, 1.0F}
                               : (AfColor){0.18F, 0.2F, 0.25F, 1.0F},
                           100120);
        af_editor_add_text(editor, x + 4.0F, top + 17.0F, editor->project.clips[index].name, 0.75F,
                           60.0F, (AfColor){0.92F, 0.94F, 0.98F, 1.0F}, 100135);
    }
    af_editor_add_rect(editor, center_left, top + 40.0F, center_right - center_left, 1.0F,
                       (AfColor){0.25F, 0.27F, 0.32F, 1.0F}, 100115);
    if (clip == NULL) {
        return;
    }
    tick_time = ceilf(editor->timeline_pan * 2.0F) * 0.5F;
    while (af_editor_time_to_x(editor, tick_time) < center_right && tick_time <= clip->duration) {
        const float x = af_editor_time_to_x(editor, tick_time);
        if (x >= ruler_x) {
            char tick_label[16];
            af_editor_add_rect(editor, x, top + 40.0F, 1.0F, 11.0F,
                               (AfColor){0.48F, 0.5F, 0.58F, 1.0F}, 100120);
            (void)snprintf(tick_label, sizeof(tick_label), "%.1f", (double)tick_time);
            af_editor_add_text(editor, x + 3.0F, top + 42.0F, tick_label, 0.72F, 24.0F,
                               (AfColor){0.58F, 0.61F, 0.69F, 1.0F}, 100125);
        }
        tick_time += 0.5F;
    }
    if (clip->loop) {
        const float loop_start = af_editor_time_to_x(editor, clip->loop_start);
        const float loop_end = af_editor_time_to_x(editor, clip->loop_end);
        af_editor_add_rect(editor, loop_start, top + 43.0F, loop_end - loop_start, 5.0F,
                           (AfColor){0.25F, 0.7F, 0.42F, 1.0F}, 100125);
        af_editor_add_rect(editor, loop_start - 3.0F, top + 40.0F, 6.0F, 15.0F,
                           (AfColor){0.35F, 0.9F, 0.5F, 1.0F}, 100130);
        af_editor_add_rect(editor, loop_end - 3.0F, top + 40.0F, 6.0F, 15.0F,
                           (AfColor){0.35F, 0.9F, 0.5F, 1.0F}, 100130);
    }
    if (editor->curve_visible) {
        af_editor_render_curve(editor, width, height);
    } else {
        for (index = 0U; index < editor->project.track_count; ++index) {
            const AfProjectTrack *track = &editor->project.tracks[index];
            float y = 0.0F;
            uint32_t key_index = 0U;
            if (track->clip_id != editor->selected_clip_id) {
                continue;
            }
            y = top + AF_EDITOR_TIMELINE_RULER_HEIGHT +
                (float)row * AF_EDITOR_TIMELINE_TRACK_HEIGHT;
            af_editor_add_rect(editor, center_left, y, AF_EDITOR_TIMELINE_HEADER_WIDTH,
                               AF_EDITOR_TIMELINE_TRACK_HEIGHT - 1.0F,
                               track->node_id == editor->selected_id &&
                                       track->property == editor->selected_property &&
                                       (track->property < AF_PROPERTY_PATH_POINT ||
                                        track->element_index == editor->selected_path_command)
                                   ? (AfColor){0.16F, 0.27F, 0.42F, 1.0F}
                                   : (AfColor){0.11F, 0.12F, 0.15F, 1.0F},
                               100115);
            af_editor_add_rect(editor, center_left + 10.0F, y + 7.0F, 5.0F, 12.0F,
                               af_editor_property_color(track->property), 100125);
            af_editor_add_text(editor, center_left + 23.0F, y + 8.0F,
                               af_editor_property_name(track->property), 1.0F,
                               AF_EDITOR_TIMELINE_HEADER_WIDTH - 54.0F,
                               (AfColor){0.72F, 0.75F, 0.82F, 1.0F}, 100125);
            if (track->property >= AF_PROPERTY_PATH_POINT) {
                char point_label[16];
                (void)snprintf(point_label, sizeof(point_label), "#%u", track->element_index + 1U);
                af_editor_add_text(editor, center_left + AF_EDITOR_TIMELINE_HEADER_WIDTH - 31.0F,
                                   y + 8.0F, point_label, 0.72F, 27.0F,
                                   (AfColor){0.45F, 0.72F, 0.82F, 1.0F}, 100125);
            }
            af_editor_add_rect(editor, ruler_x, y, center_right - ruler_x,
                               AF_EDITOR_TIMELINE_TRACK_HEIGHT - 1.0F,
                               (row & 1U) != 0U ? (AfColor){0.095F, 0.105F, 0.13F, 1.0F}
                                                : (AfColor){0.105F, 0.115F, 0.14F, 1.0F},
                               100110);
            for (key_index = 0U; key_index < editor->project.keyframe_count; ++key_index) {
                const AfProjectKeyframe *key = &editor->project.keyframes[key_index];
                const float key_x = af_editor_time_to_x(editor, key->keyframe.time);
                AfAffine2 diamond = {0};
                if (key->track_id != track->id || key_x < ruler_x - 8.0F ||
                    key_x > center_right + 8.0F) {
                    continue;
                }
                diamond = (AfAffine2){.m00 = 0.70710678F,
                                      .m01 = -0.70710678F,
                                      .m10 = 0.70710678F,
                                      .m11 = 0.70710678F,
                                      .tx = key_x,
                                      .ty = y + 7.0F};
                af_editor_add_shape(editor, AF_NODE_RECTANGLE, diamond, (AfVec2){10.0F, 10.0F},
                                    af_editor_keyframe_selected(editor, key->id)
                                        ? (AfColor){1.0F, 0.82F, 0.26F, 1.0F}
                                        : af_editor_property_color(track->property),
                                    0.0F, 100135);
            }
            ++row;
        }
    }
    {
        const float playhead_x = af_editor_time_to_x(editor, editor->playhead);
        if (playhead_x >= ruler_x && playhead_x <= center_right) {
            af_editor_add_rect(editor, playhead_x - 1.0F, top + 40.0F, 2.0F, bottom - top - 40.0F,
                               (AfColor){1.0F, 0.32F, 0.28F, 1.0F}, 100170);
            af_editor_add_shape(
                editor, AF_NODE_ELLIPSE,
                (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = playhead_x - 5.0F, .ty = top + 37.0F},
                (AfVec2){10.0F, 10.0F}, (AfColor){1.0F, 0.32F, 0.28F, 1.0F}, 0.0F, 100180);
        }
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_KEY_BOX) {
        const float x = fminf(editor->key_box_start.x, editor->key_box_current.x);
        const float y = fminf(editor->key_box_start.y, editor->key_box_current.y);
        const float box_width = fabsf(editor->key_box_current.x - editor->key_box_start.x);
        const float box_height = fabsf(editor->key_box_current.y - editor->key_box_start.y);
        af_editor_add_rect(editor, x, y, box_width, 1.0F, (AfColor){0.35F, 0.72F, 1.0F, 1.0F},
                           100190);
        af_editor_add_rect(editor, x, y + box_height, box_width, 1.0F,
                           (AfColor){0.35F, 0.72F, 1.0F, 1.0F}, 100190);
        af_editor_add_rect(editor, x, y, 1.0F, box_height, (AfColor){0.35F, 0.72F, 1.0F, 1.0F},
                           100190);
        af_editor_add_rect(editor, x + box_width, y, 1.0F, box_height,
                           (AfColor){0.35F, 0.72F, 1.0F, 1.0F}, 100190);
    }
}
