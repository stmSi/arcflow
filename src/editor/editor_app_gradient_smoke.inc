/* Exercise the real SDL dispatch path, including display coordinate scaling. */
static void af_editor_gradient_smoke_button(AfEditorState *editor, int width, int height,
                                            const AfPlatformWindowMetrics *metrics, AfVec2 point,
                                            bool down, uint8_t button) {
    SDL_Event event = {
        .button = {.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP,
                   .button = button,
                   .down = down,
                   .clicks = 1,
                   .x = point.x / metrics->pixel_scale_x,
                   .y = point.y / metrics->pixel_scale_y}};
    af_editor_handle_event(editor, &event, width, height);
}
static void af_editor_gradient_smoke_motion(AfEditorState *editor, int width, int height,
                                            const AfPlatformWindowMetrics *metrics, AfVec2 point) {
    SDL_Event event = {.motion = {.type = SDL_EVENT_MOUSE_MOTION,
                                  .state = SDL_BUTTON_LMASK,
                                  .x = point.x / metrics->pixel_scale_x,
                                  .y = point.y / metrics->pixel_scale_y}};
    af_editor_handle_event(editor, &event, width, height);
}
static AfResult af_editor_gradient_drag_smoke(AfEditorState *editor, int width, int height) {
#define GRADIENT_CHECK(c)                                                                          \
    do {                                                                                           \
        if (!(c))                                                                                  \
            return af_editor_interaction_smoke_failure(editor, __LINE__);                          \
    } while (0)
    AfPlatformWindowMetrics metrics;
    GRADIENT_CHECK(af_platform_window_metrics(&editor->window, &metrics) == AF_RESULT_SUCCESS);
    editor->active_tool = AF_EDITOR_TOOL_SELECT;
    editor->zoom = 1;
    editor->pan = (AfVec2){0, 0};
    editor->preview_playing = false;
    const AfVec2 position = af_editor_screen_to_project(
        editor, width, height, (AfVec2){editor->left_width + 100, editor->toolbar_height + 200});
    GRADIENT_CHECK(af_editor_create_primitive_at(editor, AF_NODE_RECTANGLE,
                                                 (AfEditorToolRect){position, {300, 150}}) ==
                   AF_RESULT_SUCCESS);
    AfSceneNode *node = af_scene_find(&editor->project.scene, editor->selected_id);
    AfStyle *style = af_project_find_node_style(&editor->project, editor->selected_id);
    GRADIENT_CHECK(node && style);
    node->local_transform.rotation = .12F;
    node->local_transform.scale = (AfVec2){.9F, 1.1F};
    const AfTransform2D node_transform = node->local_transform;
    const AfPaint linear = {
        .type = AF_PAINT_LINEAR_GRADIENT,
        .start = {20, 20},
        .end = {260, 65},
        .end_radius = 90,
        .angle = .25F,
        .transform = {.m00 = .9F, .m01 = .15F, .m10 = .1F, .m11 = 1, .tx = 5, .ty = 4},
        .stop_count = 4,
        .stops = {
            {0, {1, 0, 0, 1}}, {.35F, {0, 1, 0, .75F}}, {.65F, {0, 0, 1, .6F}}, {1, {1, 0, 1, 1}}}};
    for (uint32_t type = AF_PAINT_LINEAR_GRADIENT; type <= AF_PAINT_CONIC_GRADIENT; ++type) {
        style->fill = linear;
        style->fill.type = (AfPaintType)type;
        if (type != AF_PAINT_LINEAR_GRADIENT)
            style->fill.start = (AfVec2){90, 60};
        /* Start/end handles must win over overlapping first/last stops, from both edges. */
        for (uint32_t handle = 1; handle <= 2; ++handle) {
            af_editor_update_preview(editor);
            AfAffine2 frame;
            AfVec2 start, end;
            GRADIENT_CHECK(
                af_editor_gradient_geometry(editor, width, height, &frame, &start, &end));
            const AfPaint before = style->fill;
            const uint32_t cursor = editor->history.cursor;
            const AfVec2 old_point = handle == 1 ? start : end;
            const AfVec2 new_point = {old_point.x + 24, old_point.y + 13};
            const AfVec2 a = af_affine_transform_point(frame, old_point);
            const AfVec2 b = af_affine_transform_point(frame, new_point);
            const AfVec2 press = {a.x + 2, a.y - 1};
            const AfVec2 release = {b.x + 2, b.y - 1};
            af_editor_gradient_smoke_button(editor, width, height, &metrics, press, true,
                                            SDL_BUTTON_LEFT);
            GRADIENT_CHECK(editor->gradient_drag == (AfEditorGradientDrag)handle);
            af_editor_gradient_smoke_motion(
                editor, width, height, &metrics,
                (AfVec2){(press.x + release.x) * .5F, (press.y + release.y) * .5F});
            /* Mouse-up supplies the final position even without a final motion event. */
            af_editor_gradient_smoke_button(editor, width, height, &metrics, release, false,
                                            SDL_BUTTON_LEFT);
            GRADIENT_CHECK(!editor->gradient_drag && editor->history.cursor == cursor + 1);
            if (handle == 1) {
                GRADIENT_CHECK(hypotf(style->fill.start.x - new_point.x,
                                      style->fill.start.y - new_point.y) < .001F);
                GRADIENT_CHECK(memcmp(&style->fill.end, &before.end, sizeof(AfVec2)) == 0 &&
                               style->fill.end_radius == before.end_radius);
            } else if (type == AF_PAINT_LINEAR_GRADIENT) {
                GRADIENT_CHECK(hypotf(style->fill.end.x - new_point.x,
                                      style->fill.end.y - new_point.y) < .001F);
                GRADIENT_CHECK(memcmp(&style->fill.start, &before.start, sizeof(AfVec2)) == 0);
            } else {
                GRADIENT_CHECK(fabsf(style->fill.end_radius -
                                     hypotf(new_point.x - start.x, new_point.y - start.y)) < .001F);
                GRADIENT_CHECK(fabsf(style->fill.angle -
                                     atan2f(new_point.y - start.y, new_point.x - start.x)) < .001F);
            }
            const AfPaint after = style->fill;
            GRADIENT_CHECK(
                memcmp(&node_transform, &node->local_transform, sizeof(node_transform)) == 0);
            GRADIENT_CHECK(af_editor_history_undo(&editor->history, &editor->project) ==
                           AF_RESULT_SUCCESS);
            GRADIENT_CHECK(memcmp(&before, &style->fill, sizeof(before)) == 0);
            GRADIENT_CHECK(af_editor_history_redo(&editor->history, &editor->project) ==
                           AF_RESULT_SUCCESS);
            GRADIENT_CHECK(memcmp(&after, &style->fill, sizeof(after)) == 0);
            GRADIENT_CHECK(af_editor_history_undo(&editor->history, &editor->project) ==
                           AF_RESULT_SUCCESS);
        }
        /* Intermediate canvas stops move along the transformed line and can cross neighbors. */
        af_editor_update_preview(editor);
        AfAffine2 frame;
        AfVec2 start, end;
        GRADIENT_CHECK(af_editor_gradient_geometry(editor, width, height, &frame, &start, &end));
        start = af_affine_transform_point(frame, start);
        end = af_affine_transform_point(frame, end);
        const AfPaint before = style->fill;
        const AfVec2 delta = {end.x - start.x, end.y - start.y};
        const AfVec2 press = {start.x + delta.x * .35F, start.y + delta.y * .35F};
        const AfVec2 release = {press.x + delta.x * .45F, press.y + delta.y * .45F};
        af_editor_gradient_smoke_button(editor, width, height, &metrics, press, true,
                                        SDL_BUTTON_LEFT);
        GRADIENT_CHECK(editor->gradient_drag == AF_EDITOR_GRADIENT_DRAG_CANVAS_STOP);
        af_editor_gradient_smoke_motion(editor, width, height, &metrics, release);
        af_editor_gradient_smoke_button(editor, width, height, &metrics, release, false,
                                        SDL_BUTTON_LEFT);
        GRADIENT_CHECK(editor->gradient_stop == 2 &&
                       fabsf(style->fill.stops[2].offset - .8F) < .001F);
        GRADIENT_CHECK(
            memcmp(&style->fill.stops[2].color, &before.stops[1].color, sizeof(AfColor)) == 0);
        GRADIENT_CHECK(memcmp(&style->fill.start, &before.start, sizeof(AfVec2)) == 0 &&
                       memcmp(&style->fill.end, &before.end, sizeof(AfVec2)) == 0 &&
                       style->fill.end_radius == before.end_radius);
        GRADIENT_CHECK(af_editor_history_undo(&editor->history, &editor->project) ==
                       AF_RESULT_SUCCESS);
        GRADIENT_CHECK(memcmp(&before, &style->fill, sizeof(before)) == 0);
    }
    style->fill = linear;
    const float ramp_left = editor->left_width + 16;
    const float ramp_width = (float)width - editor->left_width - editor->right_width - 32;
    const float ramp_y = editor->toolbar_height + 101;
    /* First and last toolbar stops are movable too; vertical motion does not move geometry. */
    for (uint32_t endpoint = 0; endpoint < 2; ++endpoint) {
        const uint32_t stop = endpoint ? 3 : 0;
        const float offset = endpoint ? .1F : .8F;
        const AfPaint before = style->fill;
        const AfVec2 press = {ramp_left + ramp_width * before.stops[stop].offset + 2, ramp_y};
        const AfVec2 release = {press.x + ramp_width * (offset - before.stops[stop].offset),
                                ramp_y + 150};
        editor->active_tool = AF_EDITOR_TOOL_TEXT;
        af_editor_gradient_smoke_button(editor, width, height, &metrics, press, true,
                                        SDL_BUTTON_LEFT);
        GRADIENT_CHECK(editor->gradient_drag == AF_EDITOR_GRADIENT_DRAG_TOOLBAR_STOP);
        af_editor_gradient_smoke_motion(editor, width, height, &metrics, release);
        af_editor_gradient_smoke_button(editor, width, height, &metrics, release, false,
                                        SDL_BUTTON_LEFT);
        const uint32_t moved = editor->gradient_stop;
        GRADIENT_CHECK(fabsf(style->fill.stops[moved].offset - offset) < .001F &&
                       memcmp(&style->fill.stops[moved].color, &before.stops[stop].color,
                              sizeof(AfColor)) == 0);
        GRADIENT_CHECK(af_editor_history_undo(&editor->history, &editor->project) ==
                       AF_RESULT_SUCCESS);
        GRADIENT_CHECK(memcmp(&before, &style->fill, sizeof(before)) == 0);
    }
    editor->active_tool = AF_EDITOR_TOOL_SELECT;
    style->fill.stop_count = AF_MAX_GRADIENT_STOPS;
    for (uint32_t i = 0; i < AF_MAX_GRADIENT_STOPS; ++i)
        style->fill.stops[i] = (AfGradientStop){(float)i / (AF_MAX_GRADIENT_STOPS - 1),
                                                {(float)i / 15, 1 - (float)i / 15, .4F, 1}};
    style->fill.stops[7].offset = style->fill.stops[6].offset;
    af_editor_gradient_select_stop(editor, &style->fill, 7);
    const AfStyle before = *style;
    const uint32_t cursor = editor->history.cursor;
    const AfVec2 press = {ramp_left + ramp_width * style->fill.stops[7].offset, ramp_y};
    editor->dirty = false;
    af_editor_gradient_smoke_button(editor, width, height, &metrics, press, true, SDL_BUTTON_LEFT);
    af_editor_gradient_smoke_button(editor, width, height, &metrics, press, false, SDL_BUTTON_LEFT);
    GRADIENT_CHECK(!editor->dirty && editor->history.cursor == cursor &&
                   editor->gradient_stop == 7);
    /* Crossing and clamping at both ends keeps coincident stops valid and selected. */
    for (uint32_t endpoint = 0; endpoint < 2; ++endpoint) {
        af_editor_gradient_smoke_button(editor, width, height, &metrics, press, true,
                                        SDL_BUTTON_LEFT);
        GRADIENT_CHECK(editor->gradient_drag == AF_EDITOR_GRADIENT_DRAG_TOOLBAR_STOP);
        const AfVec2 release = {endpoint ? ramp_left + ramp_width + 30 : ramp_left - 30, ramp_y};
        af_editor_gradient_smoke_motion(editor, width, height, &metrics, release);
        GRADIENT_CHECK(style->fill.stops[editor->gradient_stop].offset == (endpoint ? 1 : 0));
        GRADIENT_CHECK(memcmp(&style->fill.stops[editor->gradient_stop].color,
                              &before.fill.stops[7].color, sizeof(AfColor)) == 0);
        GRADIENT_CHECK(af_style_data_validate(&editor->project.style_data) == AF_RESULT_SUCCESS);
        af_editor_key(editor, width, height,
                      &(SDL_KeyboardEvent){.key = SDLK_ESCAPE, .down = true});
        GRADIENT_CHECK(!editor->gradient_drag && !editor->dirty && editor->gradient_stop == 7 &&
                       editor->history.cursor == cursor &&
                       memcmp(style, &before, sizeof(before)) == 0);
    }
    /* Ignore wheel/right-button release during a drag, then commit on focus loss. */
    af_editor_gradient_smoke_button(editor, width, height, &metrics, press, true, SDL_BUTTON_LEFT);
    af_editor_gradient_smoke_motion(editor, width, height, &metrics,
                                    (AfVec2){press.x + ramp_width * .15F, ramp_y});
    af_editor_gradient_smoke_button(editor, width, height, &metrics, press, false,
                                    SDL_BUTTON_RIGHT);
    GRADIENT_CHECK(editor->gradient_drag == AF_EDITOR_GRADIENT_DRAG_TOOLBAR_STOP);
    af_editor_mouse_wheel(
        editor, width, height,
        &(SDL_MouseWheelEvent){.y = 1, .mouse_x = press.x, .mouse_y = ramp_y + 150});
    GRADIENT_CHECK(editor->zoom == 1);
    SDL_Event lost_focus = {.window = {.type = SDL_EVENT_WINDOW_FOCUS_LOST}};
    af_editor_handle_event(editor, &lost_focus, width, height);
    GRADIENT_CHECK(!editor->gradient_drag && editor->history.cursor == cursor + 1);
    GRADIENT_CHECK(af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS);
    GRADIENT_CHECK(memcmp(style, &before, sizeof(before)) == 0);
    style->fill = linear;
    af_editor_update_preview(editor);
    GRADIENT_CHECK(af_project_validate(&editor->project) == AF_RESULT_SUCCESS);
    af_editor_status(editor,
                     "Gradient drags verified: start / end / radius, canvas / toolbar stops, undo");
#undef GRADIENT_CHECK
    return AF_RESULT_SUCCESS;
}
