#define AF_EDITOR_WORKFLOW_PANEL_WIDTH 336.0F
#define AF_EDITOR_WORKFLOW_PANEL_HEIGHT 214.0F
#define AF_EDITOR_WORKFLOW_OVERLAY_WIDTH 620.0F
#define AF_EDITOR_WORKFLOW_ROW_HEIGHT 30.0F
#define AF_EDITOR_WORKFLOW_PALETTE_ROWS 9U
#define AF_EDITOR_WORKFLOW_SHORTCUT_ROWS 12U

typedef struct AfEditorPaletteLayout {
    float x, y, width, height;
    float row_height;
    uint32_t rows;
} AfEditorPaletteLayout;

static AfEditorPaletteLayout af_editor_palette_layout(int width, int height) {
    AfEditorPaletteLayout layout = {
        .width = fminf(AF_EDITOR_WORKFLOW_OVERLAY_WIDTH, fmaxf(160, (float)width - 32)),
        .row_height = 46};
    layout.rows = (uint32_t)af_editor_clamp(floorf(((float)height - 180) / layout.row_height), 1,
                                            (float)AF_EDITOR_WORKFLOW_PALETTE_ROWS);
    layout.height = 148 + (float)layout.rows * layout.row_height;
    layout.x = ((float)width - layout.width) * .5F;
    layout.y = fmaxf(16, fminf(86, (float)height - layout.height - 16));
    return layout;
}

static uint32_t af_editor_shortcut_modifiers(SDL_Keymod modifiers) {
    uint32_t result = 0U;
    result |= (modifiers & SDL_KMOD_CTRL) != 0U ? AF_EDITOR_SHORTCUT_MODIFIER_CTRL : 0U;
    result |= (modifiers & SDL_KMOD_SHIFT) != 0U ? AF_EDITOR_SHORTCUT_MODIFIER_SHIFT : 0U;
    result |= (modifiers & SDL_KMOD_ALT) != 0U ? AF_EDITOR_SHORTCUT_MODIFIER_ALT : 0U;
    result |= (modifiers & SDL_KMOD_GUI) != 0U ? AF_EDITOR_SHORTCUT_MODIFIER_SUPER : 0U;
    return result;
}

static uint32_t af_editor_shortcut_key(SDL_Keycode key) {
    if (key >= SDLK_A && key <= SDLK_Z) {
        return (uint32_t)'A' + (uint32_t)(key - SDLK_A);
    }
    if (key >= SDLK_0 && key <= SDLK_9) {
        return (uint32_t)'0' + (uint32_t)(key - SDLK_0);
    }
    if (key >= SDLK_F1 && key <= SDLK_F12) {
        return AF_EDITOR_KEY_F1 + (uint32_t)(key - SDLK_F1);
    }
    switch (key) {
    case SDLK_ESCAPE:
        return AF_EDITOR_KEY_ESCAPE;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        return AF_EDITOR_KEY_ENTER;
    case SDLK_TAB:
        return AF_EDITOR_KEY_TAB;
    case SDLK_SPACE:
        return AF_EDITOR_KEY_SPACE;
    case SDLK_BACKSPACE:
        return AF_EDITOR_KEY_BACKSPACE;
    case SDLK_DELETE:
        return AF_EDITOR_KEY_DELETE;
    case SDLK_HOME:
        return AF_EDITOR_KEY_HOME;
    case SDLK_END:
        return AF_EDITOR_KEY_END;
    case SDLK_PAGEUP:
        return AF_EDITOR_KEY_PAGE_UP;
    case SDLK_PAGEDOWN:
        return AF_EDITOR_KEY_PAGE_DOWN;
    case SDLK_LEFT:
        return AF_EDITOR_KEY_LEFT;
    case SDLK_RIGHT:
        return AF_EDITOR_KEY_RIGHT;
    case SDLK_UP:
        return AF_EDITOR_KEY_UP;
    case SDLK_DOWN:
        return AF_EDITOR_KEY_DOWN;
    case SDLK_MINUS:
        return (uint32_t)'-';
    case SDLK_EQUALS:
        return (uint32_t)'=';
    case SDLK_LEFTBRACKET:
        return (uint32_t)'[';
    case SDLK_RIGHTBRACKET:
        return (uint32_t)']';
    default:
        return 0U;
    }
}

static AfEditorShortcut af_editor_menu_default_shortcut(AfEditorMenuAction action) {
    switch (action) {
    case AF_EDITOR_MENU_ACTION_NEW_PROJECT:
        return (AfEditorShortcut){'N', AF_EDITOR_SHORTCUT_MODIFIER_CTRL};
    case AF_EDITOR_MENU_ACTION_OPEN_PROJECT:
        return (AfEditorShortcut){'O', AF_EDITOR_SHORTCUT_MODIFIER_CTRL};
    case AF_EDITOR_MENU_ACTION_SAVE_PROJECT:
        return (AfEditorShortcut){'S', AF_EDITOR_SHORTCUT_MODIFIER_CTRL};
    case AF_EDITOR_MENU_ACTION_SAVE_PROJECT_AS:
        return (AfEditorShortcut){'S', AF_EDITOR_SHORTCUT_MODIFIER_CTRL |
                                           AF_EDITOR_SHORTCUT_MODIFIER_SHIFT};
    case AF_EDITOR_MENU_ACTION_COMPILE:
        return (AfEditorShortcut){AF_EDITOR_KEY_F5, 0U};
    case AF_EDITOR_MENU_ACTION_EXPORT:
        return (AfEditorShortcut){'E', AF_EDITOR_SHORTCUT_MODIFIER_CTRL |
                                           AF_EDITOR_SHORTCUT_MODIFIER_SHIFT};
    case AF_EDITOR_MENU_ACTION_UNDO:
        return (AfEditorShortcut){'Z', AF_EDITOR_SHORTCUT_MODIFIER_CTRL};
    case AF_EDITOR_MENU_ACTION_REDO:
        return (AfEditorShortcut){'Y', AF_EDITOR_SHORTCUT_MODIFIER_CTRL};
    case AF_EDITOR_MENU_ACTION_DUPLICATE:
        return (AfEditorShortcut){'D', AF_EDITOR_SHORTCUT_MODIFIER_CTRL};
    case AF_EDITOR_MENU_ACTION_RENAME:
        return (AfEditorShortcut){AF_EDITOR_KEY_F2, 0U};
    case AF_EDITOR_MENU_ACTION_DELETE:
        return (AfEditorShortcut){AF_EDITOR_KEY_DELETE, 0U};
    case AF_EDITOR_MENU_ACTION_SELECT_ALL:
        return (AfEditorShortcut){'A', AF_EDITOR_SHORTCUT_MODIFIER_CTRL};
    case AF_EDITOR_MENU_ACTION_IMPORT_IMAGE:
        return (AfEditorShortcut){'I', AF_EDITOR_SHORTCUT_MODIFIER_CTRL};
    case AF_EDITOR_MENU_ACTION_COPY_OBJECTS:
        return (AfEditorShortcut){'C', AF_EDITOR_SHORTCUT_MODIFIER_CTRL};
    case AF_EDITOR_MENU_ACTION_PASTE_OBJECTS:
        return (AfEditorShortcut){'V', AF_EDITOR_SHORTCUT_MODIFIER_CTRL};
    case AF_EDITOR_MENU_ACTION_FIND_HIERARCHY:
        return (AfEditorShortcut){'F', AF_EDITOR_SHORTCUT_MODIFIER_CTRL};
    case AF_EDITOR_MENU_ACTION_COMMAND_PALETTE:
        return (AfEditorShortcut){'P', AF_EDITOR_SHORTCUT_MODIFIER_CTRL |
                                           AF_EDITOR_SHORTCUT_MODIFIER_SHIFT};
    case AF_EDITOR_MENU_ACTION_PROFILER:
        return (AfEditorShortcut){AF_EDITOR_KEY_F10, 0U};
    case AF_EDITOR_MENU_ACTION_VALIDATION:
        return (AfEditorShortcut){AF_EDITOR_KEY_F11, 0U};
    default:
        return (AfEditorShortcut){0};
    }
}

static uint32_t af_editor_menu_action_context(AfEditorMenuAction action) {
    if (action == AF_EDITOR_MENU_ACTION_COPY_OBJECTS ||
        action == AF_EDITOR_MENU_ACTION_PASTE_OBJECTS) {
        return AF_EDITOR_ACTION_CONTEXT_CANVAS | AF_EDITOR_ACTION_CONTEXT_HIERARCHY;
    }
    return AF_EDITOR_ACTION_CONTEXT_GLOBAL;
}

static AfResult af_editor_action_callback(void *user, AfEditorActionId action_id) {
    AfEditorState *editor = user;
    if (editor == NULL || action_id == AF_EDITOR_MENU_ACTION_NONE ||
        action_id >= (AfEditorActionId)AF_EDITOR_MENU_ACTION_COUNT) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    af_editor_execute_menu_action(editor, (AfEditorMenuAction)action_id, editor->window_width,
                                  editor->window_height);
    return AF_RESULT_SUCCESS;
}

static AfResult af_editor_register_action(AfEditorState *editor, AfEditorMenuAction action,
                                          const char *label, const char *tooltip,
                                          const char *keywords) {
    const AfEditorActionDefinition definition = {.id = (AfEditorActionId)action,
                                                 .label = label,
                                                 .tooltip = tooltip,
                                                 .keywords = keywords,
                                                 .default_shortcut =
                                                     af_editor_menu_default_shortcut(action),
                                                 .contexts = af_editor_menu_action_context(action),
                                                 .execute = af_editor_action_callback,
                                                 .user = editor};
    return af_editor_action_find(&editor->actions, definition.id) != NULL
               ? AF_RESULT_SUCCESS
               : af_editor_action_register(&editor->actions, &definition);
}

static const char *af_editor_action_description(AfEditorMenuAction action, AfEditorTopMenu menu) {
    switch (action) {
    case AF_EDITOR_MENU_ACTION_NEW_PROJECT:
        return "Begin with a fresh canvas.";
    case AF_EDITOR_MENU_ACTION_NEW_FROM_TEMPLATE:
        return "Get a head start with a ready-made scene.";
    case AF_EDITOR_MENU_ACTION_OPEN_PROJECT:
        return "Continue working on an Arcflow project.";
    case AF_EDITOR_MENU_ACTION_OPEN_RECENT:
        return "Pick up where you left off.";
    case AF_EDITOR_MENU_ACTION_SAVE_PROJECT:
        return "Save your latest changes to this project.";
    case AF_EDITOR_MENU_ACTION_SAVE_PROJECT_AS:
        return "Save a copy with a new name or location.";
    case AF_EDITOR_MENU_ACTION_EXPORT:
        return "Share your work as video, GIF, SVG or animation.";
    case AF_EDITOR_MENU_ACTION_UNDO:
        return "Take one step back. Experiment freely.";
    case AF_EDITOR_MENU_ACTION_REDO:
        return "Restore the change you just undid.";
    case AF_EDITOR_MENU_ACTION_DUPLICATE:
        return "Make another copy of the selected objects.";
    case AF_EDITOR_MENU_ACTION_RENAME:
        return "Give the selected layer a meaningful name.";
    case AF_EDITOR_MENU_ACTION_DELETE:
        return "Remove selected objects. Undo brings them back.";
    case AF_EDITOR_MENU_ACTION_GRID:
        return "Show or hide the canvas grid.";
    case AF_EDITOR_MENU_ACTION_SNAP:
        return "Align precisely with nearby edges and centers.";
    case AF_EDITOR_MENU_ACTION_FIT_CANVAS:
        return "See the entire document in your workspace.";
    case AF_EDITOR_MENU_ACTION_RESET_VIEW:
        return "Return to the default canvas view.";
    case AF_EDITOR_MENU_ACTION_PLAY:
        return "Preview your animation, or pause at this moment.";
    case AF_EDITOR_MENU_ACTION_STOP:
        return "Return the playhead to the start.";
    case AF_EDITOR_MENU_ACTION_ADD_KEY:
        return "Capture the selected property at the playhead.";
    case AF_EDITOR_MENU_ACTION_CURVES:
        return "Shape the speed and feel of your animation.";
    case AF_EDITOR_MENU_ACTION_IMPORT_IMAGE:
        return "Bring an image into your scene.";
    case AF_EDITOR_MENU_ACTION_FIND_HIERARCHY:
        return "Find a layer by name in your scene.";
    case AF_EDITOR_MENU_ACTION_COMMAND_PALETTE:
        return "Find any action without leaving your canvas.";
    case AF_EDITOR_MENU_ACTION_RESET_WORKSPACE:
        return "Restore the default panel arrangement.";
    case AF_EDITOR_MENU_ACTION_SHORTCUTS:
        return "Learn or customize your keyboard shortcuts.";
    case AF_EDITOR_MENU_ACTION_GUIDED_PROJECT:
        return "Create your first animation in six steps.";
    case AF_EDITOR_MENU_ACTION_COMBINE:
        return "Turn selected shapes into one editable compound path.";
    case AF_EDITOR_MENU_ACTION_BREAK_APART:
        return "Separate a compound path into individual objects.";
    case AF_EDITOR_MENU_ACTION_WELD:
        return "Merge two closed shapes into one silhouette.";
    case AF_EDITOR_MENU_ACTION_TRIM:
        return "Cut the other shape out of the primary selection.";
    case AF_EDITOR_MENU_ACTION_INTERSECT:
        return "Keep the area shared by two closed shapes.";
    case AF_EDITOR_MENU_ACTION_TEXT:
        return "Create or edit text directly on the canvas.";
    default:
        break;
    }
    switch (menu) {
    case AF_EDITOR_MENU_FILE:
        return "Manage your project and its files.";
    case AF_EDITOR_MENU_EDIT:
        return "Work with the objects in your selection.";
    case AF_EDITOR_MENU_VIEW:
        return "Make the canvas work for you.";
    case AF_EDITOR_MENU_ANIMATION:
        return "Build and refine your animation.";
    case AF_EDITOR_MENU_ASSETS:
        return "Organize the images in your project.";
    case AF_EDITOR_MENU_WORKSPACE:
        return "Arrange your creative workspace.";
    case AF_EDITOR_MENU_TOOLS:
        return "Position and align your objects precisely.";
    case AF_EDITOR_MENU_SHAPE:
        return "Refine the geometry of your selection.";
    default:
        return "Explore Arcflow and learn something new.";
    }
}

static AfResult af_editor_register_actions(AfEditorState *editor) {
    AfResult result = af_editor_action_registry_init(&editor->actions, editor->action_storage,
                                                     AF_EDITOR_MAX_ACTIONS);
    for (AfEditorTopMenu menu = AF_EDITOR_MENU_FILE;
         result == AF_RESULT_SUCCESS && menu <= AF_EDITOR_MENU_HELP;
         menu = (AfEditorTopMenu)(menu + 1)) {
        const AfEditorTopMenuInfo *info = af_editor_top_menu_info(menu);
        for (uint32_t item = 0U; info != NULL && item < info->item_count; ++item) {
            result = af_editor_register_action(
                editor, info->items[item].action, info->items[item].label,
                af_editor_action_description(info->items[item].action, menu), info->label);
        }
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_register_action(editor, AF_EDITOR_MENU_ACTION_ALIGN_RIGHT, "Align right",
                                           "Align selected objects to the right.",
                                           "layout selection tools");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_register_action(editor, AF_EDITOR_MENU_ACTION_ALIGN_BOTTOM,
                                           "Align bottom", "Align selected objects to the bottom.",
                                           "layout selection tools");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_register_action(
            editor, AF_EDITOR_MENU_ACTION_ADD_HORIZONTAL_GUIDE, "Add horizontal guide",
            "Add a guide through the canvas center.", "canvas ruler guide");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_register_action(editor, AF_EDITOR_MENU_ACTION_CLEAR_GUIDES,
                                           "Clear canvas guides", "Remove all canvas guides.",
                                           "canvas ruler guide");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_register_action(editor, AF_EDITOR_MENU_ACTION_RECOVERY,
                                           "Recovery options", "Open autosave recovery controls.",
                                           "autosave restore discard backup");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_register_action(
            editor, AF_EDITOR_MENU_ACTION_PRESET_LIBRARY, "Style preset library",
            "Save and apply reusable style presets.", "style fill stroke reusable");
    }
    return result;
}

static void af_editor_load_preferences(AfEditorState *editor, int width, int height) {
    AfFileData file = {0};
#ifdef __EMSCRIPTEN__
    char *directory = SDL_strdup("/documents/");
#else
    char *directory = SDL_GetPrefPath("Arcflow", "Editor");
#endif
    af_editor_preferences_default(&editor->preferences);
    af_editor_workspace_default(&editor->workspace);
    if (directory != NULL) {
        (void)snprintf(editor->preferences_path, sizeof(editor->preferences_path),
                       "%seditor_preferences.afprefs", directory);
        (void)snprintf(editor->workspace_path, sizeof(editor->workspace_path),
                       "%sworkspace.afworkspace", directory);
        SDL_free(directory);
    }
    if (editor->preferences_path[0] != '\0' &&
        af_file_read_limited(editor->preferences_path, AF_EDITOR_PREFERENCES_MAX_FILE_SIZE,
                             &editor->allocator, &file) == AF_RESULT_SUCCESS) {
        AfEditorPreferences parsed;
        if (af_editor_preferences_parse((const char *)file.data, file.size, &parsed) ==
            AF_RESULT_SUCCESS) {
            editor->preferences = parsed;
        }
        af_file_data_destroy(&editor->allocator, &file);
    }
    if (editor->workspace_path[0] != '\0' &&
        af_file_read_limited(editor->workspace_path, 16384U, &editor->allocator, &file) ==
            AF_RESULT_SUCCESS) {
        AfEditorWorkspace parsed;
        if (af_editor_workspace_parse((const char *)file.data, file.size, &parsed) ==
                AF_RESULT_SUCCESS &&
            af_editor_workspace_validate(&parsed, (float)width, (float)height) ==
                AF_RESULT_SUCCESS) {
            editor->workspace = parsed;
        }
        af_file_data_destroy(&editor->allocator, &file);
    } else {
        editor->workspace.left_width =
            (float)editor->preferences.workspace.docks[AF_EDITOR_DOCK_HIERARCHY].extent_pixels;
        editor->workspace.right_width =
            (float)editor->preferences.workspace.docks[AF_EDITOR_DOCK_INSPECTOR].extent_pixels;
        editor->workspace.timeline_height =
            (float)editor->preferences.workspace.docks[AF_EDITOR_DOCK_TIMELINE].extent_pixels;
    }
    (void)af_editor_workspace_validate(&editor->workspace, (float)width, (float)height);
    editor->left_width = editor->workspace.left_width;
    editor->right_width = editor->workspace.right_width;
    editor->timeline_height = editor->workspace.timeline_height;
    editor->asset_height = af_editor_clamp(
        (float)editor->preferences.workspace.docks[AF_EDITOR_DOCK_ASSET_BROWSER].extent_pixels,
        80.0F,
        fmaxf(80.0F, (float)height - editor->status_height - editor->timeline_height -
                         editor->toolbar_height - 56.0F));
    editor->asset_browser_visible |= editor->workspace.assets_visible;
    editor->profiler_visible = editor->workspace.profiling_visible;
    editor->validation_visible = editor->workspace.validation_visible;
    editor->rulers_visible = true;
    editor->measurement_visible = true;
    editor->smart_guides_visible = true;
    editor->welcome_visible =
        !editor->preferences.onboarding_complete && editor->maximum_frames == 0U;
}

static void af_editor_save_preferences(AfEditorState *editor) {
    char *text = NULL;
    size_t size = 0U;
    if (!editor->workflow_initialized || editor->preferences_read_only) {
        return;
    }
    editor->workspace.left_width = editor->left_width;
    editor->workspace.right_width = editor->right_width;
    editor->workspace.timeline_height = editor->timeline_height;
    editor->workspace.assets_visible = editor->asset_browser_visible;
    editor->workspace.profiling_visible = editor->profiler_visible;
    editor->workspace.validation_visible = editor->validation_visible;
    editor->preferences.workspace.docks[AF_EDITOR_DOCK_HIERARCHY].extent_pixels =
        (uint32_t)editor->left_width;
    editor->preferences.workspace.docks[AF_EDITOR_DOCK_INSPECTOR].extent_pixels =
        (uint32_t)editor->right_width;
    editor->preferences.workspace.docks[AF_EDITOR_DOCK_TIMELINE].extent_pixels =
        (uint32_t)editor->timeline_height;
    editor->preferences.workspace.docks[AF_EDITOR_DOCK_ASSET_BROWSER].visible =
        editor->asset_browser_visible;
    editor->preferences.workspace.docks[AF_EDITOR_DOCK_ASSET_BROWSER].extent_pixels =
        (uint32_t)editor->asset_height;
    editor->preferences.workspace.docks[AF_EDITOR_DOCK_PROFILER].visible = editor->profiler_visible;
    editor->preferences.workspace.docks[AF_EDITOR_DOCK_VALIDATION].visible =
        editor->validation_visible;
    text =
        af_allocate(&editor->allocator, AF_EDITOR_PREFERENCES_MAX_FILE_SIZE + 1U, _Alignof(char));
    if (text != NULL && editor->preferences_path[0] != '\0' &&
        af_editor_preferences_serialize(&editor->preferences, text,
                                        AF_EDITOR_PREFERENCES_MAX_FILE_SIZE + 1U,
                                        &size) == AF_RESULT_SUCCESS) {
        (void)af_file_write_atomic(editor->preferences_path, text, size, 1U);
    }
    if (text != NULL && editor->workspace_path[0] != '\0' &&
        af_editor_workspace_serialize(&editor->workspace, text,
                                      AF_EDITOR_PREFERENCES_MAX_FILE_SIZE + 1U,
                                      &size) == AF_RESULT_SUCCESS) {
        (void)af_file_write_atomic(editor->workspace_path, text, size, 1U);
    }
    af_deallocate(&editor->allocator, text);
}

static void af_editor_refresh_recovery(AfEditorState *editor, bool show_available) {
    editor->recovery_candidate =
        (AfEditorRecoveryCandidate){.struct_size = sizeof(editor->recovery_candidate),
                                    .api_version = AF_EDITOR_RECOVERY_API_VERSION};
    if (editor->project_path[0] == '\0' ||
        af_editor_recovery_init(&editor->recovery, editor->project_path,
                                SDL_GetTicksNS() / UINT64_C(1000000)) != AF_RESULT_SUCCESS) {
        editor->recovery = (AfEditorRecoveryState){0};
        return;
    }
    if (af_editor_recovery_find_candidate(&editor->recovery, AF_PROJECT_FORMAT_VERSION,
                                          &editor->allocator,
                                          &editor->recovery_candidate) == AF_RESULT_SUCCESS &&
        editor->recovery_candidate.status == AF_EDITOR_RECOVERY_STATUS_AVAILABLE &&
        show_available) {
        editor->recovery_overlay = true;
    }
}

static AfResult af_editor_workflow_init(AfEditorState *editor, int width, int height) {
    AfEditorActionId first_conflict = 0U;
    AfEditorActionId second_conflict = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    editor->window_width = width;
    editor->window_height = height;
    af_editor_load_preferences(editor, width, height);
    result = af_editor_register_actions(editor);
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_preferences_apply_shortcuts(&editor->preferences, &editor->actions,
                                                       &first_conflict, &second_conflict);
        if (result != AF_RESULT_SUCCESS) {
            editor->preferences.shortcut_override_count = 0U;
            result = AF_RESULT_SUCCESS;
        }
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_command_palette_init(&editor->command_palette, &editor->actions,
                                                AF_EDITOR_WORKFLOW_PALETTE_ROWS);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_performance_metrics_init(&editor->performance);
        editor->workflow_initialized = true;
        af_editor_collaboration_track_path(editor);
        af_editor_refresh_recovery(editor, true);
        if (editor->dirty) {
            af_editor_mark_dirty(editor);
        }
    }
    return result;
}

static void af_editor_workflow_shutdown(AfEditorState *editor) {
    if (editor != NULL) {
        af_editor_save_preferences(editor);
    }
}

static void af_editor_mark_dirty(AfEditorState *editor) {
    editor->dirty = true;
    if (editor->recovery.struct_size >= sizeof(editor->recovery)) {
        (void)af_editor_recovery_note_change(&editor->recovery,
                                             SDL_GetTicksNS() / UINT64_C(1000000));
    }
}

static void af_editor_after_project_saved(AfEditorState *editor) {
    if (!editor->workflow_initialized) {
        return;
    }
    if (editor->project_path[0] != '\0') {
        (void)af_editor_preferences_add_recent_project(&editor->preferences, editor->project_path);
        if (strcmp(editor->recovery.paths.project, editor->project_path) != 0) {
            (void)af_editor_recovery_init(&editor->recovery, editor->project_path,
                                          SDL_GetTicksNS() / UINT64_C(1000000));
        }
    }
    if (editor->recovery.struct_size >= sizeof(editor->recovery)) {
        (void)af_editor_recovery_mark_project_saved(&editor->recovery,
                                                    SDL_GetTicksNS() / UINT64_C(1000000));
    }
    af_editor_collaboration_saved(editor);
#ifdef __EMSCRIPTEN__
    af_editor_save_preferences(editor);
    af_web_remember_project(editor->project_path);
    af_web_download_file(editor->project_path);
    af_web_sync_files();
#endif
}

static void af_editor_project_changed(AfEditorState *editor) {
    if (!editor->workflow_initialized) {
        return;
    }
    if (editor->project_path[0] != '\0') {
        (void)af_editor_preferences_add_recent_project(&editor->preferences, editor->project_path);
    }
    af_editor_collaboration_track_path(editor);
    af_editor_refresh_recovery(editor, true);
}

static void af_editor_workflow_update(AfEditorState *editor, uint64_t now_ns) {
    const uint64_t now_ms = now_ns / UINT64_C(1000000);
    const uint64_t interval_ms =
        (uint64_t)editor->preferences.autosave_interval_seconds * UINT64_C(1000);
    if (!editor->workflow_initialized || interval_ms == 0U || !editor->dirty ||
        editor->recovery.struct_size < sizeof(editor->recovery) ||
        editor->recovery.current_revision == editor->recovery.autosaved_revision ||
        now_ms < editor->recovery.last_change_ms || now_ms < editor->recovery.last_autosave_ms ||
        now_ms - editor->recovery.last_change_ms < AF_EDITOR_RECOVERY_IDLE_DELAY_MS ||
        now_ms - editor->recovery.last_autosave_ms < interval_ms) {
        return;
    }
    {
        AfPackageBlob text = {0};
        bool written = false;
        AfResult result = af_project_serialize(&editor->project, &editor->allocator, &text);
        if (result == AF_RESULT_SUCCESS) {
            result = af_editor_recovery_write_autosave(&editor->recovery, AF_PROJECT_FORMAT_VERSION,
                                                       text.data, text.size, now_ms,
                                                       &editor->allocator, &written);
        }
        af_package_blob_destroy(&editor->allocator, &text);
        if (result == AF_RESULT_SUCCESS && written) {
            af_editor_status(editor, "Autosave snapshot written safely");
        } else if (result != AF_RESULT_SUCCESS) {
            af_editor_status(editor, "Autosave failed; the project file was not changed");
        }
    }
}

static bool af_editor_hierarchy_name_matches(const AfEditorState *editor, uint32_t node_index) {
    const char *name = editor->project.names[node_index];
    const char *query = editor->hierarchy_search;
    if (query[0] == '\0') {
        return true;
    }
    for (size_t start = 0U; name[start] != '\0'; ++start) {
        size_t offset = 0U;
        while (query[offset] != '\0' && name[start + offset] != '\0' &&
               tolower((unsigned char)query[offset]) ==
                   tolower((unsigned char)name[start + offset])) {
            ++offset;
        }
        if (query[offset] == '\0') {
            return true;
        }
    }
    return false;
}

static AfResult af_editor_clipboard_resolve_image_source(const AfEditorState *editor,
                                                         const AfImageBinding *binding,
                                                         char output[AF_IMAGE_PATH_CAPACITY]) {
    const AfImageAsset *asset = NULL;
    const char *last_slash = NULL;
    const char *last_backslash = NULL;
    size_t directory_length = 0U;
    size_t source_length = 0U;
    if (binding == NULL || binding->asset_id == 0U || binding->sequence_id != 0U) {
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    asset = af_image_find_const(&editor->project.image_data, binding->asset_id);
    if (asset == NULL || asset->source_path[0] == '\0') {
        return AF_RESULT_NOT_FOUND;
    }
    if (!af_image_path_is_project_relative(asset->source_path)) {
        if (snprintf(output, AF_IMAGE_PATH_CAPACITY, "%s", asset->source_path) >=
            (int)AF_IMAGE_PATH_CAPACITY) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        return AF_RESULT_SUCCESS;
    }
    last_slash = strrchr(editor->project_path, '/');
    last_backslash = strrchr(editor->project_path, '\\');
    if (last_backslash != NULL && (last_slash == NULL || last_backslash > last_slash)) {
        last_slash = last_backslash;
    }
    directory_length = last_slash == NULL ? 0U : (size_t)(last_slash - editor->project_path + 1);
    source_length = strlen(asset->source_path);
    if (directory_length + source_length >= AF_IMAGE_PATH_CAPACITY) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memcpy(output, editor->project_path, directory_length);
    memcpy(output + directory_length, asset->source_path, source_length + 1U);
    return AF_RESULT_SUCCESS;
}

static void af_editor_copy_objects(AfEditorState *editor) {
    uint64_t roots[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint32_t root_count = 0U;
    AfResult result = editor->selected_count == 0U ? AF_RESULT_NOT_FOUND : AF_RESULT_SUCCESS;
    editor->object_clipboard = (AfEditorCommand){0};
    memset(editor->object_clipboard_image_sources, 0,
           sizeof(editor->object_clipboard_image_sources));
    for (uint32_t index = 0U; result == AF_RESULT_SUCCESS && index < editor->selected_count;
         ++index) {
        if (!af_editor_selection_has_selected_ancestor(editor, editor->selected_ids[index])) {
            roots[root_count++] = editor->selected_ids[index];
        }
    }
    for (uint32_t index = 0U; result == AF_RESULT_SUCCESS && index < root_count; ++index) {
        uint32_t count = 0U;
        result = af_project_collect_subtree(
            &editor->project, roots[index],
            &editor->object_clipboard.after[editor->object_clipboard.after_count],
            AF_EDITOR_COMMAND_MAX_SNAPSHOTS - editor->object_clipboard.after_count, &count);
        editor->object_clipboard.after_count += count;
    }
    editor->object_clipboard.type = AF_EDITOR_COMMAND_DUPLICATE;
    for (uint32_t index = 0U;
         result == AF_RESULT_SUCCESS && index < editor->object_clipboard.after_count; ++index) {
        const AfProjectNodeSnapshot *snapshot = &editor->object_clipboard.after[index];
        const AfPath *path = af_path_find_node_const(&editor->project.path_data, snapshot->node.id);
        if (path != NULL) {
            AfProjectPathSnapshot *stored = NULL;
            if (editor->object_clipboard.path_after_count >= AF_EDITOR_COMMAND_MAX_PATHS ||
                path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS -
                                          editor->object_clipboard.path_command_after_count) {
                result = AF_RESULT_LIMIT_EXCEEDED;
                break;
            }
            stored =
                &editor->object_clipboard.path_after[editor->object_clipboard.path_after_count++];
            *stored = (AfProjectPathSnapshot){
                .path = *path, .command_offset = editor->object_clipboard.path_command_after_count};
            memcpy(&editor->object_clipboard
                        .path_command_after[editor->object_clipboard.path_command_after_count],
                   &editor->project.path_data.commands[path->command_start],
                   (size_t)path->command_count * sizeof(AfPathCommand));
            editor->object_clipboard.path_command_after_count += path->command_count;
        }
        if (snapshot->has_image_binding) {
            result = af_editor_clipboard_resolve_image_source(
                editor, &snapshot->image_binding, editor->object_clipboard_image_sources[index]);
        }
    }
    if (result != AF_RESULT_SUCCESS) {
        editor->object_clipboard = (AfEditorCommand){0};
        af_editor_status(editor, result == AF_RESULT_NOT_FOUND
                                     ? "Select one or more objects to copy"
                                     : "Selection exceeds clipboard limits or has a missing image");
        return;
    }
    af_editor_status(editor,
                     "Objects, vector paths, particles and images copied for cross-project paste");
}

static void af_editor_paste_objects(AfEditorState *editor) {
    uint64_t old_ids[AF_EDITOR_COMMAND_MAX_SNAPSHOTS] = {0};
    uint64_t new_ids[AF_EDITOR_COMMAND_MAX_SNAPSHOTS] = {0};
    AfProjectNodeSnapshot copies[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    AfEditorCommand command = {0};
    const uint32_t count = editor->object_clipboard.after_count;
    AfResult result = count == 0U ? AF_RESULT_NOT_FOUND : AF_RESULT_SUCCESS;
    for (uint32_t index = 0U; result == AF_RESULT_SUCCESS && index < count; ++index) {
        old_ids[index] = editor->object_clipboard.after[index].node.id;
        result = af_project_allocate_id(&editor->project, &new_ids[index]);
    }
    for (uint32_t index = 0U; result == AF_RESULT_SUCCESS && index < count; ++index) {
        bool parent_copied = false;
        copies[index] = editor->object_clipboard.after[index];
        copies[index].node.id = new_ids[index];
        copies[index].style.id = new_ids[index];
        copies[index].style.node_id = new_ids[index];
        copies[index].layout.node_id = new_ids[index];
        copies[index].index = editor->project.scene.count + index;
        for (uint32_t parent = 0U; parent < count; ++parent) {
            if (copies[index].node.parent_id == old_ids[parent]) {
                copies[index].node.parent_id = new_ids[parent];
                parent_copied = true;
                break;
            }
        }
        if (!parent_copied) {
            copies[index].node.parent_id = AF_INVALID_NODE_ID;
            copies[index].node.local_transform.position.x += 20.0F;
            copies[index].node.local_transform.position.y += 20.0F;
        }
        if (copies[index].has_emitter) {
            const uint64_t source_path_id = copies[index].emitter.path_id;
            result = af_project_allocate_animation_id(&editor->project, &copies[index].emitter.id);
            copies[index].emitter.node_id = new_ids[index];
            if (copies[index].emitter.type == AF_PARTICLE_EMITTER_VECTOR_PATH) {
                result = AF_RESULT_NOT_FOUND;
                for (uint32_t path_node = 0U; path_node < count; ++path_node) {
                    if (source_path_id == old_ids[path_node]) {
                        copies[index].emitter.path_id = new_ids[path_node];
                        result = AF_RESULT_SUCCESS;
                        break;
                    }
                }
            }
        }
        if (result == AF_RESULT_SUCCESS && copies[index].has_image_binding) {
            uint64_t imported_asset = 0U;
            uint64_t temporary_node = 0U;
            result = af_project_import_image(&editor->project, editor->project_path,
                                             editor->object_clipboard_image_sources[index],
                                             &imported_asset, &temporary_node);
            if (result == AF_RESULT_SUCCESS) {
                result = af_project_remove_node_exact(&editor->project, temporary_node);
            }
            if (result == AF_RESULT_SUCCESS) {
                copies[index].image_binding.node_id = new_ids[index];
                copies[index].image_binding.asset_id = imported_asset;
                copies[index].image_binding.sequence_id = 0U;
            }
        }
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_command_duplicate(copies, count, &command);
    }
    for (uint32_t path_index = 0U;
         result == AF_RESULT_SUCCESS && path_index < editor->object_clipboard.path_after_count;
         ++path_index) {
        const AfProjectPathSnapshot *source = &editor->object_clipboard.path_after[path_index];
        uint32_t node_index = 0U;
        while (node_index < count && source->path.node_id != old_ids[node_index]) {
            ++node_index;
        }
        if (node_index >= count || command.path_after_count >= AF_EDITOR_COMMAND_MAX_PATHS ||
            source->path.command_count >
                AF_EDITOR_COMMAND_MAX_PATH_COMMANDS - command.path_command_after_count) {
            result = AF_RESULT_LIMIT_EXCEEDED;
            break;
        }
        command.path_after[command.path_after_count] = *source;
        command.path_after[command.path_after_count].path.id = new_ids[node_index];
        command.path_after[command.path_after_count].path.node_id = new_ids[node_index];
        command.path_after[command.path_after_count].path.command_start =
            editor->project.path_data.command_count + command.path_command_after_count;
        command.path_after[command.path_after_count].path.geometry_generation = 1U;
        command.path_after[command.path_after_count].index =
            editor->project.path_data.path_count + command.path_after_count;
        command.path_after[command.path_after_count].command_offset =
            command.path_command_after_count;
        memcpy(&command.path_command_after[command.path_command_after_count],
               &editor->object_clipboard.path_command_after[source->command_offset],
               (size_t)source->path.command_count * sizeof(AfPathCommand));
        command.path_command_after_count += source->path.command_count;
        ++command.path_after_count;
    }
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result != AF_RESULT_SUCCESS) {
        af_editor_status(editor, count == 0U ? "Object clipboard is empty"
                                             : "Objects could not be pasted into this project");
        return;
    }
    (void)af_image_build_atlases(&editor->project.image_data);
    editor->selected_count = count;
    for (uint32_t index = 0U; index < count; ++index) {
        editor->selected_ids[index] = new_ids[index];
    }
    editor->selected_id = new_ids[count - 1U];
    af_editor_mark_dirty(editor);
    af_editor_status(editor, "Objects pasted with remapped paths, assets and stable identifiers");
}

static void af_editor_align_selection(AfEditorState *editor, AfEditorAlignMode mode) {
    uint64_t editable_ids[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    if (editor->selected_count < 2U) {
        af_editor_status(editor, "Select at least two objects to align");
        return;
    }
    af_editor_begin_selection_modify(editor, true);
    for (uint32_t index = 0U; index < editor->selection_drag_count; ++index) {
        editable_ids[index] = editor->selection_drag_before[index].node.id;
    }
    result = editor->selection_drag_count < 2U
                 ? AF_RESULT_INVALID_ARGUMENT
                 : af_editor_align_nodes(&editor->project.scene, editable_ids,
                                         editor->selection_drag_count, mode);
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_TRANSFORM, false);
    }
    af_editor_status(editor, result == AF_RESULT_SUCCESS ? "Selection aligned"
                                                         : "Selection could not be aligned");
}

static void af_editor_distribute_selection(AfEditorState *editor, AfEditorDistributeMode mode) {
    uint64_t editable_ids[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    if (editor->selected_count < 3U) {
        af_editor_status(editor, "Select at least three objects to distribute");
        return;
    }
    af_editor_begin_selection_modify(editor, true);
    for (uint32_t index = 0U; index < editor->selection_drag_count; ++index) {
        editable_ids[index] = editor->selection_drag_before[index].node.id;
    }
    result = editor->selection_drag_count < 3U
                 ? AF_RESULT_INVALID_ARGUMENT
                 : af_editor_distribute_nodes(&editor->project.scene, editable_ids,
                                              editor->selection_drag_count, mode);
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_TRANSFORM, false);
    }
    af_editor_status(editor, result == AF_RESULT_SUCCESS ? "Selection distributed"
                                                         : "Selection could not be distributed");
}

static void af_editor_add_guide(AfEditorState *editor, bool vertical) {
    if (editor->workspace.guide_count >= AF_EDITOR_WORKFLOW_MAX_GUIDES) {
        af_editor_status(editor, "Canvas guide limit reached");
        return;
    }
    editor->workspace.guides[editor->workspace.guide_count++] =
        (AfEditorGuide){.position = vertical ? editor->project.canvas_size.x * 0.5F
                                             : editor->project.canvas_size.y * 0.5F,
                        .vertical = vertical};
    af_editor_status(editor,
                     vertical ? "Vertical canvas guide added" : "Horizontal canvas guide added");
}

static void af_editor_reset_workspace(AfEditorState *editor) {
    af_editor_workspace_default(&editor->workspace);
    (void)af_editor_workspace_validate(&editor->workspace, (float)editor->window_width,
                                       (float)editor->window_height);
    editor->left_width = editor->workspace.left_width;
    editor->right_width = editor->workspace.right_width;
    editor->timeline_height = editor->workspace.timeline_height;
    editor->toolbar_height = 72.0F;
    editor->status_height = 28.0F;
    editor->asset_height = 190.0F;
    editor->timeline_header_width = 190.0F;
    editor->toolbar_scroll = 0.0F;
    editor->asset_browser_visible = true;
    editor->profiler_visible = false;
    editor->validation_visible = false;
    af_editor_status(editor, "Default workspace restored");
}

static bool af_editor_create_template(AfEditorState *editor, uint32_t template_index) {
    AfProject project = {0};
    const AfProjectTemplateInfo *info =
        af_project_template_info((AfProjectTemplateId)template_index);
    AfResult result = AF_RESULT_SUCCESS;
    if (info == NULL) {
        return false;
    }
    if (!af_editor_confirm_discard(editor, "Create a project from this template?")) {
        return false;
    }
    result = af_project_template_build((AfProjectTemplateId)template_index, &editor->allocator,
                                       AF_EDITOR_PROJECT_CAPACITY, &project);
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_adopt_project(editor, &project, NULL, false);
    }
    af_project_destroy(&project);
    if (result == AF_RESULT_SUCCESS) {
        char status[AF_EDITOR_STATUS_CAPACITY];
        (void)snprintf(status, sizeof(status), "%s template created", info->name);
        af_editor_status(editor, status);
    } else {
        af_editor_status(editor, "Project template creation failed");
    }
    return result == AF_RESULT_SUCCESS;
}

static void af_editor_open_recent_index(AfEditorState *editor, uint32_t index) {
    char path[AF_EDITOR_RECENT_PATH_CAPACITY];
    if (index >= editor->preferences.recent_project_count) {
        return;
    }
    (void)snprintf(path, sizeof(path), "%s", editor->preferences.recent_projects[index].path);
    editor->recent_panel_visible = false;
    if (af_editor_confirm_discard(editor, "Open a recent project?")) {
        (void)af_editor_open_project_path(editor, path);
    }
}

static void af_editor_execute_workflow_action(AfEditorState *editor, AfEditorMenuAction action,
                                              int width, int height) {
    (void)width;
    (void)height;
    switch (action) {
    case AF_EDITOR_MENU_ACTION_NEW_FROM_TEMPLATE:
        editor->template_panel_visible = true;
        editor->template_selected_index = 0U;
        break;
    case AF_EDITOR_MENU_ACTION_OPEN_RECENT:
        if (editor->preferences.recent_project_count == 0U) {
            af_editor_status(editor, "No recent projects");
        } else {
            editor->recent_panel_visible = true;
            editor->recent_selected_index = 0U;
        }
        break;
    case AF_EDITOR_MENU_ACTION_COPY_OBJECTS:
        af_editor_copy_objects(editor);
        break;
    case AF_EDITOR_MENU_ACTION_PASTE_OBJECTS:
        af_editor_paste_objects(editor);
        break;
    case AF_EDITOR_MENU_ACTION_FIND_HIERARCHY:
        editor->hierarchy_search_active = true;
        editor->timeline_focus = false;
        (void)af_platform_text_input_set(&editor->window, true);
        af_editor_status(editor, "Type to filter the scene hierarchy; Esc clears");
        break;
    case AF_EDITOR_MENU_ACTION_COMMAND_PALETTE:
        editor->hierarchy_search_active = false;
        (void)af_editor_command_palette_open(&editor->command_palette,
                                             AF_EDITOR_ACTION_CONTEXT_ALL);
        (void)af_platform_text_input_set(&editor->window, true);
        break;
    case AF_EDITOR_MENU_ACTION_ALIGN_LEFT:
        af_editor_align_selection(editor, AF_EDITOR_ALIGN_LEFT);
        break;
    case AF_EDITOR_MENU_ACTION_ALIGN_CENTER_X:
        af_editor_align_selection(editor, AF_EDITOR_ALIGN_CENTER_X);
        break;
    case AF_EDITOR_MENU_ACTION_ALIGN_RIGHT:
        af_editor_align_selection(editor, AF_EDITOR_ALIGN_RIGHT);
        break;
    case AF_EDITOR_MENU_ACTION_ALIGN_TOP:
        af_editor_align_selection(editor, AF_EDITOR_ALIGN_TOP);
        break;
    case AF_EDITOR_MENU_ACTION_ALIGN_CENTER_Y:
        af_editor_align_selection(editor, AF_EDITOR_ALIGN_CENTER_Y);
        break;
    case AF_EDITOR_MENU_ACTION_ALIGN_BOTTOM:
        af_editor_align_selection(editor, AF_EDITOR_ALIGN_BOTTOM);
        break;
    case AF_EDITOR_MENU_ACTION_DISTRIBUTE_HORIZONTAL:
        af_editor_distribute_selection(editor, AF_EDITOR_DISTRIBUTE_HORIZONTAL);
        break;
    case AF_EDITOR_MENU_ACTION_DISTRIBUTE_VERTICAL:
        af_editor_distribute_selection(editor, AF_EDITOR_DISTRIBUTE_VERTICAL);
        break;
    case AF_EDITOR_MENU_ACTION_ADD_VERTICAL_GUIDE:
        af_editor_add_guide(editor, true);
        break;
    case AF_EDITOR_MENU_ACTION_ADD_HORIZONTAL_GUIDE:
        af_editor_add_guide(editor, false);
        break;
    case AF_EDITOR_MENU_ACTION_CLEAR_GUIDES:
        editor->workspace.guide_count = 0U;
        af_editor_status(editor, "Canvas guides cleared");
        break;
    case AF_EDITOR_MENU_ACTION_TOGGLE_RULERS:
        editor->rulers_visible = !editor->rulers_visible;
        break;
    case AF_EDITOR_MENU_ACTION_TOGGLE_MEASUREMENTS:
        editor->measurement_visible = !editor->measurement_visible;
        break;
    case AF_EDITOR_MENU_ACTION_TOGGLE_SMART_GUIDES:
        editor->smart_guides_visible = !editor->smart_guides_visible;
        break;
    case AF_EDITOR_MENU_ACTION_PROFILER:
        editor->profiler_visible = !editor->profiler_visible;
        break;
    case AF_EDITOR_MENU_ACTION_VALIDATION:
        editor->validation_visible = !editor->validation_visible;
        break;
    case AF_EDITOR_MENU_ACTION_SYSTEM_INFO:
        editor->platform_panel_visible = !editor->platform_panel_visible;
        if (editor->platform_panel_visible) {
            (void)af_platform_query_capabilities(&editor->window, &editor->platform_capabilities);
        }
        break;
    case AF_EDITOR_MENU_ACTION_AUTOSAVE:
    case AF_EDITOR_MENU_ACTION_RECOVERY:
        editor->recovery_overlay = true;
        break;
    case AF_EDITOR_MENU_ACTION_RESET_WORKSPACE:
        af_editor_reset_workspace(editor);
        break;
    case AF_EDITOR_MENU_ACTION_PRESET_LIBRARY:
        editor->preset_panel_visible = !editor->preset_panel_visible;
        break;
    default:
        break;
    }
}
