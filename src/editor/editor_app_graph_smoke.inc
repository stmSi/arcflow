static AfResult af_editor_graph_smoke(AfEditorState *editor, int width, int height) {
#define GRAPH_CHECK(c)                                                                             \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            af_log(AF_LOG_ERROR, "Graph smoke failed at line %u", (unsigned)__LINE__);             \
            return AF_RESULT_INVALID_ARGUMENT;                                                     \
        }                                                                                          \
    } while (0)
    /* The previous smoke created this disposable fixture; reset it without a dialog. */
    editor->dirty = false;
    GRAPH_CHECK(af_editor_new_project(editor) == AF_RESULT_SUCCESS);
    editor->welcome_visible = false;
    editor->curve_visible = true;
    editor->timeline_focus = true;
    editor->curve_component = 0;
    editor->selected_property = AF_PROPERTY_POSITION;
    editor->preview_playing = false;
    editor->timeline_height = 300;
    af_editor_select_only(editor, editor->project.scene.nodes[0].id);
    af_editor_update_preview(editor);
    editor->playhead = 0;
    GRAPH_CHECK(af_editor_add_keyframe(editor) == AF_RESULT_SUCCESS);
    const uint64_t first_id = editor->selected_keyframes[0];
    editor->playhead = 2;
    GRAPH_CHECK(af_editor_add_keyframe(editor) == AF_RESULT_SUCCESS);
    const uint64_t last_id = editor->selected_keyframes[0];
    AfProjectKeyframe *first = af_project_find_keyframe(&editor->project, first_id);
    AfProjectKeyframe *last = af_project_find_keyframe(&editor->project, last_id);
    first->keyframe.value[0] = -1200;
    first->keyframe.value[1] = 9000;
    last->keyframe.value[0] = 2000;
    last->keyframe.value[1] = -500;
    af_editor_curve_fit(editor, width);
    AfEditorCurveView view = af_editor_curve_view(editor, width, height);
    GRAPH_CHECK(view.minimum < -1200 && view.maximum > 2000);
    AfPlatformWindowMetrics metrics;
    GRAPH_CHECK(af_platform_window_metrics(&editor->window, &metrics) == AF_RESULT_SUCCESS);
    editor->playhead = 0;
    GRAPH_CHECK(fabsf(af_editor_time_to_x(editor, 0) - af_editor_timeline_ruler_x(editor) - 24) <
                .001F);
    GRAPH_CHECK(fabsf(af_editor_x_to_time(editor, af_editor_time_to_x(editor, 1.25F)) - 1.25F) <
                .001F);
    const float row_y =
        af_editor_timeline_top(editor, height) + AF_EDITOR_TIMELINE_RULER_HEIGHT + 17;
    editor->curve_visible = false;
    for (int side = -1; side <= 1; side += 2) {
        const AfVec2 press = {af_editor_time_to_x(editor, 0) + (float)side * 6, row_y};
        af_editor_gradient_smoke_button(editor, width, height, &metrics, press, true,
                                        SDL_BUTTON_LEFT);
        GRAPH_CHECK(editor->drag_mode == AF_EDITOR_DRAG_KEYFRAMES);
        const AfVec2 release = {press.x + editor->timeline_zoom * .5F, press.y};
        af_editor_gradient_smoke_motion(editor, width, height, &metrics, release);
        af_editor_gradient_smoke_button(editor, width, height, &metrics, release, false,
                                        SDL_BUTTON_LEFT);
        first = af_project_find_keyframe(&editor->project, first_id);
        GRAPH_CHECK(fabsf(first->keyframe.time - .5F) < .001F && editor->playhead == 0);
        GRAPH_CHECK(af_editor_history_undo(&editor->history, &editor->project) ==
                    AF_RESULT_SUCCESS);
    }
    const AfVec2 marker = {af_editor_time_to_x(editor, 0),
                           af_editor_timeline_top(editor, height) + 50};
    af_editor_gradient_smoke_button(editor, width, height, &metrics, marker, true, SDL_BUTTON_LEFT);
    GRAPH_CHECK(editor->drag_mode == AF_EDITOR_DRAG_PLAYHEAD);
    const AfVec2 scrub = {af_editor_time_to_x(editor, 1), marker.y};
    af_editor_gradient_smoke_motion(editor, width, height, &metrics, scrub);
    af_editor_gradient_smoke_button(editor, width, height, &metrics, scrub, false, SDL_BUTTON_LEFT);
    GRAPH_CHECK(fabsf(editor->playhead - 1) < .001F);
    first = af_project_find_keyframe(&editor->project, first_id);
    GRAPH_CHECK(first->keyframe.time == 0);
    const SDL_MouseWheelEvent wheel = {.x = 0, .y = 2, .mouse_x = scrub.x, .mouse_y = row_y};
    af_editor_mouse_wheel(editor, width, height, &wheel);
    GRAPH_CHECK(fabsf(af_editor_x_to_time(editor, scrub.x) - 1) < .001F);
    af_editor_curve_fit(editor, width);
    editor->curve_visible = true;
    editor->playhead = 0;
    const AfVec2 divider = {af_editor_timeline_ruler_x(editor) + 2, row_y};
    af_editor_gradient_smoke_button(editor, width, height, &metrics, divider, true,
                                    SDL_BUTTON_LEFT);
    GRAPH_CHECK(editor->drag_mode == AF_EDITOR_DRAG_TIMELINE_HEADER);
    af_editor_gradient_smoke_button(editor, width, height, &metrics, divider, false,
                                    SDL_BUTTON_LEFT);
    view = af_editor_curve_view(editor, width, height);
    AfVec2 point = af_editor_curve_point(editor, view, first);
    const uint32_t cursor = editor->history.cursor;
    /* A time-zero key must win over the playhead and the adjacent divider. */
    af_editor_gradient_smoke_button(editor, width, height, &metrics, point, true, SDL_BUTTON_LEFT);
    GRAPH_CHECK(editor->drag_mode == AF_EDITOR_DRAG_KEYFRAMES);
    af_editor_gradient_smoke_button(editor, width, height, &metrics, point, false, SDL_BUTTON_LEFT);
    GRAPH_CHECK(editor->selected_keyframe_count == 1 && editor->selected_keyframes[0] == first_id);
    GRAPH_CHECK(editor->history.cursor == cursor);
    const float controls_top =
        af_editor_timeline_top(editor, height) + AF_EDITOR_TIMELINE_RULER_HEIGHT;
    const AfVec2 ease_button = {editor->left_width + 25, controls_top + 122};
    af_editor_gradient_smoke_button(editor, width, height, &metrics, ease_button, true,
                                    SDL_BUTTON_LEFT);
    af_editor_gradient_smoke_button(editor, width, height, &metrics, ease_button, false,
                                    SDL_BUTTON_LEFT);
    first = af_project_find_keyframe(&editor->project, first_id);
    GRAPH_CHECK(first->keyframe.interpolation == AF_INTERPOLATION_CUBIC_BEZIER &&
                first->keyframe.easing[0] == .42F);
    view = af_editor_curve_view(editor, width, height);
    AfVec2 start, end;
    GRAPH_CHECK(af_editor_curve_segment(editor, view, first, &start, &end));
    const AfVec2 handle = af_editor_curve_handle(start, end, first->keyframe.easing, false);
    const AfVec2 target = {start.x + (end.x - start.x) * .25F, start.y + (end.y - start.y) * .35F};
    af_editor_gradient_smoke_button(editor, width, height, &metrics, handle, true, SDL_BUTTON_LEFT);
    GRAPH_CHECK(editor->drag_mode == AF_EDITOR_DRAG_TANGENT_OUT);
    af_editor_gradient_smoke_motion(editor, width, height, &metrics, target);
    af_editor_gradient_smoke_button(editor, width, height, &metrics, target, false,
                                    SDL_BUTTON_LEFT);
    first = af_project_find_keyframe(&editor->project, first_id);
    GRAPH_CHECK(fabsf(first->keyframe.easing[0] - .25F) < .001F &&
                fabsf(first->keyframe.easing[1] - .35F) < .001F);
    GRAPH_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    first = af_project_find_keyframe(&editor->project, first_id);
    GRAPH_CHECK(first->keyframe.easing[0] == .42F && first->keyframe.easing[1] == 0);
    GRAPH_CHECK(af_editor_history_redo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    first = af_project_find_keyframe(&editor->project, first_id);
    GRAPH_CHECK(fabsf(first->keyframe.easing[1] - .35F) < .001F);
    /* Descending Y component uses the same segment-relative control mapping. */
    const AfVec2 y_button = {editor->left_width + editor->timeline_header_width - 30,
                             controls_top + 36};
    af_editor_gradient_smoke_button(editor, width, height, &metrics, y_button, true,
                                    SDL_BUTTON_LEFT);
    af_editor_gradient_smoke_button(editor, width, height, &metrics, y_button, false,
                                    SDL_BUTTON_LEFT);
    view = af_editor_curve_view(editor, width, height);
    GRAPH_CHECK(view.component == 1 && view.minimum < -500 && view.maximum > 9000);
    GRAPH_CHECK(af_editor_curve_segment(editor, view, first, &start, &end) && end.y > start.y);
    const AfVec2 incoming = af_editor_curve_handle(start, end, first->keyframe.easing, true);
    af_editor_gradient_smoke_button(editor, width, height, &metrics, incoming, true,
                                    SDL_BUTTON_LEFT);
    GRAPH_CHECK(editor->drag_mode == AF_EDITOR_DRAG_TANGENT_IN);
    af_editor_gradient_smoke_motion(editor, width, height, &metrics,
                                    (AfVec2){incoming.x - 15, incoming.y - 12});
    SDL_Event escape = {.key = {.type = SDL_EVENT_KEY_DOWN, .down = true, .key = SDLK_ESCAPE}};
    af_editor_handle_event(editor, &escape, width, height);
    GRAPH_CHECK(editor->drag_mode == AF_EDITOR_DRAG_NONE);
    first = af_project_find_keyframe(&editor->project, first_id);
    GRAPH_CHECK(first->keyframe.easing[2] == .58F && first->keyframe.easing[3] == 1);
    af_editor_gradient_smoke_button(editor, width, height, &metrics, incoming, false,
                                    SDL_BUTTON_LEFT);
    /* Group retiming clamps one shared delta, keeping spacing at either clip edge. */
    editor->selected_keyframes[0] = first_id;
    editor->selected_keyframes[1] = last_id;
    editor->selected_keyframe_count = 2;
    point = af_editor_curve_point(editor, view, first);
    af_editor_gradient_smoke_button(editor, width, height, &metrics, point, true, SDL_BUTTON_LEFT);
    GRAPH_CHECK(editor->drag_mode == AF_EDITOR_DRAG_KEYFRAMES);
    af_editor_gradient_smoke_motion(editor, width, height, &metrics,
                                    (AfVec2){point.x + 2000, point.y});
    af_editor_gradient_smoke_button(editor, width, height, &metrics,
                                    (AfVec2){point.x + 2000, point.y}, false, SDL_BUTTON_LEFT);
    first = af_project_find_keyframe(&editor->project, first_id);
    last = af_project_find_keyframe(&editor->project, last_id);
    GRAPH_CHECK(fabsf(last->keyframe.time - first->keyframe.time - 2) < .001F);
    GRAPH_CHECK(last->keyframe.time ==
                af_project_find_clip_const(&editor->project, editor->selected_clip_id)->duration);
    GRAPH_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    first = af_project_find_keyframe(&editor->project, first_id);
    GRAPH_CHECK(first->keyframe.time == 0);
    /* A last key and a constant component have no misleading editable tangents. */
    last = af_project_find_keyframe(&editor->project, last_id);
    GRAPH_CHECK(!af_editor_curve_segment(editor, view, last, &start, &end));
    last->keyframe.value[1] = first->keyframe.value[1];
    view = af_editor_curve_view(editor, width, height);
    GRAPH_CHECK(!af_editor_curve_segment(editor, view, first, &start, &end));
    last->keyframe.value[1] = -500;
    editor->curve_component = 0;
    af_editor_select_keyframe(editor, first_id, false);
    editor->selected_keyframe_count = 1;
    GRAPH_CHECK(af_editor_modify_selected_keys(editor, 7, AF_INTERPOLATION_CUBIC_BEZIER) ==
                AF_RESULT_SUCCESS);
    view = af_editor_curve_view(editor, width, height);
    first = af_project_find_keyframe(&editor->project, first_id);
    GRAPH_CHECK(af_editor_curve_segment(editor, view, first, &start, &end));
    GRAPH_CHECK(af_editor_curve_contains(
        view, af_editor_curve_handle(start, end, first->keyframe.easing, false)));
    AfProjectClip *clip = af_project_find_clip(&editor->project, editor->selected_clip_id);
    const float duration = clip->duration;
    last = af_project_find_keyframe(&editor->project, last_id);
    clip->duration = 200;
    last->keyframe.time = 180;
    af_editor_curve_fit(editor, width);
    view = af_editor_curve_view(editor, width, height);
    GRAPH_CHECK(af_editor_curve_contains(view, af_editor_curve_point(editor, view, last)));
    clip->duration = duration;
    last->keyframe.time = 2;
    af_editor_curve_fit(editor, width);
    AfPackageBlob package = {0};
    GRAPH_CHECK(af_project_compile(&editor->project, &editor->allocator, &package) ==
                AF_RESULT_SUCCESS);
    GRAPH_CHECK(af_package_validate(package.data, package.size) == AF_RESULT_SUCCESS);
    af_package_blob_destroy(&editor->allocator, &package);
    af_editor_status(editor,
                     "Graph verified: presets, components, handles, retiming, undo and cancel");
#undef GRAPH_CHECK
    return AF_RESULT_SUCCESS;
}
