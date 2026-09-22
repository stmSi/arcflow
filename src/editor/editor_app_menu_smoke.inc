/* Exercise real input routing; menu screenshots alone cannot catch canvas fall-through. */
static AfResult af_editor_menu_interaction_smoke(AfEditorState *editor, int width, int height) {
#define MENU_CHECK(condition)                                                                      \
    do {                                                                                           \
        if (!(condition))                                                                          \
            return af_editor_interaction_smoke_failure(editor, __LINE__);                          \
    } while (0)
    const AfVec2 center = af_editor_visible_center(editor, width, height);
    MENU_CHECK(af_editor_create_primitive_at(editor, AF_NODE_RECTANGLE,
                                             (AfEditorToolRect){{center.x - 40, center.y - 30},
                                                                {80, 60}}) == AF_RESULT_SUCCESS);
    const uint64_t selected = editor->selected_id;
    const uint32_t count = editor->project.scene.count;
    const AfTransform2D transform =
        af_scene_find_const(&editor->project.scene, selected)->local_transform;
    SDL_MouseMotionEvent motion = {.y = 12};
    for (AfEditorTopMenu menu = AF_EDITOR_MENU_FILE; menu <= AF_EDITOR_MENU_HELP;
         menu = (AfEditorTopMenu)(menu + 1)) {
        motion.x = af_editor_top_menu_x(editor, menu) + 2;
        af_editor_mouse_motion(editor, width, height, &motion);
        MENU_CHECK(editor->top_menu == menu && editor->project.scene.count == count);
        MENU_CHECK(af_editor_top_popup_x(editor, width) + 340 <= (float)width);
    }
    motion.x = af_editor_top_menu_x(editor, AF_EDITOR_MENU_FILE) + 2;
    af_editor_mouse_motion(editor, width, height, &motion);
    SDL_MouseButtonEvent click = {
        .button = SDL_BUTTON_LEFT, .down = true, .clicks = 1, .x = motion.x, .y = motion.y};
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    MENU_CHECK(editor->top_menu == AF_EDITOR_MENU_FILE); /* Click after hover must not close it. */
    motion.y = AF_EDITOR_MENU_HEIGHT + 4 + 30 + 10;
    af_editor_mouse_motion(editor, width, height, &motion);
    MENU_CHECK(editor->top_menu == AF_EDITOR_MENU_FILE && editor->top_menu_item == 1);
    MENU_CHECK(af_editor_splitter_at(editor, width, height) == AF_EDITOR_DRAG_NONE);
    const float zoom = editor->zoom;
    const AfVec2 pan = editor->pan;
    af_editor_mouse_wheel(editor, width, height,
                          &(SDL_MouseWheelEvent){.y = 1, .mouse_x = motion.x, .mouse_y = motion.y});
    MENU_CHECK(editor->zoom == zoom && editor->pan.x == pan.x && editor->pan.y == pan.y);
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_DOWN, .down = true});
    MENU_CHECK(editor->top_menu_item == 2);
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_DELETE, .down = true});
    MENU_CHECK(editor->project.scene.count == count);
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_RIGHT, .down = true});
    MENU_CHECK(editor->top_menu == AF_EDITOR_MENU_EDIT && editor->top_menu_item == 0);
    MENU_CHECK(memcmp(&transform,
                      &af_scene_find_const(&editor->project.scene, selected)->local_transform,
                      sizeof(transform)) == 0);
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_RIGHT, .down = true});
    const bool grid = editor->grid_visible;
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_RETURN, .down = true});
    MENU_CHECK(editor->top_menu == AF_EDITOR_MENU_NONE && editor->grid_visible != grid);
    editor->grid_visible = grid;
    motion.y = 12;
    af_editor_mouse_motion(editor, width, height, &motion);
    click.y = AF_EDITOR_MENU_HEIGHT + 1; /* Popup padding must not start a tool. */
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    MENU_CHECK(editor->drag_mode == AF_EDITOR_DRAG_NONE && editor->selected_id == selected);
    af_editor_mouse_motion(editor, width, height, &motion);
    click.x = (float)width - editor->right_width - 20;
    click.y = af_editor_timeline_top(editor, height) - 20;
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    MENU_CHECK(editor->top_menu == AF_EDITOR_MENU_NONE && editor->selected_id == selected &&
               editor->drag_mode == AF_EDITOR_DRAG_NONE);
    /* Hover does not interrupt drawing, a held mouse button, or modal editing. */
    motion.state = SDL_BUTTON_LMASK;
    af_editor_mouse_motion(editor, width, height, &motion);
    MENU_CHECK(editor->top_menu == AF_EDITOR_MENU_NONE);
    motion.state = 0;
    editor->text_panel_open = true;
    af_editor_mouse_motion(editor, width, height, &motion);
    MENU_CHECK(editor->top_menu == AF_EDITOR_MENU_NONE);
    editor->text_panel_open = false;
    editor->command_palette.open = true;
    af_editor_mouse_motion(editor, width, height, &motion);
    MENU_CHECK(editor->top_menu == AF_EDITOR_MENU_NONE);
    editor->command_palette.open = false;
    editor->tool_command_count = 1;
    af_editor_top_menu_hover(editor, width, &motion);
    MENU_CHECK(editor->top_menu == AF_EDITOR_MENU_NONE);
    editor->tool_command_count = 0;
    editor->drag_mode = AF_EDITOR_DRAG_CANVAS_BOX;
    af_editor_mouse_motion(editor, width, height, &motion);
    MENU_CHECK(editor->top_menu == AF_EDITOR_MENU_NONE);
    editor->drag_mode = AF_EDITOR_DRAG_NONE;
    af_editor_choice_open(editor, 1, width, height, editor->left_width + 10,
                          editor->toolbar_height + 36);
    af_editor_mouse_wheel(editor, width, height,
                          &(SDL_MouseWheelEvent){.y = 1, .mouse_x = click.x, .mouse_y = click.y});
    MENU_CHECK(editor->zoom == zoom);
    MENU_CHECK(af_editor_find_tooltip(editor, width, height).id == 0);
    af_editor_mouse_down(editor, width, height, &click, SDL_KMOD_NONE);
    MENU_CHECK(!editor->choice_menu && editor->selected_id == selected &&
               editor->drag_mode == AF_EDITOR_DRAG_NONE);
    af_editor_choice_open(editor, 1, width, height, editor->left_width + 10,
                          editor->toolbar_height + 36);
    af_editor_mouse_motion(editor, width, height, &motion);
    MENU_CHECK(!editor->choice_menu && editor->top_menu == AF_EDITOR_MENU_FILE);
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_ESCAPE, .down = true});
    MENU_CHECK(editor->top_menu == AF_EDITOR_MENU_NONE && editor->selected_id == selected);
    af_editor_mouse_motion(editor, width, height, &motion);
    af_editor_handle_event(editor, &(SDL_Event){.type = SDL_EVENT_WINDOW_MOUSE_LEAVE}, width,
                           height);
    MENU_CHECK(editor->top_menu == AF_EDITOR_MENU_NONE);
    af_editor_status(
        editor, "Verified: hover menus, keyboard navigation, popup dismissal and input isolation");
#undef MENU_CHECK
    return AF_RESULT_SUCCESS;
}
