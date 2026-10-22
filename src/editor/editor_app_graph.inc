/* Shared geometry keeps curve drawing, picking and tangent dragging in agreement. */
typedef struct AfEditorCurveView {
    float left, right, top, bottom, minimum, maximum;
    uint32_t component;
} AfEditorCurveView;

static uint32_t af_editor_curve_components(AfPropertyType property) {
    switch (property) {
    case AF_PROPERTY_FILL_COLOR:
    case AF_PROPERTY_OUTLINE_COLOR:
    case AF_PROPERTY_GRADIENT_STOP_COLOR:
        return 4U;
    case AF_PROPERTY_POSITION:
    case AF_PROPERTY_SCALE:
    case AF_PROPERTY_SKEW:
    case AF_PROPERTY_PATH_POINT:
    case AF_PROPERTY_PATH_CONTROL_IN:
    case AF_PROPERTY_PATH_CONTROL_OUT:
    case AF_PROPERTY_GRADIENT_START:
    case AF_PROPERTY_GRADIENT_END:
    case AF_PROPERTY_GRADIENT_RADII:
        return 2U;
    default:
        return 1U;
    }
}

static const AfProjectKeyframe *af_editor_curve_next(const AfEditorState *editor,
                                                     const AfProjectKeyframe *key) {
    const AfProjectKeyframe *next = NULL;
    for (uint32_t i = 0; i < editor->project.keyframe_count; ++i) {
        const AfProjectKeyframe *candidate = &editor->project.keyframes[i];
        if (candidate->track_id == key->track_id && candidate->keyframe.time > key->keyframe.time &&
            (next == NULL || candidate->keyframe.time < next->keyframe.time ||
             (candidate->keyframe.time == next->keyframe.time && candidate->id < next->id)))
            next = candidate;
    }
    return next;
}

static AfEditorCurveView af_editor_curve_view(const AfEditorState *editor, int width, int height) {
    const AfProjectTrack *track = af_editor_selected_track(editor);
    AfEditorCurveView view = {.left = af_editor_timeline_ruler_x(editor),
                              .right = (float)width - editor->right_width - 8,
                              .top = af_editor_timeline_top(editor, height) +
                                     AF_EDITOR_TIMELINE_RULER_HEIGHT + 14,
                              .bottom = (float)height - editor->status_height - 18,
                              .minimum = INFINITY,
                              .maximum = -INFINITY};
    if (track != NULL) {
        view.component = editor->curve_component % af_editor_curve_components(track->property);
        uint32_t count = 0;
        for (uint32_t i = 0; i < editor->project.keyframe_count; ++i) {
            const AfProjectKeyframe *key = &editor->project.keyframes[i];
            if (key->track_id != track->id)
                continue;
            if (editor->curve_order != NULL)
                editor->curve_order[count++] = i;
            const float value = key->keyframe.value[view.component];
            view.minimum = fminf(view.minimum, value);
            view.maximum = fmaxf(view.maximum, value);
        }
        af_editor_curve_sort(&editor->project, editor->curve_order, count);
        for (uint32_t i = 1; i < count; ++i) {
            const AfKeyframe *first =
                &editor->project.keyframes[editor->curve_order[i - 1]].keyframe;
            const AfKeyframe *next = &editor->project.keyframes[editor->curve_order[i]].keyframe;
            if (first->interpolation != AF_INTERPOLATION_CUBIC_BEZIER)
                continue;
            const float start = first->value[view.component];
            const float delta = next->value[view.component] - start;
            for (uint32_t j = 1; j < 4; j += 2) {
                const float control = start + delta * first->easing[j];
                view.minimum = fminf(view.minimum, control);
                view.maximum = fmaxf(view.maximum, control);
            }
        }
    }
    if (!isfinite(view.minimum)) {
        view.minimum = 0;
        view.maximum = 1;
    }
    const float padding =
        fmaxf((view.maximum - view.minimum) * .15F,
              fmaxf(.1F, fmaxf(fabsf(view.minimum), fabsf(view.maximum)) * .0001F));
    view.minimum -= padding;
    view.maximum += padding;
    if (editor->drag_mode == AF_EDITOR_DRAG_TANGENT_IN ||
        editor->drag_mode == AF_EDITOR_DRAG_TANGENT_OUT ||
        (editor->curve_visible && editor->drag_mode == AF_EDITOR_DRAG_KEYFRAMES)) {
        view.minimum = editor->curve_drag_min;
        view.maximum = editor->curve_drag_max;
    }
    return view;
}

static AfVec2 af_editor_curve_point(const AfEditorState *editor, AfEditorCurveView view,
                                    const AfProjectKeyframe *key) {
    return (AfVec2){af_editor_time_to_x(editor, key->keyframe.time),
                    view.bottom - (key->keyframe.value[view.component] - view.minimum) /
                                      (view.maximum - view.minimum) * (view.bottom - view.top)};
}

static bool af_editor_curve_segment(const AfEditorState *editor, AfEditorCurveView view,
                                    const AfProjectKeyframe *key, AfVec2 *start, AfVec2 *end) {
    const AfProjectTrack *track = af_editor_selected_track(editor);
    const AfProjectKeyframe *next = key != NULL ? af_editor_curve_next(editor, key) : NULL;
    if (track == NULL || key == NULL || key->track_id != track->id || next == NULL)
        return false;
    *start = af_editor_curve_point(editor, view, key);
    *end = af_editor_curve_point(editor, view, next);
    return end->x - start->x > .01F && fabsf(end->y - start->y) > .01F;
}

static AfVec2 af_editor_curve_handle(AfVec2 start, AfVec2 end, const float easing[4],
                                     bool incoming) {
    const uint32_t component = incoming ? 2U : 0U;
    return (AfVec2){start.x + (end.x - start.x) * easing[component],
                    start.y + (end.y - start.y) * easing[component + 1U]};
}

static bool af_editor_curve_contains(AfEditorCurveView view, AfVec2 point) {
    return point.x >= view.left && point.x <= view.right && point.y >= view.top &&
           point.y <= view.bottom;
}

/* Clip segments before submitting them so zoomed curves never cover other panels. */
static void af_editor_curve_line(AfEditorState *editor, AfEditorCurveView view, AfVec2 start,
                                 AfVec2 end, AfColor color, int32_t layer) {
    float low = 0, high = 1;
    const float dx = end.x - start.x, dy = end.y - start.y;
    const float p[] = {-dx, dx, -dy, dy};
    const float q[] = {start.x - view.left, view.right - start.x, start.y - view.top,
                       view.bottom - start.y};
    for (uint32_t i = 0; i < 4; ++i) {
        if (fabsf(p[i]) < .000001F) {
            if (q[i] < 0)
                return;
        } else if (p[i] < 0)
            low = fmaxf(low, q[i] / p[i]);
        else
            high = fminf(high, q[i] / p[i]);
    }
    if (low <= high)
        af_editor_add_line(editor, (AfVec2){start.x + low * dx, start.y + low * dy},
                           (AfVec2){start.x + high * dx, start.y + high * dy}, 2, color, layer);
}

static void af_editor_curve_fit(AfEditorState *editor, int width) {
    const AfProjectTrack *track = af_editor_selected_track(editor);
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    float first = INFINITY, last = -INFINITY;
    for (uint32_t i = 0; track != NULL && i < editor->project.keyframe_count; ++i) {
        const AfProjectKeyframe *key = &editor->project.keyframes[i];
        if (key->track_id != track->id)
            continue;
        first = fminf(first, key->keyframe.time);
        last = fmaxf(last, key->keyframe.time);
    }
    if (!isfinite(first) || last - first < .001F) {
        first = 0;
        last = clip != NULL ? clip->duration : 1;
    }
    const float available =
        (float)width - editor->right_width - af_editor_timeline_origin_x(editor) - 32;
    editor->timeline_zoom = af_editor_clamp(available / fmaxf(last - first, .1F), .01F, 1000);
    editor->timeline_pan = fmaxf(0, first);
    af_editor_status(editor, "Graph fitted to the selected track");
}

static const char *const af_editor_curve_presets[] = {"Linear",   "Hold",      "Ease in",
                                                      "Ease out", "Ease both", "Overshoot"};

static void af_editor_render_curve(AfEditorState *editor, int width, int height) {
    const AfProjectTrack *track = af_editor_selected_track(editor);
    const AfEditorCurveView view = af_editor_curve_view(editor, width, height);
    const float sidebar = editor->left_width + 10;
    const float sidebar_width = editor->timeline_header_width - 20;
    const float controls_top =
        af_editor_timeline_top(editor, height) + AF_EDITOR_TIMELINE_RULER_HEIGHT;
    if (view.bottom <= view.top || view.right <= view.left || editor->curve_order == NULL)
        return;
    if (track == NULL)
        return;
    af_editor_add_text(editor, sidebar, controls_top + 6,
                       af_project_name(&editor->project, track->node_id), .65F, sidebar_width,
                       AF_UI_TEXT, 100130);
    const uint32_t components = af_editor_curve_components(track->property);
    const char *const names[] = {"X", "Y", "Z", "W"};
    const char *const rgba[] = {"R", "G", "B", "A"};
    for (uint32_t i = 0; controls_top + 48 <= view.bottom && i < components; ++i) {
        const float x = sidebar + (float)i * sidebar_width / (float)components;
        af_editor_add_button_rect(editor, x, controls_top + 24,
                                  sidebar_width / (float)components - 4, 24,
                                  view.component == i ? AF_UI_SELECTION : AF_UI_FIELD, 100120);
        af_editor_add_text(editor, x + 8, controls_top + 31,
                           components == 1   ? "Value"
                           : components == 4 ? rgba[i]
                                             : names[i],
                           .7F, sidebar_width / (float)components - 16, AF_UI_TEXT, 100135);
    }
    for (uint32_t i = 0; i < 6; ++i) {
        const float x = sidebar + (float)(i % 2) * sidebar_width * .5F;
        const float y = controls_top + 56 + (float)(i / 2) * 28;
        if (y + 24 > view.bottom)
            break;
        af_editor_add_button_rect(editor, x, y, sidebar_width * .5F - 4, 24, AF_UI_FIELD, 100120);
        af_editor_add_text(editor, x + 6, y + 7, af_editor_curve_presets[i], .65F,
                           sidebar_width * .5F - 14, AF_UI_TEXT, 100135);
    }
    if (controls_top + 170 < view.bottom) {
        af_editor_add_button_rect(editor, sidebar, controls_top + 144, sidebar_width - 4, 24,
                                  AF_UI_FIELD, 100120);
        af_editor_add_text(editor, sidebar + 8, controls_top + 151, "Fit graph (F)", .7F,
                           sidebar_width - 20, AF_UI_TEXT, 100135);
    }
    if (controls_top + 190 <= view.bottom) {
        char readout[80] = "Select a key to ease its next segment";
        const AfProjectKeyframe *selected =
            editor->selected_keyframe_count == 1
                ? af_project_find_keyframe_const(&editor->project, editor->selected_keyframes[0])
                : NULL;
        if (selected != NULL && selected->track_id == track->id)
            (void)snprintf(readout, sizeof(readout), "%.2fs  /  %.3g",
                           (double)selected->keyframe.time,
                           (double)selected->keyframe.value[view.component]);
        af_editor_add_text(editor, sidebar, controls_top + 178, readout, .65F, sidebar_width,
                           AF_UI_MUTED, 100135);
    }
    for (uint32_t i = 0; i <= 4; ++i) {
        const float y = view.top + (view.bottom - view.top) * (float)i * .25F;
        const float value = view.maximum - (view.maximum - view.minimum) * (float)i * .25F;
        char label[32];
        (void)snprintf(label, sizeof(label), "%.3g", (double)value);
        af_editor_add_rect(editor, view.left, y, view.right - view.left, 1, AF_UI_BORDER, 100115);
        af_editor_add_text(editor, view.left + 6, y + 2, label, .6F, 68, AF_UI_MUTED, 100125);
    }
    uint32_t count = 0;
    for (uint32_t i = 0; i < editor->project.keyframe_count; ++i)
        if (editor->project.keyframes[i].track_id == track->id)
            editor->curve_order[count++] = i;
    af_editor_curve_sort(&editor->project, editor->curve_order, count);
    const AfColor color = af_editor_property_color(track->property);
    for (uint32_t i = 1; i < count; ++i) {
        const AfProjectKeyframe *previous = &editor->project.keyframes[editor->curve_order[i - 1]];
        const AfProjectKeyframe *key = &editor->project.keyframes[editor->curve_order[i]];
        const AfVec2 start = af_editor_curve_point(editor, view, previous);
        const AfVec2 end = af_editor_curve_point(editor, view, key);
        if (previous->keyframe.interpolation == AF_INTERPOLATION_STEP) {
            const AfVec2 corner = {end.x, start.y};
            af_editor_curve_line(editor, view, start, corner, color, 100130);
            af_editor_curve_line(editor, view, corner, end, color, 100130);
        } else if (previous->keyframe.interpolation == AF_INTERPOLATION_CUBIC_BEZIER) {
            AfVec2 point = start;
            for (uint32_t sample = 1; sample <= 48; ++sample) {
                const float t = (float)sample / 48;
                const AfVec2 next = {
                    start.x +
                        (end.x - start.x) * af_editor_curve_bezier(previous->keyframe.easing[0],
                                                                   previous->keyframe.easing[2], t),
                    start.y + (end.y - start.y) *
                                  af_editor_curve_bezier(previous->keyframe.easing[1],
                                                         previous->keyframe.easing[3], t)};
                af_editor_curve_line(editor, view, point, next, color, 100130);
                point = next;
            }
        } else
            af_editor_curve_line(editor, view, start, end, color, 100130);
    }
    if (editor->selected_keyframe_count == 1) {
        const AfProjectKeyframe *key =
            af_project_find_keyframe_const(&editor->project, editor->selected_keyframes[0]);
        AfVec2 start, end;
        if (key != NULL && key->keyframe.interpolation == AF_INTERPOLATION_CUBIC_BEZIER &&
            af_editor_curve_segment(editor, view, key, &start, &end)) {
            for (uint32_t i = 0; i < 2; ++i) {
                const AfVec2 handle =
                    af_editor_curve_handle(start, end, key->keyframe.easing, i != 0);
                af_editor_curve_line(editor, view, i == 0 ? start : end, handle, AF_UI_MUTED,
                                     100140);
                if (af_editor_curve_contains(view, handle)) {
                    af_editor_add_rect(editor, handle.x - 6, handle.y - 6, 12, 12, AF_UI_PANEL,
                                       100145);
                    af_editor_add_rect(editor, handle.x - 4, handle.y - 4, 8, 8, AF_UI_ACCENT,
                                       100150);
                }
            }
        }
    }
    for (uint32_t i = 0; i < count; ++i) {
        const AfProjectKeyframe *key = &editor->project.keyframes[editor->curve_order[i]];
        const AfVec2 point = af_editor_curve_point(editor, view, key);
        if (!af_editor_curve_contains(view, point))
            continue;
        af_editor_add_rect(editor, point.x - 6, point.y - 6, 12, 12, AF_UI_PANEL, 100175);
        af_editor_add_rect(
            editor, point.x - 4, point.y - 4, 8, 8,
            af_editor_keyframe_selected(editor, key->id) ? (AfColor){1, .82F, .26F, 1} : color,
            100180);
    }
}
