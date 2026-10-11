/* Create projects through File/keyboard/picker input, then draw and exercise history. */
static AfResult af_editor_template_smoke(AfEditorState *editor, int width, int height) {
#define TEMPLATE_CHECK(condition)                                                                  \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            af_log(AF_LOG_ERROR, "Project drawing check failed at line %d (%s): %s", __LINE__,     \
                   project_name, editor->status);                                                  \
            return AF_RESULT_INVALID_ARGUMENT;                                                     \
        }                                                                                          \
    } while (0)
    for (uint32_t index = 0; index < af_project_template_count() + 2U; ++index) {
        const char *project_name = index == 0U ? "File > New project" : "Ctrl+N";
        /* Each iteration discards only the disposable project from the previous test. */
        editor->dirty = false;
        if (index == 0U) {
            af_editor_mouse_motion(
                editor, width, height,
                &(SDL_MouseMotionEvent){.x = af_editor_top_menu_x(editor, AF_EDITOR_MENU_FILE) + 2,
                                        .y = 12});
            TEMPLATE_CHECK(editor->top_menu == AF_EDITOR_MENU_FILE);
            af_editor_mouse_down(
                editor, width, height,
                &(SDL_MouseButtonEvent){.button = SDL_BUTTON_LEFT,
                                        .down = true,
                                        .clicks = 1,
                                        .x = af_editor_top_popup_x(editor, width) + 20,
                                        .y = AF_EDITOR_MENU_HEIGHT + 14},
                SDL_KMOD_NONE);
            af_editor_mouse_up(editor, width, height);
            TEMPLATE_CHECK(editor->top_menu == AF_EDITOR_MENU_NONE);
        } else if (index == 1U) {
            af_editor_key(editor, width, height,
                          &(SDL_KeyboardEvent){.key = SDLK_N, .mod = SDL_KMOD_CTRL, .down = true});
        } else {
            const uint32_t template_index = index - 2U;
            project_name = af_project_template_info((AfProjectTemplateId)template_index)->slug;
            af_editor_execute_menu_action(editor, AF_EDITOR_MENU_ACTION_NEW_FROM_TEMPLATE, width,
                                          height);
            TEMPLATE_CHECK(editor->template_panel_visible);
            if ((template_index & 1U) == 0) {
                const SDL_MouseButtonEvent pick = {
                    .button = SDL_BUTTON_LEFT,
                    .down = true,
                    .clicks = 1,
                    .x = ((float)width - 780) * .5F + 38 + (float)(template_index % 3) * 250,
                    .y = ((float)height - 410) * .5F + 95 + (float)(template_index / 3) * 145};
                af_editor_mouse_down(editor, width, height, &pick, SDL_KMOD_NONE);
                af_editor_mouse_up(editor, width, height);
            } else {
                editor->template_selected_index = template_index;
                af_editor_key(editor, width, height,
                              &(SDL_KeyboardEvent){.key = SDLK_RETURN, .down = true});
            }
        }
        TEMPLATE_CHECK(!editor->template_panel_visible && editor->project_path[0] == '\0');
        af_editor_update_preview(editor);
        const SDL_Keycode shortcuts[] = {SDLK_R, SDLK_R, SDLK_O, SDLK_L, SDLK_A, SDLK_Y};
        const AfNodeType types[] = {AF_NODE_RECTANGLE, AF_NODE_ROUNDED_RECTANGLE,
                                    AF_NODE_ELLIPSE,   AF_NODE_PATH,
                                    AF_NODE_PATH,      AF_NODE_PATH};
        for (uint32_t tool = 0; tool < 6; ++tool) {
            const uint32_t count = editor->project.scene.count;
            af_editor_key(editor, width, height,
                          &(SDL_KeyboardEvent){.key = shortcuts[tool],
                                               .mod = tool == 1U ? SDL_KMOD_SHIFT : SDL_KMOD_NONE,
                                               .down = true});
            const float x = editor->left_width + 90 + (float)tool * 100;
            const float y = editor->toolbar_height + 140;
            const SDL_MouseButtonEvent press = {
                .button = SDL_BUTTON_LEFT, .down = true, .clicks = 1, .x = x, .y = y};
            af_editor_mouse_down(editor, width, height, &press, SDL_KMOD_NONE);
            TEMPLATE_CHECK(editor->drag_mode == AF_EDITOR_DRAG_TOOL_CREATE);
            af_editor_mouse_motion(
                editor, width, height,
                &(SDL_MouseMotionEvent){
                    .state = SDL_BUTTON_LMASK, .x = x + 90, .y = y + 70, .xrel = 90, .yrel = 70});
            af_editor_mouse_up(editor, width, height);
            TEMPLATE_CHECK(editor->project.scene.count == count + 1);
            const uint64_t id = editor->selected_id;
            af_editor_update_preview(editor);
            const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, id);
            TEMPLATE_CHECK(node != NULL && node->type == types[tool] && node->world_visible &&
                           node->world_opacity > 0 && node->resolved_bounds.x > 0 &&
                           node->resolved_bounds.y > 0);
            af_editor_build_render_scene(editor, width, height);
            TEMPLATE_CHECK(af_scene_find_const(&editor->render_scene, id) != NULL);
            af_editor_key(editor, width, height,
                          &(SDL_KeyboardEvent){.key = SDLK_Z, .mod = SDL_KMOD_CTRL, .down = true});
            TEMPLATE_CHECK(editor->project.scene.count == count &&
                           af_scene_find_const(&editor->project.scene, id) == NULL);
            af_editor_key(editor, width, height,
                          &(SDL_KeyboardEvent){.key = SDLK_Y, .mod = SDL_KMOD_CTRL, .down = true});
            TEMPLATE_CHECK(editor->project.scene.count == count + 1 &&
                           af_scene_find_const(&editor->project.scene, id) != NULL);
        }
        TEMPLATE_CHECK(af_project_validate(&editor->project) == AF_RESULT_SUCCESS);
    }
    af_editor_update_preview(editor);
    af_editor_status(editor,
                     "New project and all templates: drawing, visibility, undo and redo passed");
#undef TEMPLATE_CHECK
    return AF_RESULT_SUCCESS;
}
