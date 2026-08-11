typedef enum AfEditorMenuAction {
    AF_EDITOR_MENU_ACTION_NONE = 0,
    AF_EDITOR_MENU_ACTION_NEW_PROJECT,
    AF_EDITOR_MENU_ACTION_OPEN_PROJECT,
    AF_EDITOR_MENU_ACTION_SAVE_PROJECT,
    AF_EDITOR_MENU_ACTION_SAVE_PROJECT_AS,
    AF_EDITOR_MENU_ACTION_COMPILE,
    AF_EDITOR_MENU_ACTION_EXIT,
    AF_EDITOR_MENU_ACTION_UNDO,
    AF_EDITOR_MENU_ACTION_REDO,
    AF_EDITOR_MENU_ACTION_DUPLICATE,
    AF_EDITOR_MENU_ACTION_RENAME,
    AF_EDITOR_MENU_ACTION_DELETE,
    AF_EDITOR_MENU_ACTION_SELECT_ALL,
    AF_EDITOR_MENU_ACTION_GRID,
    AF_EDITOR_MENU_ACTION_SNAP,
    AF_EDITOR_MENU_ACTION_ZOOM_IN,
    AF_EDITOR_MENU_ACTION_ZOOM_OUT,
    AF_EDITOR_MENU_ACTION_RESET_VIEW,
    AF_EDITOR_MENU_ACTION_FIT_CANVAS,
    AF_EDITOR_MENU_ACTION_PLAY,
    AF_EDITOR_MENU_ACTION_STOP,
    AF_EDITOR_MENU_ACTION_ADD_KEY,
    AF_EDITOR_MENU_ACTION_CURVES,
    AF_EDITOR_MENU_ACTION_NEW_CLIP,
    AF_EDITOR_MENU_ACTION_DELETE_CLIP,
    AF_EDITOR_MENU_ACTION_IMPORT_IMAGE,
    AF_EDITOR_MENU_ACTION_ASSET_BROWSER,
    AF_EDITOR_MENU_ACTION_PACK_ATLASES,
    AF_EDITOR_MENU_ACTION_CREATE_FLIPBOOK,
    AF_EDITOR_MENU_ACTION_REIMPORT_ASSETS,
    AF_EDITOR_MENU_ACTION_BUTTON_GUIDE,
    AF_EDITOR_MENU_ACTION_SHORTCUTS,
    AF_EDITOR_MENU_ACTION_ABOUT
} AfEditorMenuAction;

typedef struct AfEditorMenuItem {
    const char *label;
    const char *shortcut;
    AfEditorMenuAction action;
} AfEditorMenuItem;

typedef struct AfEditorTopMenuInfo {
    const char *label;
    float width;
    AfEditorMenuItem items[8];
    uint32_t item_count;
} AfEditorTopMenuInfo;

static const AfEditorTopMenuInfo *af_editor_top_menu_info(AfEditorTopMenu menu) {
    static const AfEditorTopMenuInfo menus[] = {
        {0},
        {.label = "FILE",
         .width = 54.0F,
         .items = {{"NEW PROJECT", "CTRL+N", AF_EDITOR_MENU_ACTION_NEW_PROJECT},
                   {"OPEN PROJECT...", "CTRL+O", AF_EDITOR_MENU_ACTION_OPEN_PROJECT},
                   {"SAVE PROJECT", "CTRL+S", AF_EDITOR_MENU_ACTION_SAVE_PROJECT},
                   {"SAVE PROJECT AS...", "CTRL+SHIFT+S", AF_EDITOR_MENU_ACTION_SAVE_PROJECT_AS},
                   {"COMPILE RUNTIME", "F5", AF_EDITOR_MENU_ACTION_COMPILE},
                   {"EXIT", "ALT+F4", AF_EDITOR_MENU_ACTION_EXIT}},
         .item_count = 6U},
        {.label = "EDIT",
         .width = 52.0F,
         .items = {{"UNDO", "CTRL+Z", AF_EDITOR_MENU_ACTION_UNDO},
                   {"REDO", "CTRL+Y", AF_EDITOR_MENU_ACTION_REDO},
                   {"DUPLICATE", "CTRL+D", AF_EDITOR_MENU_ACTION_DUPLICATE},
                   {"RENAME", "F2", AF_EDITOR_MENU_ACTION_RENAME},
                   {"DELETE", "DEL", AF_EDITOR_MENU_ACTION_DELETE},
                   {"SELECT ALL", "CTRL+A", AF_EDITOR_MENU_ACTION_SELECT_ALL}},
         .item_count = 6U},
        {.label = "VIEW",
         .width = 56.0F,
         .items = {{"TOGGLE GRID", "G", AF_EDITOR_MENU_ACTION_GRID},
                   {"TOGGLE SNAP", "N", AF_EDITOR_MENU_ACTION_SNAP},
                   {"ZOOM IN", "+", AF_EDITOR_MENU_ACTION_ZOOM_IN},
                   {"ZOOM OUT", "-", AF_EDITOR_MENU_ACTION_ZOOM_OUT},
                   {"RESET VIEW", "0", AF_EDITOR_MENU_ACTION_RESET_VIEW},
                   {"FIT CANVAS", "SHIFT+0", AF_EDITOR_MENU_ACTION_FIT_CANVAS}},
         .item_count = 6U},
        {.label = "ANIMATION",
         .width = 94.0F,
         .items = {{"PLAY / PAUSE", "SPACE", AF_EDITOR_MENU_ACTION_PLAY},
                   {"STOP", "HOME", AF_EDITOR_MENU_ACTION_STOP},
                   {"ADD KEYFRAME", "K", AF_EDITOR_MENU_ACTION_ADD_KEY},
                   {"CURVE EDITOR", "E", AF_EDITOR_MENU_ACTION_CURVES},
                   {"NEW CLIP", "ALT+C", AF_EDITOR_MENU_ACTION_NEW_CLIP},
                   {"DELETE CLIP", "ALT+DEL", AF_EDITOR_MENU_ACTION_DELETE_CLIP}},
         .item_count = 6U},
        {.label = "ASSETS",
         .width = 68.0F,
         .items = {{"IMPORT IMAGE...", "CTRL+I", AF_EDITOR_MENU_ACTION_IMPORT_IMAGE},
                   {"ASSET BROWSER", "", AF_EDITOR_MENU_ACTION_ASSET_BROWSER},
                   {"PACK ATLASES", "", AF_EDITOR_MENU_ACTION_PACK_ATLASES},
                   {"CREATE FLIPBOOK", "", AF_EDITOR_MENU_ACTION_CREATE_FLIPBOOK},
                   {"REIMPORT ASSETS", "", AF_EDITOR_MENU_ACTION_REIMPORT_ASSETS}},
         .item_count = 5U},
        {.label = "HELP",
         .width = 56.0F,
         .items = {{"BUTTON GUIDE", "", AF_EDITOR_MENU_ACTION_BUTTON_GUIDE},
                   {"KEYBOARD SHORTCUTS", "F1", AF_EDITOR_MENU_ACTION_SHORTCUTS},
                   {"ABOUT ARCFLOW", "", AF_EDITOR_MENU_ACTION_ABOUT}},
         .item_count = 3U}};
    return menu > AF_EDITOR_MENU_NONE && menu <= AF_EDITOR_MENU_HELP ? &menus[menu] : NULL;
}

static float af_editor_top_menu_x(const AfEditorState *editor, AfEditorTopMenu menu) {
    float x = editor->left_width + 8.0F;
    for (AfEditorTopMenu current = AF_EDITOR_MENU_FILE; current < menu;
         current = (AfEditorTopMenu)(current + 1)) {
        const AfEditorTopMenuInfo *info = af_editor_top_menu_info(current);
        x += info == NULL ? 0.0F : info->width;
    }
    return x;
}

static bool af_editor_confirm_discard(AfEditorState *editor, const char *title) {
    static const SDL_MessageBoxButtonData buttons[] = {
        {.flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, .buttonID = 0, .text = "Cancel"},
        {.flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, .buttonID = 1, .text = "Discard"}};
    const SDL_MessageBoxData message = {.flags = SDL_MESSAGEBOX_WARNING,
                                        .window = editor->window.window,
                                        .title = title,
                                        .message = "The current project has unsaved changes.",
                                        .numbuttons = 2,
                                        .buttons = buttons};
    int selected = 0;
    if (!editor->dirty) {
        return true;
    }
    if (!SDL_ShowMessageBox(&message, &selected)) {
        af_editor_status(editor, "Could not open the confirmation dialog");
        return false;
    }
    return selected == 1;
}

static AfResult af_editor_adopt_project(AfEditorState *editor, AfProject *project, const char *path,
                                        bool dirty) {
    if (editor == NULL || project == NULL || project->scene.count > AF_EDITOR_PROJECT_CAPACITY ||
        af_project_validate(project) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    af_project_destroy(&editor->project);
    editor->project = *project;
    *project = (AfProject){0};
    af_editor_history_clear(&editor->history);
    editor->selected_id = 0U;
    editor->selected_count = 0U;
    editor->hierarchy_anchor_id = 0U;
    editor->hierarchy_scroll = 0U;
    editor->asset_scroll = 0U;
    editor->selected_asset_id = 0U;
    editor->selected_path_command = UINT32_MAX;
    editor->selected_keyframe_count = 0U;
    editor->selected_clip_id = editor->project.clip_count == 0U ? 0U : editor->project.clips[0].id;
    editor->selected_property = AF_PROPERTY_POSITION;
    editor->playhead = 0.0F;
    editor->preview_playing = false;
    editor->timeline_focus = false;
    editor->rename_active = false;
    editor->particle_page = 0U;
    editor->render_path_data.path_count = 0U;
    editor->render_path_data.command_count = 0U;
    editor->preview_scene.count = 0U;
    if (path == NULL) {
        editor->project_path[0] = '\0';
        editor->package_path[0] = '\0';
    } else {
        (void)snprintf(editor->project_path, sizeof(editor->project_path), "%s", path);
        af_editor_derive_package_path(editor);
    }
    editor->dirty = dirty;
    return AF_RESULT_SUCCESS;
}

static AfResult af_editor_new_project(AfEditorState *editor) {
    AfProject project = {0};
    AfEditorState staging = {0};
    AfResult result = AF_RESULT_SUCCESS;
    if (!af_editor_confirm_discard(editor, "Create a new project?")) {
        return AF_RESULT_SUCCESS;
    }
    result = af_project_create(&project, &editor->allocator, AF_EDITOR_PROJECT_CAPACITY);
    if (result == AF_RESULT_SUCCESS) {
        staging.project = project;
        result = af_editor_default_project(&staging);
        project = staging.project;
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_adopt_project(editor, &project, NULL, false);
    }
    af_project_destroy(&project);
    af_editor_status(editor, result == AF_RESULT_SUCCESS ? "New project" : "New project failed");
    return result;
}

static AfResult af_editor_open_project_path(AfEditorState *editor, const char *path) {
    AfProject project = {0};
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    uint32_t missing = 0U;
    if (path == NULL || path[0] == '\0' || strlen(path) >= sizeof(editor->project_path)) {
        return result;
    }
    result = af_project_load_file(path, &editor->allocator, &project);
    if (result == AF_RESULT_SUCCESS) {
        result = af_image_build_atlases(&project.image_data);
    }
    if (result == AF_RESULT_SUCCESS) {
        for (uint32_t index = 0U; index < project.image_data.asset_count; ++index) {
            missing += project.image_data.assets[index].missing ? 1U : 0U;
        }
        result = af_editor_adopt_project(editor, &project, path, false);
    }
    af_project_destroy(&project);
    if (result == AF_RESULT_SUCCESS && missing != 0U) {
        char status[96];
        editor->asset_browser_visible = true;
        (void)snprintf(status, sizeof(status), "Project opened; %u image source%s missing", missing,
                       missing == 1U ? " is" : "s are");
        af_editor_status(editor, status);
    } else {
        af_editor_status(editor,
                         result == AF_RESULT_SUCCESS ? "Project opened" : "Project open failed");
    }
    return result;
}

static bool af_editor_project_path_with_extension(const char *path, char output[],
                                                  size_t capacity) {
    static const char extension[] = ".afproj";
    const char *current_extension = NULL;
    size_t length = 0U;
    if (path == NULL || path[0] == '\0' || output == NULL || capacity == 0U) {
        return false;
    }
    length = strlen(path);
    current_extension = strrchr(path, '.');
    if (current_extension != NULL && strcmp(current_extension, extension) == 0) {
        if (length >= capacity) {
            return false;
        }
        memcpy(output, path, length + 1U);
        return true;
    }
    if (length > capacity - sizeof(extension)) {
        return false;
    }
    memcpy(output, path, length);
    memcpy(output + length, extension, sizeof(extension));
    return true;
}

static void SDLCALL af_editor_project_dialog_callback(void *userdata, const char *const *filelist,
                                                      int filter) {
    AfEditorState *editor = userdata;
    SDL_Event event = {0};
    char *path = NULL;
    (void)filter;
    if (editor == NULL) {
        return;
    }
    if (filelist != NULL && filelist[0] != NULL) {
        path = SDL_strdup(filelist[0]);
    }
    event.type = editor->dialog_event_type;
    event.user.code = (Sint32)editor->dialog_action;
    event.user.data1 = path;
    event.user.data2 = filelist == NULL ? (void *)(uintptr_t)1U : NULL;
    if (!SDL_PushEvent(&event)) {
        SDL_free(path);
    }
}

static void af_editor_show_project_dialog(AfEditorState *editor, AfEditorDialogAction action) {
    static const SDL_DialogFileFilter filters[] = {{"Arcflow Project", "afproj"}};
    static const SDL_DialogFileFilter image_filters[] = {
        {"Supported Images", "png;jpg;jpeg;ktx2;svg"},
        {"PNG", "png"},
        {"JPEG", "jpg;jpeg"},
        {"KTX2", "ktx2"},
        {"SVG", "svg"}};
    if (editor->dialog_active || action == AF_EDITOR_DIALOG_NONE) {
        return;
    }
    if (action == AF_EDITOR_DIALOG_OPEN_PROJECT &&
        !af_editor_confirm_discard(editor, "Open another project?")) {
        return;
    }
    editor->dialog_active = true;
    editor->dialog_action = action;
    if (action == AF_EDITOR_DIALOG_OPEN_PROJECT) {
        SDL_ShowOpenFileDialog(
            af_editor_project_dialog_callback, editor, editor->window.window, filters, 1,
            editor->project_path[0] == '\0' ? NULL : editor->project_path, false);
    } else if (action == AF_EDITOR_DIALOG_SAVE_PROJECT_AS) {
        SDL_ShowSaveFileDialog(
            af_editor_project_dialog_callback, editor, editor->window.window, filters, 1,
            editor->project_path[0] == '\0' ? "untitled.afproj" : editor->project_path);
    } else {
        SDL_ShowOpenFileDialog(af_editor_project_dialog_callback, editor, editor->window.window,
                               image_filters, 5, NULL, false);
    }
}

static void af_editor_handle_dialog_event(AfEditorState *editor, const SDL_UserEvent *event) {
    char *path = event == NULL ? NULL : event->data1;
    const AfEditorDialogAction action =
        event == NULL ? AF_EDITOR_DIALOG_NONE : (AfEditorDialogAction)event->code;
    editor->dialog_active = false;
    editor->dialog_action = AF_EDITOR_DIALOG_NONE;
    if (event != NULL && event->data2 != NULL) {
        af_editor_status(editor, "File dialog failed");
    } else if (path == NULL) {
        af_editor_status(editor, "File dialog canceled");
    } else if (action == AF_EDITOR_DIALOG_OPEN_PROJECT) {
        (void)af_editor_open_project_path(editor, path);
    } else if (action == AF_EDITOR_DIALOG_SAVE_PROJECT_AS) {
        char project_path[AF_EDITOR_PATH_CAPACITY];
        if (af_editor_project_path_with_extension(path, project_path, sizeof(project_path))) {
            (void)snprintf(editor->project_path, sizeof(editor->project_path), "%s", project_path);
            af_editor_derive_package_path(editor);
            (void)af_editor_save(editor);
        } else {
            af_editor_status(editor, "Project path is too long");
        }
    } else if (action == AF_EDITOR_DIALOG_IMPORT_IMAGE) {
        uint64_t asset_id = 0U;
        uint64_t node_id = 0U;
        const AfResult result = af_project_import_image(&editor->project, editor->project_path,
                                                        path, &asset_id, &node_id);
        if (result == AF_RESULT_SUCCESS) {
            editor->selected_asset_id = asset_id;
            editor->asset_browser_visible = true;
            af_editor_select_only(editor, node_id);
            editor->dirty = true;
            (void)af_image_build_atlases(&editor->project.image_data);
            (void)af_scene_evaluate_world(&editor->project.scene);
            af_editor_status(editor, "Image imported into the project");
        } else {
            af_editor_status(editor, result == AF_RESULT_LIMIT_EXCEEDED
                                         ? "Image is too large for the project"
                                         : "Image import failed or format is unsupported");
        }
    }
    SDL_free(path);
}

static void af_editor_select_all(AfEditorState *editor) {
    editor->selected_count = editor->project.scene.count < AF_EDITOR_COMMAND_MAX_SNAPSHOTS
                                 ? editor->project.scene.count
                                 : AF_EDITOR_COMMAND_MAX_SNAPSHOTS;
    for (uint32_t index = 0U; index < editor->selected_count; ++index) {
        editor->selected_ids[index] = editor->project.scene.nodes[index].id;
    }
    editor->selected_id =
        editor->selected_count == 0U ? 0U : editor->selected_ids[editor->selected_count - 1U];
    af_editor_status(editor, editor->selected_count == editor->project.scene.count
                                 ? "All objects selected"
                                 : "Selection limited to 64 objects");
}

static void af_editor_fit_canvas(AfEditorState *editor, int width, int height) {
    float canvas_x = 0.0F;
    float canvas_y = 0.0F;
    float canvas_width = 0.0F;
    float canvas_height = 0.0F;
    af_editor_layout(editor, width, height, &canvas_x, &canvas_y, &canvas_width, &canvas_height);
    editor->zoom = af_editor_clamp(fminf(canvas_width / editor->project.canvas_size.x,
                                         canvas_height / editor->project.canvas_size.y) *
                                       0.9F,
                                   0.1F, 8.0F);
    editor->pan = (AfVec2){(canvas_width - editor->project.canvas_size.x * editor->zoom) * 0.5F,
                           (canvas_height - editor->project.canvas_size.y * editor->zoom) * 0.5F};
}

static void af_editor_execute_menu_action(AfEditorState *editor, AfEditorMenuAction action,
                                          int width, int height) {
    switch (action) {
    case AF_EDITOR_MENU_ACTION_NEW_PROJECT:
        (void)af_editor_new_project(editor);
        break;
    case AF_EDITOR_MENU_ACTION_OPEN_PROJECT:
        af_editor_show_project_dialog(editor, AF_EDITOR_DIALOG_OPEN_PROJECT);
        break;
    case AF_EDITOR_MENU_ACTION_SAVE_PROJECT:
        if (editor->project_path[0] == '\0') {
            af_editor_show_project_dialog(editor, AF_EDITOR_DIALOG_SAVE_PROJECT_AS);
        } else {
            (void)af_editor_save(editor);
        }
        break;
    case AF_EDITOR_MENU_ACTION_SAVE_PROJECT_AS:
        af_editor_show_project_dialog(editor, AF_EDITOR_DIALOG_SAVE_PROJECT_AS);
        break;
    case AF_EDITOR_MENU_ACTION_COMPILE:
        if (editor->package_path[0] == '\0') {
            af_editor_status(editor, "Save the project before compiling");
        } else {
            (void)af_editor_compile(editor);
        }
        break;
    case AF_EDITOR_MENU_ACTION_EXIT:
        if (!editor->dialog_active && af_editor_confirm_discard(editor, "Exit Arcflow Editor?")) {
            editor->running = false;
        }
        break;
    case AF_EDITOR_MENU_ACTION_UNDO:
        if (af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS) {
            editor->dirty = true;
            af_editor_reconcile_selection(editor);
            af_editor_status(editor, "Undo");
        }
        break;
    case AF_EDITOR_MENU_ACTION_REDO:
        if (af_editor_history_redo(&editor->history, &editor->project) == AF_RESULT_SUCCESS) {
            editor->dirty = true;
            af_editor_reconcile_selection(editor);
            af_editor_status(editor, "Redo");
        }
        break;
    case AF_EDITOR_MENU_ACTION_DUPLICATE:
        (void)af_editor_duplicate_selection(editor);
        break;
    case AF_EDITOR_MENU_ACTION_RENAME:
        af_editor_begin_rename(editor);
        break;
    case AF_EDITOR_MENU_ACTION_DELETE:
        (void)af_editor_delete_selection(editor);
        break;
    case AF_EDITOR_MENU_ACTION_SELECT_ALL:
        af_editor_select_all(editor);
        break;
    case AF_EDITOR_MENU_ACTION_GRID:
        editor->grid_visible = !editor->grid_visible;
        break;
    case AF_EDITOR_MENU_ACTION_SNAP:
        editor->snapping = !editor->snapping;
        break;
    case AF_EDITOR_MENU_ACTION_ZOOM_IN:
        editor->zoom = af_editor_clamp(editor->zoom * 1.2F, 0.1F, 8.0F);
        break;
    case AF_EDITOR_MENU_ACTION_ZOOM_OUT:
        editor->zoom = af_editor_clamp(editor->zoom / 1.2F, 0.1F, 8.0F);
        break;
    case AF_EDITOR_MENU_ACTION_RESET_VIEW:
        editor->zoom = 1.0F;
        editor->pan = (AfVec2){0};
        break;
    case AF_EDITOR_MENU_ACTION_FIT_CANVAS:
        af_editor_fit_canvas(editor, width, height);
        break;
    case AF_EDITOR_MENU_ACTION_PLAY:
        editor->preview_playing = !editor->preview_playing;
        break;
    case AF_EDITOR_MENU_ACTION_STOP:
        editor->preview_playing = false;
        editor->playhead = 0.0F;
        break;
    case AF_EDITOR_MENU_ACTION_ADD_KEY:
        (void)af_editor_add_keyframe(editor);
        break;
    case AF_EDITOR_MENU_ACTION_CURVES:
        editor->curve_visible = !editor->curve_visible;
        editor->timeline_focus = true;
        break;
    case AF_EDITOR_MENU_ACTION_NEW_CLIP:
        (void)af_editor_new_clip(editor);
        break;
    case AF_EDITOR_MENU_ACTION_DELETE_CLIP:
        (void)af_editor_delete_clip(editor);
        break;
    case AF_EDITOR_MENU_ACTION_IMPORT_IMAGE:
        if (editor->project_path[0] == '\0') {
            af_editor_status(editor, "Save the project before importing images");
        } else {
            af_editor_show_project_dialog(editor, AF_EDITOR_DIALOG_IMPORT_IMAGE);
        }
        break;
    case AF_EDITOR_MENU_ACTION_ASSET_BROWSER:
        editor->asset_browser_visible = !editor->asset_browser_visible;
        af_editor_status(editor, editor->asset_browser_visible ? "Asset browser shown"
                                                               : "Asset browser hidden");
        break;
    case AF_EDITOR_MENU_ACTION_PACK_ATLASES:
        if (af_image_build_atlases(&editor->project.image_data) == AF_RESULT_SUCCESS) {
            af_editor_status(editor, "Atlases packed deterministically");
        } else {
            af_editor_status(editor, "Atlas packing failed");
        }
        break;
    case AF_EDITOR_MENU_ACTION_CREATE_FLIPBOOK:
        (void)af_editor_create_flipbook(editor);
        break;
    case AF_EDITOR_MENU_ACTION_REIMPORT_ASSETS:
        (void)af_editor_reimport_images(editor);
        break;
    case AF_EDITOR_MENU_ACTION_BUTTON_GUIDE:
    case AF_EDITOR_MENU_ACTION_SHORTCUTS:
    case AF_EDITOR_MENU_ACTION_ABOUT:
        editor->help_overlay = true;
        break;
    case AF_EDITOR_MENU_ACTION_NONE:
        break;
    }
}

static void af_editor_render_help_overlay(AfEditorState *editor, int width, int height) {
    const float box_width = 520.0F;
    const float box_height = 270.0F;
    const float x = ((float)width - box_width) * 0.5F;
    const float y = ((float)height - box_height) * 0.5F;
    if (!editor->help_overlay) {
        return;
    }
    af_editor_add_rect(editor, 0.0F, 0.0F, (float)width, (float)height,
                       (AfColor){0.0F, 0.0F, 0.0F, 0.55F}, 219990);
    af_editor_add_shape(
        editor, AF_NODE_ROUNDED_RECTANGLE, (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
        (AfVec2){box_width, box_height}, (AfColor){0.1F, 0.11F, 0.14F, 1.0F}, 8.0F, 220000);
    af_editor_add_text(editor, x + 22.0F, y + 20.0F, "ARCFLOW EDITOR QUICK GUIDE", 1.4F,
                       box_width - 44.0F, (AfColor){0.62F, 0.8F, 1.0F, 1.0F}, 220010);
    af_editor_add_text(editor, x + 22.0F, y + 58.0F,
                       "FILE: create, open, save and compile projects", 1.0F, box_width - 44.0F,
                       (AfColor){0.83F, 0.85F, 0.9F, 1.0F}, 220010);
    af_editor_add_text(editor, x + 22.0F, y + 87.0F,
                       "EDIT: undo, redo, duplicate, rename and multi-select", 1.0F,
                       box_width - 44.0F, (AfColor){0.83F, 0.85F, 0.9F, 1.0F}, 220010);
    af_editor_add_text(editor, x + 22.0F, y + 116.0F,
                       "VIEW: grid, snapping, zoom, pan and fit canvas", 1.0F, box_width - 44.0F,
                       (AfColor){0.83F, 0.85F, 0.9F, 1.0F}, 220010);
    af_editor_add_text(editor, x + 22.0F, y + 145.0F,
                       "ANIMATION: clips, timeline, keys and curve editing", 1.0F,
                       box_width - 44.0F, (AfColor){0.83F, 0.85F, 0.9F, 1.0F}, 220010);
    af_editor_add_text(editor, x + 22.0F, y + 174.0F,
                       "ASSETS: import images, inspect metadata and build atlases", 1.0F,
                       box_width - 44.0F, (AfColor){0.83F, 0.85F, 0.9F, 1.0F}, 220010);
    af_editor_add_text(editor, x + 22.0F, y + 218.0F,
                       "Hover controls for help. Hold ALT for detailed help. ESC closes this.",
                       0.9F, box_width - 44.0F, (AfColor){0.58F, 0.63F, 0.72F, 1.0F}, 220010);
}

static void af_editor_render_top_menu(AfEditorState *editor, int width, int height) {
    const float right = (float)width - editor->right_width;
    const AfColor text_color = {0.8F, 0.83F, 0.89F, 1.0F};
    af_editor_add_rect(editor, editor->left_width, 0.0F, right - editor->left_width,
                       AF_EDITOR_MENU_HEIGHT, (AfColor){0.075F, 0.082F, 0.105F, 1.0F}, 210000);
    for (AfEditorTopMenu menu = AF_EDITOR_MENU_FILE; menu <= AF_EDITOR_MENU_HELP;
         menu = (AfEditorTopMenu)(menu + 1)) {
        const AfEditorTopMenuInfo *info = af_editor_top_menu_info(menu);
        const float x = af_editor_top_menu_x(editor, menu);
        const bool hovered = editor->mouse_position.x >= x &&
                             editor->mouse_position.x < x + info->width &&
                             editor->mouse_position.y < AF_EDITOR_MENU_HEIGHT;
        if (hovered || editor->top_menu == menu) {
            af_editor_add_rect(editor, x, 0.0F, info->width, AF_EDITOR_MENU_HEIGHT,
                               editor->top_menu == menu ? (AfColor){0.18F, 0.35F, 0.58F, 1.0F}
                                                        : (AfColor){0.16F, 0.18F, 0.23F, 1.0F},
                               210010);
        }
        af_editor_add_text(editor, x + 10.0F, 9.0F, info->label, 0.95F, info->width - 16.0F,
                           text_color, 210020);
    }
    if (editor->top_menu != AF_EDITOR_MENU_NONE) {
        const AfEditorTopMenuInfo *info = af_editor_top_menu_info(editor->top_menu);
        const float x = af_editor_top_menu_x(editor, editor->top_menu);
        const float popup_width = 270.0F;
        const float popup_height = 8.0F + (float)info->item_count * 30.0F;
        af_editor_add_shape(
            editor, AF_NODE_ROUNDED_RECTANGLE,
            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = AF_EDITOR_MENU_HEIGHT},
            (AfVec2){popup_width, popup_height}, (AfColor){0.105F, 0.115F, 0.145F, 1.0F}, 5.0F,
            210030);
        for (uint32_t index = 0U; index < info->item_count; ++index) {
            const float item_y = AF_EDITOR_MENU_HEIGHT + 4.0F + (float)index * 30.0F;
            const bool hovered =
                editor->mouse_position.x >= x && editor->mouse_position.x < x + popup_width &&
                editor->mouse_position.y >= item_y && editor->mouse_position.y < item_y + 30.0F;
            if (hovered) {
                af_editor_add_rect(editor, x + 4.0F, item_y, popup_width - 8.0F, 29.0F,
                                   (AfColor){0.18F, 0.34F, 0.56F, 1.0F}, 210040);
            }
            af_editor_add_text(editor, x + 14.0F, item_y + 9.0F, info->items[index].label, 1.0F,
                               175.0F, text_color, 210050);
            af_editor_add_text(editor, x + 190.0F, item_y + 10.0F, info->items[index].shortcut,
                               0.75F, 68.0F, (AfColor){0.48F, 0.54F, 0.64F, 1.0F}, 210050);
        }
    }
    af_editor_render_help_overlay(editor, width, height);
}

static bool af_editor_top_menu_mouse_down(AfEditorState *editor, int width, int height, float x,
                                          float y) {
    const float right = (float)width - editor->right_width;
    (void)height;
    if (y < AF_EDITOR_MENU_HEIGHT && x >= editor->left_width && x < right) {
        for (AfEditorTopMenu menu = AF_EDITOR_MENU_FILE; menu <= AF_EDITOR_MENU_HELP;
             menu = (AfEditorTopMenu)(menu + 1)) {
            const AfEditorTopMenuInfo *info = af_editor_top_menu_info(menu);
            const float menu_x = af_editor_top_menu_x(editor, menu);
            if (x >= menu_x && x < menu_x + info->width) {
                editor->top_menu = editor->top_menu == menu ? AF_EDITOR_MENU_NONE : menu;
                editor->context_menu = AF_EDITOR_CONTEXT_NONE;
                return true;
            }
        }
        editor->top_menu = AF_EDITOR_MENU_NONE;
        return true;
    }
    if (editor->top_menu != AF_EDITOR_MENU_NONE) {
        const AfEditorTopMenuInfo *info = af_editor_top_menu_info(editor->top_menu);
        const float menu_x = af_editor_top_menu_x(editor, editor->top_menu);
        const float popup_width = 270.0F;
        const float first_y = AF_EDITOR_MENU_HEIGHT + 4.0F;
        if (x >= menu_x && x < menu_x + popup_width && y >= first_y &&
            y < first_y + (float)info->item_count * 30.0F) {
            const uint32_t item = (uint32_t)((y - first_y) / 30.0F);
            const AfEditorMenuAction action = info->items[item].action;
            editor->top_menu = AF_EDITOR_MENU_NONE;
            af_editor_execute_menu_action(editor, action, width, height);
            return true;
        }
        editor->top_menu = AF_EDITOR_MENU_NONE;
    }
    if (editor->help_overlay) {
        editor->help_overlay = false;
        return true;
    }
    return false;
}
