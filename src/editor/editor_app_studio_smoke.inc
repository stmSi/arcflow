/* Exercise the real event routes: visual changes must not hide active controls or
 * let palette dismissal draw on the document underneath it. */
static AfResult af_editor_studio_smoke(AfEditorState *editor, int width, int height) {
#define STUDIO_CHECK(condition)                                                                    \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            af_log(AF_LOG_ERROR, "Studio interaction check failed at line %d", __LINE__);          \
            return AF_RESULT_INVALID_ARGUMENT;                                                     \
        }                                                                                          \
    } while (0)
    const int sizes[][2] = {{320, 360}, {640, 480}, {1000, 640}, {1440, 840}};
    for (uint32_t index = 0; index < 4; ++index) {
        const AfEditorPaletteLayout box =
            af_editor_palette_layout(sizes[index][0], sizes[index][1]);
        STUDIO_CHECK(box.x >= 16 && box.y >= 16 && box.rows >= 1 &&
                     box.x + box.width <= (float)sizes[index][0] - 16 &&
                     box.y + box.height <= (float)sizes[index][1] - 16);
    }
    const uint32_t node_count = editor->project.scene.count;
    const float panel = (float)width - editor->right_width + 18;
    af_editor_select_only(editor, 0);
    SDL_MouseButtonEvent click = {
        .button = SDL_BUTTON_LEFT, .down = true, .clicks = 1, .x = panel + 30, .y = 190};
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    STUDIO_CHECK(editor->inspector_field == AF_EDITOR_INSPECTOR_NONE &&
                 editor->drag_mode == AF_EDITOR_DRAG_NONE);
    click.y = 434;
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    STUDIO_CHECK(editor->active_tool == AF_EDITOR_TOOL_RECTANGLE);
    click.y = 472;
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    STUDIO_CHECK(editor->active_tool == AF_EDITOR_TOOL_PEN);
    click.y = 510;
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    STUDIO_CHECK(editor->active_tool == AF_EDITOR_TOOL_TEXT);
    STUDIO_CHECK(editor->project.scene.count == node_count);
    af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_RECTANGLE);

    /* The visible launcher opens search; an outside click is consumed. */
    editor->hierarchy_search_active = true;
    click.y = (float)height - editor->status_height + 12;
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    STUDIO_CHECK(editor->command_palette.open && !editor->hierarchy_search_active &&
                 SDL_TextInputActive(editor->window.window));
    click.x = editor->left_width + 30;
    click.y = editor->toolbar_height + 20;
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    STUDIO_CHECK(!editor->command_palette.open && !SDL_TextInputActive(editor->window.window) &&
                 editor->project.scene.count == node_count &&
                 editor->drag_mode == AF_EDITOR_DRAG_NONE);

    (void)af_editor_action_execute(&editor->actions, AF_EDITOR_MENU_ACTION_COMMAND_PALETTE);
    (void)af_editor_command_palette_set_query(&editor->command_palette, "zzzz-no-such-command");
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_RETURN, .down = true});
    STUDIO_CHECK(editor->command_palette.open && editor->command_palette.result_count == 0);
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_ESCAPE, .down = true});
    STUDIO_CHECK(!editor->command_palette.open && !SDL_TextInputActive(editor->window.window));

    /* Keyboard and mouse activate the same result, including on compact layouts. */
    const bool grid = editor->grid_visible;
    (void)af_editor_action_execute(&editor->actions, AF_EDITOR_MENU_ACTION_COMMAND_PALETTE);
    (void)af_editor_command_palette_set_query(&editor->command_palette, "toggle grid");
    STUDIO_CHECK(editor->command_palette.result_count == 1);
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_RETURN, .down = true});
    STUDIO_CHECK(!editor->command_palette.open && editor->grid_visible != grid &&
                 !SDL_TextInputActive(editor->window.window));
    (void)af_editor_action_execute(&editor->actions, AF_EDITOR_MENU_ACTION_COMMAND_PALETTE);
    (void)af_editor_command_palette_set_query(&editor->command_palette, "toggle grid");
    const AfEditorPaletteLayout compact = af_editor_palette_layout(640, 480);
    STUDIO_CHECK(af_editor_workflow_mouse_down(editor, 640, 480, compact.x + 30, compact.y + 114));
    STUDIO_CHECK(!editor->command_palette.open && editor->grid_visible == grid);

    (void)af_editor_action_execute(&editor->actions, AF_EDITOR_MENU_ACTION_COMMAND_PALETTE);
    (void)af_editor_command_palette_set_query(&editor->command_palette, "");
    const float zoom = editor->zoom;
    af_editor_mouse_wheel(editor, width, height,
                          &(SDL_MouseWheelEvent){.y = -1, .mouse_x = panel, .mouse_y = 200});
    STUDIO_CHECK(editor->command_palette.selected_index == 1 && editor->zoom == zoom);
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_ESCAPE, .down = true});
    editor->zoom = .1F;
    click.x = panel + 20;
    click.y = 329;
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    STUDIO_CHECK(editor->zoom > .1F && editor->project.scene.count == node_count);
    af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_SELECT);
    if (node_count > 0) {
        const uint64_t id = editor->project.scene.nodes[node_count - 1].id;
        char original_name[AF_PROJECT_NAME_CAPACITY];
        (void)snprintf(original_name, sizeof(original_name), "%s",
                       af_project_name(&editor->project, id));
        af_editor_select_only(editor, id);
        af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_F2, .down = true});
        STUDIO_CHECK(editor->rename_active);
        (void)snprintf(editor->rename_buffer, sizeof(editor->rename_buffer),
                       "Studio rename preview");
        af_editor_update_title(editor);
        STUDIO_CHECK(strstr(SDL_GetWindowTitle(editor->window.window), "Studio rename preview") !=
                     NULL);
        af_editor_key(editor, width, height,
                      &(SDL_KeyboardEvent){.key = SDLK_ESCAPE, .down = true});
        STUDIO_CHECK(!editor->rename_active &&
                     strcmp(af_project_name(&editor->project, id), original_name) == 0);
        af_editor_select_only(editor, 0);
    }
    af_editor_status(
        editor, "Studio checks passed: quick actions, command search, resizing and input focus");
#undef STUDIO_CHECK
    return AF_RESULT_SUCCESS;
}
