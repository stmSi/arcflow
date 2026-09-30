static AfResult af_editor_export_smoke(AfEditorState *editor, int width, int height) {
#define EXPORT_CHECK(c)                                                                            \
    do {                                                                                           \
        if (!(c))                                                                                  \
            return af_editor_interaction_smoke_failure(editor, __LINE__);                          \
    } while (0)
    EXPORT_CHECK(af_editor_create_primitive_at(editor, AF_NODE_RECTANGLE,
                                               (AfEditorToolRect){{30, 40}, {100, 80}}) ==
                 AF_RESULT_SUCCESS);
    const uint64_t selected = editor->selected_id;
    EXPORT_CHECK(af_editor_create_primitive_at(editor, AF_NODE_ELLIPSE,
                                               (AfEditorToolRect){{270, 90}, {70, 60}}) ==
                 AF_RESULT_SUCCESS);
    const uint64_t outside = editor->selected_id;
    af_editor_select_only(editor, selected);
    af_editor_update_preview(editor);
    AfPackageBlob before = {0}, after = {0};
    EXPORT_CHECK(af_project_serialize(&editor->project, &editor->allocator, &before) ==
                 AF_RESULT_SUCCESS);
    af_editor_open_export(editor);
    float x, y, w, h;
    af_editor_export_panel_layout(width, height, &x, &y, &w, &h);
    const float right = x + w - 290;
    af_editor_mouse_down(
        editor, width, height,
        &(SDL_MouseButtonEvent){.button = SDL_BUTTON_LEFT, .x = right + 10, .y = y + 85}, 0);
    EXPORT_CHECK(editor->export_field == 1);
    EXPORT_CHECK(af_editor_workflow_text(editor, "320"));
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_TAB, .down = true});
    EXPORT_CHECK(editor->export_field == 2 && editor->export_options.size.x == 320);
    EXPORT_CHECK(af_editor_workflow_text(editor, "180"));
    af_editor_key(editor, width, height, &(SDL_KeyboardEvent){.key = SDLK_RETURN, .down = true});
    EXPORT_CHECK(!editor->export_field && editor->export_options.size.y == 180);
    af_editor_export_refresh_preview(editor);
    EXPORT_CHECK(editor->export_preview_valid && !editor->export_diagnostics.warning_count);
    EXPORT_CHECK(!editor->export_preview_dirty);
    af_editor_export_refresh_preview(editor);
    EXPORT_CHECK(editor->export_preview_valid && !editor->export_preview_dirty);
    AfEditorSnapBounds bounds;
    EXPORT_CHECK(af_editor_export_node_bounds(
        editor, af_scene_find_const(&editor->preview_scene, outside), &bounds));
    EXPORT_CHECK(bounds.maximum.x > editor->export_options.size.x);
    /* Invalid numeric input cannot launch a save dialog. */
    af_editor_export_focus(editor, 1);
    af_editor_export_panel_text(editor, "0");
    af_editor_export_start(editor);
    EXPORT_CHECK(editor->export_panel_visible && !editor->dialog_active);
    af_editor_export_focus(editor, 1);
    af_editor_export_panel_text(editor, "320");
    EXPORT_CHECK(af_editor_export_finish_field(editor));
    af_editor_export_panel_mouse(editor, width, height, right + 180, y + 186);
    EXPORT_CHECK(editor->export_options.selection_only);
    af_editor_export_panel_mouse(editor, width, height, right + 10, y + 228);
    EXPORT_CHECK(editor->export_options.origin.x <= 30 && editor->export_options.origin.y <= 40);
    EXPORT_CHECK(editor->export_options.size.x >= 100 && editor->export_options.size.x <= 104);
    /* Exercise the same write path used after the native save dialog returns. */
    char path[160];
    (void)snprintf(path, sizeof(path), "/tmp/arcflow-export-smoke-%llu.svg",
                   (unsigned long long)SDL_GetTicksNS());
    EXPORT_CHECK(af_editor_export_with_options(editor, path, AF_EDITOR_DIALOG_EXPORT_SVG) ==
                 AF_RESULT_SUCCESS);
    AfFileData file = {0};
    EXPORT_CHECK(af_file_read_limited(path, AF_PROJECT_MAX_FILE_SIZE, &editor->allocator, &file) ==
                 AF_RESULT_SUCCESS);
    EXPORT_CHECK(strstr((const char *)file.data, "<rect") &&
                 !strstr((const char *)file.data, "<ellipse"));
    af_file_data_destroy(&editor->allocator, &file);
    (void)remove(path);
    /* Compatibility loss must be visible before saving. */
    AfStyle *style = af_project_find_node_style(&editor->project, selected);
    EXPORT_CHECK(style);
    const AfStyle original_style = *style;
    style->fill.type = AF_PAINT_LINEAR_GRADIENT;
    style->fill.stop_count = 2;
    style->fill.stops[0] = (AfGradientStop){0, {1, 0, 0, 1}};
    style->fill.stops[1] = (AfGradientStop){1, {0, 0, 1, 1}};
    editor->export_preview_dirty = true;
    af_editor_export_refresh_preview(editor);
    EXPORT_CHECK(editor->export_preview_valid && editor->export_diagnostics.warning_count > 0);
    EXPORT_CHECK(!editor->export_preview_draw[af_project_find_index(&editor->project, selected)]);
    *style = original_style;
    editor->export_preview_dirty = true;
    af_editor_update_preview(editor);
    af_editor_export_refresh_preview(editor);
    EXPORT_CHECK(af_project_serialize(&editor->project, &editor->allocator, &after) ==
                 AF_RESULT_SUCCESS);
    EXPORT_CHECK(before.size == after.size && !memcmp(before.data, after.data, before.size));
    EXPORT_CHECK(editor->selected_id == selected && editor->selected_count == 1);
    af_package_blob_destroy(&editor->allocator, &before);
    af_package_blob_destroy(&editor->allocator, &after);
    /* Capture a page that visibly clips artwork beyond its right edge. */
    editor->export_options = (AfEditorExportOptions){.size = {320, 180}};
    editor->export_preview_dirty = true;
    af_editor_status(
        editor, "Export fields, scope, fit, conversion preview and document isolation verified");
#undef EXPORT_CHECK
    return AF_RESULT_SUCCESS;
}
