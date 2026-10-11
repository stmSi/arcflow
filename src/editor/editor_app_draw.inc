static void af_editor_add_shape(AfEditorState *editor, AfNodeType type, AfAffine2 transform,
                                AfVec2 bounds, AfColor color, float radius, int32_t layer) {
    AfSceneNode *node = NULL;
    if (editor->render_scene.count >= editor->render_scene.capacity) {
        return;
    }
    node = &editor->render_scene.nodes[editor->render_scene.count++];
    *node = (AfSceneNode){.id = AF_EDITOR_UI_NODE_ID_BASE + editor->render_scene.count,
                          .parent_id = AF_INVALID_NODE_ID,
                          .type = type,
                          .bounds = bounds,
                          .resolved_bounds = bounds,
                          .fill = color,
                          .opacity = 1.0F,
                          .corner_radius = radius,
                          .layer_order = layer,
                          .visible = true,
                          .world_transform = transform,
                          .world_opacity = 1.0F,
                          .world_visible = true};
}

static void af_editor_add_rect(AfEditorState *editor, float x, float y, float width, float height,
                               AfColor color, int32_t layer) {
    af_editor_add_shape(editor, AF_NODE_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
                        (AfVec2){width, height}, color, 0.0F, layer);
}

static bool af_editor_hovered_rect(const AfEditorState *editor, float x, float y, float width,
                                   float height) {
    return editor->mouse_position.x >= x && editor->mouse_position.x <= x + width &&
           editor->mouse_position.y >= y && editor->mouse_position.y <= y + height;
}

static void af_editor_add_surface(AfEditorState *editor, float x, float y, float width,
                                  float height, AfColor color, float radius, int32_t layer) {
    if (width <= 0.0F || height <= 0.0F)
        return;
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1, .m11 = 1, .tx = x, .ty = y}, (AfVec2){width, height},
                        color, radius, layer);
}

static float af_editor_button_feedback(AfEditorState *editor, float x, float y, float width,
                                       float height, int32_t layer) {
    const float bounds[] = {x, y, width, height, (float)layer};
    const uint64_t key = af_hash_bytes(bounds, sizeof(bounds));
    uint32_t slot = (uint32_t)(key % 256U);
    for (uint32_t probe = 0; probe < 8; ++probe) {
        const AfEditorHoverFeedback *entry = &editor->hover_feedback[slot];
        if (entry->key == key || entry->key == 0 || editor->rendered_frames - entry->frame > 1U)
            break;
        slot = (slot + 1U) % 256U;
    }
    AfEditorHoverFeedback *feedback = &editor->hover_feedback[slot];
    const bool covered =
        layer < 210000 &&
        (editor->command_palette.open || editor->top_menu != AF_EDITOR_MENU_NONE ||
         editor->context_menu != AF_EDITOR_CONTEXT_NONE || editor->choice_menu ||
         editor->shortcuts_overlay || editor->welcome_visible || editor->template_panel_visible ||
         editor->recent_panel_visible || editor->export_panel_visible || editor->offset_panel_open);
    const float target = !covered && af_editor_hovered_rect(editor, x, y, width, height) ? 1 : 0;
    if (feedback->key != key || editor->rendered_frames - feedback->frame > 1U) {
        *feedback = (AfEditorHoverFeedback){.key = key, .amount = 0};
    }
    const float blend = editor->preferences.accessibility.reduced_motion != AF_REDUCED_MOTION_OFF
                            ? 1.0F
                            : 1.0F - expf(-22.0F * editor->ui_delta_seconds);
    feedback->amount += (target - feedback->amount) * blend;
    feedback->frame = editor->rendered_frames;
    return feedback->amount;
}

static void af_editor_add_button_rounded(AfEditorState *editor, float x, float y, float width,
                                         float height, AfColor color, float radius, int32_t layer) {
    const float feedback = af_editor_button_feedback(editor, x, y, width, height, layer);
    const bool pressed = feedback > 0.1F && af_editor_hovered_rect(editor, x, y, width, height) &&
                         (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON_LMASK) != 0;
    const float lift = feedback * (pressed ? -0.025F : 0.055F);
    color.r = af_editor_clamp(color.r + lift, 0, 1);
    color.g = af_editor_clamp(color.g + lift, 0, 1);
    color.b = af_editor_clamp(color.b + lift, 0, 1);
    AfColor border = {color.r + 0.045F + feedback * 0.07F, color.g + 0.045F + feedback * 0.055F,
                      color.b + 0.06F + feedback * 0.10F, color.a};
    border.r = af_editor_clamp(border.r, 0, 1);
    border.g = af_editor_clamp(border.g, 0, 1);
    border.b = af_editor_clamp(border.b, 0, 1);
    if (editor->preferences.accessibility.high_contrast)
        border = AF_UI_MUTED;
    af_editor_add_surface(editor, x, y, width, height, border, radius, layer);
    af_editor_add_surface(editor, x + 1, y + 1, width - 2, height - 2, color, fmaxf(0, radius - 1),
                          layer + 1);
}

static void af_editor_add_button_rect(AfEditorState *editor, float x, float y, float width,
                                      float height, AfColor color, int32_t layer) {
    af_editor_add_button_rounded(editor, x, y, width, height, color, fminf(6.0F, height * 0.25F),
                                 layer);
}

static void af_editor_add_line(AfEditorState *editor, AfVec2 start, AfVec2 end, float thickness,
                               AfColor color, int32_t layer) {
    const float x = end.x - start.x;
    const float y = end.y - start.y;
    const float length = sqrtf(x * x + y * y);
    const float cosine = length > 0.0F ? x / length : 1.0F;
    const float sine = length > 0.0F ? y / length : 0.0F;
    af_editor_add_shape(editor, AF_NODE_RECTANGLE,
                        (AfAffine2){.m00 = cosine,
                                    .m01 = -sine,
                                    .m10 = sine,
                                    .m11 = cosine,
                                    .tx = start.x + sine * thickness * 0.5F,
                                    .ty = start.y - cosine * thickness * 0.5F},
                        (AfVec2){length, thickness}, color, 0.0F, layer);
}

static const uint8_t *af_editor_glyph(char character) {
    static const uint8_t letters[26][5] = {
        {0x7EU, 0x11U, 0x11U, 0x11U, 0x7EU}, {0x7FU, 0x49U, 0x49U, 0x49U, 0x36U},
        {0x3EU, 0x41U, 0x41U, 0x41U, 0x22U}, {0x7FU, 0x41U, 0x41U, 0x22U, 0x1CU},
        {0x7FU, 0x49U, 0x49U, 0x49U, 0x41U}, {0x7FU, 0x09U, 0x09U, 0x09U, 0x01U},
        {0x3EU, 0x41U, 0x49U, 0x49U, 0x7AU}, {0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU},
        {0x00U, 0x41U, 0x7FU, 0x41U, 0x00U}, {0x20U, 0x40U, 0x41U, 0x3FU, 0x01U},
        {0x7FU, 0x08U, 0x14U, 0x22U, 0x41U}, {0x7FU, 0x40U, 0x40U, 0x40U, 0x40U},
        {0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU}, {0x7FU, 0x04U, 0x08U, 0x10U, 0x7FU},
        {0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU}, {0x7FU, 0x09U, 0x09U, 0x09U, 0x06U},
        {0x3EU, 0x41U, 0x51U, 0x21U, 0x5EU}, {0x7FU, 0x09U, 0x19U, 0x29U, 0x46U},
        {0x46U, 0x49U, 0x49U, 0x49U, 0x31U}, {0x01U, 0x01U, 0x7FU, 0x01U, 0x01U},
        {0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU}, {0x1FU, 0x20U, 0x40U, 0x20U, 0x1FU},
        {0x3FU, 0x40U, 0x38U, 0x40U, 0x3FU}, {0x63U, 0x14U, 0x08U, 0x14U, 0x63U},
        {0x07U, 0x08U, 0x70U, 0x08U, 0x07U}, {0x61U, 0x51U, 0x49U, 0x45U, 0x43U}};
    static const uint8_t digits[10][5] = {
        {0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU}, {0x00U, 0x42U, 0x7FU, 0x40U, 0x00U},
        {0x42U, 0x61U, 0x51U, 0x49U, 0x46U}, {0x21U, 0x41U, 0x45U, 0x4BU, 0x31U},
        {0x18U, 0x14U, 0x12U, 0x7FU, 0x10U}, {0x27U, 0x45U, 0x45U, 0x45U, 0x39U},
        {0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U}, {0x01U, 0x71U, 0x09U, 0x05U, 0x03U},
        {0x36U, 0x49U, 0x49U, 0x49U, 0x36U}, {0x06U, 0x49U, 0x49U, 0x29U, 0x1EU}};
    static const uint8_t unknown[5] = {0x02U, 0x01U, 0x51U, 0x09U, 0x06U};
    static const uint8_t dash[5] = {0x08U, 0x08U, 0x08U, 0x08U, 0x08U};
    static const uint8_t dot[5] = {0x00U, 0x60U, 0x60U, 0x00U, 0x00U};
    static const uint8_t colon[5] = {0x00U, 0x36U, 0x36U, 0x00U, 0x00U};
    static const uint8_t slash[5] = {0x20U, 0x10U, 0x08U, 0x04U, 0x02U};
    static const uint8_t plus[5] = {0x08U, 0x08U, 0x3EU, 0x08U, 0x08U};
    static const uint8_t percent[5] = {0x63U, 0x13U, 0x08U, 0x64U, 0x63U};
    static const uint8_t equals[5] = {0x14U, 0x14U, 0x14U, 0x14U, 0x14U};
    static const uint8_t underscore[5] = {0x40U, 0x40U, 0x40U, 0x40U, 0x40U};
    if (character >= 'a' && character <= 'z') {
        character = (char)(character - 'a' + 'A');
    }
    if (character >= 'A' && character <= 'Z') {
        return letters[(uint32_t)(character - 'A')];
    }
    if (character >= '0' && character <= '9') {
        return digits[(uint32_t)(character - '0')];
    }
    switch (character) {
    case ' ':
        return NULL;
    case '-':
        return dash;
    case '.':
    case ',':
        return dot;
    case ':':
        return colon;
    case '/':
        return slash;
    case '+':
        return plus;
    case '%':
        return percent;
    case '=':
        return equals;
    case '_':
        return underscore;
    default:
        return unknown;
    }
}

static void af_editor_add_bitmap_text(AfEditorState *editor, float x, float y, const char *text,
                                      float scale, float maximum_width, AfColor color,
                                      int32_t layer) {
    float cursor = x;
    if (text == NULL || scale <= 0.0F || maximum_width <= 0.0F) {
        return;
    }
    for (size_t index = 0U; text[index] != '\0' && cursor + 5.0F * scale <= x + maximum_width;
         ++index) {
        const uint8_t *columns = af_editor_glyph(text[index]);
        if (columns != NULL) {
            uint8_t rows[7] = {0};
            for (uint32_t row = 0U; row < 7U; ++row) {
                for (uint32_t column = 0U; column < 5U; ++column) {
                    if ((columns[column] & (1U << row)) != 0U) {
                        rows[row] |= (uint8_t)(1U << column);
                    }
                }
            }
            for (;;) {
                uint32_t best_area = 0U;
                uint32_t best_row = 0U;
                uint32_t best_column = 0U;
                uint32_t best_width = 0U;
                uint32_t best_height = 0U;
                for (uint32_t row = 0U; row < 7U; ++row) {
                    for (uint32_t first = 0U; first < 5U; ++first) {
                        uint8_t mask = 0U;
                        if ((rows[row] & (1U << first)) == 0U) {
                            continue;
                        }
                        for (uint32_t last = first; last < 5U; ++last) {
                            if ((rows[row] & (1U << last)) == 0U) {
                                break;
                            }
                            mask |= (uint8_t)(1U << last);
                            for (uint32_t bottom = row; bottom < 7U; ++bottom) {
                                const uint32_t area = (last - first + 1U) * (bottom - row + 1U);
                                if ((rows[bottom] & mask) != mask) {
                                    break;
                                }
                                if (area > best_area) {
                                    best_area = area;
                                    best_row = row;
                                    best_column = first;
                                    best_width = last - first + 1U;
                                    best_height = bottom - row + 1U;
                                }
                            }
                        }
                    }
                }
                if (best_area == 0U) {
                    break;
                }
                af_editor_add_rect(editor, cursor + (float)best_column * scale,
                                   y + (float)best_row * scale, (float)best_width * scale,
                                   (float)best_height * scale, color, layer);
                const uint8_t clear_mask = (uint8_t)(((1U << best_width) - 1U) << best_column);
                for (uint32_t row = best_row; row < best_row + best_height; ++row) {
                    rows[row] &= (uint8_t)~clear_mask;
                }
            }
        }
        cursor += 6.0F * scale;
    }
}

static void af_editor_finish_text_path(AfEditorState *editor, uint32_t command_start, AfVec2 origin,
                                       AfColor color, int32_t layer) {
    const uint32_t command_count = editor->render_path_data.command_count - command_start;
    AfSceneNode *node = NULL;
    AfPath *path = NULL;
    uint64_t generation = 0U;
    uint64_t path_id = 0U;
    if (command_count == 0U || command_count > AF_MAX_PATH_COMMANDS_PER_PATH ||
        editor->render_scene.count >= editor->render_scene.capacity ||
        editor->render_path_data.path_count >= editor->render_path_data.path_capacity) {
        editor->render_path_data.command_count = command_start;
        return;
    }
    node = &editor->render_scene.nodes[editor->render_scene.count++];
    *node =
        (AfSceneNode){.id = AF_EDITOR_UI_NODE_ID_BASE + editor->render_scene.count,
                      .parent_id = AF_INVALID_NODE_ID,
                      .type = AF_NODE_PATH,
                      .bounds = {1.0F, 1.0F},
                      .resolved_bounds = {1.0F, 1.0F},
                      .fill = color,
                      .opacity = 1.0F,
                      .layer_order = layer,
                      .visible = true,
                      .world_transform = {.m00 = 1.0F, .m11 = 1.0F, .tx = origin.x, .ty = origin.y},
                      .world_opacity = 1.0F,
                      .world_visible = true};
    generation = af_hash_bytes(&editor->render_path_data.commands[command_start],
                               (size_t)command_count * sizeof(AfPathCommand));
    /* Screen position and preceding UI nodes must not invalidate tessellated text.
     * Probe duplicate outlines so each instance still has a unique path record. */
    path_id = AF_EDITOR_UI_NODE_ID_BASE | (generation & UINT64_C(0x0FFFFFFFFFFFFFFF));
    while (af_path_find_const(&editor->render_path_data, path_id) != NULL) {
        path_id = AF_EDITOR_UI_NODE_ID_BASE | ((path_id + 1U) & UINT64_C(0x0FFFFFFFFFFFFFFF));
    }
    path = &editor->render_path_data.paths[editor->render_path_data.path_count++];
    *path = (AfPath){.id = path_id,
                     .node_id = node->id,
                     .command_start = command_start,
                     .command_count = command_count,
                     .fill_rule = AF_PATH_FILL_NON_ZERO,
                     .geometry_generation = generation == 0U ? 1U : generation};
}

static void af_editor_add_text(AfEditorState *editor, float x, float y, const char *text,
                               float scale, float maximum_width, AfColor color, int32_t layer) {
    AfTextLayout layout = {.glyphs = editor->text_glyphs,
                           .glyph_capacity = AF_EDITOR_TEXT_GLYPH_CAPACITY,
                           .lines = editor->text_lines,
                           .line_capacity = AF_EDITOR_TEXT_LINE_CAPACITY};
    const size_t byte_count = text == NULL ? 0U : strlen(text);
    float font_size = ceilf(9.0F + scale * 4.0F);
    AfTextRequest request = {.utf8 = text,
                             .byte_count = (uint32_t)byte_count,
                             .font_ids = {AF_EDITOR_TEXT_FONT_ID},
                             .font_count = 1U,
                             .font_size = font_size,
                             .maximum_width = maximum_width,
                             .line_spacing = 1.0F,
                             .maximum_lines = 1U,
                             .direction = AF_TEXT_DIRECTION_LTR,
                             .alignment = AF_TEXT_ALIGN_START,
                             .wrap = AF_TEXT_WRAP_NONE,
                             .kerning = true,
                             .ligatures = true,
                             .truncate = true};
    uint32_t command_start = editor->render_path_data.command_count;
    uint32_t batch_glyph_count = 0U;
    if (editor->text_engine == NULL || editor->text_glyphs == NULL || editor->text_lines == NULL ||
        byte_count == 0U || byte_count > UINT32_MAX || scale <= 0.0F || maximum_width <= 0.0F) {
        af_editor_add_bitmap_text(editor, x, y, text, scale, maximum_width, color, layer);
        return;
    }
    for (;;) {
        if (af_text_shape(editor->text_engine, &request, &layout) != AF_RESULT_SUCCESS) {
            af_editor_add_bitmap_text(editor, x, y, text, scale, maximum_width, color, layer);
            return;
        }
        if (layout.bounds.x <= maximum_width || font_size <= 10.0F) {
            break;
        }
        {
            float fitted_size = floorf(font_size * maximum_width / layout.bounds.x);
            if (fitted_size >= font_size) {
                fitted_size = font_size - 1.0F;
            }
            font_size = fmaxf(10.0F, fitted_size);
            request.font_size = font_size;
        }
    }
    for (uint32_t index = 0U; index < layout.glyph_count; ++index) {
        const AfGlyphPlacement *glyph = &layout.glyphs[index];
        const float glyph_width =
            fmaxf(fabsf(glyph->advance.x), glyph->bearing.x + glyph->bitmap_size.x);
        uint32_t written = 0U;
        uint32_t batch_commands = editor->render_path_data.command_count - command_start;
        uint32_t available = AF_MAX_PATH_COMMANDS_PER_PATH - batch_commands;
        AfResult result = AF_RESULT_SUCCESS;
        if (glyph->position.x < 0.0F || glyph->position.x + glyph_width > maximum_width + 0.25F) {
            break;
        }
        if (batch_glyph_count >= AF_EDITOR_TEXT_GLYPHS_PER_PATH) {
            af_editor_finish_text_path(editor, command_start, (AfVec2){x, y}, color, layer);
            command_start = editor->render_path_data.command_count;
            batch_glyph_count = 0U;
            available = AF_MAX_PATH_COMMANDS_PER_PATH;
        }
        if (available >
            editor->render_path_data.command_capacity - editor->render_path_data.command_count) {
            available =
                editor->render_path_data.command_capacity - editor->render_path_data.command_count;
        }
        if (available == 0U) {
            break;
        }
        result = af_text_glyph_outline(
            editor->text_engine, glyph->font_id, glyph->glyph_id, font_size, glyph->position,
            &editor->render_path_data.commands[editor->render_path_data.command_count], available,
            &written);
        if (result == AF_RESULT_LIMIT_EXCEEDED && batch_commands != 0U) {
            af_editor_finish_text_path(editor, command_start, (AfVec2){x, y}, color, layer);
            command_start = editor->render_path_data.command_count;
            batch_glyph_count = 0U;
            available =
                editor->render_path_data.command_capacity - editor->render_path_data.command_count;
            if (available > AF_MAX_PATH_COMMANDS_PER_PATH) {
                available = AF_MAX_PATH_COMMANDS_PER_PATH;
            }
            result =
                available == 0U
                    ? AF_RESULT_LIMIT_EXCEEDED
                    : af_text_glyph_outline(editor->text_engine, glyph->font_id, glyph->glyph_id,
                                            font_size, glyph->position,
                                            &editor->render_path_data
                                                 .commands[editor->render_path_data.command_count],
                                            available, &written);
        }
        if (result != AF_RESULT_SUCCESS) {
            break;
        }
        editor->render_path_data.command_count += written;
        if (written != 0U) {
            ++batch_glyph_count;
        }
    }
    af_editor_finish_text_path(editor, command_start, (AfVec2){x, y}, color, layer);
}

/* Exercise cache identity across movement, UI insertion, duplicate labels and edits. */
static AfResult af_editor_text_cache_smoke(AfEditorState *editor) {
    const uint32_t node_start = editor->render_scene.count;
    const uint32_t path_start = editor->render_path_data.path_count;
    const uint32_t command_start = editor->render_path_data.command_count;
    const AfColor color = {1, 1, 1, 1};
    AfPath original = {0};
    bool valid = true;
    af_editor_add_text(editor, 20, 30, "CACHE", 1, 200, color, 1);
    if (editor->render_path_data.path_count != path_start + 1U) {
        valid = false;
    } else {
        original = editor->render_path_data.paths[path_start];
    }
    editor->render_scene.count = node_start;
    editor->render_path_data.path_count = path_start;
    editor->render_path_data.command_count = command_start;
    af_editor_add_rect(editor, 0, 0, 10, 10, color, 0);
    af_editor_add_text(editor, -150.25F, 420.5F, "CACHE", 1, 200, (AfColor){0, 1, 0, 1}, 2);
    if (editor->render_path_data.path_count != path_start + 1U) {
        valid = false;
    } else {
        const AfPath *moved = &editor->render_path_data.paths[path_start];
        const AfSceneNode *node = &editor->render_scene.nodes[node_start + 1U];
        valid = valid && original.id == moved->id && original.node_id != moved->node_id &&
                original.geometry_generation == moved->geometry_generation &&
                original.command_count == moved->command_count &&
                node->world_transform.tx == -150.25F && node->world_transform.ty == 420.5F;
    }
    af_editor_add_text(editor, 50, 60, "CACHE", 1, 200, color, 1);
    af_editor_add_text(editor, 50, 60, "EDIT", 1, 200, color, 1);
    if (editor->render_path_data.path_count != path_start + 3U) {
        valid = false;
    } else {
        const AfPath *duplicate = &editor->render_path_data.paths[path_start + 1U];
        const AfPath *edited = &editor->render_path_data.paths[path_start + 2U];
        valid = valid && duplicate->id != original.id &&
                duplicate->geometry_generation == original.geometry_generation &&
                edited->id != original.id &&
                edited->geometry_generation != original.geometry_generation;
    }
    editor->render_scene.count = node_start;
    editor->render_path_data.path_count = path_start;
    editor->render_path_data.command_count = command_start;
    if (!valid) {
        af_log(AF_LOG_ERROR, "Text cache identity smoke failed");
    }
    return valid ? AF_RESULT_SUCCESS : AF_RESULT_INVALID_ARGUMENT;
}

static const AfEditorContextMenuInfo *af_editor_context_info(AfEditorContextMenu menu) {
    static const AfEditorContextMenuInfo menus[] = {
        {0},
        {.title = "SCENE",
         .items = {"DUPLICATE", "RENAME", "UNPARENT", "DELETE", "GROUP SELECTION"},
         .item_count = 5U},
        {.title = "CANVAS",
         .items = {"ADD RECTANGLE", "ADD ROUNDED RECT", "ADD ELLIPSE", "ADD PATH", "TOGGLE GRID",
                   "TOGGLE SNAP"},
         .item_count = 6U},
        {.title = "INSPECTOR",
         .items = {"RESET TRANSFORM", "RESET APPEARANCE", "BRING FORWARD", "SEND BACKWARD",
                   "DUPLICATE", "DELETE"},
         .item_count = 6U},
        {.title = "TIMELINE",
         .items = {"ADD KEYFRAME", "DELETE KEYFRAMES", "COPY KEYFRAMES", "PASTE KEYFRAMES",
                   "TOGGLE CURVES", "NEW CLIP"},
         .item_count = 6U},
        {.title = "TOOLS",
         .items = {"SAVE PROJECT", "COMPILE PACKAGE", "TOGGLE GRID", "TOGGLE SNAP", "UNDO", "REDO"},
         .item_count = 6U},
        {.title = "RECTANGLE",
         .items = {"DUPLICATE", "RENAME", "MAKE ROUNDED", "FIT TO CANVAS", "TOGGLE PARTICLES",
                   "BRING FORWARD", "DELETE", "CONVERT TO PATH"},
         .item_count = 8U},
        {.title = "ROUNDED RECTANGLE",
         .items = {"DUPLICATE", "RENAME", "INCREASE RADIUS", "MAKE RECTANGLE", "TOGGLE PARTICLES",
                   "BRING FORWARD", "DELETE", "CONVERT TO PATH"},
         .item_count = 8U},
        {.title = "ELLIPSE",
         .items = {"DUPLICATE", "RENAME", "MAKE CIRCLE", "FIT TO CANVAS", "TOGGLE PARTICLES",
                   "BRING FORWARD", "DELETE", "CONVERT TO PATH"},
         .item_count = 8U},
        {.title = "VECTOR PATH",
         .items = {"DUPLICATE", "RENAME", "TOGGLE FILL RULE", "EDIT POINTS", "TOGGLE PARTICLES",
                   "REVERSE CONTOUR", "DELETE"},
         .item_count = 7U},
        {.title = "GROUP",
         .items = {"DUPLICATE", "RENAME", "ADD RECT CHILD", "UNPARENT", "TOGGLE PARTICLES",
                   "DELETE"},
         .item_count = 6U},
        {.title = "IMAGE",
         .items = {"DUPLICATE", "RENAME", "SELECT ASSET", "REPAIR / REIMPORT...", "CREATE FLIPBOOK",
                   "TOGGLE NINE-SLICE", "DELETE"},
         .item_count = 7U},
        {.title = "IMAGE ASSET",
         .items = {"SELECT USERS", "CREATE FLIPBOOK", "REPAIR / REIMPORT...", "PACK ATLASES",
                   "DELETE UNUSED ASSET"},
         .item_count = 5U},
        {.title = "PARTICLE EMITTER",
         .items = {"RESET EMITTER", "TRIGGER BURST", "TOGGLE ENABLED", "NEXT OUTPUT", "NEXT SHAPE",
                   "OPEN LOD PAGE", "REMOVE EMITTER"},
         .item_count = 7U}};
    return menu > AF_EDITOR_CONTEXT_NONE && menu <= AF_EDITOR_CONTEXT_PARTICLE ? &menus[menu]
                                                                               : NULL;
}

static AfEditorContextMenu af_editor_context_for_node(const AfSceneNode *node) {
    if (node == NULL) {
        return AF_EDITOR_CONTEXT_CANVAS;
    }
    switch (node->type) {
    case AF_NODE_RECTANGLE:
        return AF_EDITOR_CONTEXT_RECTANGLE;
    case AF_NODE_ROUNDED_RECTANGLE:
        return AF_EDITOR_CONTEXT_ROUNDED_RECTANGLE;
    case AF_NODE_ELLIPSE:
        return AF_EDITOR_CONTEXT_ELLIPSE;
    case AF_NODE_PATH:
        return AF_EDITOR_CONTEXT_PATH;
    case AF_NODE_GROUP:
        return AF_EDITOR_CONTEXT_GROUP;
    case AF_NODE_IMAGE:
        return AF_EDITOR_CONTEXT_IMAGE;
    default:
        return AF_EDITOR_CONTEXT_CANVAS;
    }
}
