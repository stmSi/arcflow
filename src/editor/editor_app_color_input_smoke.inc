/* Exercise color inputs through the same mouse routes as transform fields. */
static AfResult af_editor_color_input_smoke(AfEditorState *editor, int width, int height) {
#define COLOR_INPUT_CHECK(condition)                                                               \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            af_log(AF_LOG_ERROR, "Color input check failed at line %d: %s", __LINE__,              \
                   editor->status);                                                                \
            return AF_RESULT_INVALID_ARGUMENT;                                                     \
        }                                                                                          \
    } while (0)
    const SDL_Keymod saved_modifiers = SDL_GetModState();
    SDL_SetModState(SDL_KMOD_NONE);
    editor->timeline_focus = false;
    editor->preview_playing = false;
    for (uint32_t mode = 0U; mode < 4U; ++mode) {
        editor->playhead = 0.0F;
        editor->color_target_outline = false;
        COLOR_INPUT_CHECK(af_editor_create_primitive_at(
                              editor, AF_NODE_RECTANGLE,
                              (AfEditorToolRect){{100.0F + (float)mode * 120.0F, 100.0F},
                                                 {90.0F, 70.0F}}) == AF_RESULT_SUCCESS);
        af_editor_update_preview(editor);
        if (mode == 1U)
            editor->color_target_outline = true;
        else if (mode == 2U)
            af_editor_gradient_set_type(editor, AF_PAINT_LINEAR_GRADIENT);
        COLOR_INPUT_CHECK(af_editor_apply_inspector_color(
            editor, (AfColor){18.0F / 255.0F, 52.0F / 255.0F, 86.0F / 255.0F, 120.0F / 255.0F},
            false));
        if (mode == 3U) {
            editor->selected_property = AF_PROPERTY_FILL_COLOR;
            COLOR_INPUT_CHECK(af_editor_add_keyframe(editor) == AF_RESULT_SUCCESS);
            editor->playhead = 1.0F;
            af_editor_update_preview(editor);
        }
        for (AfEditorInspectorField field = AF_EDITOR_INSPECTOR_COLOR_HEX;
             field <= AF_EDITOR_INSPECTOR_COLOR_A; field = (AfEditorInspectorField)(field + 1)) {
            float x, field_width, initial, actual;
            COLOR_INPUT_CHECK(
                af_editor_inspector_field_rect(editor, width, field, &x, &field_width));
            COLOR_INPUT_CHECK(af_editor_inspector_numeric_value(editor, field, &initial));
            const float y = field == AF_EDITOR_INSPECTOR_COLOR_HEX
                                ? AF_EDITOR_COLOR_HEX_Y + 12.0F
                                : AF_EDITOR_COLOR_COMPONENT_Y + 12.0F +
                                      (float)((field - AF_EDITOR_INSPECTOR_COLOR_R) / 2) *
                                          AF_EDITOR_INSPECTOR_FIELD_STEP;
            SDL_MouseButtonEvent click = {
                .button = SDL_BUTTON_LEFT, .down = true, .x = x + field_width - 10.0F, .y = y};
            const uint32_t before_step = editor->history.cursor;
            af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
            af_editor_mouse_up(editor, width, height);
            COLOR_INPUT_CHECK(editor->inspector_field == AF_EDITOR_INSPECTOR_NONE &&
                              editor->history.cursor == (before_step < editor->history.capacity
                                                             ? before_step + 1U
                                                             : editor->history.capacity));
            COLOR_INPUT_CHECK(af_editor_inspector_numeric_value(editor, field, &actual) &&
                              actual == initial + 1.0F);
            click.x = x + field_width - 30.0F;
            af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
            af_editor_mouse_up(editor, width, height);
            COLOR_INPUT_CHECK(af_editor_inspector_numeric_value(editor, field, &actual) &&
                              actual == initial);

            /* Motion events merge into one undo item, including animated fill keys. */
            const uint32_t before_drag = editor->history.cursor;
            click.x = x + 20.0F;
            af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
            COLOR_INPUT_CHECK(editor->drag_mode == AF_EDITOR_DRAG_INSPECTOR_SCRUB);
            for (uint32_t move = 1U; move <= 2U; ++move) {
                af_editor_mouse_motion(editor, width, height,
                                       &(SDL_MouseMotionEvent){.state = SDL_BUTTON_LMASK,
                                                               .x = click.x + (float)move * 12.0F,
                                                               .y = y,
                                                               .xrel = 12.0F});
            }
            af_editor_mouse_up(editor, width, height);
            COLOR_INPUT_CHECK(editor->history.cursor == (before_drag < editor->history.capacity
                                                             ? before_drag + 1U
                                                             : editor->history.capacity));
            COLOR_INPUT_CHECK(af_editor_inspector_numeric_value(editor, field, &actual) &&
                              actual == initial + 24.0F);
            COLOR_INPUT_CHECK(af_editor_history_undo(&editor->history, &editor->project) ==
                              AF_RESULT_SUCCESS);
            af_editor_update_preview(editor);
            COLOR_INPUT_CHECK(af_editor_inspector_numeric_value(editor, field, &actual) &&
                              actual == initial);
            COLOR_INPUT_CHECK(af_editor_history_redo(&editor->history, &editor->project) ==
                              AF_RESULT_SUCCESS);
            af_editor_update_preview(editor);
            COLOR_INPUT_CHECK(af_editor_inspector_numeric_value(editor, field, &actual) &&
                              actual == initial + 24.0F);
            COLOR_INPUT_CHECK(af_editor_history_undo(&editor->history, &editor->project) ==
                              AF_RESULT_SUCCESS);
            af_editor_update_preview(editor);

            /* A stationary click still starts direct text editing. */
            af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
            af_editor_mouse_up(editor, width, height);
            COLOR_INPUT_CHECK(editor->inspector_field == field);
            COLOR_INPUT_CHECK(af_editor_finish_inspector_edit(editor, false));
        }
        /* Hex carries between channels, clamps at white, and preserves the alpha byte. */
        af_editor_begin_inspector_edit(editor, AF_EDITOR_INSPECTOR_COLOR_HEX);
        af_editor_append_inspector_text(editor, "#12FFFF40");
        COLOR_INPUT_CHECK(af_editor_finish_inspector_edit(editor, true));
        COLOR_INPUT_CHECK(af_editor_step_inspector_value(editor, AF_EDITOR_INSPECTOR_COLOR_HEX, 1));
        char hex[16];
        af_editor_inspector_format_hex_color(
            af_editor_inspector_color(
                editor, af_scene_find_const(&editor->preview_scene, editor->selected_id)),
            hex, sizeof(hex));
        COLOR_INPUT_CHECK(strcmp(hex, "#13000040") == 0);
        COLOR_INPUT_CHECK(af_editor_apply_inspector_number(editor, AF_EDITOR_INSPECTOR_COLOR_HEX,
                                                           16777215.0F, 0, false));
        COLOR_INPUT_CHECK(af_editor_step_inspector_value(editor, AF_EDITOR_INSPECTOR_COLOR_HEX, 1));
        af_editor_inspector_format_hex_color(
            af_editor_inspector_color(
                editor, af_scene_find_const(&editor->preview_scene, editor->selected_id)),
            hex, sizeof(hex));
        COLOR_INPUT_CHECK(strcmp(hex, "#FFFFFF40") == 0);
        COLOR_INPUT_CHECK(af_editor_apply_inspector_number(editor, AF_EDITOR_INSPECTOR_COLOR_R,
                                                           -10.0F, 0, false));
        COLOR_INPUT_CHECK(af_editor_apply_inspector_number(editor, AF_EDITOR_INSPECTOR_COLOR_A,
                                                           300.0F, 0, false));
        af_editor_inspector_format_hex_color(
            af_editor_inspector_color(
                editor, af_scene_find_const(&editor->preview_scene, editor->selected_id)),
            hex, sizeof(hex));
        COLOR_INPUT_CHECK(strcmp(hex, "#00FFFFFF") == 0);
    }
    SDL_SetModState(saved_modifiers);
    COLOR_INPUT_CHECK(af_project_validate(&editor->project) == AF_RESULT_SUCCESS);
#undef COLOR_INPUT_CHECK
    return AF_RESULT_SUCCESS;
}
