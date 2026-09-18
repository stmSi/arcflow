static bool af_editor_is_text(const AfEditorState *editor, uint64_t id) {
    const uint32_t index = af_project_find_index(&editor->project, id);
    return index != UINT32_MAX && editor->project.texts[index].mode != AF_PROJECT_TEXT_NONE;
}

static uint64_t af_editor_document_font(AfEditorState *editor, const AfProjectText *text) {
    static const char *families[] = {"LiberationSans", "LiberationSerif", "LiberationMono"};
    static const char *styles[] = {"Regular", "Bold", "Italic", "BoldItalic"};
    if (text->font_family > 2U)
        return 0;
    const uint32_t style = (text->bold ? 1U : 0U) + (text->italic ? 2U : 0U);
    const uint32_t index = text->font_family * 4U + style;
    const uint64_t id = 2U + index;
    if (editor->document_font_files[index].data)
        return id;
    char path[AF_EDITOR_PATH_CAPACITY];
    (void)snprintf(path, sizeof(path), "%s/%s-%s.ttf", ARCFLOW_DOCUMENT_FONT_DIR,
                   families[text->font_family], styles[style]);
    AfFileData *file = &editor->document_font_files[index];
    AfResult result = af_file_read_limited(path, 4U * 1024U * 1024U, &editor->allocator, file);
    if (result != AF_RESULT_SUCCESS) {
        char resource[128];
        (void)snprintf(resource, sizeof(resource), "fonts/%s-%s.ttf", families[text->font_family],
                       styles[style]);
        result = af_platform_resource_path(resource, path, sizeof(path));
        if (result == AF_RESULT_SUCCESS)
            result = af_file_read_limited(path, 4U * 1024U * 1024U, &editor->allocator, file);
    }
    if (result == AF_RESULT_SUCCESS)
        result = af_text_font_load_memory(editor->text_engine, id, file->data, file->size, 0);
    if (result != AF_RESULT_SUCCESS) {
        af_file_data_destroy(&editor->allocator, file);
        return 0;
    }
    return id;
}

static void af_editor_open_text_panel(AfEditorState *editor, int width, int height) {
    if (editor->tool_command_count || editor->drag_mode != AF_EDITOR_DRAG_NONE) {
        af_editor_status(editor, "Finish or cancel drawing before editing text");
        return;
    }
    editor->text_panel_id =
        af_editor_is_text(editor, editor->selected_id) ? editor->selected_id : 0;
    editor->text_panel_position = af_editor_visible_center(editor, width, height);
    editor->text_panel_draft = (AfProjectText){
        .mode = AF_PROJECT_TEXT_ARTISTIC, .utf8 = "Text", .font_size = 48, .width = 320};
    if (editor->text_panel_id) {
        editor->text_panel_draft =
            editor->project.texts[af_project_find_index(&editor->project, editor->text_panel_id)];
    } else {
        const AfPath *path =
            af_path_find_node_const(&editor->project.path_data, editor->selected_id);
        const AfSceneNode *node = af_scene_find_const(&editor->project.scene, editor->selected_id);
        if (path && node && path->command_count <= AF_PROJECT_TEXT_GUIDE_COMMANDS) {
            editor->text_panel_position =
                (AfVec2){node->world_transform.tx, node->world_transform.ty};
            editor->text_panel_draft.guide_count = path->command_count;
            for (uint32_t i = 0; i < path->command_count; ++i) {
                AfPathCommand c = editor->project.path_data.commands[path->command_start + i];
                AfAffine2 frame = node->world_transform;
                frame.tx = frame.ty = 0;
                c.point = af_affine_transform_point(frame, c.point);
                c.control1 = af_affine_transform_point(frame, c.control1);
                c.control2 = af_affine_transform_point(frame, c.control2);
                editor->text_panel_draft.guide[i] = c;
            }
        }
    }
    (void)snprintf(editor->text_panel_numbers[0], 32, "%.6g",
                   (double)editor->text_panel_draft.font_size);
    (void)snprintf(editor->text_panel_numbers[1], 32, "%.6g",
                   (double)editor->text_panel_draft.width);
    (void)snprintf(editor->text_panel_numbers[2], 32, "%.6g",
                   (double)editor->text_panel_draft.offset);
    (void)snprintf(editor->text_panel_numbers[3], 32, "%.6g",
                   (double)editor->text_panel_draft.letter_spacing);
    (void)snprintf(editor->text_panel_numbers[4], 32, "%.6g",
                   (double)(editor->text_panel_draft.line_spacing > 0
                                ? editor->text_panel_draft.line_spacing
                                : 1.2F));
    AfColor color = {.9F, .94F, 1, 1};
    if (editor->text_panel_id)
        color = af_project_find_node_style(&editor->project, editor->text_panel_id)->fill.solid;
    af_editor_inspector_format_hex_color(color, editor->text_panel_numbers[5], 32);
    (void)snprintf(editor->text_panel_original_color, 32, "%s", editor->text_panel_numbers[5]);
    editor->text_panel_open = true;
    editor->text_panel_field = 0;
    editor->text_panel_replace = true;
    editor->preview_playing = false;
}

static char *af_editor_text_panel_buffer(AfEditorState *editor, size_t *capacity) {
    *capacity = editor->text_panel_field ? 32U : AF_PROJECT_TEXT_BYTES;
    return editor->text_panel_field ? editor->text_panel_numbers[editor->text_panel_field - 1U]
                                    : editor->text_panel_draft.utf8;
}

static void af_editor_text_panel_append(AfEditorState *editor, const char *text) {
    size_t capacity;
    char *buffer = af_editor_text_panel_buffer(editor, &capacity);
    const size_t length = editor->text_panel_replace ? 0U : strlen(buffer);
    if (strlen(text) >= capacity - length) {
        af_editor_status(editor, "Text field capacity reached");
        return;
    }
    memcpy(buffer + length, text, strlen(text) + 1U);
    editor->text_panel_replace = false;
}

static void af_editor_apply_text_panel(AfEditorState *editor) {
    AfProjectText text = editor->text_panel_draft;
    float values[5];
    for (uint32_t i = 0; i < 5; ++i) {
        char *end;
        values[i] = strtof(editor->text_panel_numbers[i], &end);
        if (end == editor->text_panel_numbers[i] || *end || !isfinite(values[i])) {
            af_editor_status(editor, "Enter valid numbers for size, frame and spacing");
            return;
        }
    }
    text.font_size = values[0];
    text.width = values[1];
    text.offset = values[2];
    text.letter_spacing = values[3];
    text.line_spacing = values[4];
    AfColor color;
    if (!af_editor_inspector_parse_hex_color(editor->text_panel_numbers[5], &color)) {
        af_editor_status(editor, "Enter a color as #RRGGBB or #RRGGBBAA");
        return;
    }
    const uint64_t font_id = af_editor_document_font(editor, &text);
    if (!font_id) {
        af_editor_status(editor, "Selected font could not be loaded");
        return;
    }
    if (text.mode != AF_PROJECT_TEXT_ON_PATH)
        text.guide_count = 0;
    AfEditorCommand command;
    AfResult result =
        af_editor_command_text(&editor->project, editor->text_engine, font_id,
                               editor->text_panel_id, editor->text_panel_position, &text, &command);
    if (result == AF_RESULT_SUCCESS &&
        (!editor->text_panel_id ||
         strcmp(editor->text_panel_original_color, editor->text_panel_numbers[5]) != 0)) {
        command.after[0].node.fill = color;
        command.after[0].style.fill = af_paint_solid(color);
        command.after[0].style.inherit_flags &= ~(uint32_t)AF_STYLE_INHERIT_FILL;
        (void)af_style_mark_paint_changed(&command.after[0].style);
    }
    if (result == AF_RESULT_SUCCESS)
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    if (result != AF_RESULT_SUCCESS) {
        af_editor_status(
            editor,
            result == AF_RESULT_LIMIT_EXCEEDED
                ? "Text exceeds path length or geometry capacity · shorten text or reduce size"
                : "Check text and size (1–1000); path text needs one selected contour; font must "
                  "contain every glyph");
        return;
    }
    editor->text_panel_open = false;
    af_editor_select_only(editor, command.target_id);
    af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_SELECT);
    af_editor_mark_dirty(editor);
    af_editor_status(
        editor, "Text saved · T edits content and layout · Convert to Path unlocks glyph nodes");
}

static void af_editor_text_panel_key(AfEditorState *editor, const SDL_KeyboardEvent *event) {
    const bool control = (event->mod & SDL_KMOD_CTRL) != 0;
    const bool shift = (event->mod & SDL_KMOD_SHIFT) != 0;
    size_t capacity;
    char *buffer = af_editor_text_panel_buffer(editor, &capacity);
    (void)capacity;
    if (event->key == SDLK_ESCAPE)
        editor->text_panel_open = false;
    else if (event->key == SDLK_TAB) {
        editor->text_panel_field = (editor->text_panel_field + (shift ? 6U : 1U)) % 7U;
        editor->text_panel_replace = true;
    } else if (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER) {
        if (shift && editor->text_panel_field == 0)
            af_editor_text_panel_append(editor, "\n");
        else
            af_editor_apply_text_panel(editor);
    } else if (control && event->key == SDLK_A)
        editor->text_panel_replace = true;
    else if (control && event->key == SDLK_C)
        (void)SDL_SetClipboardText(buffer);
    else if (control && event->key == SDLK_V) {
        char *clipboard = SDL_GetClipboardText();
        if (clipboard) {
            af_editor_text_panel_append(editor, clipboard);
            SDL_free(clipboard);
        }
    } else if (event->key == SDLK_BACKSPACE || event->key == SDLK_DELETE) {
        if (editor->text_panel_replace)
            buffer[0] = '\0';
        else
            af_editor_backspace_utf8(buffer);
        editor->text_panel_replace = false;
    } else if (control && event->key == SDLK_B)
        editor->text_panel_draft.bold = !editor->text_panel_draft.bold;
    else if (control && event->key == SDLK_I)
        editor->text_panel_draft.italic = !editor->text_panel_draft.italic;
    else if (control && event->key == SDLK_F)
        editor->text_panel_draft.font_family = (editor->text_panel_draft.font_family + 1U) % 3U;
    else if (control && event->key >= SDLK_1 && event->key <= SDLK_3) {
        editor->text_panel_draft.mode = (AfProjectTextMode)(event->key - SDLK_1 + 1U);
    }
}

static void af_editor_text_panel_mouse(AfEditorState *editor, int width, int height, float x,
                                       float y) {
    const float left = ((float)width - 760) * .5F, top = ((float)height - 560) * .5F;
    if (x < left + 20 || x > left + 740)
        return;
    if (y >= top + 52 && y < top + 86) {
        const uint32_t mode = (uint32_t)((x - left - 20) / 240) + 1U;
        if (mode <= 3)
            editor->text_panel_draft.mode = (AfProjectTextMode)mode;
    } else if (y >= top + 100 && y < top + 134) {
        if (x < left + 370)
            editor->text_panel_draft.font_family = (editor->text_panel_draft.font_family + 1U) % 3U;
        else if (x < left + 450)
            editor->text_panel_draft.bold = !editor->text_panel_draft.bold;
        else if (x < left + 530)
            editor->text_panel_draft.italic = !editor->text_panel_draft.italic;
        else
            editor->text_panel_draft.alignment = (editor->text_panel_draft.alignment + 1U) % 3U;
    } else if (y >= top + 150 && y < top + 260) {
        editor->text_panel_field = 0;
        editor->text_panel_replace = true;
    } else if ((y >= top + 300 && y < top + 340) || (y >= top + 380 && y < top + 420)) {
        editor->text_panel_field = (uint32_t)((x - left - 20) / 240) + (y < top + 340 ? 1U : 4U);
        if (editor->text_panel_field > 6)
            editor->text_panel_field = 6;
        editor->text_panel_replace = true;
    } else if (y >= top + 510 && y < top + 550) {
        if (x < left + 360)
            af_editor_apply_text_panel(editor);
        else if (x >= left + 400)
            editor->text_panel_open = false;
    }
}

static void af_editor_render_text_panel(AfEditorState *editor, int width, int height) {
    if (!editor->text_panel_open)
        return;
    const float x = ((float)width - 760) * .5F, y = ((float)height - 560) * .5F;
    const AfColor white = {.9F, .94F, 1, 1}, accent = {.45F, .75F, 1, 1},
                  active = {.18F, .35F, .52F, 1}, idle = {.13F, .17F, .23F, 1};
    af_editor_add_rect(editor, 0, 0, (float)width, (float)height, (AfColor){0, 0, 0, .6F}, 260000);
    af_editor_add_rect(editor, x, y, 760, 560, (AfColor){.08F, .1F, .14F, 1}, 260001);
    af_editor_add_text(editor, x + 20, y + 18, "TEXT · CONTENT & FORMATTING", 1.2F, 720, accent,
                       260003);
    const char *modes[] = {"ARTISTIC (Ctrl+1)", "PARAGRAPH (Ctrl+2)", "ON PATH (Ctrl+3)"};
    const char *labels[] = {"FONT SIZE",           "PARAGRAPH WIDTH",  "PATH START",
                            "LETTER SPACING (px)", "LINE SPACING (x)", "COLOR (#RRGGBBAA)"};
    const char *families[] = {"FONT: Liberation Sans >", "FONT: Liberation Serif >",
                              "FONT: Liberation Mono >"};
    const char *alignment[] = {"ALIGN: LEFT", "ALIGN: CENTER", "ALIGN: RIGHT"};
    for (uint32_t i = 0; i < 3; ++i) {
        const float fx = x + 20 + (float)i * 240;
        af_editor_add_rect(
            editor, fx, y + 52, 232, 34,
            editor->text_panel_draft.mode == (AfProjectTextMode)(i + 1U) ? active : idle, 260002);
        af_editor_add_text(editor, fx + 8, y + 63, modes[i], .85F, 216, white, 260003);
    }
    const float fx[] = {20, 370, 450, 530}, fw[] = {342, 72, 72, 210};
    const char *format[] = {families[editor->text_panel_draft.font_family], "BOLD", "ITALIC",
                            alignment[editor->text_panel_draft.alignment]};
    for (uint32_t i = 0; i < 4; ++i) {
        const bool enabled = i == 1   ? editor->text_panel_draft.bold
                             : i == 2 ? editor->text_panel_draft.italic
                                      : false;
        af_editor_add_rect(editor, x + fx[i], y + 100, fw[i], 34, enabled ? active : idle, 260002);
        af_editor_add_text(editor, x + fx[i] + 8, y + 112, format[i], .9F, fw[i] - 16, white,
                           260003);
    }
    for (uint32_t i = 0; i < 6; ++i) {
        const float bx = x + 20 + (float)(i % 3) * 240, by = y + 300 + (float)(i / 3) * 80;
        af_editor_add_text(editor, bx, by - 22, labels[i], .85F, 228, accent, 260003);
        af_editor_add_rect(editor, bx, by, 232, 40,
                           editor->text_panel_field == i + 1U ? active : idle, 260002);
        af_editor_add_text(editor, bx + 8, by + 12, editor->text_panel_numbers[i], 1, 216, white,
                           260003);
    }
    AfColor swatch;
    if (af_editor_inspector_parse_hex_color(editor->text_panel_numbers[5], &swatch))
        af_editor_add_rect(editor, x + 704, y + 388, 24, 24, swatch, 260003);
    af_editor_add_rect(editor, x + 20, y + 150, 720, 110,
                       editor->text_panel_field == 0 ? active : idle, 260002);
    const char *cursor = editor->text_panel_draft.utf8;
    for (uint32_t line = 0; line < 4 && *cursor; ++line) {
        char display[AF_PROJECT_TEXT_BYTES];
        const char *end = strchr(cursor, '\n');
        const size_t count = end ? (size_t)(end - cursor) : strlen(cursor);
        memcpy(display, cursor, count);
        display[count] = '\0';
        af_editor_add_text(editor, x + 28, y + 165 + (float)line * 23, display, 1.1F, 704, white,
                           260003);
        if (!end)
            break;
        cursor = end + 1;
    }
    af_editor_add_text(
        editor, x + 20, y + 440,
        "Tab: fields · Ctrl+A: replace · Ctrl+B/I: style · Ctrl+F: font · Shift+Enter: newline",
        .76F, 720, white, 260003);
    af_editor_add_text(editor, x + 20, y + 468, editor->status, .76F, 720, accent, 260003);
    af_editor_add_rect(editor, x + 20, y + 510, 340, 40, active, 260002);
    af_editor_add_rect(editor, x + 400, y + 510, 340, 40, idle, 260002);
    af_editor_add_text(editor, x + 32, y + 522, "APPLY (Enter)", 1, 316, white, 260003);
    af_editor_add_text(editor, x + 412, y + 522, "CANCEL (Esc)", 1, 316, white, 260003);
}

static void af_editor_finish_text_tool(AfEditorState *editor, int width, int height) {
    const AfVec2 start = editor->tool_start, end = editor->tool_current;
    editor->drag_mode = AF_EDITOR_DRAG_NONE;
    if (af_editor_is_text(editor, editor->selected_id))
        af_editor_select_only(editor, 0);
    af_editor_open_text_panel(editor, width, height);
    if (!editor->text_panel_open)
        return;
    editor->text_panel_position = (AfVec2){fminf(start.x, end.x), fminf(start.y, end.y)};
    if (fabsf(end.x - start.x) * editor->zoom > 8) {
        editor->text_panel_draft.mode = AF_PROJECT_TEXT_PARAGRAPH;
        (void)snprintf(editor->text_panel_numbers[1], 32, "%.6g", (double)fabsf(end.x - start.x));
    }
    af_editor_status(editor,
                     "Type text · choose font and formatting · Enter applies · Esc cancels");
}
