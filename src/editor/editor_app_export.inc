static const AfEditorDialogAction af_editor_export_actions[6] = {
    AF_EDITOR_DIALOG_EXPORT_PACKAGE,   AF_EDITOR_DIALOG_EXPORT_LOTTIE,
    AF_EDITOR_DIALOG_EXPORT_DOTLOTTIE, AF_EDITOR_DIALOG_EXPORT_SVG,
    AF_EDITOR_DIALOG_EXPORT_GIF,       AF_EDITOR_DIALOG_EXPORT_MP4};
static const char *const af_editor_export_formats[6] = {
    "Runtime package (.afp)",   "Lottie JSON (.json)", "dotLottie (.lottie)",
    "SVG current frame (.svg)", "Animated GIF (.gif)", "MP4 video (.mp4)"};
static void af_editor_export_panel_layout(int width, int height, float *x, float *y, float *w,
                                          float *h) {
    *w = fminf(1000, (float)width - 24);
    *h = fminf(660, (float)height - 24);
    *x = ((float)width - *w) * .5F;
    *y = ((float)height - *h) * .5F;
}
static void af_editor_open_export(AfEditorState *editor) {
    editor->export_options = (AfEditorExportOptions){
        .size = {roundf(editor->project.canvas_size.x), roundf(editor->project.canvas_size.y)}};
    editor->export_format = 3;
    editor->export_field = 0;
    editor->export_ready = true;
    editor->export_panel_visible = true;
    editor->preview_playing = false;
    editor->export_preview_dirty = true;
    editor->top_menu = AF_EDITOR_MENU_NONE;
    editor->choice_menu = 0;
    af_editor_status(editor, "Set export size and scope, review the preview, then export");
}
static bool af_editor_export_visible(const AfEditorState *editor, uint64_t id) {
    return !editor->export_options.selection_only ||
           af_editor_export_includes(&editor->project, id, editor->selected_ids,
                                     editor->selected_count);
}
static bool af_editor_export_node_bounds(const AfEditorState *editor, const AfSceneNode *source,
                                         AfEditorSnapBounds *bounds) {
    return af_editor_scene_node_bounds(&editor->preview_scene, source, true, bounds);
}
static void af_editor_export_fit(AfEditorState *editor) {
    editor->export_preview_dirty = true;
    af_editor_update_preview(editor);
    AfEditorSnapBounds all = {0};
    bool found = false;
    for (uint32_t i = 0; i < editor->preview_scene.count; ++i) {
        const AfSceneNode *node = &editor->preview_scene.nodes[i];
        AfEditorSnapBounds b;
        if (!af_editor_export_visible(editor, node->id) ||
            !af_editor_export_node_bounds(editor, node, &b))
            continue;
        if (!found) {
            all = b;
            found = true;
        } else {
            all.minimum.x = fminf(all.minimum.x, b.minimum.x);
            all.minimum.y = fminf(all.minimum.y, b.minimum.y);
            all.maximum.x = fmaxf(all.maximum.x, b.maximum.x);
            all.maximum.y = fmaxf(all.maximum.y, b.maximum.y);
        }
    }
    if (!found) {
        af_editor_status(editor, "No visible objects in the chosen export scope");
        return;
    }
    const AfVec2 size = {ceilf(all.maximum.x) - floorf(all.minimum.x),
                         ceilf(all.maximum.y) - floorf(all.minimum.y)};
    if (size.x > 16384 || size.y > 16384) {
        af_editor_status(editor, "Fitted content exceeds the 16,384 pixel export limit");
        return;
    }
    editor->export_options.origin = (AfVec2){floorf(all.minimum.x), floorf(all.minimum.y)};
    editor->export_options.size = (AfVec2){fmaxf(1, size.x), fmaxf(1, size.y)};
}
static bool af_editor_export_finish_field(AfEditorState *editor) {
    if (!editor->export_field)
        return true;
    char *end = NULL;
    const long value = strtol(editor->export_buffer, &end, 10);
    if (!end || *end || value < 1 || value > 16384) {
        af_editor_status(editor, "Export width and height must be between 1 and 16,384 pixels");
        return false;
    }
    if (editor->export_field == 1)
        editor->export_options.size.x = (float)value;
    else
        editor->export_options.size.y = (float)value;
    editor->export_field = 0;
    (void)af_platform_text_input_set(&editor->window, false);
    return true;
}
static void af_editor_export_focus(AfEditorState *editor, uint32_t field) {
    editor->export_field = field;
    editor->export_replace = true;
    (void)snprintf(
        editor->export_buffer, sizeof(editor->export_buffer), "%.0f",
        (double)(field == 1 ? editor->export_options.size.x : editor->export_options.size.y));
    (void)af_platform_text_input_set(&editor->window, true);
}
static void af_editor_export_panel_text(AfEditorState *editor, const char *text) {
    editor->export_preview_dirty = true;
    if (!editor->export_field || !text)
        return;
    const size_t incoming = strlen(text);
    for (size_t i = 0; i < incoming; ++i)
        if (text[i] < '0' || text[i] > '9')
            return;
    const size_t length = editor->export_replace ? 0 : strlen(editor->export_buffer);
    if (length + incoming >= sizeof(editor->export_buffer))
        return;
    memcpy(editor->export_buffer + length, text, incoming + 1);
    editor->export_replace = false;
    char *end = NULL;
    const long value = strtol(editor->export_buffer, &end, 10);
    if (end && !*end && value >= 1 && value <= 16384) {
        if (editor->export_field == 1)
            editor->export_options.size.x = (float)value;
        else
            editor->export_options.size.y = (float)value;
    }
}
static void af_editor_export_close(AfEditorState *editor) {
    editor->export_panel_visible = false;
    editor->export_field = 0;
    (void)af_platform_text_input_set(&editor->window, false);
}
static void af_editor_export_start(AfEditorState *editor) {
    if (!af_editor_export_finish_field(editor))
        return;
    const AfVec2 size = editor->export_options.size;
    if (!isfinite(size.x) || !isfinite(size.y) || size.x < 1 || size.y < 1 || size.x > 16384 ||
        size.y > 16384) {
        af_editor_status(editor, "Export width and height must be between 1 and 16,384 pixels");
        return;
    }
    if (editor->export_options.selection_only && !editor->selected_count) {
        af_editor_status(editor, "Select objects before exporting the selection");
        return;
    }
    if (editor->export_format == 5 && (((uint32_t)editor->export_options.size.x % 2) ||
                                       ((uint32_t)editor->export_options.size.y % 2))) {
        af_editor_status(editor, "MP4 requires even width and height; adjust the export size");
        return;
    }
    af_editor_export_close(editor);
    af_editor_show_project_dialog(editor, af_editor_export_actions[editor->export_format]);
}
static void af_editor_export_panel_mouse(AfEditorState *editor, int width, int height, float px,
                                         float py) {
    editor->export_preview_dirty = true;
    float x, y, w, h;
    af_editor_export_panel_layout(width, height, &x, &y, &w, &h);
    if (px < x || px >= x + w || py < y || py >= y + h) {
        af_editor_export_close(editor);
        return;
    }
    const float right = x + w - 290;
    if (px >= right + 132 && px < right + 260 && py >= y + h - 54 && py < y + h - 20) {
        af_editor_export_close(editor);
        return;
    }
    if (!af_editor_export_finish_field(editor))
        return;
    if (px >= right && px < right + 260) {
        if (py >= y + 76 && py < y + 108)
            af_editor_export_focus(editor, 1);
        else if (py >= y + 122 && py < y + 154)
            af_editor_export_focus(editor, 2);
        else if (py >= y + 176 && py < y + 208) {
            const bool selected = px >= right + 132;
            if (selected && !editor->selected_count) {
                af_editor_status(editor, "Select objects before choosing Selection");
                return;
            }
            editor->export_options.selection_only = selected;
        } else if (py >= y + 216 && py < y + 248) {
            if (px < right + 132)
                af_editor_export_fit(editor);
            else {
                editor->export_options.origin = (AfVec2){0};
                editor->export_options.size = editor->project.canvas_size;
            }
        } else if (py >= y + 282 && py < y + 462)
            editor->export_format = (uint32_t)((py - y - 282) / 30);
        else if (py >= y + h - 54 && py < y + h - 20) {
            if (px < right + 132)
                af_editor_export_start(editor);
            else
                af_editor_export_close(editor);
        }
    }
}
static void af_editor_export_panel_key(AfEditorState *editor, const SDL_KeyboardEvent *event) {
    editor->export_preview_dirty = true;
    if (event->key == SDLK_ESCAPE) {
        af_editor_export_close(editor);
        return;
    }
    if (editor->export_field) {
        if (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER)
            (void)af_editor_export_finish_field(editor);
        else if (event->key == SDLK_TAB) {
            const uint32_t next = editor->export_field == 1 ? 2 : 1;
            if (af_editor_export_finish_field(editor))
                af_editor_export_focus(editor, next);
        } else if (event->key == SDLK_BACKSPACE) {
            if (editor->export_replace)
                editor->export_buffer[0] = '\0';
            else
                af_editor_backspace_utf8(editor->export_buffer);
            editor->export_replace = false;
            af_editor_export_panel_text(editor, "");
        } else if ((event->mod & SDL_KMOD_CTRL) && event->key == SDLK_A)
            editor->export_replace = true;
        return;
    }
    if (event->key >= SDLK_1 && event->key <= SDLK_6)
        editor->export_format = (uint32_t)(event->key - SDLK_1);
    else if (event->key == SDLK_UP || event->key == SDLK_DOWN)
        editor->export_format = (editor->export_format + (event->key == SDLK_UP ? 5 : 1)) % 6;
    else if (event->key == SDLK_TAB)
        af_editor_export_focus(editor, 1);
    else if (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER)
        af_editor_export_start(editor);
}
static AfResult af_editor_export_with_options(AfEditorState *editor, const char *path,
                                              AfEditorDialogAction action) {
#ifdef __EMSCRIPTEN__
    af_web_task_begin("Preparing download…");
#endif
    AfProject staging = {0};
    const AfEditorExportOptions options =
        editor->export_ready ? editor->export_options
                             : (AfEditorExportOptions){.size = editor->project.canvas_size};
    AfResult result =
        af_editor_export_project(&editor->project, &options, editor->selected_ids,
                                 editor->selected_count, &editor->allocator, &staging);
    if (result != AF_RESULT_SUCCESS) {
        af_editor_status(editor, "Could not prepare the requested export scope or dimensions");
#ifdef __EMSCRIPTEN__
        af_web_task_end(0);
#endif
        return result;
    }
    AfProject original = editor->project;
    editor->project = staging;
    const char *extensions[] = {".afp", ".json", ".lottie", ".svg", ".gif", ".mp4"};
    uint32_t format = 0;
    while (format < 6 && af_editor_export_actions[format] != action)
        ++format;
    char output[AF_EDITOR_PATH_CAPACITY];
    if (format >= 6 ||
        !af_editor_export_path_with_extension(path, extensions[format], output, sizeof(output))) {
        result = AF_RESULT_INVALID_ARGUMENT;
        af_editor_status(editor, "Invalid export format or destination path too long");
    } else if (format == 0)
        result = af_editor_export_package_path(editor, output);
    else if (format == 1 || format == 2)
        result = af_editor_export_lottie_path(editor, output, format == 2);
    else if (format == 3)
        result = af_editor_export_svg_path(editor, output);
    else
        result = af_editor_export_media_path(editor, output, format == 4);
    editor->project = original;
    af_editor_update_preview(editor);
    af_project_destroy(&staging);
#ifdef __EMSCRIPTEN__
    af_web_task_end(result == AF_RESULT_SUCCESS);
    if (result == AF_RESULT_SUCCESS) {
        af_web_download_file(output);
        af_web_sync_files();
    }
#endif
    return result;
}

/* Run format compatibility checks only when settings change. */
static void af_editor_export_refresh_preview(AfEditorState *editor) {
    if (!editor->export_preview_dirty)
        return;
    editor->export_preview_dirty = false;
    editor->export_preview_valid = false;
    af_interchange_diagnostics_clear(&editor->export_diagnostics);
    editor->export_visible_count = editor->export_outside_count = 0;
    for (uint32_t i = 0; i < editor->preview_scene.count; ++i) {
        const AfSceneNode *node = &editor->preview_scene.nodes[i];
        editor->export_preview_draw[i] = node->world_visible && node->type != AF_NODE_GROUP &&
                                         af_editor_export_visible(editor, node->id);
        AfEditorSnapBounds bounds;
        if (!editor->export_preview_draw[i] || !af_editor_export_node_bounds(editor, node, &bounds))
            continue;
        ++editor->export_visible_count;
        if (bounds.minimum.x < editor->export_options.origin.x ||
            bounds.minimum.y < editor->export_options.origin.y ||
            bounds.maximum.x > editor->export_options.origin.x + editor->export_options.size.x ||
            bounds.maximum.y > editor->export_options.origin.y + editor->export_options.size.y)
            ++editor->export_outside_count;
    }
    if (editor->export_format == 0)
        return;
    AfProject staging = {0};
    AfResult result =
        af_editor_export_project(&editor->project, &editor->export_options, editor->selected_ids,
                                 editor->selected_count, &editor->allocator, &staging);
    if (result != AF_RESULT_SUCCESS) {
        (void)af_interchange_diagnostics_push(&editor->export_diagnostics,
                                              AF_INTERCHANGE_DIAGNOSTIC_ERROR,
                                              AF_INTERCHANGE_DIAGNOSTIC_UNSUPPORTED_FEATURE, 0, 0,
                                              0, 0, "Could not prepare export preview");
        return;
    }
    AfProject original = editor->project;
    editor->project = staging;
    const AfPropertyType property = editor->selected_property;
    const uint32_t command = editor->selected_path_command;
    char *output = NULL;
    size_t output_size = 0;
    if (editor->export_format == 1 || editor->export_format == 2) {
        result = af_editor_build_lottie(editor, &output, &output_size, &editor->export_diagnostics);
    } else {
        af_editor_update_preview(editor);
        size_t capacity = 256U * 1024U;
        while (capacity <= AF_PROJECT_MAX_FILE_SIZE) {
            output = af_allocate(&editor->allocator, capacity, _Alignof(char));
            if (!output) {
                result = AF_RESULT_OUT_OF_MEMORY;
                break;
            }
            af_interchange_diagnostics_clear(&editor->export_diagnostics);
            result = af_interchange_export_static_svg(&editor->preview_scene,
                                                      editor->export_options.size, output, capacity,
                                                      &output_size, &editor->export_diagnostics);
            if (result != AF_RESULT_LIMIT_EXCEEDED || capacity == AF_PROJECT_MAX_FILE_SIZE)
                break;
            af_deallocate(&editor->allocator, output);
            output = NULL;
            capacity *= 2;
        }
        editor->export_preview_valid = result == AF_RESULT_SUCCESS;
    }
    if (result != AF_RESULT_SUCCESS) {
        (void)af_interchange_diagnostics_push(
            &editor->export_diagnostics, AF_INTERCHANGE_DIAGNOSTIC_WARNING,
            AF_INTERCHANGE_DIAGNOSTIC_UNSUPPORTED_FEATURE, 0, 0, 0, 0,
            "Converted preview unavailable; showing the Arcflow scene");
    }
    af_deallocate(&editor->allocator, output);
    editor->project = original;
    editor->selected_property = property;
    editor->selected_path_command = command;
    af_editor_update_preview(editor);
    af_project_destroy(&staging);
    if (editor->export_preview_valid)
        for (uint32_t i = 0; i < editor->preview_scene.count; ++i)
            editor->export_preview_draw[i] =
                editor->export_preview_draw[i] &&
                af_interchange_static_svg_node_supported(&editor->preview_scene,
                                                         &editor->preview_scene.nodes[i]);
}

static void af_editor_render_export_panel(AfEditorState *editor, int width, int height) {
    if (!editor->export_panel_visible)
        return;
    if (!editor->export_ready)
        af_editor_open_export(editor);
    af_editor_export_refresh_preview(editor);
    float x, y, w, h;
    af_editor_export_panel_layout(width, height, &x, &y, &w, &h);
    const float right = x + w - 290, preview_w = w - 340, preview_h = h - 126;
    const AfColor background = {.075F, .085F, .11F, 1}, white = {.86F, .9F, .97F, 1},
                  blue = {.3F, .7F, 1, 1};
    af_editor_add_rect(editor, 0, 0, (float)width, (float)height, background, 289000);
    const float scale =
        fminf(preview_w / editor->export_options.size.x, preview_h / editor->export_options.size.y);
    const float pw = editor->export_options.size.x * scale,
                ph = editor->export_options.size.y * scale;
    const float page_x = x + 20 + (preview_w - pw) * .5F, page_y = y + 64 + (preview_h - ph) * .5F;
    const AfAffine2 camera = {.m00 = scale,
                              .m11 = scale,
                              .tx = page_x - editor->export_options.origin.x * scale,
                              .ty = page_y - editor->export_options.origin.y * scale};
    af_editor_add_rect(editor, page_x, page_y, pw, ph, (AfColor){.96F, .96F, .96F, 1}, 289001);
    const uint32_t outside = editor->export_outside_count, visible = editor->export_visible_count;
    for (uint32_t i = 0; i < editor->preview_scene.count; ++i) {
        const AfSceneNode *node = &editor->preview_scene.nodes[i];
        if (!editor->export_preview_draw[i])
            continue;
        if (editor->render_scene.count >= AF_MAX_NODES - AF_EDITOR_UI_RESERVE)
            continue;
        AfSceneNode *copy = &editor->render_scene.nodes[editor->render_scene.count++];
        *copy = *node;
        copy->world_transform = af_affine_multiply(camera, node->world_transform);
        copy->layer_order = 300000 + af_editor_clamp_layer(node->layer_order);
    }

    /* Clip only the thumbnail, keeping the editor's persistent scene and geometry untouched. */
    af_editor_add_rect(editor, 0, 0, (float)width, page_y, background, 320000);
    af_editor_add_rect(editor, 0, page_y + ph, (float)width, (float)height - page_y - ph,
                       background, 320000);
    af_editor_add_rect(editor, 0, page_y, page_x, ph, background, 320000);
    af_editor_add_rect(editor, page_x + pw, page_y, (float)width - page_x - pw, ph, background,
                       320000);
    af_editor_add_line(editor, (AfVec2){page_x, page_y}, (AfVec2){page_x + pw, page_y}, 1, blue,
                       320001);
    af_editor_add_line(editor, (AfVec2){page_x + pw, page_y}, (AfVec2){page_x + pw, page_y + ph}, 1,
                       blue, 320001);
    af_editor_add_line(editor, (AfVec2){page_x, page_y + ph}, (AfVec2){page_x + pw, page_y + ph}, 1,
                       blue, 320001);
    af_editor_add_line(editor, (AfVec2){page_x, page_y}, (AfVec2){page_x, page_y + ph}, 1, blue,
                       320001);
    af_editor_add_text(editor, x + 20, y + 18, "EXPORT", 1.3F, preview_w, blue, 330010);
    char text[160];
    (void)snprintf(text, sizeof(text), "%s at %.2fs · %.0f × %.0f px",
                   editor->export_preview_valid ? "SVG-compatible preview"
                                                : "Arcflow scene preview",
                   (double)editor->playhead, (double)editor->export_options.size.x,
                   (double)editor->export_options.size.y);
    af_editor_add_text(editor, x + 20, y + 44, text, .8F, preview_w, white, 330010);
    (void)snprintf(text, sizeof(text), "%u objects · %u extend outside the output boundary",
                   visible, outside);
    af_editor_add_text(editor, x + 20, y + h - 48, text, .75F, preview_w,
                       outside ? (AfColor){1, .65F, .35F, 1} : white, 330010);
    af_editor_add_text(editor, x + 20, y + h - 26,
                       editor->export_format == 5
                           ? "Preview paper is white; MP4 does not retain transparency"
                           : "Preview paper is white; export keeps format transparency",
                       .65F, preview_w, (AfColor){.55F, .63F, .74F, 1}, 330010);
    af_editor_add_text(editor, right, y + 57, "WIDTH (PX)", .75F, 260, white, 330010);
    af_editor_add_text(editor, right, y + 108, "HEIGHT (PX)", .75F, 260, white, 330010);
    for (uint32_t field = 1; field <= 2; ++field) {
        const float fy = y + 76 + (float)(field - 1) * 46;
        af_editor_add_button_rect(editor, right, fy, 260, 32, (AfColor){.15F, .2F, .27F, 1},
                                  330000);
        (void)snprintf(
            text, sizeof(text), "%.0f",
            (double)(field == 1 ? editor->export_options.size.x : editor->export_options.size.y));
        af_editor_add_text(editor, right + 10, fy + 11,
                           editor->export_field == field ? editor->export_buffer : text, .9F, 238,
                           editor->export_field == field ? blue : white, 330010);
    }
    const char *scope[] = {"Canvas", "Selection"};
    for (uint32_t i = 0; i < 2; ++i) {
        const float sx = right + (float)i * 132;
        af_editor_add_button_rect(editor, sx, y + 176, 128, 32,
                                  editor->export_options.selection_only == (i == 1)
                                      ? (AfColor){.18F, .37F, .55F, 1}
                                      : (AfColor){.15F, .2F, .27F, 1},
                                  330000);
        af_editor_add_text(editor, sx + 10, y + 188, scope[i], .85F, 108, white, 330010);
        af_editor_add_button_rect(editor, sx, y + 216, 128, 32, (AfColor){.15F, .2F, .27F, 1},
                                  330000);
        af_editor_add_text(editor, sx + 10, y + 228, i ? "Page size" : "Fit content", .8F, 108,
                           white, 330010);
    }
    (void)snprintf(text, sizeof(text), "Origin %.0f, %.0f", (double)editor->export_options.origin.x,
                   (double)editor->export_options.origin.y);
    af_editor_add_text(editor, right, y + 258, text, .65F, 260, (AfColor){.57F, .65F, .75F, 1},
                       330010);
    for (uint32_t i = 0; i < 6; ++i) {
        af_editor_add_button_rect(editor, right, y + 282 + (float)i * 30, 260, 28,
                                  editor->export_format == i ? (AfColor){.18F, .37F, .55F, 1}
                                                             : (AfColor){.12F, .16F, .22F, 1},
                                  330000);
        af_editor_add_text(editor, right + 8, y + 291 + (float)i * 30, af_editor_export_formats[i],
                           .75F, 244, white, 330010);
    }
    af_editor_add_text(editor, right, y + 474,
                       editor->export_format >= 4 ? "Animation · 30 FPS · requires FFmpeg"
                                                  : "Review the current frame before exporting",
                       .65F, 260, (AfColor){.57F, .65F, .75F, 1}, 330010);

    (void)snprintf(text, sizeof(text), "%u compatibility warnings",
                   editor->export_diagnostics.warning_count);
    af_editor_add_text(
        editor, right, y + 496, text, .7F, 260,
        editor->export_diagnostics.warning_count ? (AfColor){1, .65F, .35F, 1} : white, 330010);

    if (editor->export_diagnostics.count) {
        const char *message = editor->export_diagnostics.items[0].message;
        for (uint32_t line = 0; *message && line < 5 && 516 + (float)line * 12 < h - 64; ++line) {
            size_t length = strlen(message);
            if (length > 54) {
                length = 54;
                while (length > 0 && message[length] != ' ')
                    --length;
                if (!length)
                    length = 54;
            }
            char part[56];
            memcpy(part, message, length);
            part[length] = '\0';
            af_editor_add_text(editor, right, y + 516 + (float)line * 12, part, .65F, 260,
                               (AfColor){1, .65F, .35F, 1}, 330010);
            message += length;
            while (*message == ' ')
                ++message;
        }
    } else if (editor->export_format == 0)
        af_editor_add_text(editor, right, y + 516, "Package playback viewport is set by its host",
                           .65F, 260, (AfColor){.57F, .65F, .75F, 1}, 330010);
    else if (editor->export_format < 3)
        af_editor_add_text(editor, right, y + 516, "Scene preview; Lottie may approximate features",
                           .65F, 260, (AfColor){.57F, .65F, .75F, 1}, 330010);

    af_editor_add_text(editor, x + 20, y + h - 72, editor->status, .65F, preview_w,
                       (AfColor){.57F, .65F, .75F, 1}, 330010);
    for (uint32_t i = 0; i < 2; ++i) {
        af_editor_add_button_rect(
            editor, right + (float)i * 132, y + h - 54, 128, 34,
            i ? (AfColor){.15F, .2F, .27F, 1} : (AfColor){.13F, .43F, .62F, 1}, 330000);
        af_editor_add_text(editor, right + (float)i * 132 + 10, y + h - 42, i ? "Close" : "Export…",
                           .9F, 108, white, 330010);
    }
}
