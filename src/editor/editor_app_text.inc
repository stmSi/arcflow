static bool af_editor_is_text(const AfEditorState *editor, uint64_t id) {
    const uint32_t index = af_project_find_index(&editor->project, id);
    return index != UINT32_MAX && editor->project.texts[index].mode != AF_PROJECT_TEXT_NONE;
}

static uint64_t af_editor_document_font(AfEditorState *editor, const AfProjectText *text) {
    if (text->font_name[0]) {
        const AfEditorFontFace *face =
            af_editor_fonts_find(&editor->font_catalog, text->font_name, text->bold, text->italic);
        if (!face)
            return 0;
        if (editor->system_font_face == face)
            return 14;
        AfFileData file = {0};
        AfTextEngine *engine = NULL;
        AfResult result =
            af_file_read_limited(face->path, 64U * 1024U * 1024U, &editor->allocator, &file);
        if (result == AF_RESULT_SUCCESS)
            result = af_text_engine_create(&editor->allocator, &engine);
        if (result == AF_RESULT_SUCCESS)
            result = af_text_font_load_memory(engine, 14, file.data, file.size, face->face_index);
        if (result != AF_RESULT_SUCCESS) {
            af_text_engine_destroy(engine);
            af_file_data_destroy(&editor->allocator, &file);
            return 0;
        }
        af_text_engine_destroy(editor->system_font_engine);
        af_file_data_destroy(&editor->allocator, &editor->system_font_file);
        editor->system_font_engine = engine;
        editor->system_font_file = file;
        editor->system_font_face = face;
        return 14;
    }
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
    if (!editor->font_catalog.faces) {
        char bundled[AF_EDITOR_PATH_CAPACITY];
        SDL_PathInfo info;
        const char *directory = ARCFLOW_DOCUMENT_FONT_DIR;
        if (!SDL_GetPathInfo(directory, &info) &&
            af_platform_resource_path("fonts", bundled, sizeof(bundled)) == AF_RESULT_SUCCESS)
            directory = bundled;
        (void)af_editor_fonts_discover(&editor->font_catalog, directory);
    }
    editor->font_search[0] = '\0';
    editor->font_scroll = 0;
    editor->text_caret = strlen(editor->text_panel_draft.utf8);
    editor->text_anchor = 0;
    editor->text_preview_hash = 0;
    editor->text_preview_valid = false;
    (void)af_platform_text_input_set(&editor->window, true);
    editor->text_panel_open = true;
    editor->text_panel_field = 0;
    editor->text_panel_replace = true;
    editor->preview_playing = false;
}

static char *af_editor_text_panel_buffer(AfEditorState *editor, size_t *capacity) {
    if (editor->text_panel_field == 7) {
        *capacity = sizeof(editor->font_search);
        return editor->font_search;
    }
    *capacity = editor->text_panel_field ? 32U : AF_PROJECT_TEXT_BYTES;
    return editor->text_panel_field ? editor->text_panel_numbers[editor->text_panel_field - 1U]
                                    : editor->text_panel_draft.utf8;
}
static size_t af_editor_text_previous(const char *text, size_t offset) {
    if (offset)
        --offset;
    while (offset && ((unsigned char)text[offset] & 0xc0U) == 0x80U)
        --offset;
    return offset;
}
static size_t af_editor_text_next(const char *text, size_t offset) {
    if (text[offset])
        ++offset;
    while (((unsigned char)text[offset] & 0xc0U) == 0x80U)
        ++offset;
    return offset;
}
static void af_editor_text_focus(AfEditorState *editor, uint32_t field, bool select_all) {
    editor->text_panel_field = field;
    size_t capacity;
    editor->text_caret = strlen(af_editor_text_panel_buffer(editor, &capacity));
    editor->text_anchor = select_all ? 0 : editor->text_caret;
    editor->text_panel_replace = select_all;
}
static void af_editor_text_close(AfEditorState *editor) {
    editor->text_panel_open = false;
    editor->text_dragging = false;
    editor->text_preview_valid = false;
    (void)af_platform_text_input_set(&editor->window, false);
}
static void af_editor_text_panel_append(AfEditorState *editor, const char *text) {
    size_t capacity;
    char *buffer = af_editor_text_panel_buffer(editor, &capacity);
    size_t length = strlen(buffer), added = strlen(text);
    size_t first =
        editor->text_caret < editor->text_anchor ? editor->text_caret : editor->text_anchor;
    size_t last =
        editor->text_caret > editor->text_anchor ? editor->text_caret : editor->text_anchor;
    if (editor->text_panel_replace) {
        first = 0;
        last = length;
    }
    if (first > length)
        first = length;
    if (last > length)
        last = length;
    if (added >= capacity - (length - (last - first)) ||
        (added && !af_text_utf8_valid(text, (uint32_t)added))) {
        af_editor_status(editor, "Text field capacity reached or invalid UTF-8");
        return;
    }
    memmove(buffer + first + added, buffer + last, length - last + 1);
    memcpy(buffer + first, text, added);
    editor->text_caret = editor->text_anchor = first + added;
    editor->text_panel_replace = false;
    if (editor->text_panel_field == 7)
        editor->font_scroll = 0;
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
        af_editor_status(
            editor, "Font or requested bold/italic face is unavailable; choose an installed face");
        return;
    }
    if (text.mode != AF_PROJECT_TEXT_ON_PATH)
        text.guide_count = 0;
    AfEditorCommand command;
    AfResult result = af_editor_command_text(
        &editor->project, text.font_name[0] ? editor->system_font_engine : editor->text_engine,
        font_id, editor->text_panel_id, editor->text_panel_position, &text, &command);
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
    af_editor_text_close(editor);
    af_editor_select_only(editor, command.target_id);
    af_editor_set_vector_tool(editor, AF_EDITOR_TOOL_SELECT);
    af_editor_mark_dirty(editor);
    af_editor_status(editor, "Text saved · double-click to edit on canvas · T creates text");
}

static const char *af_editor_text_family(const AfProjectText *text) {
    static const char *names[] = {"Liberation Sans", "Liberation Serif", "Liberation Mono"};
    return text->font_name[0] ? text->font_name : names[text->font_family];
}
static AfAffine2 af_editor_text_frame(AfEditorState *editor, int width, int height) {
    const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->text_panel_id);
    AfAffine2 frame = node ? node->world_transform : af_affine_identity();
    if (!node) {
        frame.tx = editor->text_panel_position.x;
        frame.ty = editor->text_panel_position.y;
    }
    return af_affine_multiply(af_editor_camera(editor, width, height), frame);
}
static void af_editor_text_preview_update(AfEditorState *editor) {
    uint64_t hash =
        af_hash_bytes(&editor->text_panel_draft, sizeof(editor->text_panel_draft)) ^
        af_hash_bytes(editor->text_panel_numbers, sizeof(editor->text_panel_numbers)) ^
        af_hash_bytes(&editor->text_panel_position, sizeof(editor->text_panel_position));
    if (hash == editor->text_preview_hash)
        return;
    editor->text_preview_hash = hash;
    editor->text_preview_valid = false;
    memset(editor->text_caret_valid, 0, sizeof(editor->text_caret_valid));
    AfProjectText text = editor->text_panel_draft;
    float *fields[] = {&text.font_size, &text.width, &text.offset, &text.letter_spacing,
                       &text.line_spacing};
    for (uint32_t i = 0; i < 5; ++i) {
        char *end;
        *fields[i] = strtof(editor->text_panel_numbers[i], &end);
        if (end == editor->text_panel_numbers[i] || *end || !isfinite(*fields[i]))
            return;
    }
    if (text.mode != AF_PROJECT_TEXT_ON_PATH)
        text.guide_count = 0;
    editor->text_caret_height = text.font_size;
    editor->text_carets[0] = (AfVec2){0, text.font_size};
    editor->text_caret_valid[0] = true;
    editor->text_caret_tops[0] = (AfVec2){0};
    if (!text.utf8[0])
        return;
    if (!af_project_text_valid(&text))
        return;
    const uint64_t font = af_editor_document_font(editor, &text);
    if (!font) {
        af_editor_status(editor, "Font/style unavailable · choose a family or disable bold/italic");
        return;
    }
    AfTextEngine *engine = text.font_name[0] ? editor->system_font_engine : editor->text_engine;
    if (!editor->text_preview)
        editor->text_preview = calloc(1, sizeof(*editor->text_preview));
    if (!editor->text_preview)
        return;
    AfResult result =
        af_editor_command_text(&editor->project, engine, font, editor->text_panel_id,
                               editor->text_panel_position, &text, editor->text_preview);
    if (result != AF_RESULT_SUCCESS) {
        af_editor_status(editor,
                         "Text preview unavailable · check glyph support, size and path length");
        return;
    }
    editor->text_preview_valid = true;
    AfColor color;
    if (af_editor_inspector_parse_hex_color(editor->text_panel_numbers[5], &color) &&
        (!editor->text_panel_id ||
         strcmp(editor->text_panel_numbers[5], editor->text_panel_original_color))) {
        editor->text_preview->after[0].node.fill = color;
        editor->text_preview->after[0].style.fill = af_paint_solid(color);
        editor->text_preview->after[0].style.inherit_flags &= ~(uint32_t)AF_STYLE_INHERIT_FILL;
        editor->text_preview->after[0].style.paint_generation = hash ? hash : 1;
    }
    const uint32_t style_count = editor->project.style_data.count + 1;
    if (editor->text_preview_styles.capacity < style_count) {
        AfStyle *styles =
            realloc(editor->text_preview_styles.styles, style_count * sizeof(AfStyle));
        if (!styles) {
            editor->text_preview_valid = false;
            return;
        }
        editor->text_preview_styles.styles = styles;
        editor->text_preview_styles.capacity = style_count;
    }
    memcpy(editor->text_preview_styles.styles, editor->project.style_data.styles,
           editor->project.style_data.count * sizeof(AfStyle));
    editor->text_preview_styles.count = editor->project.style_data.count;
    bool replaced = false;
    for (uint32_t i = 0; i < editor->text_preview_styles.count; ++i)
        if (editor->text_preview_styles.styles[i].node_id == editor->text_panel_id) {
            editor->text_preview_styles.styles[i] = editor->text_preview->after[0].style;
            replaced = true;
            break;
        }
    if (!replaced)
        editor->text_preview_styles.styles[editor->text_preview_styles.count++] =
            editor->text_preview->after[0].style;
    AfGlyphPlacement glyphs[AF_PROJECT_TEXT_BYTES];
    AfTextLine lines[AF_MAX_TEXT_LINES];
    AfTextLayout layout = {.glyphs = glyphs,
                           .glyph_capacity = AF_PROJECT_TEXT_BYTES,
                           .lines = lines,
                           .line_capacity = AF_MAX_TEXT_LINES};
    AfTextRequest request = {
        .utf8 = text.utf8,
        .byte_count = (uint32_t)strlen(text.utf8),
        .font_ids = {font},
        .font_count = 1,
        .font_size = text.font_size,
        .maximum_width = text.mode == AF_PROJECT_TEXT_PARAGRAPH ? text.width : 0,
        .maximum_lines = AF_MAX_TEXT_LINES,
        .line_spacing = text.line_spacing > 0 ? text.line_spacing : 1.2F,
        .letter_spacing = text.letter_spacing,
        .alignment = text.mode == AF_PROJECT_TEXT_ON_PATH ? AF_TEXT_ALIGN_START
                                                          : (AfTextAlignment)text.alignment,
        .wrap = text.mode == AF_PROJECT_TEXT_PARAGRAPH ? AF_TEXT_WRAP_WORD : AF_TEXT_WRAP_NONE,
        .kerning = true,
        .ligatures = text.letter_spacing == 0};
    if (af_text_shape(engine, &request, &layout) != AF_RESULT_SUCCESS)
        return;
    for (uint32_t i = 0; i < layout.glyph_count; ++i) {
        const AfGlyphPlacement *glyph = &glyphs[i];
        if (glyph->cluster >= AF_PROJECT_TEXT_BYTES)
            continue;
        editor->text_carets[glyph->cluster] = glyph->position;
        editor->text_caret_valid[glyph->cluster] = true;
        size_t next = af_editor_text_next(text.utf8, glyph->cluster);
        if (next < AF_PROJECT_TEXT_BYTES) {
            editor->text_carets[next] =
                (AfVec2){glyph->position.x + glyph->advance.x, glyph->position.y};
            editor->text_caret_valid[next] = true;
        }
    }
    /* Newline positions, including a trailing empty line, are valid caret stops. */
    for (size_t i = 0; text.utf8[i]; i = af_editor_text_next(text.utf8, i)) {
        if (text.utf8[i] == '\n' && !editor->text_caret_valid[i + 1]) {
            editor->text_carets[i + 1] =
                (AfVec2){0, editor->text_carets[i].y + text.font_size * request.line_spacing};
            editor->text_caret_valid[i + 1] = true;
        }
        if (!editor->text_caret_valid[i]) {
            editor->text_carets[i] = editor->text_carets[af_editor_text_previous(text.utf8, i)];
            editor->text_caret_valid[i] = true;
        }
    }
    for (size_t i = 0; i <= strlen(text.utf8); ++i) {
        editor->text_caret_tops[i] = editor->text_carets[i];
        editor->text_caret_tops[i].y -= text.font_size;
    }
    if (text.mode == AF_PROJECT_TEXT_ON_PATH) {
        AfVec2 points[AF_MAX_PATH_FLATTENED_POINTS];
        AfPathContour contours[AF_MAX_PATH_CONTOURS];
        AfFlattenedPath flat = {.points = points,
                                .point_capacity = AF_MAX_PATH_FLATTENED_POINTS,
                                .contours = contours,
                                .contour_capacity = AF_MAX_PATH_CONTOURS};
        AfPath path = {
            .id = 1, .node_id = 1, .geometry_generation = 1, .command_count = text.guide_count};
        AfPathData data = {.paths = &path,
                           .path_count = 1,
                           .path_capacity = 1,
                           .commands = text.guide,
                           .command_count = text.guide_count,
                           .command_capacity = AF_PROJECT_TEXT_GUIDE_COMMANDS};
        if (af_path_flatten(&data, &path, .1F, &flat) != AF_RESULT_SUCCESS)
            return;
        const uint32_t segments = contours[0].point_count - (contours[0].closed ? 0U : 1U);
        float length = 0;
        for (uint32_t j = 0; j < segments; ++j) {
            AfVec2 a = points[j], b = points[(j + 1) % contours[0].point_count];
            length += hypotf(b.x - a.x, b.y - a.y);
        }
        const float start = text.offset + (length - layout.bounds.x) * (text.alignment == 1   ? .5F
                                                                        : text.alignment == 2 ? 1
                                                                                              : 0);
        for (size_t i = 0; i <= strlen(text.utf8); ++i) {
            if (!editor->text_caret_valid[i])
                continue;
            float distance = start + editor->text_carets[i].x;
            for (uint32_t j = 0; j < segments; ++j) {
                AfVec2 a = points[j], b = points[(j + 1) % contours[0].point_count];
                float span = hypotf(b.x - a.x, b.y - a.y);
                if (span < 1e-6F)
                    continue;
                if (distance <= span || j + 1 == segments) {
                    const float dx = (b.x - a.x) / span, dy = (b.y - a.y) / span;
                    editor->text_carets[i] = (AfVec2){a.x + dx * distance, a.y + dy * distance};
                    editor->text_caret_tops[i] =
                        (AfVec2){editor->text_carets[i].x + dy * text.font_size,
                                 editor->text_carets[i].y - dx * text.font_size};
                    break;
                }
                distance -= span;
            }
        }
    }
}
static void af_editor_render_text_canvas(AfEditorState *editor, int width, int height) {
    if (!editor->text_panel_open)
        return;
    af_editor_text_preview_update(editor);
    const AfAffine2 frame = af_editor_text_frame(editor, width, height);
    if (editor->text_preview_valid) {
        editor->render_scene.styles = &editor->text_preview_styles;
        const uint32_t start = editor->render_path_data.command_count;
        const uint32_t count = editor->text_preview->path_command_after_count;
        if (count <= editor->render_path_data.command_capacity - start) {
            memcpy(editor->render_path_data.commands + start,
                   editor->text_preview->path_command_after, count * sizeof(AfPathCommand));
            editor->render_path_data.command_count += count;
            AfSceneNode *existing = af_scene_find(&editor->render_scene, editor->text_panel_id);
            AfPath *path = af_path_find_node(&editor->render_path_data, editor->text_panel_id);
            if (existing && path) {
                path->command_start = start;
                path->command_count = count;
                path->geometry_generation =
                    editor->text_preview_hash ? editor->text_preview_hash : 1;
                existing->bounds = existing->resolved_bounds =
                    editor->text_preview->after[0].node.bounds;
            } else {
                uint32_t node_count = editor->render_scene.count;
                af_editor_finish_text_path(editor, start, (AfVec2){0},
                                           editor->text_preview->after[0].node.fill, 80000);
                if (editor->render_scene.count > node_count) {
                    editor->render_scene.nodes[node_count].world_transform = frame;
                    editor->text_preview_styles.styles[editor->text_preview_styles.count - 1]
                        .node_id = editor->render_scene.nodes[node_count].id;
                }
            }
        }
    }
    if (editor->text_panel_field != 0)
        return;
    const size_t first =
        editor->text_caret < editor->text_anchor ? editor->text_caret : editor->text_anchor;
    const size_t last =
        editor->text_caret > editor->text_anchor ? editor->text_caret : editor->text_anchor;
    const char *text = editor->text_panel_draft.utf8;
    for (size_t i = first; i < last && text[i]; i = af_editor_text_next(text, i)) {
        const size_t next = af_editor_text_next(text, i);
        AfVec2 a = editor->text_carets[i], b = editor->text_carets[next];
        if (editor->text_panel_draft.mode != AF_PROJECT_TEXT_ON_PATH && fabsf(a.y - b.y) > 1)
            b = (AfVec2){a.x + editor->text_caret_height * .3F, a.y};
        a.x += (editor->text_caret_tops[i].x - a.x) * .4F;
        a.y += (editor->text_caret_tops[i].y - a.y) * .4F;
        b.x += (editor->text_caret_tops[next].x - b.x) * .4F;
        b.y += (editor->text_caret_tops[next].y - b.y) * .4F;
        af_editor_add_line(editor, af_affine_transform_point(frame, a),
                           af_affine_transform_point(frame, b),
                           editor->text_caret_height * hypotf(frame.m01, frame.m11),
                           (AfColor){.25F, .55F, 1, .3F}, 80001);
    }
    if ((SDL_GetTicks() / 500) % 2 == 0 && editor->text_caret < AF_PROJECT_TEXT_BYTES) {
        AfVec2 bottom = editor->text_carets[editor->text_caret],
               top = editor->text_caret_tops[editor->text_caret];
        bottom = af_affine_transform_point(frame, bottom);
        top = af_affine_transform_point(frame, top);
        af_editor_add_line(editor, top, bottom, 2, (AfColor){.6F, .85F, 1, 1}, 80002);
        SDL_Rect area = {(int)top.x, (int)top.y, 2,
                         (int)fmaxf(1, hypotf(bottom.x - top.x, bottom.y - top.y))};
        (void)SDL_SetTextInputArea(editor->window.window, &area, 0);
    }
}
static uint32_t af_editor_text_font_results(AfEditorState *editor, uint32_t *indices,
                                            uint32_t capacity);
static void af_editor_text_choose_font(AfEditorState *editor, uint32_t index) {
    const AfEditorFontFace *face = &editor->font_catalog.faces[index];
    (void)snprintf(editor->text_panel_draft.font_name, sizeof(editor->text_panel_draft.font_name),
                   "%s", face->family);
    if (!af_editor_fonts_find(&editor->font_catalog, face->family, editor->text_panel_draft.bold,
                              editor->text_panel_draft.italic)) {
        editor->text_panel_draft.bold = face->bold;
        editor->text_panel_draft.italic = face->italic;
    }
    af_editor_text_focus(editor, 0, false);
}
static void af_editor_text_panel_key(AfEditorState *editor, const SDL_KeyboardEvent *event) {
    const bool control = (event->mod & SDL_KMOD_CTRL) != 0,
               shift = (event->mod & SDL_KMOD_SHIFT) != 0;
    size_t capacity;
    char *buffer = af_editor_text_panel_buffer(editor, &capacity);
    const size_t length = strlen(buffer);
    if (editor->text_panel_field == 7 &&
        (event->key == SDLK_UP || event->key == SDLK_DOWN || event->key == SDLK_RETURN)) {
        uint32_t indices[11];
        uint32_t count = af_editor_text_font_results(editor, indices, 11);
        if (event->key == SDLK_RETURN && count)
            af_editor_text_choose_font(editor, indices[0]);
        else if (event->key == SDLK_DOWN && count > 1)
            ++editor->font_scroll;
        else if (event->key == SDLK_UP && editor->font_scroll)
            --editor->font_scroll;
        return;
    }
    if (event->key == SDLK_ESCAPE) {
        if (editor->text_panel_field == 7)
            af_editor_text_focus(editor, 0, false);
        else
            af_editor_text_close(editor);
    } else if (event->key == SDLK_TAB) {
        af_editor_text_focus(editor, (editor->text_panel_field + (shift ? 6U : 1U)) % 7U, true);
    } else if (event->key == SDLK_RETURN || event->key == SDLK_KP_ENTER) {
        if (control)
            af_editor_apply_text_panel(editor);
        else if (editor->text_panel_field == 0)
            af_editor_text_panel_append(editor, "\n");
        else
            af_editor_text_focus(editor, 0, false);
    } else if (control && event->key == SDLK_A) {
        editor->text_anchor = 0;
        editor->text_caret = length;
        editor->text_panel_replace = true;
    } else if (control && (event->key == SDLK_C || event->key == SDLK_X)) {
        size_t first =
            editor->text_anchor < editor->text_caret ? editor->text_anchor : editor->text_caret;
        size_t last =
            editor->text_anchor > editor->text_caret ? editor->text_anchor : editor->text_caret;
        if (last <= length && last > first) {
            char copy[AF_PROJECT_TEXT_BYTES];
            memcpy(copy, buffer + first, last - first);
            copy[last - first] = '\0';
            (void)SDL_SetClipboardText(copy);
            if (event->key == SDLK_X)
                af_editor_text_panel_append(editor, "");
        }
    } else if (control && event->key == SDLK_V) {
        char *clipboard = SDL_GetClipboardText();
        if (clipboard) {
            af_editor_text_panel_append(editor, clipboard);
            SDL_free(clipboard);
        }
    } else if (event->key == SDLK_BACKSPACE || event->key == SDLK_DELETE) {
        if (editor->text_caret == editor->text_anchor && !editor->text_panel_replace)
            editor->text_anchor = event->key == SDLK_BACKSPACE
                                      ? af_editor_text_previous(buffer, editor->text_caret)
                                      : af_editor_text_next(buffer, editor->text_caret);
        af_editor_text_panel_append(editor, "");
    } else if (event->key == SDLK_LEFT || event->key == SDLK_RIGHT || event->key == SDLK_HOME ||
               event->key == SDLK_END) {
        size_t pos = editor->text_caret;
        if (event->key == SDLK_LEFT)
            pos = af_editor_text_previous(buffer, pos);
        if (event->key == SDLK_RIGHT)
            pos = af_editor_text_next(buffer, pos);
        if (event->key == SDLK_HOME) {
            while (pos && buffer[pos - 1] != '\n')
                --pos;
            if (control)
                pos = 0;
        }
        if (event->key == SDLK_END) {
            while (buffer[pos] && buffer[pos] != '\n')
                ++pos;
            if (control)
                pos = length;
        }
        editor->text_caret = pos;
        if (!shift)
            editor->text_anchor = pos;
        editor->text_panel_replace = false;
    } else if ((event->key == SDLK_UP || event->key == SDLK_DOWN) &&
               editor->text_panel_field == 0) {
        af_editor_text_preview_update(editor);
        const AfVec2 current = editor->text_carets[editor->text_caret];
        const float direction = event->key == SDLK_UP ? -1 : 1;
        size_t best = editor->text_caret;
        float best_score = INFINITY;
        for (size_t i = 0; i <= length; ++i) {
            if (!editor->text_caret_valid[i])
                continue;
            AfVec2 point = editor->text_carets[i];
            const float dy = (point.y - current.y) * direction;
            if (dy <= 1)
                continue;
            const float score = dy * 1000 + fabsf(point.x - current.x);
            if (score < best_score) {
                best_score = score;
                best = i;
            }
        }
        editor->text_caret = best;
        if (!shift)
            editor->text_anchor = best;
        editor->text_panel_replace = false;
    } else if (control && event->key == SDLK_B)
        editor->text_panel_draft.bold = !editor->text_panel_draft.bold;
    else if (control && event->key == SDLK_I)
        editor->text_panel_draft.italic = !editor->text_panel_draft.italic;
    else if (control && event->key == SDLK_F)
        af_editor_text_focus(editor, 7, true);
    else if (control && event->key >= SDLK_1 && event->key <= SDLK_3)
        editor->text_panel_draft.mode = (AfProjectTextMode)(event->key - SDLK_1 + 1U);
}
static uint32_t af_editor_text_font_results(AfEditorState *editor, uint32_t *indices,
                                            uint32_t capacity) {
    uint32_t count = 0, matched = 0;
    for (uint32_t i = 0; i < editor->font_catalog.count; ++i) {
        const AfEditorFontFace *face = &editor->font_catalog.faces[i];
        if (i && strcmp(face->family, editor->font_catalog.faces[i - 1].family) == 0)
            continue;
        if (!SDL_strcasestr(face->family, editor->font_search))
            continue;
        if (matched++ < editor->font_scroll)
            continue;
        indices[count++] = i;
        if (count == capacity)
            break;
    }
    return count;
}
static void af_editor_text_panel_mouse(AfEditorState *editor, int width, int height, float x,
                                       float y) {
    const float left = editor->left_width + 6, top = editor->toolbar_height;
    const float available = (float)width - editor->left_width - editor->right_width - 12;
    const float cell = available / 6;
    if (editor->text_panel_field == 7 && x >= left && x < left + fminf(available, 360) &&
        y >= top + 108 && y < top + 388) {
        uint32_t indices[10];
        uint32_t count = af_editor_text_font_results(editor, indices, 10);
        uint32_t row = (uint32_t)((y - top - 108) / 28);
        if (row < count) {
            af_editor_text_choose_font(editor, indices[row]);
        }
        return;
    }
    if (x >= left && x < left + available && y >= top && y < top + 108) {
        uint32_t col = (uint32_t)((x - left) / cell), row = (uint32_t)((y - top) / 36);
        if (row == 0) {
            if (col < 2)
                af_editor_text_focus(editor, 7, true);
            else if (col == 2)
                af_editor_text_focus(editor, 1, true);
            else if (col == 3)
                editor->text_panel_draft.bold = !editor->text_panel_draft.bold;
            else if (col == 4)
                editor->text_panel_draft.italic = !editor->text_panel_draft.italic;
            else
                editor->text_panel_draft.alignment = (editor->text_panel_draft.alignment + 1) % 3;
        } else if (row == 1) {
            if (col == 0)
                editor->text_panel_draft.mode =
                    (AfProjectTextMode)(editor->text_panel_draft.mode % 3 + 1);
            else if (col < 5)
                af_editor_text_focus(editor, col + 1, true);
            else
                af_editor_text_focus(editor, 0, false);
        } else {
            if (col < 2)
                af_editor_text_focus(editor, 6, true);
            else if (col < 4)
                af_editor_apply_text_panel(editor);
            else
                af_editor_text_close(editor);
        }
        return;
    }
    float cx, cy, cw, ch;
    af_editor_layout(editor, width, height, &cx, &cy, &cw, &ch);
    if (x >= cx && x < cx + cw && y >= cy + 108 && y < cy + ch) {
        af_editor_text_preview_update(editor);
        const AfAffine2 frame = af_editor_text_frame(editor, width, height);
        size_t best = 0;
        float distance = INFINITY;
        for (size_t i = 0; i <= strlen(editor->text_panel_draft.utf8); ++i) {
            if (!editor->text_caret_valid[i])
                continue;
            AfVec2 point = editor->text_carets[i];
            point.x += (editor->text_caret_tops[i].x - point.x) * .4F;
            point.y += (editor->text_caret_tops[i].y - point.y) * .4F;
            point = af_affine_transform_point(frame, point);
            float d = hypotf(point.x - x, point.y - y);
            if (d < distance) {
                best = i;
                distance = d;
            }
        }
        editor->text_dragging = true;
        const size_t anchor = editor->text_anchor;
        const bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
        af_editor_text_focus(editor, 0, false);
        editor->text_caret = best;
        editor->text_anchor = shift ? anchor : best;
    } else {
        af_editor_apply_text_panel(editor);
    }
}
/* Match the UI label's shaped advances so numeric/search fields have a real caret. */
static float af_editor_text_field_x(AfEditorState *editor, const char *label, size_t byte,
                                    float width) {
    AfGlyphPlacement glyphs[AF_PROJECT_TEXT_BYTES];
    AfTextLine line;
    AfTextLayout layout = {.glyphs = glyphs,
                           .glyph_capacity = AF_PROJECT_TEXT_BYTES,
                           .lines = &line,
                           .line_capacity = 1};
    AfTextRequest request = {.utf8 = label,
                             .byte_count = (uint32_t)strlen(label),
                             .font_ids = {AF_EDITOR_TEXT_FONT_ID},
                             .font_count = 1,
                             .font_size = 12,
                             .maximum_width = width,
                             .maximum_lines = 1,
                             .line_spacing = 1,
                             .direction = AF_TEXT_DIRECTION_LTR,
                             .kerning = true,
                             .ligatures = true,
                             .truncate = true};
    if (!request.byte_count)
        return 0;
    for (;;) {
        if (af_text_shape(editor->text_engine, &request, &layout) != AF_RESULT_SUCCESS)
            return 0;
        if (layout.bounds.x <= width || request.font_size <= 6)
            break;
        request.font_size = fmaxf(
            6, fminf(request.font_size - 1, floorf(request.font_size * width / layout.bounds.x)));
    }
    float x = 0;
    for (uint32_t i = 0; i < layout.glyph_count; ++i) {
        if (glyphs[i].cluster >= byte)
            return glyphs[i].position.x;
        x = glyphs[i].position.x + glyphs[i].advance.x;
    }
    return fminf(width, x);
}
static void af_editor_render_text_panel(AfEditorState *editor, int width, int height) {
    (void)height;
    const float x = editor->left_width + 6, y = editor->toolbar_height;
    const float available = (float)width - editor->left_width - editor->right_width - 12;
    const AfColor white = {.9F, .94F, 1, 1}, active = {.18F, .35F, .52F, 1},
                  idle = {.13F, .17F, .23F, 1};
    if (!editor->text_panel_open) {
        if (af_editor_is_text(editor, editor->selected_id)) {
            const AfProjectText *text =
                &editor->project
                     .texts[af_project_find_index(&editor->project, editor->selected_id)];
            char label[256];
            (void)snprintf(label, sizeof(label),
                           "%s  ·  %.0f px  %s %s  ·  Double-click text to edit and format",
                           af_editor_text_family(text), (double)text->font_size,
                           text->bold ? "Bold" : "", text->italic ? "Italic" : "");
            af_editor_add_rect(editor, x - 6, y, available + 12, 36, idle, 100150);
            af_editor_add_text(editor, x + 6, y + 12, label, .9F, available - 12, white, 100153);
        }
        return;
    }
    const float cell = available / 6;
    af_editor_add_rect(editor, x - 6, y, available + 12, 108, (AfColor){.08F, .1F, .14F, 1},
                       100150);
    const char *align[] = {"Left", "Center", "Right"};
    const char *mode[] = {"", "Artistic", "Paragraph", "On path"};
    char size[64], frame[64], offset[64], tracking[64], leading[64], color[64];
    (void)snprintf(size, 64, "Size %s", editor->text_panel_numbers[0]);
    (void)snprintf(frame, 64, "Width %s", editor->text_panel_numbers[1]);
    (void)snprintf(offset, 64, "Start %s", editor->text_panel_numbers[2]);
    (void)snprintf(tracking, 64, "Tracking %s", editor->text_panel_numbers[3]);
    (void)snprintf(leading, 64, "Leading %s", editor->text_panel_numbers[4]);
    (void)snprintf(color, 64, "Color %s", editor->text_panel_numbers[5]);
    const char *labels[] = {editor->text_panel_field == 7
                                ? editor->font_search
                                : af_editor_text_family(&editor->text_panel_draft),
                            size,
                            "Bold",
                            "Italic",
                            align[editor->text_panel_draft.alignment],
                            mode[editor->text_panel_draft.mode],
                            frame,
                            offset,
                            tracking,
                            leading,
                            "Edit text",
                            color,
                            "Done (Ctrl+Enter)",
                            "Cancel (Esc)"};
    const uint32_t columns[] = {0, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 2, 4};
    const uint32_t rows[] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2};
    const uint32_t spans[] = {2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2};
    const uint32_t fields[] = {7, 1, 99, 99, 99, 99, 2, 3, 4, 5, 0, 6, 99, 99};
    for (uint32_t i = 0; i < 14; ++i) {
        float bx = x + (float)columns[i] * cell, by = y + (float)rows[i] * 36 + 3,
              bw = (float)spans[i] * cell - 4;
        bool selected = editor->text_panel_field == fields[i] ||
                        (i == 2 && editor->text_panel_draft.bold) ||
                        (i == 3 && editor->text_panel_draft.italic);
        af_editor_add_rect(editor, bx, by, bw, 30, selected ? active : idle, 100151);
        af_editor_add_text(editor, bx + 6, by + 10, labels[i], .85F, bw - 12, white, 100153);
        if (editor->text_panel_field == fields[i] && fields[i] != 0) {
            size_t capacity;
            const char *buffer = af_editor_text_panel_buffer(editor, &capacity);
            const size_t prefix = strlen(labels[i]) - strlen(buffer);
            const float caret =
                af_editor_text_field_x(editor, labels[i], prefix + editor->text_caret, bw - 12);
            const float anchor =
                af_editor_text_field_x(editor, labels[i], prefix + editor->text_anchor, bw - 12);
            if (caret != anchor)
                af_editor_add_rect(editor, bx + 6 + fminf(caret, anchor), by + 5,
                                   fabsf(caret - anchor), 20, (AfColor){.35F, .6F, 1, .35F},
                                   100154);
            if ((SDL_GetTicks() / 500) % 2 == 0)
                af_editor_add_rect(editor, bx + 6 + caret, by + 6, 1, 18, white, 100155);
        }
    }
    if (editor->text_panel_field == 7) {
        uint32_t indices[10];
        uint32_t count = af_editor_text_font_results(editor, indices, 10);
        const float fw = fminf(available, 360);
        af_editor_add_rect(editor, x, y + 108, fw, 308, idle, 100154);
        if (count)
            af_editor_add_rect(editor, x + 2, y + 110, fw - 4, 26, active, 100155);
        for (uint32_t i = 0; i < count; ++i)
            af_editor_add_text(editor, x + 8, y + 118 + (float)i * 28,
                               editor->font_catalog.faces[indices[i]].family, 1, fw - 16, white,
                               100156);
        af_editor_add_text(editor, x + 8, y + 398,
                           count ? "Type to filter · wheel to scroll"
                                 : "No installed families match",
                           .8F, fw - 16, white, 100155);
    }
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
                     "Type text · choose font and formatting · Ctrl+Enter saves · Esc cancels");
}
