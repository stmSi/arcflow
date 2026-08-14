static void af_editor_render_search(AfEditorState *editor) {
    char text[AF_EDITOR_COMMAND_QUERY_CAPACITY + 16U];
    (void)snprintf(text, sizeof(text), "%s%s", editor->hierarchy_search_active ? "> " : "? ",
                   editor->hierarchy_search[0] == '\0' ? "SEARCH  CTRL+F"
                                                       : editor->hierarchy_search);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = 8.0F, .ty = 48.0F},
                        (AfVec2){editor->left_width - 16.0F, 25.0F},
                        editor->hierarchy_search_active ? (AfColor){0.14F, 0.24F, 0.38F, 1.0F}
                                                        : (AfColor){0.11F, 0.12F, 0.15F, 1.0F},
                        4.0F, 201000);
    af_editor_add_text(editor, 16.0F, 56.0F, text, 0.72F, editor->left_width - 32.0F,
                       (AfColor){0.7F, 0.75F, 0.84F, 1.0F}, 201010);
}

static void af_editor_render_profiler(AfEditorState *editor, int width) {
    char line[128];
    const float x = (float)width - editor->right_width - AF_EDITOR_WORKFLOW_PANEL_WIDTH - 12.0F;
    const float y = AF_EDITOR_TOOLBAR_HEIGHT + 12.0F;
    if (!editor->profiler_visible) {
        return;
    }
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
                        (AfVec2){AF_EDITOR_WORKFLOW_PANEL_WIDTH, AF_EDITOR_WORKFLOW_PANEL_HEIGHT},
                        (AfColor){0.075F, 0.085F, 0.11F, 0.97F}, 7.0F, 202000);
    af_editor_add_text(editor, x + 16.0F, y + 15.0F, "LIVE PROFILER  [F10]", 1.15F,
                       AF_EDITOR_WORKFLOW_PANEL_WIDTH - 32.0F, (AfColor){0.58F, 0.82F, 1.0F, 1.0F},
                       202010);
    (void)snprintf(line, sizeof(line), "FRAME  %.2f MS   %.0f FPS", editor->last_frame_ms,
                   editor->last_frame_ms > 0.001F ? 1000.0F / editor->last_frame_ms : 0.0F);
    af_editor_add_text(editor, x + 16.0F, y + 48.0F, line, 0.9F, 300.0F,
                       (AfColor){0.83F, 0.87F, 0.94F, 1.0F}, 202010);
    (void)snprintf(line, sizeof(line), "SCENE  %u NODES   %u PATHS", editor->project.scene.count,
                   editor->project.path_data.path_count);
    af_editor_add_text(editor, x + 16.0F, y + 76.0F, line, 0.82F, 300.0F,
                       (AfColor){0.7F, 0.75F, 0.84F, 1.0F}, 202010);
    (void)snprintf(line, sizeof(line), "ANIMATION  %u TRACKS   %u KEYS",
                   editor->project.track_count, editor->project.keyframe_count);
    af_editor_add_text(editor, x + 16.0F, y + 104.0F, line, 0.82F, 300.0F,
                       (AfColor){0.7F, 0.75F, 0.84F, 1.0F}, 202010);
    (void)snprintf(line, sizeof(line), "PARTICLES  %u EMITTERS   IMAGES %u",
                   editor->project.particle_data.count, editor->project.image_data.asset_count);
    af_editor_add_text(editor, x + 16.0F, y + 132.0F, line, 0.82F, 300.0F,
                       (AfColor){0.7F, 0.75F, 0.84F, 1.0F}, 202010);
    (void)snprintf(line, sizeof(line), "UNDO  %zu KB   ALLOCATIONS %llu",
                   af_editor_history_memory_used(&editor->history) / 1024U,
                   (unsigned long long)editor->allocator.total_allocations);
    af_editor_add_text(editor, x + 16.0F, y + 160.0F, line, 0.82F, 300.0F,
                       (AfColor){0.7F, 0.75F, 0.84F, 1.0F}, 202010);
    af_editor_add_text(editor, x + 16.0F, y + 188.0F, "CLICK PANEL TO CLOSE", 0.65F, 300.0F,
                       (AfColor){0.45F, 0.51F, 0.61F, 1.0F}, 202010);
}

static uint32_t af_editor_missing_asset_count(const AfEditorState *editor, uint64_t *out_node_id) {
    uint32_t count = 0U;
    if (out_node_id != NULL) {
        *out_node_id = 0U;
    }
    for (uint32_t asset = 0U; asset < editor->project.image_data.asset_count; ++asset) {
        if (!editor->project.image_data.assets[asset].missing) {
            continue;
        }
        ++count;
        if (out_node_id != NULL && *out_node_id == 0U) {
            for (uint32_t binding = 0U; binding < editor->project.image_data.binding_count;
                 ++binding) {
                if (af_editor_binding_uses_asset(&editor->project.image_data,
                                                 &editor->project.image_data.bindings[binding],
                                                 editor->project.image_data.assets[asset].id)) {
                    *out_node_id = editor->project.image_data.bindings[binding].node_id;
                    break;
                }
            }
        }
    }
    return count;
}

static void af_editor_render_validation(AfEditorState *editor) {
    char line[128];
    uint64_t affected_id = 0U;
    const uint32_t missing = af_editor_missing_asset_count(editor, &affected_id);
    const AfResult validation = af_project_validate(&editor->project);
    const float x = editor->left_width + 12.0F;
    const float y = AF_EDITOR_TOOLBAR_HEIGHT + 12.0F;
    if (!editor->validation_visible) {
        return;
    }
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
                        (AfVec2){AF_EDITOR_WORKFLOW_PANEL_WIDTH, AF_EDITOR_WORKFLOW_PANEL_HEIGHT},
                        (AfColor){0.075F, 0.085F, 0.11F, 0.97F}, 7.0F, 202000);
    af_editor_add_text(editor, x + 16.0F, y + 15.0F, "PROJECT VALIDATION  [F11]", 1.1F, 304.0F,
                       (AfColor){0.62F, 0.9F, 0.72F, 1.0F}, 202010);
    (void)snprintf(line, sizeof(line), "STRUCTURE   %s",
                   validation == AF_RESULT_SUCCESS ? "PASS" : "ERROR");
    af_editor_add_text(editor, x + 16.0F, y + 50.0F, line, 0.88F, 300.0F,
                       validation == AF_RESULT_SUCCESS ? (AfColor){0.55F, 0.9F, 0.65F, 1.0F}
                                                       : (AfColor){1.0F, 0.46F, 0.4F, 1.0F},
                       202010);
    (void)snprintf(line, sizeof(line), "MISSING RESOURCES   %u", missing);
    af_editor_add_text(editor, x + 16.0F, y + 82.0F, line, 0.88F, 300.0F,
                       missing == 0U ? (AfColor){0.55F, 0.9F, 0.65F, 1.0F}
                                     : (AfColor){1.0F, 0.68F, 0.3F, 1.0F},
                       202010);
    (void)snprintf(line, sizeof(line), "REVISION %llu   LOCK %s",
                   (unsigned long long)editor->project.metadata.revision,
                   editor->project_read_only ? "READ ONLY" : "OWNED");
    af_editor_add_text(editor, x + 16.0F, y + 112.0F, line, 0.78F, 300.0F,
                       editor->project_read_only ? (AfColor){1.0F, 0.68F, 0.3F, 1.0F}
                                                 : (AfColor){0.55F, 0.9F, 0.65F, 1.0F},
                       202010);
    af_editor_add_text(editor, x + 16.0F, y + 140.0F,
                       editor->external_change_visible ? "DISK CHANGED: FILE > RELOAD FROM DISK"
                                                       : "DISK STATE: CURRENT",
                       0.68F, 300.0F,
                       editor->external_change_visible ? (AfColor){1.0F, 0.58F, 0.32F, 1.0F}
                                                       : (AfColor){0.54F, 0.65F, 0.76F, 1.0F},
                       202010);
    af_editor_add_text(editor, x + 16.0F, y + 168.0F,
                       missing != 0U && affected_id != 0U ? "CLICK: SELECT AFFECTED OBJECT"
                                                          : "NO OBJECT ERRORS TO NAVIGATE",
                       0.68F, 300.0F, (AfColor){0.62F, 0.73F, 0.9F, 1.0F}, 202010);
}

static void af_editor_render_platform(AfEditorState *editor, int width, int height) {
    AfPlatformWindowMetrics metrics = {0};
    char line[192];
    const float box_width = 560.0F;
    const float box_height = 350.0F;
    const float x = ((float)width - box_width) * 0.5F;
    const float y = ((float)height - box_height) * 0.5F;
    if (!editor->platform_panel_visible) {
        return;
    }
    (void)af_platform_window_metrics(&editor->window, &metrics);
    af_editor_add_rect(editor, 0.0F, 0.0F, (float)width, (float)height,
                       (AfColor){0.0F, 0.0F, 0.0F, 0.55F}, 235900);
    af_editor_add_shape(
        editor, AF_NODE_ROUNDED_RECTANGLE, (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
        (AfVec2){box_width, box_height}, (AfColor){0.075F, 0.085F, 0.11F, 1.0F}, 8.0F, 236000);
    af_editor_add_text(editor, x + 22.0F, y + 18.0F, "SYSTEM, DISPLAYS AND INPUT", 1.2F, 500.0F,
                       (AfColor){0.61F, 0.82F, 1.0F, 1.0F}, 236010);
    (void)snprintf(line, sizeof(line), "OS %.78s   VIDEO %.78s",
                   editor->platform_capabilities.operating_system,
                   editor->platform_capabilities.video_driver);
    af_editor_add_text(editor, x + 22.0F, y + 54.0F, line, 0.8F, 510.0F,
                       (AfColor){0.82F, 0.86F, 0.94F, 1.0F}, 236010);
    (void)snprintf(line, sizeof(line), "WINDOW %d x %d   PIXELS %d x %d   SCALE %.2f x %.2f",
                   metrics.logical_width, metrics.logical_height, metrics.pixel_width,
                   metrics.pixel_height, metrics.pixel_scale_x, metrics.pixel_scale_y);
    af_editor_add_text(editor, x + 22.0F, y + 84.0F, line, 0.76F, 510.0F,
                       (AfColor){0.68F, 0.75F, 0.86F, 1.0F}, 236010);
    (void)snprintf(line, sizeof(line), "DISPLAYS %u%s   KEYBOARDS %u   TOUCH %u",
                   editor->platform_capabilities.display_count,
                   editor->platform_capabilities.display_list_truncated ? "+" : "",
                   editor->platform_capabilities.keyboard_count,
                   editor->platform_capabilities.touch_device_count);
    af_editor_add_text(editor, x + 22.0F, y + 114.0F, line, 0.76F, 510.0F,
                       (AfColor){0.68F, 0.75F, 0.86F, 1.0F}, 236010);
    for (uint32_t index = 0U; index < editor->platform_capabilities.display_count && index < 4U;
         ++index) {
        const AfPlatformDisplay *display = &editor->platform_capabilities.displays[index];
        (void)snprintf(line, sizeof(line), "%u  %s   %d x %d   DPI %.2f%s", index + 1U,
                       display->name, display->bounds.w, display->bounds.h, display->content_scale,
                       display->hdr_enabled ? "   HDR" : "");
        af_editor_add_text(editor, x + 34.0F, y + 150.0F + (float)index * 27.0F, line, 0.68F,
                           490.0F, (AfColor){0.61F, 0.68F, 0.79F, 1.0F}, 236010);
    }
    af_editor_add_text(editor, x + 22.0F, y + 272.0F,
                       "TEXT INPUT, CLIPBOARD, FILE DIALOGS AND PEN EVENTS: SDL PLATFORM LAYER",
                       0.64F, 510.0F, (AfColor){0.53F, 0.65F, 0.78F, 1.0F}, 236010);
    af_editor_add_text(editor, x + 22.0F, y + 304.0F,
                       "ALT+ENTER FULLSCREEN   CLICK ANYWHERE TO CLOSE", 0.68F, 510.0F,
                       (AfColor){0.5F, 0.72F, 0.9F, 1.0F}, 236010);
}

static void af_editor_render_update(AfEditorState *editor, int width, int height) {
    static const char *const statuses[] = {"CURRENT", "UPDATE AVAILABLE", "CHANNEL BLOCKED",
                                           "INCOMPATIBLE"};
    char line[256];
    const float box_width = 580.0F;
    const float box_height = 300.0F;
    const float x = ((float)width - box_width) * 0.5F;
    const float y = ((float)height - box_height) * 0.5F;
    if (!editor->update_panel_visible) {
        return;
    }
    af_editor_add_rect(editor, 0.0F, 0.0F, (float)width, (float)height,
                       (AfColor){0.0F, 0.0F, 0.0F, 0.55F}, 237900);
    af_editor_add_shape(
        editor, AF_NODE_ROUNDED_RECTANGLE, (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
        (AfVec2){box_width, box_height}, (AfColor){0.075F, 0.085F, 0.11F, 1.0F}, 8.0F, 238000);
    af_editor_add_text(editor, x + 22.0F, y + 18.0F, "ARCFLOW RELEASE AND UPDATES", 1.2F, 520.0F,
                       (AfColor){0.61F, 0.82F, 1.0F, 1.0F}, 238010);
    (void)snprintf(line, sizeof(line), "VERSION %s   CHANNEL %s", AF_VERSION_STRING,
                   AF_RELEASE_CHANNEL);
    af_editor_add_text(editor, x + 22.0F, y + 58.0F, line, 0.82F, 530.0F,
                       (AfColor){0.82F, 0.86F, 0.94F, 1.0F}, 238010);
    (void)snprintf(line, sizeof(line), "PACKAGE FORMAT %u   RUNTIME ABI %u",
                   AF_PACKAGE_FORMAT_VERSION, AF_RUNTIME_VERSION);
    af_editor_add_text(editor, x + 22.0F, y + 88.0F, line, 0.74F, 530.0F,
                       (AfColor){0.65F, 0.73F, 0.84F, 1.0F}, 238010);
    (void)snprintf(line, sizeof(line), "BUILD %.96s", AF_BUILD_ID);
    af_editor_add_text(editor, x + 22.0F, y + 118.0F, line, 0.64F, 530.0F,
                       (AfColor){0.53F, 0.62F, 0.74F, 1.0F}, 238010);
    if (editor->update_checked) {
        (void)snprintf(line, sizeof(line), "MANIFEST %u.%u.%u  %s",
                       editor->update_manifest.version.major, editor->update_manifest.version.minor,
                       editor->update_manifest.version.patch,
                       statuses[(uint32_t)editor->update_status]);
    } else {
        (void)snprintf(line, sizeof(line), "SELECT A .AFUPDATE MANIFEST TO CHECK THIS CHANNEL");
    }
    af_editor_add_text(editor, x + 22.0F, y + 158.0F, line, 0.78F, 530.0F,
                       editor->update_checked && editor->update_status == AF_UPDATE_STATUS_AVAILABLE
                           ? (AfColor){0.6F, 0.9F, 0.68F, 1.0F}
                           : (AfColor){0.72F, 0.76F, 0.84F, 1.0F},
                       238010);
    af_editor_add_text(editor, x + 22.0F, y + 202.0F,
                       "ARTIFACTS ARE HASHED, SIGNATURE-VERIFIED AND ATOMICALLY STAGED",
                       0.68F, 530.0F, (AfColor){0.57F, 0.71F, 0.85F, 1.0F}, 238010);
    af_editor_add_text(editor, x + 22.0F, y + 232.0F,
                       "AUTOMATIC INSTALL IS OPT-IN; PROJECTS AND SETTINGS ARE OUTSIDE APP FILES",
                       0.64F, 530.0F, (AfColor){0.52F, 0.61F, 0.73F, 1.0F}, 238010);
    af_editor_add_text(editor, x + 22.0F, y + 266.0F, "CLICK ANYWHERE TO CLOSE", 0.66F, 530.0F,
                       (AfColor){0.48F, 0.68F, 0.88F, 1.0F}, 238010);
}

static void af_editor_render_palette(AfEditorState *editor, int width) {
    const float box_width = AF_EDITOR_WORKFLOW_OVERLAY_WIDTH;
    const float x = ((float)width - box_width) * 0.5F;
    const float y = 86.0F;
    uint32_t first_result = 0U;
    if (!editor->command_palette.open) {
        return;
    }
    first_result = (editor->command_palette.selected_index / AF_EDITOR_WORKFLOW_PALETTE_ROWS) *
                   AF_EDITOR_WORKFLOW_PALETTE_ROWS;
    af_editor_add_rect(editor, 0.0F, 0.0F, (float)editor->window_width,
                       (float)editor->window_height, (AfColor){0.0F, 0.0F, 0.0F, 0.42F}, 229900);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
                        (AfVec2){box_width, 82.0F + AF_EDITOR_WORKFLOW_PALETTE_ROWS * 38.0F},
                        (AfColor){0.075F, 0.085F, 0.11F, 1.0F}, 8.0F, 230000);
    af_editor_add_text(editor, x + 20.0F, y + 16.0F, "COMMAND PALETTE", 1.2F, 240.0F,
                       (AfColor){0.61F, 0.82F, 1.0F, 1.0F}, 230010);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 16.0F, .ty = y + 43.0F},
                        (AfVec2){box_width - 32.0F, 32.0F}, (AfColor){0.14F, 0.18F, 0.25F, 1.0F},
                        4.0F, 230010);
    af_editor_add_text(editor, x + 28.0F, y + 53.0F,
                       editor->command_palette.query[0] == '\0' ? "TYPE A COMMAND..."
                                                                : editor->command_palette.query,
                       0.9F, box_width - 56.0F, (AfColor){0.86F, 0.89F, 0.95F, 1.0F}, 230020);
    for (uint32_t row = 0U; first_result + row < editor->command_palette.result_count &&
                            row < AF_EDITOR_WORKFLOW_PALETTE_ROWS;
         ++row) {
        const uint32_t result_index = first_result + row;
        const AfEditorAction *action = af_editor_action_find(
            &editor->actions, editor->command_palette.results[result_index].action_id);
        char shortcut[AF_EDITOR_SHORTCUT_TEXT_CAPACITY];
        const float row_y = y + 84.0F + (float)row * 38.0F;
        if (action == NULL) {
            continue;
        }
        if (result_index == editor->command_palette.selected_index) {
            af_editor_add_rect(editor, x + 12.0F, row_y, box_width - 24.0F, 34.0F,
                               (AfColor){0.17F, 0.33F, 0.55F, 1.0F}, 230010);
        }
        (void)af_editor_shortcut_format(action->shortcut, shortcut, sizeof(shortcut));
        af_editor_add_text(editor, x + 24.0F, row_y + 10.0F, action->label, 0.9F, 390.0F,
                           (AfColor){0.84F, 0.87F, 0.93F, 1.0F}, 230020);
        af_editor_add_text(editor, x + 470.0F, row_y + 11.0F, shortcut, 0.7F, 120.0F,
                           (AfColor){0.5F, 0.58F, 0.69F, 1.0F}, 230020);
    }
    if (editor->command_palette.result_count == 0U) {
        af_editor_add_text(editor, x + 24.0F, y + 102.0F, "NO MATCHING COMMANDS", 0.9F,
                           box_width - 48.0F, (AfColor){0.7F, 0.5F, 0.5F, 1.0F}, 230020);
    }
}

static void af_editor_render_shortcuts(AfEditorState *editor, int width, int height) {
    const float box_width = 680.0F;
    const float box_height = 498.0F;
    const float x = ((float)width - box_width) * 0.5F;
    const float y = ((float)height - box_height) * 0.5F;
    uint32_t start = 0U;
    if (!editor->shortcuts_overlay) {
        return;
    }
    if (editor->shortcut_selected_index >= AF_EDITOR_WORKFLOW_SHORTCUT_ROWS / 2U) {
        start = editor->shortcut_selected_index - AF_EDITOR_WORKFLOW_SHORTCUT_ROWS / 2U;
    }
    if (start + AF_EDITOR_WORKFLOW_SHORTCUT_ROWS > editor->actions.count) {
        start = editor->actions.count > AF_EDITOR_WORKFLOW_SHORTCUT_ROWS
                    ? editor->actions.count - AF_EDITOR_WORKFLOW_SHORTCUT_ROWS
                    : 0U;
    }
    af_editor_add_rect(editor, 0.0F, 0.0F, (float)width, (float)height,
                       (AfColor){0.0F, 0.0F, 0.0F, 0.55F}, 231900);
    af_editor_add_shape(
        editor, AF_NODE_ROUNDED_RECTANGLE, (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
        (AfVec2){box_width, box_height}, (AfColor){0.075F, 0.085F, 0.11F, 1.0F}, 8.0F, 232000);
    af_editor_add_text(editor, x + 20.0F, y + 16.0F, "CONFIGURABLE KEYBOARD SHORTCUTS", 1.2F,
                       440.0F, (AfColor){0.61F, 0.82F, 1.0F, 1.0F}, 232010);
    af_editor_add_text(editor, x + 20.0F, y + 42.0F,
                       editor->shortcut_capture ? "PRESS A KEY COMBINATION (ESC CANCELS)"
                                                : "CLICK A ROW OR USE ARROWS; ENTER TO REBIND",
                       0.72F, 620.0F, (AfColor){0.55F, 0.62F, 0.72F, 1.0F}, 232010);
    for (uint32_t row = 0U;
         row < AF_EDITOR_WORKFLOW_SHORTCUT_ROWS && start + row < editor->actions.count; ++row) {
        const uint32_t index = start + row;
        const AfEditorAction *action = &editor->actions.actions[index];
        char shortcut[AF_EDITOR_SHORTCUT_TEXT_CAPACITY];
        const float row_y = y + 70.0F + (float)row * 33.0F;
        if (index == editor->shortcut_selected_index) {
            af_editor_add_rect(editor, x + 12.0F, row_y, box_width - 24.0F, 30.0F,
                               editor->shortcut_capture ? (AfColor){0.48F, 0.28F, 0.16F, 1.0F}
                                                        : (AfColor){0.17F, 0.33F, 0.55F, 1.0F},
                               232010);
        }
        (void)af_editor_shortcut_format(action->shortcut, shortcut, sizeof(shortcut));
        af_editor_add_text(editor, x + 24.0F, row_y + 9.0F, action->label, 0.78F, 440.0F,
                           (AfColor){0.84F, 0.87F, 0.93F, 1.0F}, 232020);
        af_editor_add_text(editor, x + 500.0F, row_y + 9.0F,
                           shortcut[0] == '\0' ? "UNBOUND" : shortcut, 0.72F, 150.0F,
                           (AfColor){0.54F, 0.62F, 0.73F, 1.0F}, 232020);
    }
    af_editor_add_text(editor, x + 20.0F, y + box_height - 25.0F,
                       "SHORTCUTS ARE SAVED AS USER PREFERENCES, NOT IN THE PROJECT", 0.66F, 630.0F,
                       (AfColor){0.45F, 0.51F, 0.61F, 1.0F}, 232020);
}

static const char *af_editor_autosave_label(const AfEditorState *editor) {
    if (editor->preferences.autosave_interval_seconds == 0U) {
        return "AUTOSAVE: OFF";
    }
    if (editor->preferences.autosave_interval_seconds == 30U) {
        return "AUTOSAVE: 30 SECONDS";
    }
    if (editor->preferences.autosave_interval_seconds == 120U) {
        return "AUTOSAVE: 2 MINUTES";
    }
    return "AUTOSAVE: 5 MINUTES";
}

static void af_editor_render_recovery(AfEditorState *editor, int width, int height) {
    const float box_width = 540.0F;
    const float box_height = 270.0F;
    const float x = ((float)width - box_width) * 0.5F;
    const float y = ((float)height - box_height) * 0.5F;
    if (!editor->recovery_overlay) {
        return;
    }
    af_editor_add_rect(editor, 0.0F, 0.0F, (float)width, (float)height,
                       (AfColor){0.0F, 0.0F, 0.0F, 0.55F}, 233900);
    af_editor_add_shape(
        editor, AF_NODE_ROUNDED_RECTANGLE, (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
        (AfVec2){box_width, box_height}, (AfColor){0.075F, 0.085F, 0.11F, 1.0F}, 8.0F, 234000);
    af_editor_add_text(editor, x + 22.0F, y + 18.0F, "AUTOSAVE AND RECOVERY", 1.2F, 450.0F,
                       (AfColor){0.61F, 0.82F, 1.0F, 1.0F}, 234010);
    af_editor_add_text(editor, x + 22.0F, y + 53.0F,
                       editor->recovery_candidate.status == AF_EDITOR_RECOVERY_STATUS_AVAILABLE
                           ? "A RECOVERABLE AUTOSAVE IS AVAILABLE"
                           : "NO RECOVERABLE AUTOSAVE FOUND",
                       0.84F, 490.0F,
                       editor->recovery_candidate.status == AF_EDITOR_RECOVERY_STATUS_AVAILABLE
                           ? (AfColor){1.0F, 0.72F, 0.36F, 1.0F}
                           : (AfColor){0.58F, 0.84F, 0.66F, 1.0F},
                       234010);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 22.0F, .ty = y + 90.0F},
                        (AfVec2){496.0F, 38.0F}, (AfColor){0.16F, 0.28F, 0.46F, 1.0F}, 4.0F,
                        234010);
    af_editor_add_text(editor, x + 38.0F, y + 102.0F, af_editor_autosave_label(editor), 0.82F,
                       460.0F, (AfColor){0.84F, 0.88F, 0.95F, 1.0F}, 234020);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 22.0F, .ty = y + 148.0F},
                        (AfVec2){236.0F, 42.0F}, (AfColor){0.18F, 0.42F, 0.3F, 1.0F}, 4.0F, 234010);
    af_editor_add_text(editor, x + 52.0F, y + 162.0F, "RESTORE AUTOSAVE", 0.78F, 180.0F,
                       (AfColor){0.9F, 0.95F, 0.92F, 1.0F}, 234020);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 282.0F, .ty = y + 148.0F},
                        (AfVec2){236.0F, 42.0F}, (AfColor){0.42F, 0.2F, 0.22F, 1.0F}, 4.0F, 234010);
    af_editor_add_text(editor, x + 312.0F, y + 162.0F, "DISCARD AUTOSAVE", 0.78F, 180.0F,
                       (AfColor){0.96F, 0.9F, 0.9F, 1.0F}, 234020);
    af_editor_add_text(editor, x + 22.0F, y + 221.0F,
                       "PROJECT SAVES ARE ATOMIC AND KEEP ROTATING BACKUPS. ESC CLOSES.", 0.66F,
                       495.0F, (AfColor){0.48F, 0.55F, 0.65F, 1.0F}, 234020);
}

static void af_editor_render_preset_library(AfEditorState *editor, int width, int height) {
    const float box_width = 430.0F;
    const float box_height = 330.0F;
    const float x = ((float)width - box_width) * 0.5F;
    const float y = ((float)height - box_height) * 0.5F;
    uint32_t row = 0U;
    if (!editor->preset_panel_visible) {
        return;
    }
    af_editor_add_rect(editor, 0.0F, 0.0F, (float)width, (float)height,
                       (AfColor){0.0F, 0.0F, 0.0F, 0.5F}, 235900);
    af_editor_add_shape(
        editor, AF_NODE_ROUNDED_RECTANGLE, (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
        (AfVec2){box_width, box_height}, (AfColor){0.075F, 0.085F, 0.11F, 1.0F}, 8.0F, 236000);
    af_editor_add_text(editor, x + 20.0F, y + 18.0F, "STYLE PRESET LIBRARY", 1.15F, 360.0F,
                       (AfColor){0.61F, 0.82F, 1.0F, 1.0F}, 236010);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 18.0F, .ty = y + 55.0F},
                        (AfVec2){394.0F, 38.0F}, (AfColor){0.17F, 0.34F, 0.55F, 1.0F}, 4.0F,
                        236010);
    af_editor_add_text(editor, x + 38.0F, y + 68.0F, "SAVE SELECTED STYLE AS PRESET", 0.76F, 350.0F,
                       (AfColor){0.88F, 0.92F, 0.98F, 1.0F}, 236020);
    for (uint32_t index = 0U; index < editor->project.style_data.count && row < 6U; ++index) {
        const AfStyle *style = &editor->project.style_data.styles[index];
        char label[80];
        if (style->node_id != 0U) {
            continue;
        }
        (void)snprintf(label, sizeof(label), "PRESET %u   CLICK TO APPLY", row + 1U);
        af_editor_add_rect(editor, x + 18.0F, y + 106.0F + (float)row * 32.0F, 394.0F, 28.0F,
                           (AfColor){0.12F, 0.14F, 0.18F, 1.0F}, 236010);
        af_editor_add_rect(editor, x + 28.0F, y + 113.0F + (float)row * 32.0F, 14.0F, 14.0F,
                           style->fill.solid, 236020);
        af_editor_add_text(editor, x + 54.0F, y + 115.0F + (float)row * 32.0F, label, 0.7F, 340.0F,
                           (AfColor){0.75F, 0.8F, 0.89F, 1.0F}, 236020);
        ++row;
    }
    if (row == 0U) {
        af_editor_add_text(editor, x + 24.0F, y + 122.0F, "NO SAVED PRESETS", 0.78F, 380.0F,
                           (AfColor){0.52F, 0.58F, 0.68F, 1.0F}, 236020);
    }
}

static void af_editor_render_templates(AfEditorState *editor, int width, int height) {
    static const char *const names[] = {"EMPTY CANVAS", "UI LAYOUT", "ANIMATED VFX"};
    static const char *const descriptions[] = {"Blank scene with a Main Clip",
                                               "Responsive 1440 x 900 starter scene",
                                               "Shapes, animation timeline and particles"};
    const float box_width = 690.0F;
    const float box_height = 250.0F;
    const float x = ((float)width - box_width) * 0.5F;
    const float y = ((float)height - box_height) * 0.5F;
    if (!editor->template_panel_visible) {
        return;
    }
    af_editor_add_rect(editor, 0.0F, 0.0F, (float)width, (float)height,
                       (AfColor){0.0F, 0.0F, 0.0F, 0.52F}, 237900);
    af_editor_add_shape(
        editor, AF_NODE_ROUNDED_RECTANGLE, (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
        (AfVec2){box_width, box_height}, (AfColor){0.075F, 0.085F, 0.11F, 1.0F}, 8.0F, 238000);
    af_editor_add_text(editor, x + 22.0F, y + 18.0F, "NEW PROJECT FROM TEMPLATE", 1.18F, 500.0F,
                       (AfColor){0.61F, 0.82F, 1.0F, 1.0F}, 238010);
    af_editor_add_text(editor, x + 22.0F, y + 44.0F,
                       "LEFT / RIGHT SELECTS  -  ENTER CREATES  -  ESC CLOSES", 0.68F, 640.0F,
                       (AfColor){0.48F, 0.55F, 0.65F, 1.0F}, 238010);
    for (uint32_t index = 0U; index < 3U; ++index) {
        const float card_x = x + 18.0F + (float)index * 222.0F;
        const bool selected = index == editor->template_selected_index;
        af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = card_x, .ty = y + 75.0F},
                            (AfVec2){210.0F, 145.0F},
                            selected ? (AfColor){0.18F, 0.34F, 0.56F, 1.0F}
                                     : (AfColor){0.12F, 0.14F, 0.18F, 1.0F},
                            6.0F, 238010);
        af_editor_add_rect(editor, card_x + 16.0F, y + 92.0F, 178.0F, 54.0F,
                           index == 0U   ? (AfColor){0.2F, 0.22F, 0.27F, 1.0F}
                           : index == 1U ? (AfColor){0.24F, 0.55F, 0.84F, 1.0F}
                                         : (AfColor){0.49F, 0.32F, 0.76F, 1.0F},
                           238020);
        af_editor_add_text(editor, card_x + 16.0F, y + 162.0F, names[index], 0.84F, 178.0F,
                           (AfColor){0.88F, 0.91F, 0.97F, 1.0F}, 238020);
        af_editor_add_text(editor, card_x + 16.0F, y + 188.0F, descriptions[index], 0.62F, 178.0F,
                           (AfColor){0.6F, 0.67F, 0.77F, 1.0F}, 238020);
    }
}

static void af_editor_render_recent_projects(AfEditorState *editor, int width, int height) {
    const float box_width = 650.0F;
    const float box_height = 390.0F;
    const float x = ((float)width - box_width) * 0.5F;
    const float y = ((float)height - box_height) * 0.5F;
    if (!editor->recent_panel_visible) {
        return;
    }
    af_editor_add_rect(editor, 0.0F, 0.0F, (float)width, (float)height,
                       (AfColor){0.0F, 0.0F, 0.0F, 0.52F}, 239900);
    af_editor_add_shape(
        editor, AF_NODE_ROUNDED_RECTANGLE, (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
        (AfVec2){box_width, box_height}, (AfColor){0.075F, 0.085F, 0.11F, 1.0F}, 8.0F, 240000);
    af_editor_add_text(editor, x + 22.0F, y + 18.0F, "RECENT PROJECTS", 1.18F, 400.0F,
                       (AfColor){0.61F, 0.82F, 1.0F, 1.0F}, 240010);
    af_editor_add_text(editor, x + 22.0F, y + 44.0F,
                       "UP / DOWN SELECTS  -  ENTER OPENS  -  ESC CLOSES", 0.68F, 600.0F,
                       (AfColor){0.48F, 0.55F, 0.65F, 1.0F}, 240010);
    if (editor->preferences.recent_project_count == 0U) {
        af_editor_add_text(editor, x + 24.0F, y + 96.0F, "NO RECENT PROJECTS", 0.9F,
                           box_width - 48.0F, (AfColor){0.58F, 0.64F, 0.74F, 1.0F}, 240020);
        af_editor_add_text(editor, x + 24.0F, y + 124.0F,
                           "OPEN OR SAVE A PROJECT AND IT WILL APPEAR HERE", 0.72F,
                           box_width - 48.0F, (AfColor){0.45F, 0.51F, 0.61F, 1.0F}, 240020);
    }
    for (uint32_t row = 0U; row < editor->preferences.recent_project_count && row < 9U; ++row) {
        const float row_y = y + 76.0F + (float)row * 32.0F;
        if (row == editor->recent_selected_index) {
            af_editor_add_rect(editor, x + 14.0F, row_y, box_width - 28.0F, 29.0F,
                               (AfColor){0.18F, 0.34F, 0.56F, 1.0F}, 240010);
        }
        af_editor_add_text(editor, x + 24.0F, row_y + 9.0F,
                           editor->preferences.recent_projects[row].path, 0.73F, box_width - 48.0F,
                           (AfColor){0.8F, 0.84F, 0.92F, 1.0F}, 240020);
    }
}

static void af_editor_render_workflow(AfEditorState *editor, int width, int height) {
    af_editor_render_search(editor);
    af_editor_render_profiler(editor, width);
    af_editor_render_validation(editor);
    af_editor_render_platform(editor, width, height);
    af_editor_render_update(editor, width, height);
    af_editor_render_palette(editor, width);
    af_editor_render_shortcuts(editor, width, height);
    af_editor_render_recovery(editor, width, height);
    af_editor_render_preset_library(editor, width, height);
    af_editor_render_templates(editor, width, height);
    af_editor_render_recent_projects(editor, width, height);
}

static void af_editor_render_canvas_workflow(AfEditorState *editor, int width, int height,
                                             AfAffine2 camera) {
    float canvas_x = 0.0F;
    float canvas_y = 0.0F;
    float canvas_width = 0.0F;
    float canvas_height = 0.0F;
    af_editor_layout(editor, width, height, &canvas_x, &canvas_y, &canvas_width, &canvas_height);
    if (editor->rulers_visible) {
        af_editor_add_rect(editor, canvas_x, canvas_y, canvas_width, 20.0F,
                           (AfColor){0.09F, 0.1F, 0.125F, 0.96F}, 90030);
        af_editor_add_rect(editor, canvas_x, canvas_y, 20.0F, canvas_height,
                           (AfColor){0.09F, 0.1F, 0.125F, 0.96F}, 90030);
        for (float value = 0.0F; value <= editor->project.canvas_size.x; value += 100.0F) {
            const AfVec2 point = af_affine_transform_point(camera, (AfVec2){value, 0.0F});
            char label[24];
            if (point.x >= canvas_x + 20.0F && point.x < canvas_x + canvas_width) {
                af_editor_add_rect(editor, point.x, canvas_y + 13.0F, 1.0F, 7.0F,
                                   (AfColor){0.42F, 0.48F, 0.58F, 1.0F}, 90040);
                (void)snprintf(label, sizeof(label), "%.0f", value);
                af_editor_add_text(editor, point.x + 3.0F, canvas_y + 4.0F, label, 0.52F, 38.0F,
                                   (AfColor){0.48F, 0.54F, 0.64F, 1.0F}, 90040);
            }
        }
        for (float value = 0.0F; value <= editor->project.canvas_size.y; value += 100.0F) {
            const AfVec2 point = af_affine_transform_point(camera, (AfVec2){0.0F, value});
            if (point.y >= canvas_y + 20.0F && point.y < canvas_y + canvas_height) {
                af_editor_add_rect(editor, canvas_x + 13.0F, point.y, 7.0F, 1.0F,
                                   (AfColor){0.42F, 0.48F, 0.58F, 1.0F}, 90040);
            }
        }
    }
    for (uint32_t index = 0U; index < editor->workspace.guide_count; ++index) {
        const AfEditorGuide *guide = &editor->workspace.guides[index];
        const AfVec2 point =
            af_affine_transform_point(camera, guide->vertical ? (AfVec2){guide->position, 0.0F}
                                                              : (AfVec2){0.0F, guide->position});
        af_editor_add_rect(
            editor, guide->vertical ? point.x : canvas_x, guide->vertical ? canvas_y : point.y,
            guide->vertical ? 1.5F : canvas_width, guide->vertical ? canvas_height : 1.5F,
            (AfColor){0.8F, 0.25F, 0.72F, 0.85F}, 90050);
    }
    if (editor->smart_guides_visible && editor->selected_count == 1U) {
        const AfSceneNode *node = af_scene_find_const(&editor->project.scene, editor->selected_id);
        if (node != NULL && node->parent_id == AF_INVALID_NODE_ID) {
            const float center_x = node->local_transform.position.x + node->bounds.x * 0.5F;
            const float center_y = node->local_transform.position.y + node->bounds.y * 0.5F;
            if (fabsf(center_x - editor->project.canvas_size.x * 0.5F) < 0.01F) {
                const AfVec2 point = af_affine_transform_point(
                    camera, (AfVec2){editor->project.canvas_size.x * 0.5F, 0.0F});
                af_editor_add_rect(editor, point.x, canvas_y, 1.0F, canvas_height,
                                   (AfColor){0.2F, 0.9F, 0.75F, 0.9F}, 90055);
            }
            if (fabsf(center_y - editor->project.canvas_size.y * 0.5F) < 0.01F) {
                const AfVec2 point = af_affine_transform_point(
                    camera, (AfVec2){0.0F, editor->project.canvas_size.y * 0.5F});
                af_editor_add_rect(editor, canvas_x, point.y, canvas_width, 1.0F,
                                   (AfColor){0.2F, 0.9F, 0.75F, 0.9F}, 90055);
            }
        }
    }
    if (editor->measurement_visible && editor->selected_count == 1U) {
        const AfSceneNode *node = af_scene_find_const(&editor->project.scene, editor->selected_id);
        if (node != NULL) {
            const AfVec2 origin = af_affine_transform_point(camera, node->local_transform.position);
            char measurement[112];
            (void)snprintf(measurement, sizeof(measurement), "X %.1f  Y %.1f   W %.1f  H %.1f",
                           node->local_transform.position.x, node->local_transform.position.y,
                           node->bounds.x, node->bounds.y);
            af_editor_add_shape(
                editor, AF_NODE_ROUNDED_RECTANGLE,
                (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = origin.x, .ty = origin.y - 25.0F},
                (AfVec2){230.0F, 20.0F}, (AfColor){0.08F, 0.09F, 0.12F, 0.9F}, 3.0F, 90060);
            af_editor_add_text(editor, origin.x + 7.0F, origin.y - 18.0F, measurement, 0.56F,
                               216.0F, (AfColor){0.72F, 0.78F, 0.88F, 1.0F}, 90070);
        }
    }
}
