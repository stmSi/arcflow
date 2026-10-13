static void af_editor_render_search(AfEditorState *editor) {
    af_editor_add_surface(editor, 10, 50, editor->left_width - 20, 24,
                          editor->hierarchy_search_active ? AF_UI_SELECTION : AF_UI_SURFACE, 5,
                          201000);
    af_editor_add_shape(editor, AF_NODE_ELLIPSE,
                        (AfAffine2){.m00 = 1, .m11 = 1, .tx = 18, .ty = 56}, (AfVec2){9, 9},
                        AF_UI_MUTED, 0, 201005);
    af_editor_add_shape(editor, AF_NODE_ELLIPSE,
                        (AfAffine2){.m00 = 1, .m11 = 1, .tx = 19.5F, .ty = 57.5F}, (AfVec2){6, 6},
                        editor->hierarchy_search_active ? AF_UI_SELECTION : AF_UI_SURFACE, 0,
                        201006);
    af_editor_add_line(editor, (AfVec2){25, 63}, (AfVec2){29, 67}, 1.5F, AF_UI_MUTED, 201006);
    af_editor_add_text(editor, 36, 57,
                       editor->hierarchy_search[0] ? editor->hierarchy_search : "Find a layer", .8F,
                       editor->left_width - 50,
                       editor->hierarchy_search_active ? AF_UI_TEXT : AF_UI_MUTED, 201010);
    if (editor->hierarchy_search_active)
        af_editor_add_rect(editor, 15, 73, editor->left_width - 30, 1, AF_UI_ACCENT, 201010);
}

static void af_editor_render_profiler(AfEditorState *editor, int width) {
    char line[128];
    const float x = (float)width - editor->right_width - AF_EDITOR_WORKFLOW_PANEL_WIDTH - 12.0F;
    const float y = editor->toolbar_height + 12.0F;
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
    const AfVkFrameMetrics frame = af_vk_renderer_frame_metrics(editor->renderer);
    if (frame.gpu_valid)
        (void)snprintf(line, sizeof(line), "GPU %.2f MS   UPLOAD %.2f MS", frame.gpu_ms, frame.upload_ms);
    else
        (void)snprintf(line, sizeof(line), "GPU N/A   UPLOAD %.2f MS", frame.upload_ms);
    af_editor_add_text(editor, x + 16.0F, y + 188.0F, line, 0.65F, 300.0F,
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
    if (!editor->validation_visible) {
        return;
    }
    char line[128];
    uint64_t affected_id = 0U;
    const uint32_t missing = af_editor_missing_asset_count(editor, &affected_id);
    const AfResult validation = af_project_validate(&editor->project);
    const float x = editor->left_width + 12.0F;
    const float y = editor->toolbar_height + 12.0F;
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
    if (missing != 0U && affected_id != 0U) {
        af_editor_add_button_rect(editor, x + 12.0F, y + 156.0F,
                                  AF_EDITOR_WORKFLOW_PANEL_WIDTH - 24.0F, 38.0F,
                                  (AfColor){0.14F, 0.24F, 0.38F, 1.0F}, 202009);
    } else {
        af_editor_add_rect(editor, x + 12.0F, y + 156.0F, AF_EDITOR_WORKFLOW_PANEL_WIDTH - 24.0F,
                           38.0F, (AfColor){0.1F, 0.11F, 0.14F, 1.0F}, 202009);
    }
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
                       "ARTIFACTS ARE HASHED, SIGNATURE-VERIFIED AND ATOMICALLY STAGED", 0.68F,
                       530.0F, (AfColor){0.57F, 0.71F, 0.85F, 1.0F}, 238010);
    af_editor_add_text(editor, x + 22.0F, y + 232.0F,
                       "AUTOMATIC INSTALL IS OPT-IN; PROJECTS AND SETTINGS ARE OUTSIDE APP FILES",
                       0.64F, 530.0F, (AfColor){0.52F, 0.61F, 0.73F, 1.0F}, 238010);
    af_editor_add_text(editor, x + 22.0F, y + 266.0F, "CLICK ANYWHERE TO CLOSE", 0.66F, 530.0F,
                       (AfColor){0.48F, 0.68F, 0.88F, 1.0F}, 238010);
}

static void af_editor_render_palette(AfEditorState *editor, int width) {
    if (!editor->command_palette.open)
        return;
    const AfEditorPaletteLayout box = af_editor_palette_layout(width, editor->window_height);
    const float x = box.x, y = box.y;
    editor->command_palette.page_size = box.rows;
    const uint32_t first_result = (editor->command_palette.selected_index / box.rows) * box.rows;
    af_editor_add_rect(editor, 0, 0, (float)width, (float)editor->window_height,
                       (AfColor){0.025F, 0.028F, 0.04F, .72F}, 229900);
    af_editor_add_surface(editor, x - 6, y + 6, box.width + 12, box.height + 8,
                          (AfColor){0, 0, 0, .22F}, 16, 229990);
    af_editor_add_surface(editor, x - 1, y - 1, box.width + 2, box.height + 2, AF_UI_BORDER, 13,
                          230000);
    af_editor_add_surface(editor, x, y, box.width, box.height, AF_UI_PANEL, 12, 230001);
    af_editor_add_text(editor, x + 22, y + 20, "Find your flow", 1.8F, box.width - 100, AF_UI_TEXT,
                       230020);
    af_editor_add_button_rect(editor, x + box.width - 60, y + 14, 40, 25, AF_UI_SURFACE, 230010);
    af_editor_add_text(editor, x + box.width - 50, y + 21, "Esc", .65F, 26, AF_UI_MUTED, 230020);
    af_editor_add_surface(editor, x + 16, y + 52, box.width - 32, 40, AF_UI_ACCENT, 7, 230010);
    af_editor_add_surface(editor, x + 17, y + 53, box.width - 34, 38, AF_UI_FIELD, 6, 230011);
    af_editor_add_text(editor, x + 30, y + 66,
                       editor->command_palette.query[0] ? editor->command_palette.query
                                                        : "Search tools, actions, anything...",
                       1.05F, box.width - 60,
                       editor->command_palette.query[0] ? AF_UI_TEXT : AF_UI_MUTED, 230020);
    for (uint32_t row = 0;
         row < box.rows && first_result + row < editor->command_palette.result_count; ++row) {
        const uint32_t index = first_result + row;
        const AfEditorAction *action = af_editor_action_find(
            &editor->actions, editor->command_palette.results[index].action_id);
        if (action == NULL)
            continue;
        const float row_y = y + 104 + (float)row * box.row_height;
        const bool selected = index == editor->command_palette.selected_index;
        char shortcut[AF_EDITOR_SHORTCUT_TEXT_CAPACITY];
        (void)af_editor_shortcut_format(action->shortcut, shortcut, sizeof(shortcut));
        af_editor_add_button_rect(editor, x + 12, row_y, box.width - 24, box.row_height - 4,
                                  selected ? AF_UI_SELECTION : AF_UI_PANEL, 230010);
        if (selected)
            af_editor_add_surface(editor, x + 12, row_y + 10, 3, 22, AF_UI_ACCENT, 1.5F, 230015);
        const float shortcut_width = shortcut[0] && box.width >= 400 ? 120 : 0;
        af_editor_add_text(editor, x + 26, row_y + 7, action->label, .95F,
                           box.width - 52 - shortcut_width, AF_UI_TEXT, 230020);
        af_editor_add_text(editor, x + 26, row_y + 26, action->tooltip, .65F,
                           box.width - 52 - shortcut_width, AF_UI_MUTED, 230020);
        if (shortcut_width > 0)
            af_editor_add_text(editor, x + box.width - 142, row_y + 15, shortcut, .7F, 116,
                               selected ? AF_UI_ACCENT : AF_UI_MUTED, 230020);
    }
    if (editor->command_palette.result_count == 0) {
        af_editor_add_text(editor, x + 26, y + 128, "No commands found", 1.3F, box.width - 52,
                           AF_UI_TEXT, 230020);
        af_editor_add_text(editor, x + 26, y + 155,
                           "Try a tool name, like rectangle, export or align.", .9F, box.width - 52,
                           AF_UI_MUTED, 230020);
    }
    const float footer = y + box.height - 34;
    af_editor_add_rect(editor, x + 16, footer - 8, box.width - 32, 1, AF_UI_BORDER, 230010);
    af_editor_add_text(editor, x + 24, footer + 5, "↑ ↓  Navigate     Enter  Run     Esc  Close",
                       .7F, box.width - 48, AF_UI_MUTED, 230020);
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
        af_editor_add_button_rect(editor, x + 12.0F, row_y, box_width - 24.0F, 30.0F,
                                  index == editor->shortcut_selected_index
                                      ? (editor->shortcut_capture
                                             ? (AfColor){0.48F, 0.28F, 0.16F, 1.0F}
                                             : (AfColor){0.17F, 0.33F, 0.55F, 1.0F})
                                      : (AfColor){0.1F, 0.12F, 0.16F, 1.0F},
                                  232010);
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
    af_editor_add_button_rounded(editor, x + 22.0F, y + 90.0F, 496.0F, 38.0F,
                                 (AfColor){0.16F, 0.28F, 0.46F, 1.0F}, 4.0F, 234010);
    af_editor_add_text(editor, x + 38.0F, y + 102.0F, af_editor_autosave_label(editor), 0.82F,
                       460.0F, (AfColor){0.84F, 0.88F, 0.95F, 1.0F}, 234020);
    af_editor_add_button_rounded(editor, x + 22.0F, y + 148.0F, 236.0F, 42.0F,
                                 (AfColor){0.18F, 0.42F, 0.3F, 1.0F}, 4.0F, 234010);
    af_editor_add_text(editor, x + 52.0F, y + 162.0F, "RESTORE AUTOSAVE", 0.78F, 180.0F,
                       (AfColor){0.9F, 0.95F, 0.92F, 1.0F}, 234020);
    af_editor_add_button_rounded(editor, x + 282.0F, y + 148.0F, 236.0F, 42.0F,
                                 (AfColor){0.42F, 0.2F, 0.22F, 1.0F}, 4.0F, 234010);
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
    af_editor_add_button_rounded(editor, x + 18.0F, y + 55.0F, 394.0F, 38.0F,
                                 (AfColor){0.17F, 0.34F, 0.55F, 1.0F}, 4.0F, 236010);
    af_editor_add_text(editor, x + 38.0F, y + 68.0F, "SAVE SELECTED STYLE AS PRESET", 0.76F, 350.0F,
                       (AfColor){0.88F, 0.92F, 0.98F, 1.0F}, 236020);
    for (uint32_t index = 0U; index < editor->project.style_data.count && row < 6U; ++index) {
        const AfStyle *style = &editor->project.style_data.styles[index];
        char label[80];
        if (style->node_id != 0U) {
            continue;
        }
        (void)snprintf(label, sizeof(label), "PRESET %u   CLICK TO APPLY", row + 1U);
        af_editor_add_button_rect(editor, x + 18.0F, y + 106.0F + (float)row * 32.0F, 394.0F, 28.0F,
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
    const float box_width = 780.0F;
    const float box_height = 410.0F;
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
                       "ARROWS SELECT  -  ENTER CREATES  -  ESC CLOSES", 0.68F, 720.0F,
                       (AfColor){0.48F, 0.55F, 0.65F, 1.0F}, 238010);
    for (uint32_t index = 0U; index < af_project_template_count(); ++index) {
        const AfProjectTemplateInfo *info = af_project_template_info((AfProjectTemplateId)index);
        const float card_x = x + 18.0F + (float)(index % 3U) * 250.0F;
        const float card_y = y + 75.0F + (float)(index / 3U) * 145.0F;
        const bool selected = index == editor->template_selected_index;
        af_editor_add_button_rounded(editor, card_x, card_y, 238.0F, 132.0F,
                                     selected ? (AfColor){0.18F, 0.34F, 0.56F, 1.0F}
                                              : (AfColor){0.12F, 0.14F, 0.18F, 1.0F},
                                     6.0F, 238010);
        af_editor_add_rect(
            editor, card_x + 14.0F, card_y + 14.0F, 210.0F, 38.0F,
            index == AF_PROJECT_TEMPLATE_BLANK             ? (AfColor){0.2F, 0.22F, 0.27F, 1.0F}
            : index == AF_PROJECT_TEMPLATE_RESPONSIVE_UI   ? (AfColor){0.24F, 0.55F, 0.84F, 1.0F}
            : index == AF_PROJECT_TEMPLATE_PARTICLE_EFFECT ? (AfColor){0.49F, 0.32F, 0.76F, 1.0F}
                                                           : (AfColor){0.2F, 0.45F, 0.58F, 1.0F},
            238020);
        af_editor_add_text(editor, card_x + 14.0F, card_y + 62.0F, info->name, 0.78F, 210.0F,
                           (AfColor){0.88F, 0.91F, 0.97F, 1.0F}, 238020);
        af_editor_add_text(editor, card_x + 14.0F, card_y + 88.0F, info->description, 0.57F, 210.0F,
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
        af_editor_add_button_rect(editor, x + 14.0F, row_y, box_width - 28.0F, 29.0F,
                                  row == editor->recent_selected_index
                                      ? (AfColor){0.18F, 0.34F, 0.56F, 1.0F}
                                      : (AfColor){0.1F, 0.12F, 0.16F, 1.0F},
                                  240010);
        af_editor_add_text(editor, x + 24.0F, row_y + 9.0F,
                           editor->preferences.recent_projects[row].path, 0.73F, box_width - 48.0F,
                           (AfColor){0.8F, 0.84F, 0.92F, 1.0F}, 240020);
    }
}

static void af_editor_render_export(AfEditorState *editor, int width, int height) {
    af_editor_render_export_panel(editor, width, height);
}

static void af_editor_render_workflow(AfEditorState *editor, int width, int height) {
    af_editor_render_studio_chrome(editor, width, height);
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
    af_editor_render_export(editor, width, height);
    af_editor_render_onboarding(editor, width, height);
    af_editor_render_gradient(editor, width);
    af_editor_render_text_panel(editor, width, height);
    af_editor_render_choices(editor);
    af_editor_render_appearance(editor, width, height);
    if (editor->offset_panel_open) {
        const float x = ((float)width - 540.0F) * 0.5F, y = ((float)height - 210.0F) * 0.5F;
        af_editor_add_rect(editor, 0, 0, (float)width, (float)height, (AfColor){0, 0, 0, 0.6F},
                           260000);
        af_editor_add_rect(editor, x, y, 540, 210, (AfColor){0.08F, 0.1F, 0.14F, 1}, 260001);
        af_editor_add_text(editor, x + 20, y + 20, "CONTOUR / OFFSET", 1.2F, 500,
                           (AfColor){0.7F, 0.85F, 1, 1}, 260002);
        af_editor_add_text(editor, x + 20, y + 58, "Distance in object units: + expands, - insets",
                           0.85F, 500, (AfColor){1, 1, 1, 1}, 260002);
        af_editor_add_rect(editor, x + 20, y + 88, 500, 36, (AfColor){0.15F, 0.2F, 0.28F, 1},
                           260002);
        af_editor_add_text(editor, x + 28, y + 97, editor->offset_buffer, 1.0F, 480,
                           (AfColor){1, 1, 1, 1}, 260003);
        af_editor_add_text(editor, x + 20, y + 140, "Uses the outline join style and miter limit",
                           0.8F, 500, (AfColor){0.7F, 0.75F, 0.8F, 1}, 260002);
        af_editor_add_rect(editor, x + 20, y + 166, 240, 36, (AfColor){.18F, .35F, .52F, 1},
                           260002);
        af_editor_add_rect(editor, x + 280, y + 166, 240, 36, (AfColor){.13F, .17F, .23F, 1},
                           260002);
        af_editor_add_text(editor, x + 28, y + 178, "APPLY (Enter)", .9F, 220,
                           (AfColor){1, 1, 1, 1}, 260003);
        af_editor_add_text(editor, x + 288, y + 178, "CANCEL (Esc)", .9F, 220,
                           (AfColor){1, 1, 1, 1}, 260003);
    }
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
        /* Derive ticks from the visible world interval, including negative coordinates.
         * A 1/2/5 progression keeps labels readable at every zoom. Indexing ticks (rather
         * than repeatedly adding a float) also avoids stalled loops after distant pans. */
        for (uint32_t axis = 0U; axis < 2U; ++axis) {
            const double scale = axis == 0U ? camera.m00 : camera.m11;
            const double offset = axis == 0U ? camera.tx : camera.ty;
            const double low = (axis == 0U ? canvas_x : canvas_y) + 20.0;
            const double high = axis == 0U ? canvas_x + canvas_width : canvas_y + canvas_height;
            if (scale <= 0.0 || high <= low) {
                continue;
            }
            const double target = 90.0 / scale;
            const double decade = pow(10.0, floor(log10(target)));
            const double ratio = target / decade;
            const double major = decade * (ratio <= 1.0   ? 1.0
                                           : ratio <= 2.0 ? 2.0
                                           : ratio <= 5.0 ? 5.0
                                                          : 10.0);
            const double minor = major / 5.0;
            const double first = ceil((low - offset) / scale / minor);
            for (uint32_t tick = 0U; tick < 512U; ++tick) {
                const double ordinal = first + (double)tick;
                const double value = ordinal * minor;
                const float pixel = (float)(value * scale + offset);
                if ((double)pixel >= high) {
                    break;
                }
                const bool labeled = fabs(remainder(ordinal, 5.0)) < 0.01;
                const float length = labeled ? 8.0F : 4.0F;
                af_editor_add_rect(editor, axis == 0U ? pixel : canvas_x + 20.0F - length,
                                   axis == 0U ? canvas_y + 20.0F - length : pixel,
                                   axis == 0U ? 1.0F : length, axis == 0U ? length : 1.0F,
                                   (AfColor){0.42F, 0.48F, 0.58F, 1.0F}, 90040);
                if (labeled) {
                    char label[32];
                    (void)snprintf(label, sizeof(label), "%.6g", value == 0.0 ? 0.0 : value);
                    if (axis == 0U) {
                        af_editor_add_text(editor, pixel + 3.0F, canvas_y + 3.0F, label, 0.48F,
                                           fminf(80.0F, canvas_x + canvas_width - pixel - 3.0F),
                                           (AfColor){0.58F, 0.64F, 0.74F, 1.0F}, 90040);
                    } else {
                        for (uint32_t character = 0U;
                             label[character] != '\0' && pixel + 3.0F + (float)character * 10.0F <
                                                             canvas_y + canvas_height - 10.0F;
                             ++character) {
                            const char digit[] = {label[character], '\0'};
                            af_editor_add_text(
                                editor, canvas_x + 3.0F, pixel + 3.0F + (float)character * 10.0F,
                                digit, 0.48F, 14.0F, (AfColor){0.58F, 0.64F, 0.74F, 1}, 90040);
                        }
                    }
                }
            }
        }
    }
    /* The page boundary makes artwork outside the document visible at every zoom. */
    const AfVec2 page_start = af_affine_transform_point(camera, (AfVec2){0, 0});
    const AfVec2 page_end = af_affine_transform_point(camera, editor->project.canvas_size);
    const AfColor page_line = {.4F, .52F, .65F, .85F};
    af_editor_add_line(editor, page_start, (AfVec2){page_end.x, page_start.y}, 1.5F, page_line,
                       -89900);
    af_editor_add_line(editor, (AfVec2){page_end.x, page_start.y}, page_end, 1.5F, page_line,
                       -89900);
    af_editor_add_line(editor, page_end, (AfVec2){page_start.x, page_end.y}, 1.5F, page_line,
                       -89900);
    af_editor_add_line(editor, (AfVec2){page_start.x, page_end.y}, page_start, 1.5F, page_line,
                       -89900);
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
    af_editor_render_snap_guides(editor, camera, canvas_x, canvas_y, canvas_width, canvas_height);
    if (!editor->text_panel_open && editor->measurement_visible && editor->selected_count == 1U) {
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
