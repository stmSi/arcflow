/* Quick actions for the document overview and persistent workspace footer. */
static bool af_editor_studio_mouse_down(AfEditorState *editor, int width, int height, float x,
                                        float y) {
    const float right = (float)width - editor->right_width;
    const float footer = (float)height - editor->status_height;
    if (editor->top_menu != AF_EDITOR_MENU_NONE || editor->choice_menu ||
        editor->context_menu != AF_EDITOR_CONTEXT_NONE)
        return false;
    if (y >= footer + 4 && y < (float)height) {
        if (x >= right + 18 && x < (float)width - 18) {
            (void)af_editor_action_execute(&editor->actions, AF_EDITOR_MENU_ACTION_COMMAND_PALETTE);
            return true;
        }
        if (x >= right - 76 && x < right - 10) {
            af_editor_execute_menu_action(editor, AF_EDITOR_MENU_ACTION_FIT_CANVAS, width, height);
            return true;
        }
        return true;
    }
    if (x >= right + 18 && x < (float)width - 18 && editor->selected_count == 0) {
        if (y >= 313 && y <= 345)
            af_editor_execute_menu_action(editor, AF_EDITOR_MENU_ACTION_FIT_CANVAS, width, height);
        for (uint32_t row = 0; row < 3; ++row) {
            if (y >= 423 + (float)row * 38 && y < 453 + (float)row * 38) {
                const AfEditorVectorTool tools[] = {AF_EDITOR_TOOL_RECTANGLE, AF_EDITOR_TOOL_PEN,
                                                    AF_EDITOR_TOOL_TEXT};
                af_editor_set_vector_tool(editor, tools[row]);
            }
        }
        return true;
    }
    if (editor->asset_browser_visible && editor->project.image_data.asset_count == 0 &&
        editor->asset_height >= 128 && x >= 12 && x < editor->left_width - 12) {
        const float top = af_editor_asset_browser_top(editor, height);
        if (y >= top + 88 && y < top + 118) {
            af_editor_show_project_dialog(editor, AF_EDITOR_DIALOG_IMPORT_IMAGE);
            return true;
        }
    }
    return false;
}

static void af_editor_render_studio_chrome(AfEditorState *editor, int width, int height) {
    const float y = (float)height - editor->status_height;
    const float right = (float)width - editor->right_width;
    char zoom[24];
    af_editor_add_rect(editor, 0, y, (float)width, editor->status_height, AF_UI_PANEL, 100200);
    af_editor_add_rect(editor, 0, y, (float)width, 1, AF_UI_BORDER, 100201);
    af_editor_add_line(editor, (AfVec2){12, y + 19}, (AfVec2){18, y + 8}, 2, AF_UI_ACCENT, 100210);
    af_editor_add_line(editor, (AfVec2){18, y + 8}, (AfVec2){24, y + 19}, 2, AF_UI_ACCENT, 100210);
    af_editor_add_line(editor, (AfVec2){16, y + 16}, (AfVec2){25, y + 16}, 2, AF_UI_ACCENT, 100210);
    af_editor_add_text(editor, 33, y + 8, "arcflow", 1.1F, 65, AF_UI_TEXT, 100210);
    af_editor_add_shape(
        editor, AF_NODE_ELLIPSE,
        (AfAffine2){.m00 = 1, .m11 = 1, .tx = editor->left_width - 81, .ty = y + 12},
        (AfVec2){5, 5}, editor->dirty ? AF_UI_WARNING : AF_UI_SUCCESS, 0, 100210);
    af_editor_add_text(editor, editor->left_width - 69, y + 9, editor->dirty ? "Modified" : "Ready",
                       0.65F, 61, AF_UI_MUTED, 100210);
    af_editor_add_text(editor, editor->left_width + 12, y + 9, editor->status, 0.8F,
                       right - editor->left_width - 104, AF_UI_MUTED, 100210);
    (void)snprintf(zoom, sizeof(zoom), "%.0f%%", (double)editor->zoom * 100);
    af_editor_add_button_rect(editor, right - 76, y + 4, 66, editor->status_height - 7,
                              AF_UI_SURFACE, 100205);
    af_editor_add_text(editor, right - 61, y + 9, zoom, 0.8F, 48, AF_UI_TEXT, 100210);
    af_editor_add_button_rect(editor, right + 18, y + 4, editor->right_width - 36,
                              editor->status_height - 7, AF_UI_SURFACE, 100205);
    af_editor_add_text(editor, right + 28, y + 9, "Commands", 0.8F, editor->right_width - 56,
                       AF_UI_TEXT, 100210);
    if (editor->right_width >= 240)
        af_editor_add_text(editor, (float)width - 112, y + 9, "Ctrl+Shift+P", 0.65F, 84,
                           AF_UI_MUTED, 100210);
}
