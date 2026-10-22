typedef enum AfEditorMenuAction {
    AF_EDITOR_MENU_ACTION_NONE = 0,
    AF_EDITOR_MENU_ACTION_NEW_PROJECT,
    AF_EDITOR_MENU_ACTION_OPEN_PROJECT,
    AF_EDITOR_MENU_ACTION_RELOAD_PROJECT,
    AF_EDITOR_MENU_ACTION_SAVE_PROJECT,
    AF_EDITOR_MENU_ACTION_SAVE_PROJECT_AS,
    AF_EDITOR_MENU_ACTION_COMPILE,
    AF_EDITOR_MENU_ACTION_EXIT,
    AF_EDITOR_MENU_ACTION_UNDO,
    AF_EDITOR_MENU_ACTION_REDO,
    AF_EDITOR_MENU_ACTION_DUPLICATE,
    AF_EDITOR_MENU_ACTION_COMBINE,
    AF_EDITOR_MENU_ACTION_BREAK_APART,
    AF_EDITOR_MENU_ACTION_RENAME,
    AF_EDITOR_MENU_ACTION_DELETE,
    AF_EDITOR_MENU_ACTION_SELECT_ALL,
    AF_EDITOR_MENU_ACTION_GRID,
    AF_EDITOR_MENU_ACTION_SNAP,
    AF_EDITOR_MENU_ACTION_ZOOM_IN,
    AF_EDITOR_MENU_ACTION_ZOOM_OUT,
    AF_EDITOR_MENU_ACTION_RESET_VIEW,
    AF_EDITOR_MENU_ACTION_FIT_CANVAS,
    AF_EDITOR_MENU_ACTION_FULLSCREEN,
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
    AF_EDITOR_MENU_ACTION_NEW_FROM_TEMPLATE,
    AF_EDITOR_MENU_ACTION_OPEN_RECENT,
    AF_EDITOR_MENU_ACTION_COPY_OBJECTS,
    AF_EDITOR_MENU_ACTION_PASTE_OBJECTS,
    AF_EDITOR_MENU_ACTION_FIND_HIERARCHY,
    AF_EDITOR_MENU_ACTION_COMMAND_PALETTE,
    AF_EDITOR_MENU_ACTION_ALIGN_LEFT,
    AF_EDITOR_MENU_ACTION_ALIGN_CENTER_X,
    AF_EDITOR_MENU_ACTION_ALIGN_RIGHT,
    AF_EDITOR_MENU_ACTION_ALIGN_TOP,
    AF_EDITOR_MENU_ACTION_ALIGN_CENTER_Y,
    AF_EDITOR_MENU_ACTION_ALIGN_BOTTOM,
    AF_EDITOR_MENU_ACTION_DISTRIBUTE_HORIZONTAL,
    AF_EDITOR_MENU_ACTION_DISTRIBUTE_VERTICAL,
    AF_EDITOR_MENU_ACTION_ADD_VERTICAL_GUIDE,
    AF_EDITOR_MENU_ACTION_ADD_HORIZONTAL_GUIDE,
    AF_EDITOR_MENU_ACTION_CLEAR_GUIDES,
    AF_EDITOR_MENU_ACTION_TOGGLE_RULERS,
    AF_EDITOR_MENU_ACTION_TOGGLE_MEASUREMENTS,
    AF_EDITOR_MENU_ACTION_TOGGLE_SMART_GUIDES,
    AF_EDITOR_MENU_ACTION_PROFILER,
    AF_EDITOR_MENU_ACTION_VALIDATION,
    AF_EDITOR_MENU_ACTION_SYSTEM_INFO,
    AF_EDITOR_MENU_ACTION_AUTOSAVE,
    AF_EDITOR_MENU_ACTION_RECOVERY,
    AF_EDITOR_MENU_ACTION_RESET_WORKSPACE,
    AF_EDITOR_MENU_ACTION_PRESET_LIBRARY,
    AF_EDITOR_MENU_ACTION_WELCOME,
    AF_EDITOR_MENU_ACTION_GUIDED_PROJECT,
    AF_EDITOR_MENU_ACTION_EXAMPLE_BROWSER,
    AF_EDITOR_MENU_ACTION_BUTTON_GUIDE,
    AF_EDITOR_MENU_ACTION_SHORTCUTS,
    AF_EDITOR_MENU_ACTION_CHECK_UPDATES,
    AF_EDITOR_MENU_ACTION_ABOUT,
    AF_EDITOR_MENU_ACTION_EXPORT,
    /* Append actions: numeric IDs are persisted in shortcut preferences. */
    AF_EDITOR_MENU_ACTION_WELD,
    AF_EDITOR_MENU_ACTION_TRIM,
    AF_EDITOR_MENU_ACTION_INTERSECT,
    AF_EDITOR_MENU_ACTION_JOIN_ENDPOINTS,
    AF_EDITOR_MENU_ACTION_CONTINUE_PATH,
    AF_EDITOR_MENU_ACTION_OUTLINE_OBJECT,
    AF_EDITOR_MENU_ACTION_OFFSET,
    AF_EDITOR_MENU_ACTION_TEXT,
    AF_EDITOR_MENU_ACTION_APPEARANCE,
    AF_EDITOR_MENU_ACTION_COUNT
} AfEditorMenuAction;

static void af_editor_execute_workflow_action(AfEditorState *editor, AfEditorMenuAction action,
                                              int width, int height);

typedef struct AfEditorMenuItem {
    const char *label;
    const char *shortcut;
    AfEditorMenuAction action;
} AfEditorMenuItem;

typedef struct AfEditorTopMenuInfo {
    const char *label;
    float width;
    AfEditorMenuItem items[10];
    uint32_t item_count;
} AfEditorTopMenuInfo;

static const AfEditorTopMenuInfo *af_editor_top_menu_info(AfEditorTopMenu menu) {
    static const AfEditorTopMenuInfo menus[] = {
        {0},
        {.label = "File",
         .width = 54.0F,
         .items = {{"New project", "CTRL+N", AF_EDITOR_MENU_ACTION_NEW_PROJECT},
                   {"New from template", "", AF_EDITOR_MENU_ACTION_NEW_FROM_TEMPLATE},
                   {"Open project...", "CTRL+O", AF_EDITOR_MENU_ACTION_OPEN_PROJECT},
                   {"Open recent", "", AF_EDITOR_MENU_ACTION_OPEN_RECENT},
                   {"Reload from disk", "", AF_EDITOR_MENU_ACTION_RELOAD_PROJECT},
                   {"Save project", "CTRL+S", AF_EDITOR_MENU_ACTION_SAVE_PROJECT},
                   {"Save project as...", "CTRL+SHIFT+S", AF_EDITOR_MENU_ACTION_SAVE_PROJECT_AS},
                   {"Compile runtime", "F5", AF_EDITOR_MENU_ACTION_COMPILE},
                   {"Export...", "CTRL+SHIFT+E", AF_EDITOR_MENU_ACTION_EXPORT},
                   {"Exit", "ALT+F4", AF_EDITOR_MENU_ACTION_EXIT}},
         .item_count = 10U},
        {.label = "Edit",
         .width = 52.0F,
         .items = {{"Undo", "CTRL+Z", AF_EDITOR_MENU_ACTION_UNDO},
                   {"Redo", "CTRL+Y", AF_EDITOR_MENU_ACTION_REDO},
                   {"Copy objects", "CTRL+C", AF_EDITOR_MENU_ACTION_COPY_OBJECTS},
                   {"Paste objects", "CTRL+V", AF_EDITOR_MENU_ACTION_PASTE_OBJECTS},
                   {"Duplicate", "CTRL+D", AF_EDITOR_MENU_ACTION_DUPLICATE},
                   {"Rename", "F2", AF_EDITOR_MENU_ACTION_RENAME},
                   {"Delete", "DEL", AF_EDITOR_MENU_ACTION_DELETE},
                   {"Select all", "CTRL+A", AF_EDITOR_MENU_ACTION_SELECT_ALL}},
         .item_count = 8U},
        {.label = "View",
         .width = 56.0F,
         .items = {{"Toggle grid", "G", AF_EDITOR_MENU_ACTION_GRID},
                   {"Toggle snap", "N", AF_EDITOR_MENU_ACTION_SNAP},
                   {"Zoom in", "+", AF_EDITOR_MENU_ACTION_ZOOM_IN},
                   {"Zoom out", "-", AF_EDITOR_MENU_ACTION_ZOOM_OUT},
                   {"Reset view", "0", AF_EDITOR_MENU_ACTION_RESET_VIEW},
                   {"Fit canvas", "SHIFT+0", AF_EDITOR_MENU_ACTION_FIT_CANVAS},
                   {"Fullscreen", "ALT+ENTER", AF_EDITOR_MENU_ACTION_FULLSCREEN}},
         .item_count = 7U},
        {.label = "Animation",
         .width = 94.0F,
         .items = {{"Play / pause", "SPACE", AF_EDITOR_MENU_ACTION_PLAY},
                   {"Stop", "HOME", AF_EDITOR_MENU_ACTION_STOP},
                   {"Add keyframe", "K", AF_EDITOR_MENU_ACTION_ADD_KEY},
                   {"Curve editor", "F6", AF_EDITOR_MENU_ACTION_CURVES},
                   {"New clip", "ALT+C", AF_EDITOR_MENU_ACTION_NEW_CLIP},
                   {"Delete clip", "ALT+DEL", AF_EDITOR_MENU_ACTION_DELETE_CLIP}},
         .item_count = 6U},
        {.label = "Assets",
         .width = 68.0F,
         .items = {{"Import image...", "CTRL+I", AF_EDITOR_MENU_ACTION_IMPORT_IMAGE},
                   {"Asset browser", "", AF_EDITOR_MENU_ACTION_ASSET_BROWSER},
                   {"Pack atlases", "", AF_EDITOR_MENU_ACTION_PACK_ATLASES},
                   {"Create flipbook", "", AF_EDITOR_MENU_ACTION_CREATE_FLIPBOOK},
                   {"Reimport assets", "", AF_EDITOR_MENU_ACTION_REIMPORT_ASSETS}},
         .item_count = 5U},
        {.label = "Workspace",
         .width = 96.0F,
         .items = {{"Command palette", "CTRL+SHIFT+P", AF_EDITOR_MENU_ACTION_COMMAND_PALETTE},
                   {"Find in hierarchy", "CTRL+F", AF_EDITOR_MENU_ACTION_FIND_HIERARCHY},
                   {"Profile panel", "F10", AF_EDITOR_MENU_ACTION_PROFILER},
                   {"Validation panel", "F11", AF_EDITOR_MENU_ACTION_VALIDATION},
                   {"System / displays", "", AF_EDITOR_MENU_ACTION_SYSTEM_INFO},
                   {"Rulers", "", AF_EDITOR_MENU_ACTION_TOGGLE_RULERS},
                   {"Measurements", "", AF_EDITOR_MENU_ACTION_TOGGLE_MEASUREMENTS},
                   {"Smart guides", "", AF_EDITOR_MENU_ACTION_TOGGLE_SMART_GUIDES},
                   {"Reset workspace", "", AF_EDITOR_MENU_ACTION_RESET_WORKSPACE},
                   {"Appearance...", "CTRL+,", AF_EDITOR_MENU_ACTION_APPEARANCE}},
         .item_count = 10U},
        {.label = "Tools",
         .width = 62.0F,
         .items = {{"Align left", "", AF_EDITOR_MENU_ACTION_ALIGN_LEFT},
                   {"Align center x", "", AF_EDITOR_MENU_ACTION_ALIGN_CENTER_X},
                   {"Align top", "", AF_EDITOR_MENU_ACTION_ALIGN_TOP},
                   {"Align center y", "", AF_EDITOR_MENU_ACTION_ALIGN_CENTER_Y},
                   {"Distribute h", "", AF_EDITOR_MENU_ACTION_DISTRIBUTE_HORIZONTAL},
                   {"Distribute v", "", AF_EDITOR_MENU_ACTION_DISTRIBUTE_VERTICAL},
                   {"Canvas guides", "", AF_EDITOR_MENU_ACTION_ADD_VERTICAL_GUIDE},
                   {"Autosave / recovery", "", AF_EDITOR_MENU_ACTION_AUTOSAVE}},
         .item_count = 8U},
        {.label = "Shape",
         .width = 62.0F,
         .items = {{"Weld", "", AF_EDITOR_MENU_ACTION_WELD},
                   {"Trim primary", "", AF_EDITOR_MENU_ACTION_TRIM},
                   {"Intersect", "", AF_EDITOR_MENU_ACTION_INTERSECT},
                   {"Combine paths", "CTRL+L", AF_EDITOR_MENU_ACTION_COMBINE},
                   {"Break apart", "CTRL+SHIFT+L", AF_EDITOR_MENU_ACTION_BREAK_APART},
                   {"Join endpoints", "", AF_EDITOR_MENU_ACTION_JOIN_ENDPOINTS},
                   {"Continue path", "", AF_EDITOR_MENU_ACTION_CONTINUE_PATH},
                   {"Outline to object", "", AF_EDITOR_MENU_ACTION_OUTLINE_OBJECT},
                   {"Contour / offset...", "", AF_EDITOR_MENU_ACTION_OFFSET},
                   {"Create / edit text...", "T", AF_EDITOR_MENU_ACTION_TEXT}},
         .item_count = 10U},
        {.label = "Help",
         .width = 56.0F,
         .items = {{"Welcome", "", AF_EDITOR_MENU_ACTION_WELCOME},
                   {"Guided first project", "", AF_EDITOR_MENU_ACTION_GUIDED_PROJECT},
                   {"Example browser", "", AF_EDITOR_MENU_ACTION_EXAMPLE_BROWSER},
                   {"Reference topics", "", AF_EDITOR_MENU_ACTION_BUTTON_GUIDE},
                   {"Keyboard shortcuts", "F1", AF_EDITOR_MENU_ACTION_SHORTCUTS},
                   {"Check for updates...", "", AF_EDITOR_MENU_ACTION_CHECK_UPDATES},
                   {"About arcflow", "", AF_EDITOR_MENU_ACTION_ABOUT}},
         .item_count = 7U}};
    return menu > AF_EDITOR_MENU_NONE && menu <= AF_EDITOR_MENU_HELP ? &menus[menu] : NULL;
}

static float af_editor_top_menu_scale(const AfEditorState *editor) {
    float total = 0.0F;
    for (AfEditorTopMenu menu = AF_EDITOR_MENU_FILE; menu <= AF_EDITOR_MENU_HELP;
         menu = (AfEditorTopMenu)(menu + 1)) {
        total += af_editor_top_menu_info(menu)->width;
    }
    return af_editor_clamp(
        ((float)editor->window_width - editor->left_width - editor->right_width - 16.0F) / total,
        0.1F, 1.0F);
}

static float af_editor_top_menu_x(const AfEditorState *editor, AfEditorTopMenu menu) {
    float x = editor->left_width + 8.0F;
    for (AfEditorTopMenu current = AF_EDITOR_MENU_FILE; current < menu;
         current = (AfEditorTopMenu)(current + 1)) {
        const AfEditorTopMenuInfo *info = af_editor_top_menu_info(current);
        x += info == NULL ? 0.0F : info->width * af_editor_top_menu_scale(editor);
    }
    return x;
}

/* Hover menus share the same input boundaries as modal workflows. */
static bool af_editor_menu_blocked(const AfEditorState *editor) {
    return editor->dialog_active || editor->text_panel_open || editor->offset_panel_open ||
           editor->rename_active || editor->inspector_field != AF_EDITOR_INSPECTOR_NONE ||
           editor->hierarchy_search_active || editor->command_palette.open ||
           editor->welcome_visible || editor->example_browser_visible ||
           editor->guide_browser_visible || editor->export_panel_visible ||
           editor->platform_panel_visible || editor->update_panel_visible ||
           editor->template_panel_visible || editor->recent_panel_visible ||
           editor->shortcuts_overlay || editor->recovery_overlay || editor->preset_panel_visible ||
           editor->help_overlay || editor->appearance_visible;
}
static float af_editor_top_popup_x(const AfEditorState *editor, int width) {
    return af_editor_clamp(af_editor_top_menu_x(editor, editor->top_menu), 0,
                           fmaxf(0, (float)width - 340));
}
static void af_editor_top_menu_open(AfEditorState *editor, AfEditorTopMenu menu) {
    if (editor->top_menu != menu)
        editor->top_menu_item = UINT32_MAX;
    editor->top_menu = menu;
    editor->context_menu = AF_EDITOR_CONTEXT_NONE;
    editor->choice_menu = 0;
    editor->hovered_tooltip_id = 0;
}
static void af_editor_top_menu_hover(AfEditorState *editor, int width,
                                     const SDL_MouseMotionEvent *event) {
    if (af_editor_menu_blocked(editor) || event->state || editor->gradient_drag ||
        editor->drag_mode != AF_EDITOR_DRAG_NONE || editor->tool_command_count)
        return;
    if (event->y >= 0 && event->y < AF_EDITOR_MENU_HEIGHT) {
        for (AfEditorTopMenu menu = AF_EDITOR_MENU_FILE; menu <= AF_EDITOR_MENU_HELP;
             menu = (AfEditorTopMenu)(menu + 1)) {
            const float x = af_editor_top_menu_x(editor, menu);
            if (event->x >= x && event->x < x + af_editor_top_menu_info(menu)->width *
                                                    af_editor_top_menu_scale(editor)) {
                af_editor_top_menu_open(editor, menu);
                return;
            }
        }
    }
    if (editor->top_menu != AF_EDITOR_MENU_NONE) {
        const float x = af_editor_top_popup_x(editor, width), y = AF_EDITOR_MENU_HEIGHT + 4;
        const uint32_t count = af_editor_top_menu_info(editor->top_menu)->item_count;
        editor->top_menu_item =
            event->x >= x && event->x < x + 340 && event->y >= y && event->y < y + (float)count * 30
                ? (uint32_t)((event->y - y) / 30)
                : UINT32_MAX;
    }
}

static bool af_editor_confirm_discard(AfEditorState *editor, const char *title) {
#ifdef __EMSCRIPTEN__
    return !editor->dirty || af_web_confirm_discard(title) != 0;
#else
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
#endif
}

/* Loading reserves by text line count; authoring needs room to add and duplicate nodes. */
static AfResult af_editor_reserve_authoring_nodes(AfEditorState *editor, AfProject *project) {
    if (project->scene.count > AF_EDITOR_PROJECT_CAPACITY)
        return AF_RESULT_LIMIT_EXCEEDED;
    if (project->scene.capacity >= AF_EDITOR_PROJECT_CAPACITY) {
        project->scene.capacity = AF_EDITOR_PROJECT_CAPACITY;
        return AF_RESULT_SUCCESS;
    }
    AfSceneNode *nodes = af_allocate_array(&editor->allocator, AF_EDITOR_PROJECT_CAPACITY,
                                           sizeof(*nodes), _Alignof(AfSceneNode));
    AfProjectText *texts = af_allocate_array(&editor->allocator, AF_EDITOR_PROJECT_CAPACITY,
                                             sizeof(*texts), _Alignof(AfProjectText));
    char (*names)[AF_PROJECT_NAME_CAPACITY] = af_allocate_array(
        &editor->allocator, AF_EDITOR_PROJECT_CAPACITY, sizeof(*names), _Alignof(char));
    if (!nodes || !texts || !names) {
        af_deallocate(&editor->allocator, nodes);
        af_deallocate(&editor->allocator, texts);
        af_deallocate(&editor->allocator, names);
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memcpy(nodes, project->scene.nodes, (size_t)project->scene.count * sizeof(*nodes));
    memcpy(texts, project->texts, (size_t)project->scene.count * sizeof(*texts));
    memcpy(names, project->names, (size_t)project->scene.count * sizeof(*names));
    af_deallocate(&editor->allocator, project->scene.nodes);
    af_deallocate(&editor->allocator, project->texts);
    af_deallocate(&editor->allocator, project->names);
    project->scene.nodes = nodes;
    project->texts = texts;
    project->names = names;
    project->scene.capacity = AF_EDITOR_PROJECT_CAPACITY;
    return AF_RESULT_SUCCESS;
}

static AfResult af_editor_adopt_project(AfEditorState *editor, AfProject *project, const char *path,
                                        bool dirty) {
    if (editor == NULL || project == NULL || project->scene.count > AF_EDITOR_PROJECT_CAPACITY ||
        af_project_validate(project) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    const AfResult reserve = af_editor_reserve_authoring_nodes(editor, project);
    if (reserve != AF_RESULT_SUCCESS)
        return reserve;
    af_editor_collaboration_release(editor);
    af_project_destroy(&editor->project);
    editor->project = *project;
    /* Scene data references belong to the new owner, not the temporary project. */
    editor->project.scene.paths = &editor->project.path_data;
    editor->project.scene.styles = &editor->project.style_data;
    editor->project.scene.layouts = &editor->project.layout_data;
    editor->project.scene.particles = &editor->project.particle_data;
    editor->project.scene.images = &editor->project.image_data;
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
    editor->frames_visible = false;
    editor->playhead = 0;
    editor->timeline_pan = 0;
    editor->selected_clip_id = editor->project.clip_count == 0U ? 0U : editor->project.clips[0].id;
    for (uint32_t i = 0; i < editor->project.track_count; ++i)
        if (editor->project.tracks[i].cel_length) {
            editor->selected_clip_id = editor->project.tracks[i].clip_id;
            af_editor_toggle_frames(editor);
            break;
        }
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
#ifdef __EMSCRIPTEN__
        af_web_remember_project(editor->project_path);
#endif
    }
    editor->dirty = dirty;
    af_editor_project_changed(editor);
    return AF_RESULT_SUCCESS;
}

static AfResult af_editor_new_project(AfEditorState *editor) {
    AfEditorState staging = {0};
    AfResult result = AF_RESULT_SUCCESS;
    if (!af_editor_confirm_discard(editor, "Create a new project?")) {
        return AF_RESULT_SUCCESS;
    }
    result = af_project_create(&staging.project, &editor->allocator, AF_EDITOR_PROJECT_CAPACITY);
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_default_project(&staging);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_adopt_project(editor, &staging.project, NULL, false);
    }
    af_project_destroy(&staging.project);
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
#ifdef __EMSCRIPTEN__
    af_web_task_begin("Opening project…");
#endif
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
#ifdef __EMSCRIPTEN__
    af_web_task_end(result == AF_RESULT_SUCCESS);
#endif
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

static bool af_editor_export_path_with_extension(const char *path, const char *extension,
                                                 char output[], size_t capacity) {
    const char *current_extension = NULL;
    size_t length = 0U;
    size_t extension_length = 0U;
    if (path == NULL || path[0] == '\0' || extension == NULL || extension[0] != '.' ||
        output == NULL || capacity == 0U) {
        return false;
    }
    length = strlen(path);
    extension_length = strlen(extension);
    current_extension = strrchr(path, '.');
    if (current_extension != NULL && strcmp(current_extension, extension) == 0) {
        if (length >= capacity) {
            return false;
        }
        memcpy(output, path, length + 1U);
        return true;
    }
    if (length >= capacity || extension_length >= capacity - length) {
        return false;
    }
    memcpy(output, path, length);
    memcpy(output + length, extension, extension_length + 1U);
    return true;
}

static void af_editor_export_default_path(const AfEditorState *editor, const char *extension,
                                          char output[], size_t capacity) {
    const char *slash = NULL;
    const char *backslash = NULL;
    const char *dot = NULL;
    size_t prefix = 0U;
    const size_t extension_length = strlen(extension);
    if (editor->project_path[0] == '\0') {
        (void)snprintf(output, capacity, "untitled%s", extension);
        return;
    }
    prefix = strlen(editor->project_path);
    slash = strrchr(editor->project_path, '/');
    backslash = strrchr(editor->project_path, '\\');
    dot = strrchr(editor->project_path, '.');
    if (dot != NULL && (slash == NULL || dot > slash) && (backslash == NULL || dot > backslash)) {
        prefix = (size_t)(dot - editor->project_path);
    }
    if (prefix >= capacity || extension_length >= capacity - prefix) {
        (void)snprintf(output, capacity, "untitled%s", extension);
        return;
    }
    memcpy(output, editor->project_path, prefix);
    memcpy(output + prefix, extension, extension_length + 1U);
}

static AfResult af_editor_export_package_path(AfEditorState *editor, const char *path) {
    AfPackageBlob package = {0};
    char status[AF_EDITOR_STATUS_CAPACITY];
    AfResult result = af_project_compile_release(&editor->project, &editor->allocator, &package);
    if (result == AF_RESULT_SUCCESS) {
        result = af_package_validate(package.data, package.size);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_file_write_atomic(path, package.data, package.size, 1U);
    }
    if (result == AF_RESULT_SUCCESS) {
        (void)snprintf(status, sizeof(status), "Runtime package exported (%zu KB)",
                       (package.size + 1023U) / 1024U);
        af_editor_status(editor, status);
    } else {
        af_editor_status(editor, "Runtime package export failed; destination was not replaced");
    }
    af_package_blob_destroy(&editor->allocator, &package);
    return result;
}

static AfResult af_editor_export_svg_path(AfEditorState *editor, const char *path) {
    AfInterchangeDiagnostics diagnostics = {0};
    char status[AF_EDITOR_STATUS_CAPACITY];
    char *output = NULL;
    size_t capacity = 256U * 1024U;
    size_t output_size = 0U;
    AfResult result = AF_RESULT_LIMIT_EXCEEDED;
    af_editor_update_preview(editor);
    while (capacity <= AF_PROJECT_MAX_FILE_SIZE) {
        output = af_allocate(&editor->allocator, capacity, _Alignof(char));
        if (output == NULL) {
            result = AF_RESULT_OUT_OF_MEMORY;
            break;
        }
        af_interchange_diagnostics_clear(&diagnostics);
        result =
            af_interchange_export_static_svg(&editor->preview_scene, editor->project.canvas_size,
                                             output, capacity, &output_size, &diagnostics);
        if (result != AF_RESULT_LIMIT_EXCEEDED || capacity == AF_PROJECT_MAX_FILE_SIZE) {
            break;
        }
        af_deallocate(&editor->allocator, output);
        output = NULL;
        capacity =
            capacity > AF_PROJECT_MAX_FILE_SIZE / 2U ? AF_PROJECT_MAX_FILE_SIZE : capacity * 2U;
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_file_write_atomic(path, output, output_size, 1U);
    }
    if (result == AF_RESULT_SUCCESS && diagnostics.warning_count != 0U) {
        (void)snprintf(status, sizeof(status),
                       "Static SVG exported with %u compatibility warning%s",
                       diagnostics.warning_count, diagnostics.warning_count == 1U ? "" : "s");
        af_editor_status(editor, status);
    } else if (result == AF_RESULT_SUCCESS) {
        af_editor_status(editor, "Static SVG exported at the current playhead frame");
    } else {
        af_editor_status(editor, "Static SVG export failed; destination was not replaced");
    }
    if (output != NULL) {
        af_deallocate(&editor->allocator, output);
    }
    return result;
}

static AfResult af_editor_build_lottie(AfEditorState *editor, char **out_output,
                                       size_t *out_output_size,
                                       AfInterchangeDiagnostics *diagnostics) {
    char *output = NULL;
    size_t capacity = 256U * 1024U;
    size_t output_size = 0U;
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    if (editor == NULL || out_output == NULL || out_output_size == NULL) {
        return result;
    }
    *out_output = NULL;
    *out_output_size = 0U;
    if (af_project_find_clip_const(&editor->project, editor->selected_clip_id) == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    while (capacity <= AF_PROJECT_MAX_FILE_SIZE) {
        output = af_allocate(&editor->allocator, capacity, _Alignof(char));
        if (output == NULL) {
            return AF_RESULT_OUT_OF_MEMORY;
        }
        af_interchange_diagnostics_clear(diagnostics);
        result = af_interchange_export_lottie_json(
            &editor->project, editor->selected_clip_id,
            &(AfLottieExportOptions){.frame_rate = AF_INTERCHANGE_DEFAULT_FRAME_RATE}, output,
            capacity, &output_size, diagnostics);
        if (result != AF_RESULT_LIMIT_EXCEEDED || capacity == AF_PROJECT_MAX_FILE_SIZE) {
            break;
        }
        af_deallocate(&editor->allocator, output);
        output = NULL;
        capacity =
            capacity > AF_PROJECT_MAX_FILE_SIZE / 2U ? AF_PROJECT_MAX_FILE_SIZE : capacity * 2U;
    }
    if (result == AF_RESULT_SUCCESS) {
        *out_output = output;
        *out_output_size = output_size;
    } else if (output != NULL) {
        af_deallocate(&editor->allocator, output);
    }
    return result;
}

static AfResult af_editor_export_lottie_path(AfEditorState *editor, const char *path,
                                             bool dotlottie) {
    AfInterchangeDiagnostics diagnostics = {0};
    char status[AF_EDITOR_STATUS_CAPACITY];
    char *lottie = NULL;
    size_t lottie_size = 0U;
    uint8_t *archive = NULL;
    size_t archive_capacity = 0U;
    size_t archive_size = 0U;
    AfResult result = af_editor_build_lottie(editor, &lottie, &lottie_size, &diagnostics);
    if (result == AF_RESULT_SUCCESS && dotlottie) {
        const size_t block_overhead = ((lottie_size + 65534U) / 65535U + 4U) * 5U;
        if (lottie_size > SIZE_MAX - block_overhead - 2048U) {
            result = AF_RESULT_LIMIT_EXCEEDED;
        } else {
            archive_capacity = lottie_size + block_overhead + 2048U;
            archive = af_allocate(&editor->allocator, archive_capacity, _Alignof(uint8_t));
            if (archive == NULL) {
                result = AF_RESULT_OUT_OF_MEMORY;
            }
        }
        if (result == AF_RESULT_SUCCESS) {
            result = af_interchange_pack_dotlottie(lottie, lottie_size, archive, archive_capacity,
                                                   &archive_size, &diagnostics);
        }
    }
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_file_write_atomic(path, dotlottie ? (const void *)archive : (const void *)lottie,
                                 dotlottie ? archive_size : lottie_size, 1U);
    }
    if (result == AF_RESULT_SUCCESS && diagnostics.warning_count != 0U) {
        (void)snprintf(status, sizeof(status), "%s exported with %u compatibility warning%s",
                       dotlottie ? "dotLottie" : "Lottie JSON", diagnostics.warning_count,
                       diagnostics.warning_count == 1U ? "" : "s");
        af_editor_status(editor, status);
    } else if (result == AF_RESULT_SUCCESS) {
        af_editor_status(editor, dotlottie ? "dotLottie animation exported"
                                           : "Lottie JSON animation exported");
    } else if (result == AF_RESULT_NOT_FOUND) {
        af_editor_status(editor, "Create or select an animation clip before exporting");
    } else {
        af_editor_status(editor, dotlottie ? "dotLottie export failed; destination was not replaced"
                                           : "Lottie export failed; destination was not replaced");
    }
    if (archive != NULL) {
        af_deallocate(&editor->allocator, archive);
    }
    if (lottie != NULL) {
        af_deallocate(&editor->allocator, lottie);
    }
    return result;
}

static void af_editor_remove_export_frames(const char *prefix, uint32_t frame_count) {
    char frame_path[AF_EDITOR_PATH_CAPACITY];
    if (prefix == NULL) {
        return;
    }
    for (uint32_t frame = 0U; frame < frame_count; ++frame) {
        if (snprintf(frame_path, sizeof(frame_path), "%s%06u.svg", prefix, frame) > 0) {
            (void)remove(frame_path);
        }
    }
}

static bool af_editor_ffmpeg_sequence_pattern(const char *prefix, char *output, size_t capacity) {
    size_t position = 0U;
    if (prefix == NULL || output == NULL || capacity == 0U) {
        return false;
    }
    for (size_t index = 0U; prefix[index] != '\0'; ++index) {
        const size_t bytes = prefix[index] == '%' ? 2U : 1U;
        if (bytes >= capacity - position) {
            return false;
        }
        output[position++] = prefix[index];
        if (prefix[index] == '%') {
            output[position++] = '%';
        }
    }
    if (9U >= capacity - position) {
        return false;
    }
    memcpy(output + position, "%06d.svg", 9U);
    return true;
}

static AfResult af_editor_export_media_path(AfEditorState *editor, const char *path, bool gif) {
    const AfProjectClip *clip =
        af_project_find_clip_const(&editor->project, editor->selected_clip_id);
    const float frame_rate = 30.0F;
    const float previous_playhead = editor->playhead;
    const bool previous_playing = editor->preview_playing;
    const uint64_t unique = SDL_GetTicksNS();
    char prefix[AF_EDITOR_PATH_CAPACITY];
    char pattern[AF_EDITOR_PATH_CAPACITY];
    char frame_path[AF_EDITOR_PATH_CAPACITY];
    char temporary_output[AF_EDITOR_PATH_CAPACITY];
    char frame_rate_text[24];
    char *svg = NULL;
    size_t svg_capacity = 512U * 1024U;
    size_t svg_size = 0U;
    uint32_t frame_count = 0U;
    uint32_t written_frames = 0U;
    bool compatibility_warning = false;
    AfResult result = AF_RESULT_SUCCESS;
    if (clip == NULL) {
        af_editor_status(editor, "Create or select an animation clip before exporting");
        return AF_RESULT_NOT_FOUND;
    }
    if (!isfinite(clip->duration) || clip->duration < 0.0F ||
        ceilf(clip->duration * frame_rate) > (float)AF_MAX_IMAGE_SEQUENCE_FRAMES) {
        af_editor_status(editor, "Media export exceeds the 2,048 frame safety limit");
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    frame_count = (uint32_t)fmaxf(1.0F, ceilf(clip->duration * frame_rate));
    {
        const int prefix_length = snprintf(prefix, sizeof(prefix), "%s.arcflow-%llu-frame-", path,
                                           (unsigned long long)unique);
        const int temporary_length =
            snprintf(temporary_output, sizeof(temporary_output), "%s.arcflow-%llu.tmp", path,
                     (unsigned long long)unique);
        if (prefix_length < 0 || (size_t)prefix_length >= sizeof(prefix) || temporary_length < 0 ||
            (size_t)temporary_length >= sizeof(temporary_output) ||
            strlen(prefix) + 10U >= sizeof(frame_path) ||
            !af_editor_ffmpeg_sequence_pattern(prefix, pattern, sizeof(pattern))) {
            af_editor_status(editor, "Export path is too long for temporary frames");
            return AF_RESULT_LIMIT_EXCEEDED;
        }
    }
    svg = af_allocate(&editor->allocator, svg_capacity, _Alignof(char));
    if (svg == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    editor->preview_playing = false;
    for (uint32_t frame = 0U; frame < frame_count; ++frame) {
        AfInterchangeDiagnostics diagnostics = {0};
        editor->playhead = fminf(clip->duration, (float)frame / frame_rate);
        af_editor_update_preview(editor);
        for (;;) {
            result = af_interchange_export_static_svg(&editor->preview_scene,
                                                      editor->project.canvas_size, svg,
                                                      svg_capacity, &svg_size, &diagnostics);
            if (result != AF_RESULT_LIMIT_EXCEEDED || svg_capacity == AF_PROJECT_MAX_FILE_SIZE) {
                break;
            }
            af_deallocate(&editor->allocator, svg);
            svg_capacity = svg_capacity > AF_PROJECT_MAX_FILE_SIZE / 2U ? AF_PROJECT_MAX_FILE_SIZE
                                                                        : svg_capacity * 2U;
            svg = af_allocate(&editor->allocator, svg_capacity, _Alignof(char));
            if (svg == NULL) {
                result = AF_RESULT_OUT_OF_MEMORY;
                break;
            }
            af_interchange_diagnostics_clear(&diagnostics);
        }
        if (result != AF_RESULT_SUCCESS ||
            snprintf(frame_path, sizeof(frame_path), "%s%06u.svg", prefix, frame) < 0) {
            break;
        }
        result = af_file_write(frame_path, svg, svg_size);
        if (result != AF_RESULT_SUCCESS) {
            break;
        }
        compatibility_warning = compatibility_warning || diagnostics.warning_count != 0U;
        ++written_frames;
    }
    editor->playhead = previous_playhead;
    editor->preview_playing = previous_playing;
    af_editor_update_preview(editor);
    if (result == AF_RESULT_SUCCESS) {
        SDL_Process *process = NULL;
        int exit_code = -1;
        (void)snprintf(frame_rate_text, sizeof(frame_rate_text), "%.9g", (double)frame_rate);
        if (gif) {
            const char *const arguments[] = {
                "ffmpeg",
                "-hide_banner",
                "-loglevel",
                "error",
                "-nostdin",
                "-y",
                "-framerate",
                frame_rate_text,
                "-start_number",
                "0",
                "-i",
                pattern,
                "-filter_complex",
                "split[frames][palette];[palette]palettegen=reserve_transparent=1[p];"
                "[frames][p]paletteuse=dither=sierra2_4a",
                "-loop",
                "0",
                "-f",
                "gif",
                temporary_output,
                NULL};
            process = SDL_CreateProcess(arguments, false);
        } else {
            const char *const arguments[] = {
                "ffmpeg",
                "-hide_banner",
                "-loglevel",
                "error",
                "-nostdin",
                "-y",
                "-framerate",
                frame_rate_text,
                "-start_number",
                "0",
                "-i",
                pattern,
                "-an",
                "-vf",
                "scale=trunc(iw/2)*2:trunc(ih/2)*2:flags=lanczos,format=yuv420p",
                "-c:v",
                "libx264",
                "-preset",
                "medium",
                "-crf",
                "18",
                "-movflags",
                "+faststart",
                "-f",
                "mp4",
                temporary_output,
                NULL};
            process = SDL_CreateProcess(arguments, false);
        }
        if (process == NULL) {
            result = AF_RESULT_UNSUPPORTED_FEATURE;
        } else {
            (void)SDL_WaitProcess(process, true, &exit_code);
            SDL_DestroyProcess(process);
            if (exit_code != 0) {
                result = AF_RESULT_IO_ERROR;
            }
        }
        if (result == AF_RESULT_SUCCESS && !SDL_RenamePath(temporary_output, path)) {
            result = AF_RESULT_IO_ERROR;
        }
    }
    (void)remove(temporary_output);
    af_editor_remove_export_frames(prefix, written_frames);
    if (svg != NULL) {
        af_deallocate(&editor->allocator, svg);
    }
    if (result == AF_RESULT_SUCCESS && compatibility_warning) {
        af_editor_status(editor, gif ? "GIF exported; unsupported vector effects were omitted"
                                     : "MP4 exported; unsupported vector effects were omitted");
    } else if (result == AF_RESULT_SUCCESS) {
        af_editor_status(editor, gif ? "Animated GIF exported" : "H.264 MP4 exported");
    } else if (result == AF_RESULT_UNSUPPORTED_FEATURE) {
        af_editor_status(editor, "FFmpeg was not found; install it to export GIF or MP4");
    } else {
        af_editor_status(editor, gif ? "GIF export failed; destination was not replaced"
                                     : "MP4 export failed; destination was not replaced");
    }
    return result;
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
#ifdef __EMSCRIPTEN__
        {"Supported Images", "png;jpg;jpeg;ktx2;svg;webp;avif;gif;bmp"},
#else
        {"Supported Images", "png;jpg;jpeg;ktx2;svg"},
#endif
        {"PNG", "png"},
        {"JPEG", "jpg;jpeg"},
        {"KTX2", "ktx2"},
        {"SVG", "svg"}};
    static const SDL_DialogFileFilter update_filters[] = {{"Arcflow Update Manifest", "afupdate"}};
    static const SDL_DialogFileFilter package_filters[] = {{"Arcflow Runtime Package", "afp"}};
    static const SDL_DialogFileFilter svg_filters[] = {{"Scalable Vector Graphics", "svg"}};
    static const SDL_DialogFileFilter lottie_filters[] = {{"Lottie JSON Animation", "json"}};
    static const SDL_DialogFileFilter dotlottie_filters[] = {{"dotLottie Animation", "lottie"}};
    static const SDL_DialogFileFilter gif_filters[] = {{"Animated GIF", "gif"}};
    static const SDL_DialogFileFilter mp4_filters[] = {{"MPEG-4 Video", "mp4"}};
    if (editor->dialog_active || action == AF_EDITOR_DIALOG_NONE) {
        return;
    }
#ifdef __EMSCRIPTEN__
    if (action == AF_EDITOR_DIALOG_EXPORT_GIF || action == AF_EDITOR_DIALOG_EXPORT_MP4) {
        af_editor_status(editor, "GIF and MP4 export require the desktop app with FFmpeg");
        return;
    }
#endif
    if (action == AF_EDITOR_DIALOG_OPEN_PROJECT &&
        !af_editor_confirm_discard(editor, "Open another project?")) {
        return;
    }
    editor->dialog_active = true;
    editor->dialog_action = action;
    if (action == AF_EDITOR_DIALOG_OPEN_PROJECT) {
        (void)af_platform_show_open_file_dialog(
            &editor->window, af_editor_project_dialog_callback, editor, filters, 1,
            editor->project_path[0] == '\0' ? NULL : editor->project_path, false);
    } else if (action == AF_EDITOR_DIALOG_SAVE_PROJECT_AS) {
        (void)af_platform_show_save_file_dialog(
            &editor->window, af_editor_project_dialog_callback, editor, filters, 1,
            editor->project_path[0] == '\0' ? "untitled.afproj" : editor->project_path);
    } else if (action == AF_EDITOR_DIALOG_UPDATE_MANIFEST) {
        (void)af_platform_show_open_file_dialog(&editor->window, af_editor_project_dialog_callback,
                                                editor, update_filters, 1, NULL, false);
    } else if (action == AF_EDITOR_DIALOG_EXPORT_PACKAGE) {
        char default_path[AF_EDITOR_PATH_CAPACITY];
        af_editor_export_default_path(editor, ".afp", default_path, sizeof(default_path));
        (void)af_platform_show_save_file_dialog(&editor->window, af_editor_project_dialog_callback,
                                                editor, package_filters, 1, default_path);
    } else if (action == AF_EDITOR_DIALOG_EXPORT_SVG) {
        char default_path[AF_EDITOR_PATH_CAPACITY];
        af_editor_export_default_path(editor, ".svg", default_path, sizeof(default_path));
        (void)af_platform_show_save_file_dialog(&editor->window, af_editor_project_dialog_callback,
                                                editor, svg_filters, 1, default_path);
    } else if (action == AF_EDITOR_DIALOG_EXPORT_LOTTIE) {
        char default_path[AF_EDITOR_PATH_CAPACITY];
        af_editor_export_default_path(editor, ".json", default_path, sizeof(default_path));
        (void)af_platform_show_save_file_dialog(&editor->window, af_editor_project_dialog_callback,
                                                editor, lottie_filters, 1, default_path);
    } else if (action == AF_EDITOR_DIALOG_EXPORT_DOTLOTTIE) {
        char default_path[AF_EDITOR_PATH_CAPACITY];
        af_editor_export_default_path(editor, ".lottie", default_path, sizeof(default_path));
        (void)af_platform_show_save_file_dialog(&editor->window, af_editor_project_dialog_callback,
                                                editor, dotlottie_filters, 1, default_path);
    } else if (action == AF_EDITOR_DIALOG_EXPORT_GIF) {
        char default_path[AF_EDITOR_PATH_CAPACITY];
        af_editor_export_default_path(editor, ".gif", default_path, sizeof(default_path));
        (void)af_platform_show_save_file_dialog(&editor->window, af_editor_project_dialog_callback,
                                                editor, gif_filters, 1, default_path);
    } else if (action == AF_EDITOR_DIALOG_EXPORT_MP4) {
        char default_path[AF_EDITOR_PATH_CAPACITY];
        af_editor_export_default_path(editor, ".mp4", default_path, sizeof(default_path));
        (void)af_platform_show_save_file_dialog(&editor->window, af_editor_project_dialog_callback,
                                                editor, mp4_filters, 1, default_path);
    } else {
        (void)af_platform_show_open_file_dialog(&editor->window, af_editor_project_dialog_callback,
                                                editor, image_filters, 5, NULL, false);
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
            char previous_path[AF_EDITOR_PATH_CAPACITY];
            char previous_package_path[AF_EDITOR_PATH_CAPACITY];
            const bool previous_external_change = editor->external_change_visible;
            (void)snprintf(previous_path, sizeof(previous_path), "%s", editor->project_path);
            (void)snprintf(previous_package_path, sizeof(previous_package_path), "%s",
                           editor->package_path);
            af_editor_collaboration_release(editor);
            (void)snprintf(editor->project_path, sizeof(editor->project_path), "%s", project_path);
            af_editor_derive_package_path(editor);
            af_editor_collaboration_track_path(editor);
            if (af_editor_save(editor) != AF_RESULT_SUCCESS) {
                af_editor_collaboration_release(editor);
                (void)snprintf(editor->project_path, sizeof(editor->project_path), "%s",
                               previous_path);
                (void)snprintf(editor->package_path, sizeof(editor->package_path), "%s",
                               previous_package_path);
                af_editor_collaboration_track_path(editor);
                editor->external_change_visible = previous_external_change;
            }
        } else {
            af_editor_status(editor, "Project path is too long");
        }
    } else if (action == AF_EDITOR_DIALOG_EXPORT_PACKAGE || action == AF_EDITOR_DIALOG_EXPORT_SVG ||
               action == AF_EDITOR_DIALOG_EXPORT_LOTTIE ||
               action == AF_EDITOR_DIALOG_EXPORT_DOTLOTTIE ||
               action == AF_EDITOR_DIALOG_EXPORT_GIF || action == AF_EDITOR_DIALOG_EXPORT_MP4) {
        (void)af_editor_export_with_options(editor, path, action);
    } else if (action == AF_EDITOR_DIALOG_REPAIR_IMAGE) {
        const AfResult result = af_project_repair_image_source(
            &editor->project, editor->project_path, editor->selected_asset_id, path);
        if (result == AF_RESULT_SUCCESS) {
            (void)af_image_build_atlases(&editor->project.image_data);
            af_editor_mark_dirty(editor);
            af_editor_status(editor, "Missing image source repaired");
        } else {
            af_editor_status(editor, "Replacement image is invalid or could not be copied");
        }
    } else if (action == AF_EDITOR_DIALOG_UPDATE_MANIFEST) {
        AfFileData manifest_file = {0};
        AfSemanticVersion current = {0};
        AfReleaseChannel channel = AF_RELEASE_CHANNEL_DEVELOPMENT;
        AfResult update_result = af_file_read_limited(path, AF_UPDATE_MANIFEST_MAX_SIZE,
                                                      &editor->allocator, &manifest_file);
        if (update_result == AF_RESULT_SUCCESS) {
            update_result = af_update_manifest_parse(manifest_file.data, manifest_file.size,
                                                     &editor->update_manifest);
        }
        if (update_result == AF_RESULT_SUCCESS) {
            update_result = af_semantic_version_parse(AF_VERSION_STRING, &current);
        }
        if (update_result == AF_RESULT_SUCCESS) {
            update_result = af_release_channel_parse(AF_RELEASE_CHANNEL, &channel);
        }
        if (update_result == AF_RESULT_SUCCESS) {
            editor->update_status = af_update_check(&editor->update_manifest, current, channel,
                                                    AF_RUNTIME_VERSION, AF_PACKAGE_FORMAT_VERSION);
            editor->update_checked = true;
        } else {
            editor->update_checked = false;
        }
        editor->update_panel_visible = true;
        af_file_data_destroy(&editor->allocator, &manifest_file);
        af_editor_status(editor, update_result == AF_RESULT_SUCCESS
                                     ? "Update manifest checked; artifact signature is required"
                                     : "Update manifest is invalid or unsupported");
    } else if (action == AF_EDITOR_DIALOG_IMPORT_IMAGE) {
        uint64_t asset_id = 0U;
        uint64_t node_id = 0U;
        const char *image_project_path = editor->project_path;
#ifdef __EMSCRIPTEN__
        if (image_project_path[0] == '\0')
            image_project_path = "/documents/untitled.afproj";
        char *decoded_path = af_web_prepare_image(path);
        if (!decoded_path) {
            af_editor_status(editor, "Image could not be decoded or is too large");
            SDL_free(path);
            return;
        }
        SDL_free(path);
        path = decoded_path;
#endif
        const AfResult result = af_project_import_image(&editor->project, image_project_path, path,
                                                        &asset_id, &node_id);
        if (result == AF_RESULT_SUCCESS) {
            editor->selected_asset_id = asset_id;
            editor->asset_browser_visible = true;
            af_editor_select_only(editor, node_id);
            af_editor_mark_dirty(editor);
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
    editor->selected_count = 0;
    for (uint32_t i = 0; i < editor->project.scene.count &&
                         editor->selected_count < AF_EDITOR_COMMAND_MAX_SNAPSHOTS;
         ++i) {
        const AfSceneNode *node = &editor->project.scene.nodes[i];
        const AfProjectTrack *cel = af_editor_frame_owner(&editor->project, node->id);
        if (!af_editor_frame_node_active(editor, node->id) || (cel && cel->node_id == node->id))
            continue;
        editor->selected_ids[editor->selected_count++] = node->id;
    }
    editor->selected_id =
        editor->selected_count ? editor->selected_ids[editor->selected_count - 1] : 0;
    af_editor_status(editor, "Visible drawing objects selected");
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
    case AF_EDITOR_MENU_ACTION_RELOAD_PROJECT:
        if (editor->project_path[0] == '\0') {
            af_editor_status(editor, "Save the project before reloading");
        } else if (af_editor_confirm_discard(editor, "Reload project from disk?")) {
            char reload_path[AF_EDITOR_PATH_CAPACITY];
            (void)snprintf(reload_path, sizeof(reload_path), "%s", editor->project_path);
            (void)af_editor_open_project_path(editor, reload_path);
        }
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
    case AF_EDITOR_MENU_ACTION_EXPORT:
        af_editor_open_export(editor);
        break;
    case AF_EDITOR_MENU_ACTION_EXIT:
        if (!editor->dialog_active && af_editor_confirm_discard(editor, "Exit Arcflow Editor?")) {
            editor->running = false;
        }
        break;
    case AF_EDITOR_MENU_ACTION_UNDO:
        if (af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS) {
            af_editor_mark_dirty(editor);
            af_editor_reconcile_selection(editor);
            af_editor_status(editor, "Undo");
        }
        break;
    case AF_EDITOR_MENU_ACTION_REDO:
        if (af_editor_history_redo(&editor->history, &editor->project) == AF_RESULT_SUCCESS) {
            af_editor_mark_dirty(editor);
            af_editor_reconcile_selection(editor);
            af_editor_status(editor, "Redo");
        }
        break;
    case AF_EDITOR_MENU_ACTION_DUPLICATE:
        if (editor->frames_visible && editor->timeline_focus)
            af_editor_frame_new(editor, true);
        else if (editor->timeline_focus)
            (void)af_editor_paste_keyframes(editor, true);
        else
            (void)af_editor_duplicate_selection(editor);
        break;
    case AF_EDITOR_MENU_ACTION_RENAME:
        af_editor_begin_rename(editor);
        break;
    case AF_EDITOR_MENU_ACTION_DELETE:
        if (editor->frames_visible && editor->timeline_focus)
            af_editor_frame_delete(editor);
        else if (editor->timeline_focus)
            (void)af_editor_delete_keyframes(editor);
        else
            (void)af_editor_delete_selection(editor);
        break;
    case AF_EDITOR_MENU_ACTION_SELECT_ALL:
        if (editor->timeline_focus) {
            const AfProjectTrack *selected = af_editor_selected_track(editor);
            af_editor_clear_key_selection(editor);
            for (uint32_t i = 0; i < editor->project.keyframe_count; ++i) {
                const AfProjectKeyframe *key = &editor->project.keyframes[i];
                const AfProjectTrack *track =
                    af_project_find_track_const(&editor->project, key->track_id);
                if (track != NULL && !track->cel_length &&
                    track->clip_id == editor->selected_clip_id &&
                    (!editor->curve_visible || (selected != NULL && track->id == selected->id)))
                    af_editor_select_keyframe(editor, key->id, true);
            }
        } else
            af_editor_select_all(editor);
        break;
    case AF_EDITOR_MENU_ACTION_GRID:
        editor->grid_visible = !editor->grid_visible;
        break;
    case AF_EDITOR_MENU_ACTION_COMBINE:
        af_editor_compound_selection(editor, false);
        break;
    case AF_EDITOR_MENU_ACTION_WELD:
        af_editor_boolean_selection(editor, AF_PATH_BOOLEAN_UNION);
        break;
    case AF_EDITOR_MENU_ACTION_TRIM:
        af_editor_boolean_selection(editor, AF_PATH_BOOLEAN_SUBTRACT);
        break;
    case AF_EDITOR_MENU_ACTION_INTERSECT:
        af_editor_boolean_selection(editor, AF_PATH_BOOLEAN_INTERSECT);
        break;
    case AF_EDITOR_MENU_ACTION_BREAK_APART:
        af_editor_compound_selection(editor, true);
        break;
    case AF_EDITOR_MENU_ACTION_JOIN_ENDPOINTS:
        af_editor_join_endpoints(editor);
        break;
    case AF_EDITOR_MENU_ACTION_CONTINUE_PATH:
        af_editor_continue_path(editor);
        break;
    case AF_EDITOR_MENU_ACTION_OUTLINE_OBJECT:
        (void)af_editor_expand_outline(editor, false, 0.0F);
        break;
    case AF_EDITOR_MENU_ACTION_OFFSET:
        if (editor->selected_count == 1U && editor->tool_command_count == 0U) {
            editor->offset_panel_open = true;
            editor->offset_replace = true;
            (void)snprintf(editor->offset_buffer, sizeof(editor->offset_buffer), "8");
        } else
            af_editor_status(editor, "Select one closed shape and finish drawing first");
        break;
    case AF_EDITOR_MENU_ACTION_TEXT:
        af_editor_open_text_panel(editor, width, height);
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
    case AF_EDITOR_MENU_ACTION_FULLSCREEN: {
        bool fullscreen = false;
        const AfResult fullscreen_result =
            af_platform_window_toggle_fullscreen(&editor->window, &fullscreen);
        af_editor_status(editor, fullscreen_result == AF_RESULT_SUCCESS
                                     ? (fullscreen ? "Fullscreen enabled" : "Fullscreen disabled")
                                     : "Fullscreen transition failed");
        break;
    }
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
        editor->frames_visible = false;
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
#ifndef __EMSCRIPTEN__
        if (editor->project_path[0] == '\0') {
            af_editor_status(editor, "Save the project before importing images");
        } else {
#endif
            af_editor_show_project_dialog(editor, AF_EDITOR_DIALOG_IMPORT_IMAGE);
#ifndef __EMSCRIPTEN__
        }
#endif
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
    case AF_EDITOR_MENU_ACTION_NEW_FROM_TEMPLATE:
    case AF_EDITOR_MENU_ACTION_OPEN_RECENT:
    case AF_EDITOR_MENU_ACTION_COPY_OBJECTS:
    case AF_EDITOR_MENU_ACTION_PASTE_OBJECTS:
    case AF_EDITOR_MENU_ACTION_FIND_HIERARCHY:
    case AF_EDITOR_MENU_ACTION_COMMAND_PALETTE:
    case AF_EDITOR_MENU_ACTION_ALIGN_LEFT:
    case AF_EDITOR_MENU_ACTION_ALIGN_CENTER_X:
    case AF_EDITOR_MENU_ACTION_ALIGN_RIGHT:
    case AF_EDITOR_MENU_ACTION_ALIGN_TOP:
    case AF_EDITOR_MENU_ACTION_ALIGN_CENTER_Y:
    case AF_EDITOR_MENU_ACTION_ALIGN_BOTTOM:
    case AF_EDITOR_MENU_ACTION_DISTRIBUTE_HORIZONTAL:
    case AF_EDITOR_MENU_ACTION_DISTRIBUTE_VERTICAL:
    case AF_EDITOR_MENU_ACTION_ADD_VERTICAL_GUIDE:
    case AF_EDITOR_MENU_ACTION_ADD_HORIZONTAL_GUIDE:
    case AF_EDITOR_MENU_ACTION_CLEAR_GUIDES:
    case AF_EDITOR_MENU_ACTION_TOGGLE_RULERS:
    case AF_EDITOR_MENU_ACTION_TOGGLE_MEASUREMENTS:
    case AF_EDITOR_MENU_ACTION_TOGGLE_SMART_GUIDES:
    case AF_EDITOR_MENU_ACTION_PROFILER:
    case AF_EDITOR_MENU_ACTION_VALIDATION:
    case AF_EDITOR_MENU_ACTION_SYSTEM_INFO:
    case AF_EDITOR_MENU_ACTION_AUTOSAVE:
    case AF_EDITOR_MENU_ACTION_RECOVERY:
    case AF_EDITOR_MENU_ACTION_RESET_WORKSPACE:
    case AF_EDITOR_MENU_ACTION_PRESET_LIBRARY:
    case AF_EDITOR_MENU_ACTION_APPEARANCE:
        af_editor_execute_workflow_action(editor, action, width, height);
        break;
    case AF_EDITOR_MENU_ACTION_WELCOME:
        af_editor_onboarding_show_welcome(editor);
        break;
    case AF_EDITOR_MENU_ACTION_GUIDED_PROJECT:
        af_editor_onboarding_start_guided(editor);
        break;
    case AF_EDITOR_MENU_ACTION_EXAMPLE_BROWSER:
        af_editor_onboarding_show_examples(editor);
        break;
    case AF_EDITOR_MENU_ACTION_BUTTON_GUIDE:
        af_editor_onboarding_show_guides(editor, AF_ONBOARDING_QUICK_START);
        break;
    case AF_EDITOR_MENU_ACTION_ABOUT:
        editor->help_overlay = true;
        break;
    case AF_EDITOR_MENU_ACTION_SHORTCUTS:
        editor->shortcuts_overlay = true;
        editor->shortcut_capture = false;
        break;
    case AF_EDITOR_MENU_ACTION_CHECK_UPDATES:
        editor->update_panel_visible = true;
        af_editor_show_project_dialog(editor, AF_EDITOR_DIALOG_UPDATE_MANIFEST);
        break;
    case AF_EDITOR_MENU_ACTION_NONE:
    case AF_EDITOR_MENU_ACTION_COUNT:
        break;
    }
}

static void af_editor_render_help_overlay(AfEditorState *editor, int width, int height) {
    static const char *const link_labels[] = {"OPEN SHORTCUTS", "OPEN COMMAND PALETTE",
                                              "OPEN RECOVERY", "OPEN VALIDATION"};
    const float box_width = 620.0F;
    const float box_height = 390.0F;
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
                       "FILE: create, open, save, compile and export projects", 1.0F,
                       box_width - 44.0F, (AfColor){0.83F, 0.85F, 0.9F, 1.0F}, 220010);
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
    af_editor_add_text(editor, x + 22.0F, y + 203.0F,
                       "WORKSPACE + TOOLS: panels, guides, alignment and validation", 0.92F,
                       box_width - 44.0F, (AfColor){0.83F, 0.85F, 0.9F, 1.0F}, 220010);
    af_editor_add_text(editor, x + 22.0F, y + 237.0F, "QUICK LINKS", 0.78F, box_width - 44.0F,
                       (AfColor){0.5F, 0.68F, 0.9F, 1.0F}, 220010);
    for (uint32_t index = 0U; index < 4U; ++index) {
        const float button_x = x + 22.0F + (float)(index % 2U) * 288.0F;
        const float button_y = y + 258.0F + (float)(index / 2U) * 38.0F;
        af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = button_x, .ty = button_y},
                            (AfVec2){272.0F, 31.0F}, (AfColor){0.15F, 0.29F, 0.48F, 1.0F}, 4.0F,
                            220010);
        af_editor_add_text(editor, button_x + 14.0F, button_y + 10.0F, link_labels[index], 0.7F,
                           244.0F, (AfColor){0.86F, 0.91F, 0.98F, 1.0F}, 220020);
    }
    af_editor_add_text(editor, x + 22.0F, y + 352.0F,
                       "Hover controls for help. Hold ALT for detailed help. ESC closes this.",
                       0.9F, box_width - 44.0F, (AfColor){0.58F, 0.63F, 0.72F, 1.0F}, 220010);
}

static bool af_editor_top_menu_key(AfEditorState *editor, int width, int height,
                                   const SDL_KeyboardEvent *event) {
    if (editor->top_menu == AF_EDITOR_MENU_NONE)
        return false;
    const AfEditorTopMenuInfo *info = af_editor_top_menu_info(editor->top_menu);
    if (event->key == SDLK_ESCAPE)
        editor->top_menu = AF_EDITOR_MENU_NONE;
    else if (event->key == SDLK_LEFT || event->key == SDLK_RIGHT) {
        const uint32_t count = (uint32_t)AF_EDITOR_MENU_HELP;
        af_editor_top_menu_open(editor,
                                (AfEditorTopMenu)(1 + ((uint32_t)editor->top_menu - 1 +
                                                       (event->key == SDLK_LEFT ? count - 1 : 1)) %
                                                          count));
        editor->top_menu_item = 0;
    } else if (event->key == SDLK_DOWN || event->key == SDLK_UP) {
        const uint32_t count = info->item_count;
        editor->top_menu_item =
            editor->top_menu_item >= count
                ? (event->key == SDLK_DOWN ? 0 : count - 1)
                : (editor->top_menu_item + (event->key == SDLK_DOWN ? 1 : count - 1)) % count;
    } else if (event->key == SDLK_HOME)
        editor->top_menu_item = 0;
    else if (event->key == SDLK_END)
        editor->top_menu_item = info->item_count - 1;
    else if ((event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER) &&
             editor->top_menu_item < info->item_count) {
        const AfEditorMenuAction action = info->items[editor->top_menu_item].action;
        editor->top_menu = AF_EDITOR_MENU_NONE;
        af_editor_execute_menu_action(editor, action, width, height);
    } else if (event->mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) {
        editor->top_menu = AF_EDITOR_MENU_NONE;
        return false; /* Keep standard shortcuts usable while a menu is open. */
    }
    return true;
}

static void af_editor_render_top_menu(AfEditorState *editor, int width, int height) {
    const float right = (float)width - editor->right_width;
    const AfColor text_color = AF_UI_TEXT;
    af_editor_add_panel(editor, editor->left_width, 0, right - editor->left_width,
                        AF_EDITOR_MENU_HEIGHT, AF_UI_FIELD, 0, 210000);
    for (AfEditorTopMenu menu = AF_EDITOR_MENU_FILE; menu <= AF_EDITOR_MENU_HELP;
         menu = (AfEditorTopMenu)(menu + 1)) {
        const AfEditorTopMenuInfo *info = af_editor_top_menu_info(menu);
        const float x = af_editor_top_menu_x(editor, menu);
        const float menu_width = info->width * af_editor_top_menu_scale(editor);
        const bool hovered = editor->mouse_position.x >= x &&
                             editor->mouse_position.x < x + menu_width &&
                             editor->mouse_position.y < AF_EDITOR_MENU_HEIGHT;
        if (hovered || editor->top_menu == menu) {
            af_editor_add_rect(editor, x, 0.0F, menu_width, AF_EDITOR_MENU_HEIGHT,
                               editor->top_menu == menu ? AF_UI_SELECTION : AF_UI_SURFACE, 210010);
        }
        af_editor_add_text(editor, x + 10.0F, 9.0F, info->label, 0.95F, menu_width - 16.0F,
                           text_color, 210020);
    }
    if (editor->top_menu != AF_EDITOR_MENU_NONE) {
        const AfEditorTopMenuInfo *info = af_editor_top_menu_info(editor->top_menu);
        const float x = af_editor_top_popup_x(editor, width);
        const float popup_width = 340.0F;
        const float popup_height = 8.0F + (float)info->item_count * 30.0F;
        af_editor_add_popup(editor, x, AF_EDITOR_MENU_HEIGHT, popup_width, popup_height, 210030);
        for (uint32_t index = 0U; index < info->item_count; ++index) {
            const float item_y = AF_EDITOR_MENU_HEIGHT + 4.0F + (float)index * 30.0F;
            if (index == editor->top_menu_item) {
                af_editor_add_surface(editor, x + 4.0F, item_y, popup_width - 8.0F, 29.0F,
                                      AF_UI_SELECTION, 4, 210040);
            }
            af_editor_add_text(editor, x + 14.0F, item_y + 9.0F, info->items[index].label, 1.0F,
                               205.0F, text_color, 210050);
            af_editor_add_text(editor, x + 224.0F, item_y + 10.0F, info->items[index].shortcut,
                               0.75F, 104.0F, AF_UI_MUTED, 210050);
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
            if (x >= menu_x && x < menu_x + info->width * af_editor_top_menu_scale(editor)) {
                af_editor_top_menu_open(editor, menu);
                return true;
            }
        }
        editor->top_menu = AF_EDITOR_MENU_NONE;
        return true;
    }
    if (editor->top_menu != AF_EDITOR_MENU_NONE) {
        const AfEditorTopMenuInfo *info = af_editor_top_menu_info(editor->top_menu);
        const float menu_x = af_editor_top_popup_x(editor, width);
        const float popup_width = 340.0F;
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
        return true; /* Dismissal must not select or draw behind the popup. */
    }
    if (editor->help_overlay) {
        const float box_x = ((float)width - 620.0F) * 0.5F;
        const float box_y = ((float)height - 390.0F) * 0.5F;
        if (x >= box_x + 22.0F && x < box_x + 598.0F && y >= box_y + 258.0F && y < box_y + 327.0F) {
            const uint32_t column = (uint32_t)((x - box_x - 22.0F) / 288.0F);
            const uint32_t row = (uint32_t)((y - box_y - 258.0F) / 38.0F);
            const uint32_t link = row * 2U + column;
            editor->help_overlay = false;
            if (link == 0U) {
                editor->shortcuts_overlay = true;
                editor->shortcut_capture = false;
            } else if (link == 1U) {
                (void)af_editor_command_palette_open(&editor->command_palette,
                                                     AF_EDITOR_ACTION_CONTEXT_GLOBAL);
                (void)af_platform_text_input_set(&editor->window, true);
            } else if (link == 2U) {
                af_editor_refresh_recovery(editor, true);
            } else if (link == 3U) {
                editor->validation_visible = true;
            }
            return true;
        }
        editor->help_overlay = false;
        return true;
    }
    return false;
}
