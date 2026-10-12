static AfResult af_editor_gradient_tool_smoke(AfEditorState *editor, int width, int height) {
#define DRAW_CHECK(condition)                                                                      \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            af_log(AF_LOG_ERROR, "Gradient tool smoke failed at line %u", (unsigned)__LINE__);     \
            return AF_RESULT_INVALID_ARGUMENT;                                                     \
        }                                                                                          \
    } while (0)
    for (uint32_t target = 0; target < 2; ++target) {
        for (AfPaintType type = AF_PAINT_LINEAR_GRADIENT; type <= AF_PAINT_CONIC_GRADIENT; ++type) {
            editor->dirty = false;
            DRAW_CHECK(af_editor_new_project(editor) == AF_RESULT_SUCCESS);
            editor->welcome_visible = false;
            editor->color_target_outline = false;
            DRAW_CHECK(af_editor_create_primitive_at(editor, AF_NODE_RECTANGLE,
                                                     (AfEditorToolRect){{240, 180}, {240, 160}}) ==
                       AF_RESULT_SUCCESS);
            const uint64_t id = editor->selected_id;
            AfStyle *style = af_project_find_node_style(&editor->project, id);
            style->stroke.width = 8;
            /* Exercise object and paint coordinates, rather than only screen-aligned shapes. */
            AfSceneNode *node = af_scene_find(&editor->project.scene, id);
            node->local_transform.rotation = .2F;
            node->local_transform.scale = (AfVec2){1.1F, .9F};
            AfPaint *paint = target ? &style->stroke_paint : &style->fill;
            paint->transform = (AfAffine2){.m00 = .8F, .m11 = .9F, .tx = 10, .ty = 5};
            af_editor_update_preview(editor);
            af_editor_key(editor, width, height,
                          &(SDL_KeyboardEvent){.key = SDLK_G, .mod = SDL_KMOD_SHIFT, .down = true});
            DRAW_CHECK(editor->active_tool == AF_EDITOR_TOOL_GRADIENT);
            af_editor_choice_open(editor, 1, width, height, editor->left_width + 6,
                                  editor->toolbar_height + 36);
            af_editor_choice_apply(editor, (uint32_t)type);
            DRAW_CHECK(editor->gradient_tool_type == type && paint->type == AF_PAINT_SOLID);
            if (target) {
                const float cell =
                    ((float)width - editor->left_width - editor->right_width - 12) / 6;
                af_editor_mouse_down(editor, width, height,
                                     &(SDL_MouseButtonEvent){.button = SDL_BUTTON_LEFT,
                                                             .x = editor->left_width + 12 + cell,
                                                             .y = editor->toolbar_height + 16},
                                     SDL_KMOD_NONE);
                DRAW_CHECK(editor->color_target_outline);
            }
            const AfStyle before = *style;
            node = af_scene_find(&editor->preview_scene, id);
            const AfAffine2 frame =
                af_affine_multiply(af_editor_camera(editor, width, height),
                                   af_affine_multiply(node->world_transform, paint->transform));
            const AfVec2 a = af_affine_transform_point(frame, (AfVec2){40, 40});
            const AfVec2 b = af_affine_transform_point(frame, (AfVec2){200, 110});
            const uint32_t cursor = editor->history.cursor;
            af_editor_select_only(editor, 0);
            af_editor_mouse_down(
                editor, width, height,
                &(SDL_MouseButtonEvent){.button = SDL_BUTTON_LEFT, .x = a.x, .y = a.y, .clicks = 1},
                SDL_KMOD_NONE);
            DRAW_CHECK(editor->selected_id == id &&
                       editor->gradient_drag == AF_EDITOR_GRADIENT_DRAG_DRAW);
            af_editor_mouse_motion(editor, width, height,
                                   &(SDL_MouseMotionEvent){.x = b.x, .y = b.y});
            af_editor_mouse_up(editor, width, height);
            DRAW_CHECK(paint->type == type && paint->stop_count == 2);
            DRAW_CHECK(!target || style->stroke.enabled);
            DRAW_CHECK(fabsf(paint->start.x - 40) < .001F && fabsf(paint->start.y - 40) < .001F);
            DRAW_CHECK(type == AF_PAINT_LINEAR_GRADIENT
                           ? fabsf(paint->end.x - 200) < .001F && fabsf(paint->end.y - 110) < .001F
                           : fabsf(paint->end_radius - hypotf(160, 70)) < .001F &&
                                 fabsf(paint->angle - atan2f(70, 160)) < .001F);
            DRAW_CHECK(memcmp(target ? &style->fill : &style->stroke_paint,
                              target ? &before.fill : &before.stroke_paint, sizeof(AfPaint)) == 0);
            DRAW_CHECK(editor->history.cursor == cursor + 1);
            const AfPaint applied = *paint;
            DRAW_CHECK(af_editor_history_undo(&editor->history, &editor->project) ==
                       AF_RESULT_SUCCESS);
            DRAW_CHECK(
                memcmp(paint, target ? &before.stroke_paint : &before.fill, sizeof(*paint)) == 0);
            DRAW_CHECK(af_editor_history_redo(&editor->history, &editor->project) ==
                       AF_RESULT_SUCCESS);
            DRAW_CHECK(memcmp(paint, &applied, sizeof(*paint)) == 0);
            af_editor_update_preview(editor);
            const AfVec2 c = af_affine_transform_point(frame, (AfVec2){80, 20});
            af_editor_mouse_down(
                editor, width, height,
                &(SDL_MouseButtonEvent){.button = SDL_BUTTON_LEFT, .x = c.x, .y = c.y, .clicks = 1},
                SDL_KMOD_NONE);
            DRAW_CHECK(editor->gradient_drag == AF_EDITOR_GRADIENT_DRAG_DRAW);
            af_editor_mouse_motion(editor, width, height,
                                   &(SDL_MouseMotionEvent){.x = b.x, .y = b.y});
            af_editor_key(editor, width, height,
                          &(SDL_KeyboardEvent){.key = SDLK_ESCAPE, .down = true});
            DRAW_CHECK(memcmp(paint, &applied, sizeof(*paint)) == 0 &&
                       editor->history.cursor == cursor + 1);
            /* A click is not a zero-length gradient or an undo entry. */
            af_editor_mouse_down(
                editor, width, height,
                &(SDL_MouseButtonEvent){.button = SDL_BUTTON_LEFT, .x = c.x, .y = c.y, .clicks = 1},
                SDL_KMOD_NONE);
            af_editor_mouse_up(editor, width, height);
            DRAW_CHECK(memcmp(paint, &applied, sizeof(*paint)) == 0 &&
                       editor->history.cursor == cursor + 1);
            /* Key only the center: static radius/direction and animation must undo together. */
            if (type == AF_PAINT_CONIC_GRADIENT) {
                editor->selected_property = AF_PROPERTY_GRADIENT_START;
                DRAW_CHECK(af_editor_add_keyframe(editor) == AF_RESULT_SUCCESS);
                editor->playhead = .5F;
                af_editor_update_preview(editor);
                const uint32_t keys = editor->project.keyframe_count;
                const uint32_t keyed_cursor = editor->history.cursor;
                af_editor_mouse_down(
                    editor, width, height,
                    &(SDL_MouseButtonEvent){
                        .button = SDL_BUTTON_LEFT, .x = c.x, .y = c.y, .clicks = 1},
                    SDL_KMOD_NONE);
                DRAW_CHECK(editor->gradient_drag == AF_EDITOR_GRADIENT_DRAG_DRAW);
                af_editor_mouse_motion(editor, width, height,
                                       &(SDL_MouseMotionEvent){.x = b.x, .y = b.y});
                af_editor_mouse_up(editor, width, height);
                DRAW_CHECK(editor->history.cursor == keyed_cursor + 1 &&
                           editor->project.keyframe_count == keys + 1);
                DRAW_CHECK(paint->start.x == applied.start.x &&
                           paint->end_radius != applied.end_radius);
                DRAW_CHECK(af_editor_history_undo(&editor->history, &editor->project) ==
                           AF_RESULT_SUCCESS);
                DRAW_CHECK(memcmp(paint, &applied, sizeof(*paint)) == 0 &&
                           editor->project.keyframe_count == keys);
                DRAW_CHECK(af_editor_history_redo(&editor->history, &editor->project) ==
                           AF_RESULT_SUCCESS);
                DRAW_CHECK(editor->project.keyframe_count == keys + 1);
            }
        }
    }
    af_editor_update_preview(editor);
    af_editor_status(
        editor, "Gradient tool verified: Fill/Outline, all types, transforms, animation and undo");
#undef DRAW_CHECK
    return AF_RESULT_SUCCESS;
}
