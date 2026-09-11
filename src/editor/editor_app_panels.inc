static void af_editor_render_hierarchy(AfEditorState *editor, int height) {
    uint32_t visible_rows = 0U;
    uint32_t row = 0U;
    const float top = editor->toolbar_height + 8.0F;
    af_editor_add_text(editor, 14.0F, 15.0F, "SCENE HIERARCHY", 1.5F, editor->left_width - 28.0F,
                       (AfColor){0.78F, 0.81F, 0.88F, 1.0F}, 100125);
    af_editor_add_button_rect(editor, 12.0F, 30.0F, 94.0F, 17.0F,
                              af_editor_hovered_rect(editor, 12, 30, 94, 17)
                                  ? (AfColor){0.22F, 0.42F, 0.62F, 1}
                                  : (AfColor){0.16F, 0.23F, 0.32F, 1},
                              100120);
    af_editor_add_text(editor, 18, 35, "GROUP  CTRL+G", 0.57F, 84, (AfColor){0.7F, 0.84F, 1, 1},
                       100125);
    if ((float)height > top + editor->status_height) {
        const float browser_height = editor->asset_browser_visible ? editor->asset_height : 0.0F;
        visible_rows = (uint32_t)(fmaxf(0.0F, (float)height - top - editor->status_height -
                                                  editor->timeline_height - browser_height) /
                                  AF_EDITOR_ROW_HEIGHT);
    }
    for (row = 0U; row < visible_rows && editor->hierarchy_scroll + row < editor->hierarchy_count;
         ++row) {
        const uint32_t hierarchy_index = editor->hierarchy_scroll + row;
        const uint32_t node_index = editor->hierarchy_order[hierarchy_index];
        const AfSceneNode *node = &editor->project.scene.nodes[node_index];
        const float y = top + (float)row * AF_EDITOR_ROW_HEIGHT;
        const float indent = 12.0F + (float)editor->hierarchy_depth[hierarchy_index] * 14.0F;
        AfColor row_color = (row & 1U) != 0U ? (AfColor){0.105F, 0.115F, 0.14F, 1.0F}
                                             : (AfColor){0.12F, 0.13F, 0.155F, 1.0F};
        if (node->id == editor->selected_id) {
            row_color = (AfColor){0.16F, 0.34F, 0.58F, 1.0F};
        } else if (af_editor_selection_contains(editor, node->id)) {
            row_color = (AfColor){0.12F, 0.25F, 0.4F, 1.0F};
        } else if (af_editor_hovered_rect(editor, 6.0F, y, editor->left_width - 12.0F,
                                          AF_EDITOR_ROW_HEIGHT - 2.0F)) {
            row_color = (AfColor){0.18F, 0.2F, 0.24F, 1.0F};
        }
        af_editor_add_rect(editor, 6.0F, y, editor->left_width - 12.0F, AF_EDITOR_ROW_HEIGHT - 2.0F,
                           row_color, 100110);
        af_editor_add_shape(
            editor,
            node->type == AF_NODE_GROUP || node->type == AF_NODE_PATH || node->type == AF_NODE_IMAGE
                ? AF_NODE_ROUNDED_RECTANGLE
                : node->type,
            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = indent, .ty = y + 6.0F},
            (AfVec2){15.0F, 15.0F},
            node->type == AF_NODE_GROUP ? (AfColor){0.72F, 0.58F, 0.22F, 1.0F} : node->fill,
            node->type == AF_NODE_GROUP ? 3.0F : node->corner_radius, 100120);
        af_editor_add_text(editor, indent + 23.0F, y + 10.0F, editor->project.names[node_index],
                           1.2F, editor->left_width - indent - 34.0F,
                           (AfColor){0.78F, 0.81F, 0.88F, 1.0F}, 100125);
        if (editor->drag_mode == AF_EDITOR_DRAG_HIERARCHY && editor->hierarchy_drag_started &&
            af_editor_hovered_rect(editor, 0, y, editor->left_width, AF_EDITOR_ROW_HEIGHT)) {
            const bool inside = af_editor_hierarchy_drop_inside(node, editor->mouse_position.y - y);
            const bool valid = inside ? af_editor_hierarchy_can_drop_inside(editor, node->id)
                                      : af_editor_can_reorder(&editor->project, editor->hierarchy_drag_id, node->id);
            const AfColor highlight =
                valid ? (AfColor){0.25F, 0.8F, 1, 1} : (AfColor){1, 0.35F, 0.25F, 1};
            if (inside) {
                af_editor_add_rect(editor, 6, y, editor->left_width - 12, 2, highlight, 100140);
                af_editor_add_rect(editor, 6, y + AF_EDITOR_ROW_HEIGHT - 3, editor->left_width - 12,
                                   2, highlight, 100140);
                af_editor_add_rect(editor, 6, y, 2, AF_EDITOR_ROW_HEIGHT - 2, highlight, 100140);
                af_editor_add_rect(editor, editor->left_width - 8, y, 2, AF_EDITOR_ROW_HEIGHT - 2,
                                   highlight, 100140);
            } else {
                const float line_y =
                    y + (editor->mouse_position.y >= y + AF_EDITOR_ROW_HEIGHT * 0.5F
                             ? AF_EDITOR_ROW_HEIGHT
                             : 0.0F);
                af_editor_add_rect(editor, indent, line_y - 2, editor->left_width - indent - 8, 4,
                                   highlight, 100140);
            }
        }
    }
}

static float af_editor_asset_browser_top(const AfEditorState *editor, int height) {
    return (float)height - editor->status_height - editor->timeline_height - editor->asset_height;
}

static uint32_t af_editor_asset_visible_rows(const AfEditorState *editor) {
    return (uint32_t)(fmaxf(0.0F, editor->asset_height - 64.0F) / 27.0F);
}

static void af_editor_render_asset_browser(AfEditorState *editor, int height) {
    static const char *const formats[] = {"PNG", "JPEG", "KTX2", "SVG"};
    const float top = af_editor_asset_browser_top(editor, height);
    if (!editor->asset_browser_visible) {
        return;
    }
    const uint32_t visible_rows = af_editor_asset_visible_rows(editor);
    af_editor_add_rect(editor, 0.0F, top, editor->left_width, editor->asset_height,
                       (AfColor){0.07F, 0.078F, 0.098F, 1.0F}, 100105);
    af_editor_add_rect(editor, 0.0F, top, editor->left_width, 2.0F,
                       (AfColor){0.24F, 0.28F, 0.36F, 1.0F}, 100115);
    af_editor_add_text(editor, 12.0F, top + 12.0F, "IMAGE ASSETS", 1.1F, editor->left_width - 24.0F,
                       (AfColor){0.68F, 0.78F, 0.94F, 1.0F}, 100125);
    af_editor_add_text(editor, 12.0F, top + 29.0F, "ASSETS > IMPORT IMAGE", 0.58F,
                       editor->left_width - 24.0F, (AfColor){0.42F, 0.47F, 0.56F, 1.0F}, 100125);
    for (uint32_t row = 0U;
         row < visible_rows && editor->asset_scroll + row < editor->project.image_data.asset_count;
         ++row) {
        const uint32_t index = editor->asset_scroll + row;
        const AfImageAsset *asset = &editor->project.image_data.assets[index];
        const float y = top + 48.0F + (float)row * 27.0F;
        char info[96];
        const uint32_t uses = af_editor_asset_use_count(editor, asset->id);
        const bool selected = asset->id == editor->selected_asset_id;
        af_editor_add_button_rect(editor, 7.0F, y, editor->left_width - 14.0F, 24.0F,
                                  selected ? (AfColor){0.16F, 0.34F, 0.58F, 1.0F}
                                           : (AfColor){0.12F, 0.13F, 0.16F, 1.0F},
                                  100115);
        af_editor_add_rect(editor, 12.0F, y + 5.0F, 14.0F, 14.0F,
                           asset->missing ? (AfColor){0.9F, 0.27F, 0.24F, 1.0F}
                                          : (AfColor){0.31F, 0.7F, 0.94F, 1.0F},
                           100125);
        (void)snprintf(info, sizeof(info), "%s  %ux%u  %s  %u USE%s", asset->name, asset->width,
                       asset->height, formats[asset->source_format], uses, uses == 1U ? "" : "S");
        af_editor_add_text(editor, 32.0F, y + 8.0F, info, 0.72F, editor->left_width - 44.0F,
                           (AfColor){0.76F, 0.8F, 0.88F, 1.0F}, 100130);
    }
    if (editor->project.image_data.asset_count == 0U) {
        af_editor_add_text(editor, 12.0F, top + 62.0F, "NO IMAGES IMPORTED", 0.75F,
                           editor->left_width - 24.0F, (AfColor){0.42F, 0.47F, 0.56F, 1.0F},
                           100125);
    } else if (editor->project.image_data.asset_count > visible_rows) {
        char range[48];
        const uint32_t last =
            editor->asset_scroll + visible_rows < editor->project.image_data.asset_count
                ? editor->asset_scroll + visible_rows
                : editor->project.image_data.asset_count;
        (void)snprintf(range, sizeof(range), "%u-%u OF %u / SCROLL", editor->asset_scroll + 1U,
                       last, editor->project.image_data.asset_count);
        af_editor_add_text(editor, 12.0F, top + editor->asset_height - 12.0F, range, 0.58F,
                           editor->left_width - 24.0F, (AfColor){0.42F, 0.47F, 0.56F, 1.0F},
                           100125);
    }
}

static void af_editor_render_image_inspector(AfEditorState *editor, int width,
                                             const AfSceneNode *selected) {
    static const char *const formats[] = {"PNG", "JPEG", "KTX2", "SVG"};
    const float x = (float)width - editor->right_width + 18.0F;
    const AfImageBinding *binding =
        af_image_find_binding_const(&editor->project.image_data, selected->id);
    const AfImageAsset *asset =
        binding == NULL ? NULL
                        : af_image_find_const(&editor->project.image_data, binding->asset_id);
    const AfImageSequence *sequence =
        binding == NULL
            ? NULL
            : af_image_find_sequence_const(&editor->project.image_data, binding->sequence_id);
    if (asset == NULL && sequence != NULL && sequence->frame_count != 0U) {
        asset =
            af_image_find_const(&editor->project.image_data,
                                editor->project.image_data.sequence_frames[sequence->frame_start]);
    }
    char text[128];
    if (asset == NULL) {
        af_editor_add_text(editor, x, 72.0F, "MISSING IMAGE BINDING", 0.9F,
                           editor->right_width - 36.0F, (AfColor){0.95F, 0.32F, 0.28F, 1.0F},
                           100125);
        return;
    }
    (void)snprintf(text, sizeof(text), "%s  %u x %u", formats[asset->source_format], asset->width,
                   asset->height);
    af_editor_add_text(editor, x, 68.0F, "IMAGE RESOURCE", 0.9F, editor->right_width - 36.0F,
                       (AfColor){0.44F, 0.75F, 1.0F, 1.0F}, 100125);
    af_editor_add_text(editor, x, 91.0F, text, 1.0F, editor->right_width - 36.0F,
                       (AfColor){0.78F, 0.81F, 0.88F, 1.0F}, 100125);
    af_editor_add_text(editor, x, 113.0F, asset->source_path, 0.68F, editor->right_width - 36.0F,
                       asset->missing ? (AfColor){0.95F, 0.32F, 0.28F, 1.0F}
                                      : (AfColor){0.5F, 0.56F, 0.66F, 1.0F},
                       100125);
    {
        const char *labels[] = {
            asset->trim_enabled ? "TRIM: ON" : "TRIM: OFF",
            asset->mipmaps_enabled ? "MIPS: ON" : "MIPS: OFF",
            asset->color_space == AF_IMAGE_COLOR_SRGB ? "COLOR: SRGB" : "COLOR: LINEAR",
            asset->compression == AF_IMAGE_COMPRESSION_LOSSLESS ? "PACKAGE: LOSSLESS"
                                                                : "PACKAGE: RAW",
            binding->display_mode == AF_IMAGE_DISPLAY_NINE_SLICE ? "DISPLAY: NINE-SLICE"
                                                                 : "DISPLAY: STRETCH"};
        for (uint32_t row = 0U; row < 5U; ++row) {
            const float y = 150.0F + (float)row * 38.0F;
            af_editor_add_button_rect(editor, x, y, editor->right_width - 36.0F, 32.0F,
                                      (AfColor){0.2F, 0.23F, 0.29F, 1.0F}, 100120);
            af_editor_add_text(editor, x + 10.0F, y + 10.0F, labels[row], 0.82F,
                               editor->right_width - 56.0F, (AfColor){0.78F, 0.82F, 0.9F, 1.0F},
                               100130);
        }
    }
    af_editor_add_text(editor, x, 353.0F, "NINE-SLICE BORDERS", 0.78F, editor->right_width - 36.0F,
                       (AfColor){0.52F, 0.58F, 0.68F, 1.0F}, 100125);
    {
        const uint16_t values[] = {binding->borders.left, binding->borders.top,
                                   binding->borders.right, binding->borders.bottom};
        const char *names[] = {"LEFT", "TOP", "RIGHT", "BOTTOM"};
        for (uint32_t row = 0U; row < 4U; ++row) {
            const float y = 375.0F + (float)row * 36.0F;
            char value[48];
            (void)snprintf(value, sizeof(value), "%s %u", names[row], values[row]);
            af_editor_add_button_rect(editor, x, y, 34.0F, 28.0F,
                                      (AfColor){0.24F, 0.26F, 0.32F, 1.0F}, 100120);
            af_editor_add_button_rect(editor, x + 38.0F, y, 34.0F, 28.0F,
                                      (AfColor){0.24F, 0.26F, 0.32F, 1.0F}, 100120);
            af_editor_add_text(editor, x + 10.0F, y + 9.0F, "-", 0.9F, 12.0F,
                               (AfColor){0.82F, 0.84F, 0.9F, 1.0F}, 100130);
            af_editor_add_text(editor, x + 50.0F, y + 9.0F, "+", 0.9F, 12.0F,
                               (AfColor){0.82F, 0.84F, 0.9F, 1.0F}, 100130);
            af_editor_add_text(editor, x + 82.0F, y + 8.0F, value, 0.85F,
                               editor->right_width - 118.0F, (AfColor){0.7F, 0.73F, 0.81F, 1.0F},
                               100130);
        }
    }
    af_editor_add_text(editor, x, 528.0F, "FLIPBOOK", 0.78F, editor->right_width - 36.0F,
                       (AfColor){0.52F, 0.58F, 0.68F, 1.0F}, 100125);
    {
        char sequence_info[96];
        (void)snprintf(sequence_info, sizeof(sequence_info),
                       sequence == NULL ? "CREATE FROM ALL ASSETS" : "REMOVE %s (%u FRAMES)",
                       sequence == NULL ? "" : sequence->name,
                       sequence == NULL ? 0U : sequence->frame_count);
        af_editor_add_button_rect(editor, x, 548.0F, editor->right_width - 36.0F, 32.0F,
                                  (AfColor){0.2F, 0.23F, 0.29F, 1.0F}, 100120);
        af_editor_add_text(editor, x + 10.0F, 558.0F, sequence_info, 0.76F,
                           editor->right_width - 56.0F, (AfColor){0.78F, 0.82F, 0.9F, 1.0F},
                           100130);
    }
    if (sequence != NULL) {
        char fps[48];
        af_editor_add_button_rect(editor, x, 586.0F, 34.0F, 28.0F,
                                  (AfColor){0.24F, 0.26F, 0.32F, 1.0F}, 100120);
        af_editor_add_button_rect(editor, x + 38.0F, 586.0F, 34.0F, 28.0F,
                                  (AfColor){0.24F, 0.26F, 0.32F, 1.0F}, 100120);
        af_editor_add_text(editor, x + 10.0F, 595.0F, "-", 0.9F, 12.0F,
                           (AfColor){0.82F, 0.84F, 0.9F, 1.0F}, 100130);
        af_editor_add_text(editor, x + 50.0F, 595.0F, "+", 0.9F, 12.0F,
                           (AfColor){0.82F, 0.84F, 0.9F, 1.0F}, 100130);
        (void)snprintf(fps, sizeof(fps), "FPS %.1F", (double)binding->frames_per_second);
        af_editor_add_text(editor, x + 82.0F, 594.0F, fps, 0.85F, editor->right_width - 118.0F,
                           (AfColor){0.7F, 0.73F, 0.81F, 1.0F}, 100130);
        af_editor_add_button_rect(editor, x, 622.0F, editor->right_width - 36.0F, 30.0F,
                                  (AfColor){0.2F, 0.23F, 0.29F, 1.0F}, 100120);
        af_editor_add_text(editor, x + 10.0F, 631.0F, binding->loop ? "LOOP: ON" : "LOOP: OFF",
                           0.82F, editor->right_width - 56.0F, (AfColor){0.78F, 0.82F, 0.9F, 1.0F},
                           100130);
    }
}

static void af_editor_render_tool_icon(AfEditorState *editor, AfEditorVectorTool tool, float x,
                                       float y, AfColor color) {
    AfVec2 points[40];
    uint32_t count = 0U;
    bool closed = false;
    if (tool == AF_EDITOR_TOOL_RECTANGLE || tool == AF_EDITOR_TOOL_ROUNDED_RECTANGLE ||
        tool == AF_EDITOR_TOOL_ELLIPSE || tool == AF_EDITOR_TOOL_POLYGON ||
        tool == AF_EDITOR_TOOL_STAR) {
        const uint32_t sides = tool == AF_EDITOR_TOOL_STAR      ? 10U
                               : tool == AF_EDITOR_TOOL_POLYGON ? 6U
                               : tool == AF_EDITOR_TOOL_ELLIPSE ? 24U
                                                                : 4U;
        for (uint32_t i = 0U; i < sides; ++i) {
            if (sides == 4U) {
                points[count++] =
                    (AfVec2){i == 0U || i == 3U ? 1.0F : 19.0F, i < 2U ? 1.0F : 17.0F};
            } else {
                const float angle = (float)i * 6.283185307F / (float)sides - 1.570796327F;
                const float radius = tool == AF_EDITOR_TOOL_STAR && (i & 1U) ? 4.0F : 9.0F;
                points[count++] =
                    (AfVec2){10.0F + cosf(angle) * radius, 9.0F + sinf(angle) * radius};
            }
        }
        closed = true;
        if (tool == AF_EDITOR_TOOL_ROUNDED_RECTANGLE) {
            const AfVec2 rounded[] = {{5, 1},   {15, 1}, {19, 5}, {19, 13},
                                      {15, 17}, {5, 17}, {1, 13}, {1, 5}};
            memcpy(points, rounded, sizeof(rounded));
            count = 8U;
        }
    } else if (tool == AF_EDITOR_TOOL_SELECT) {
        const AfVec2 cursor[] = {{3, 0}, {3, 17}, {8, 12}, {12, 19}, {15, 17}, {11, 10}, {18, 10}};
        memcpy(points, cursor, sizeof(cursor));
        count = 7U;
        closed = true;
    } else if (tool == AF_EDITOR_TOOL_PEN) {
        const AfVec2 pen[] = {{10, 0}, {3, 11}, {10, 18}, {17, 11}, {10, 0}, {10, 11}};
        memcpy(points, pen, sizeof(pen));
        count = 6U;
    } else if (tool == AF_EDITOR_TOOL_KNIFE || tool == AF_EDITOR_TOOL_PENCIL) {
        const AfVec2 blade[] = {{2, 18}, {7, 8}, {15, 0}, {20, 5}, {12, 13}, {2, 18}, {7, 8}};
        memcpy(points, blade, sizeof(blade));
        count = 7U;
    } else if (tool == AF_EDITOR_TOOL_ARC || tool == AF_EDITOR_TOOL_SPIRAL) {
        for (uint32_t i = 0U; i < 32U; ++i) {
            const float t = (float)i / 31.0F;
            const float radius = tool == AF_EDITOR_TOOL_SPIRAL ? 1.0F + 8.0F * t : 9.0F;
            const float angle = t * (tool == AF_EDITOR_TOOL_SPIRAL ? 12.56637F : 4.712389F);
            points[count++] = (AfVec2){10.0F + cosf(angle) * radius, 9.0F + sinf(angle) * radius};
        }
    } else {
        points[count++] = (AfVec2){2, 16};
        points[count++] = (AfVec2){18, 2};
    }
    for (uint32_t i = 1U; i < count + (closed ? 1U : 0U); ++i) {
        const AfVec2 a = points[i - 1U], b = points[i % count];
        af_editor_add_line(editor, (AfVec2){x + a.x, y + a.y}, (AfVec2){x + b.x, y + b.y}, 1.6F,
                           color, 100125);
    }
    if (tool == AF_EDITOR_TOOL_NODE || tool == AF_EDITOR_TOOL_LINE) {
        af_editor_add_rect(editor, x, y + 14.0F, 5.0F, 5.0F, color, 100130);
        af_editor_add_rect(editor, x + 16.0F, y, 5.0F, 5.0F, color, 100130);
    }
    if (tool == AF_EDITOR_TOOL_SCISSORS || tool == AF_EDITOR_TOOL_SEGMENT_DELETE) {
        af_editor_add_line(editor, (AfVec2){x + 3.0F, y + 2.0F}, (AfVec2){x + 18.0F, y + 17.0F},
                           1.6F, color, 100125);
        if (tool == AF_EDITOR_TOOL_SCISSORS) {
            for (uint32_t i = 0U; i < 2U; ++i) {
                af_editor_add_shape(
                    editor, AF_NODE_ELLIPSE,
                    (AfAffine2){.m00 = 1, .m11 = 1, .tx = x - 1.0F, .ty = y + (float)i * 13.0F},
                    (AfVec2){6, 6}, color, 0, 100125);
            }
        } else {
            af_editor_add_rect(editor, x + 7.0F, y + 6.0F, 7.0F, 6.0F,
                               (AfColor){1.0F, 0.4F, 0.3F, 1.0F}, 100130);
        }
    }
}

static void af_editor_render_toolbar(AfEditorState *editor, int width) {
    const float start = af_editor_toolbar_origin(editor, width);
    const float right = (float)width - editor->right_width;
    const float top = AF_EDITOR_MENU_HEIGHT;
    const float tool_step = 42.0F;
    const float grid_x = start + (float)AF_EDITOR_TOOL_COUNT * tool_step + 8.0F;
    for (uint32_t index = 0U; index < AF_EDITOR_TOOL_COUNT; ++index) {
        const float x = start + (float)index * tool_step;
        if (x < editor->left_width + 4.0F || x + 38.0F > right - 4.0F)
            continue;
        const bool active = editor->active_tool == (AfEditorVectorTool)index;
        const bool hovered = af_editor_hovered_rect(editor, x, top + 5.0F, 38.0F, 34.0F);
        const AfColor button = active    ? (AfColor){0.15F, 0.48F, 0.78F, 1.0F}
                               : hovered ? (AfColor){0.25F, 0.31F, 0.41F, 1.0F}
                                         : (AfColor){0.18F, 0.2F, 0.25F, 1.0F};
        af_editor_add_button_rect(editor, x, top + 5.0F, 38.0F, 34.0F, button, 100110);
        af_editor_render_tool_icon(editor, (AfEditorVectorTool)index, x + 9.0F, top + 7.0F,
                                   (AfColor){0.76F, 0.87F, 1.0F, 1.0F});
        af_editor_add_text(editor, x + 3.0F, top + 30.0F,
                           af_editor_vector_tool_label((AfEditorVectorTool)index), 0.43F, 32.0F,
                           (AfColor){0.9F, 0.92F, 0.97F, 1.0F}, 100135);
    }
    if (grid_x < editor->left_width + 4.0F || grid_x + 88.0F > right - 4.0F)
        return;
    af_editor_add_button_rect(editor, grid_x, top + 3.0F, 88.0F, 18.0F,
                              (AfColor){0.18F, 0.2F, 0.25F, 1.0F}, 100110);
    af_editor_add_button_rect(editor, grid_x, top + 22.0F, 88.0F, 20.0F,
                              (AfColor){0.18F, 0.2F, 0.25F, 1.0F}, 100110);
    af_editor_add_rect(editor, grid_x + 6.0F, top + 8.0F, 7.0F, 9.0F,
                       editor->grid_visible ? (AfColor){0.35F, 0.7F, 0.98F, 1.0F}
                                            : (AfColor){0.4F, 0.42F, 0.48F, 1.0F},
                       100125);
    af_editor_add_rect(editor, grid_x + 6.0F, top + 27.0F, 7.0F, 9.0F,
                       editor->snapping ? (AfColor){0.96F, 0.55F, 0.25F, 1.0F}
                                        : (AfColor){0.4F, 0.42F, 0.48F, 1.0F},
                       100125);
    af_editor_add_text(editor, grid_x + 19.0F, top + 8.0F,
                       editor->grid_visible ? "GRID ON" : "GRID OFF", 0.65F, 62.0F,
                       (AfColor){0.76F, 0.79F, 0.86F, 1.0F}, 100130);
    af_editor_add_text(editor, grid_x + 19.0F, top + 27.0F,
                       editor->snapping ? "SNAP ON" : "SNAP OFF", 0.65F, 62.0F,
                       (AfColor){0.76F, 0.79F, 0.86F, 1.0F}, 100130);
}

static void af_editor_render_inspector_field(AfEditorState *editor, float x, float y, float width,
                                             const char *label, const char *value,
                                             AfEditorInspectorField field) {
    const bool editing = editor->inspector_field == field;
    const bool scrubbing = editor->drag_mode == AF_EDITOR_DRAG_INSPECTOR_SCRUB &&
                           editor->inspector_scrub_field == field;
    const bool active = editing || scrubbing;
    const bool steppers = field >= AF_EDITOR_INSPECTOR_POSITION_X;
    const bool hovered =
        af_editor_hovered_rect(editor, x, y, width, AF_EDITOR_INSPECTOR_FIELD_HEIGHT);
    char display[64];
    (void)snprintf(display, sizeof(display), "%s  %s", label,
                   editing ? editor->inspector_buffer : value);
    af_editor_add_button_rect(editor, x, y, width, AF_EDITOR_INSPECTOR_FIELD_HEIGHT,
                              active ? (AfColor){0.16F, 0.43F, 0.72F, 1.0F}
                                     : (hovered ? (AfColor){0.2F, 0.23F, 0.29F, 1.0F}
                                                : (AfColor){0.135F, 0.15F, 0.19F, 1.0F}),
                              100120);
    af_editor_add_text(
        editor, x + 7.0F, y + 8.0F, display, 0.68F, width - (steppers ? 52.0F : 12.0F),
        active ? (AfColor){0.96F, 0.98F, 1.0F, 1.0F} : (AfColor){0.74F, 0.78F, 0.87F, 1.0F},
        100135);
    if (steppers) {
        const float minus_x = x + width - 40.0F;
        const float plus_x = x + width - 19.0F;
        const AfColor idle = {0.22F, 0.25F, 0.32F, 1.0F};
        const AfColor hover = {0.3F, 0.42F, 0.58F, 1.0F};
        af_editor_add_button_rect(
            editor, minus_x, y + 2.0F, 19.0F, AF_EDITOR_INSPECTOR_FIELD_HEIGHT - 4.0F,
            af_editor_hovered_rect(editor, minus_x, y, 19.0F, AF_EDITOR_INSPECTOR_FIELD_HEIGHT)
                ? hover
                : idle,
            100140);
        af_editor_add_button_rect(
            editor, plus_x, y + 2.0F, 19.0F, AF_EDITOR_INSPECTOR_FIELD_HEIGHT - 4.0F,
            af_editor_hovered_rect(editor, plus_x, y, 19.0F, AF_EDITOR_INSPECTOR_FIELD_HEIGHT)
                ? hover
                : idle,
            100140);
        af_editor_add_text(editor, minus_x + 6.0F, y + 8.0F, "-", 0.72F, 9.0F,
                           (AfColor){0.9F, 0.93F, 0.98F, 1.0F}, 100145);
        af_editor_add_text(editor, plus_x + 5.0F, y + 8.0F, "+", 0.72F, 10.0F,
                           (AfColor){0.9F, 0.93F, 0.98F, 1.0F}, 100145);
    }
    if (active) {
        af_editor_add_rect(editor, x + 3.0F, y + AF_EDITOR_INSPECTOR_FIELD_HEIGHT - 3.0F,
                           width - (steppers ? 46.0F : 6.0F), 2.0F,
                           (AfColor){0.48F, 0.82F, 1.0F, 1.0F}, 100140);
    }
}

static void af_editor_render_color_picker(AfEditorState *editor, float x,
                                          const AfSceneNode *selected) {
    const AfColor color = af_editor_inspector_color(editor, selected);
    float hue = editor->color_hue;
    float saturation = 0.0F;
    float value = 0.0F;
    char hexadecimal[16];
    af_editor_color_hsv(color, &hue, &saturation, &value);
    if (saturation > 0.0001F) {
        editor->color_hue = hue;
    } else {
        hue = editor->color_hue;
    }
    af_editor_add_button_rect(editor, x, AF_EDITOR_COLOR_TARGET_Y, 72.0F,
                              AF_EDITOR_COLOR_TARGET_HEIGHT,
                              editor->color_target_outline ? (AfColor){0.18F, 0.2F, 0.25F, 1.0F}
                                                           : (AfColor){0.18F, 0.46F, 0.74F, 1.0F},
                              100120);
    af_editor_add_button_rect(editor, x + 78.0F, AF_EDITOR_COLOR_TARGET_Y, 96.0F,
                              AF_EDITOR_COLOR_TARGET_HEIGHT,
                              editor->color_target_outline ? (AfColor){0.18F, 0.46F, 0.74F, 1.0F}
                                                           : (AfColor){0.18F, 0.2F, 0.25F, 1.0F},
                              100120);
    af_editor_add_text(editor, x + 19.0F, AF_EDITOR_COLOR_TARGET_Y + 6.0F, "FILL", 0.62F, 40.0F,
                       (AfColor){0.88F, 0.91F, 0.97F, 1.0F}, 100130);
    af_editor_add_text(editor, x + 88.0F, AF_EDITOR_COLOR_TARGET_Y + 6.0F, "OUTLINE", 0.62F, 74.0F,
                       (AfColor){0.88F, 0.91F, 0.97F, 1.0F}, 100130);
    for (uint32_t row = 0U; row < 8U; ++row) {
        for (uint32_t column = 0U; column < 12U; ++column) {
            const float cell_width = AF_EDITOR_COLOR_SV_WIDTH / 12.0F;
            const float cell_height = AF_EDITOR_COLOR_SV_HEIGHT / 8.0F;
            const float cell_saturation = ((float)column + 0.5F) / 12.0F;
            const float cell_value = 1.0F - ((float)row + 0.5F) / 8.0F;
            af_editor_add_rect(editor, x + (float)column * cell_width,
                               AF_EDITOR_COLOR_SV_Y + (float)row * cell_height, cell_width + 0.4F,
                               cell_height + 0.4F,
                               af_editor_hsv_color(hue, cell_saturation, cell_value, 1.0F), 100120);
        }
    }
    for (uint32_t row = 0U; row < 12U; ++row) {
        const float segment_height = AF_EDITOR_COLOR_SV_HEIGHT / 12.0F;
        af_editor_add_rect(
            editor, x + AF_EDITOR_COLOR_HUE_X, AF_EDITOR_COLOR_SV_Y + (float)row * segment_height,
            AF_EDITOR_COLOR_HUE_WIDTH, segment_height + 0.4F,
            af_editor_hsv_color(((float)row + 0.5F) / 12.0F, 1.0F, 1.0F, 1.0F), 100120);
    }
    for (uint32_t column = 0U; column < 12U; ++column) {
        const float segment_width = (AF_EDITOR_COLOR_HUE_X + AF_EDITOR_COLOR_HUE_WIDTH) / 12.0F;
        const AfColor checker = (column & 1U) != 0U ? (AfColor){0.18F, 0.19F, 0.22F, 1.0F}
                                                    : (AfColor){0.38F, 0.4F, 0.44F, 1.0F};
        AfColor alpha_color = color;
        alpha_color.a = ((float)column + 0.5F) / 12.0F;
        af_editor_add_rect(editor, x + (float)column * segment_width, AF_EDITOR_COLOR_ALPHA_Y,
                           segment_width + 0.4F, AF_EDITOR_COLOR_ALPHA_HEIGHT, checker, 100120);
        af_editor_add_rect(editor, x + (float)column * segment_width, AF_EDITOR_COLOR_ALPHA_Y,
                           segment_width + 0.4F, AF_EDITOR_COLOR_ALPHA_HEIGHT, alpha_color, 100125);
    }
    {
        const float marker_x = x + saturation * AF_EDITOR_COLOR_SV_WIDTH;
        const float marker_y = AF_EDITOR_COLOR_SV_Y + (1.0F - value) * AF_EDITOR_COLOR_SV_HEIGHT;
        const AfColor marker = value > 0.55F ? (AfColor){0.03F, 0.04F, 0.06F, 1.0F}
                                             : (AfColor){0.96F, 0.97F, 1.0F, 1.0F};
        af_editor_add_rect(editor, marker_x - 5.0F, marker_y - 5.0F, 10.0F, 2.0F, marker, 100140);
        af_editor_add_rect(editor, marker_x - 5.0F, marker_y + 3.0F, 10.0F, 2.0F, marker, 100140);
        af_editor_add_rect(editor, marker_x - 5.0F, marker_y - 5.0F, 2.0F, 10.0F, marker, 100140);
        af_editor_add_rect(editor, marker_x + 3.0F, marker_y - 5.0F, 2.0F, 10.0F, marker, 100140);
    }
    af_editor_add_rect(editor, x + AF_EDITOR_COLOR_HUE_X - 2.0F,
                       AF_EDITOR_COLOR_SV_Y + hue * AF_EDITOR_COLOR_SV_HEIGHT - 1.0F,
                       AF_EDITOR_COLOR_HUE_WIDTH + 4.0F, 3.0F, (AfColor){0.96F, 0.97F, 1.0F, 1.0F},
                       100140);
    af_editor_add_rect(editor,
                       x + color.a * (AF_EDITOR_COLOR_HUE_X + AF_EDITOR_COLOR_HUE_WIDTH) - 1.0F,
                       AF_EDITOR_COLOR_ALPHA_Y - 2.0F, 3.0F, AF_EDITOR_COLOR_ALPHA_HEIGHT + 4.0F,
                       (AfColor){0.96F, 0.97F, 1.0F, 1.0F}, 100140);
    af_editor_add_rect(editor, x + 176.0F, AF_EDITOR_COLOR_SV_Y, 48.0F, 36.0F,
                       (AfColor){0.16F, 0.17F, 0.2F, 1.0F}, 100120);
    af_editor_add_rect(editor, x + 180.0F, AF_EDITOR_COLOR_SV_Y + 4.0F, 40.0F, 28.0F, color,
                       100130);
    af_editor_inspector_format_hex_color(color, hexadecimal, sizeof(hexadecimal));
    af_editor_render_inspector_field(editor, x, AF_EDITOR_COLOR_HEX_Y, editor->right_width - 36.0F,
                                     "HEX", hexadecimal, AF_EDITOR_INSPECTOR_COLOR_HEX);
    {
        const float available = editor->right_width - 36.0F;
        const float field_width = (available - 12.0F) * 0.25F;
        char values[4][16];
        const char *labels[4] = {"R", "G", "B", "A"};
        const AfEditorInspectorField fields[4] = {
            AF_EDITOR_INSPECTOR_COLOR_R, AF_EDITOR_INSPECTOR_COLOR_G, AF_EDITOR_INSPECTOR_COLOR_B,
            AF_EDITOR_INSPECTOR_COLOR_A};
        (void)snprintf(values[0], sizeof(values[0]), "%u", (uint32_t)roundf(color.r * 255.0F));
        (void)snprintf(values[1], sizeof(values[1]), "%u", (uint32_t)roundf(color.g * 255.0F));
        (void)snprintf(values[2], sizeof(values[2]), "%u", (uint32_t)roundf(color.b * 255.0F));
        (void)snprintf(values[3], sizeof(values[3]), "%u", (uint32_t)roundf(color.a * 255.0F));
        for (uint32_t component = 0U; component < 4U; ++component) {
            af_editor_render_inspector_field(
                editor, x + (float)component * (field_width + 4.0F), AF_EDITOR_COLOR_COMPONENT_Y,
                field_width, labels[component], values[component], fields[component]);
        }
    }
}

static void af_editor_particle_labels(const AfEditorState *editor, const AfParticleEmitter *emitter,
                                      char labels[AF_EDITOR_PARTICLE_ROW_COUNT][64]) {
    static const char *const type_names[] = {"POINT", "LINE", "RECT", "CIRCLE", "PATH"};
    static const char *const output_names[] = {"BILLBOARD", "UI",   "FLIPBOOK", "TRAIL",
                                               "RIBBON",    "MESH", "DECAL",    "HEAT",
                                               "REFRACT",   "SOFT", "LIGHT"};
    const AfVfxLodSettings *lod = &editor->project.particle_data.lod;
    if (editor->particle_page == 0U) {
        (void)snprintf(labels[0], 64U, "ENABLED %s", emitter->enabled ? "ON" : "OFF");
        (void)snprintf(labels[1], 64U, "RATE %.0f / S", (double)emitter->spawn_rate);
        (void)snprintf(labels[2], 64U, "LIFE MIN %.2f", (double)emitter->lifetime_min);
        (void)snprintf(labels[3], 64U, "LIFE MAX %.2f", (double)emitter->lifetime_max);
        (void)snprintf(labels[4], 64U, "CAPACITY %u", emitter->capacity);
        (void)snprintf(labels[5], 64U, "SHAPE %s", type_names[emitter->type]);
        (void)snprintf(labels[6], 64U, "BURST %u", emitter->burst_count);
        (void)snprintf(labels[7], 64U, "SEED %u", emitter->seed);
    } else if (editor->particle_page == 1U) {
        (void)snprintf(labels[0], 64U, "VELOCITY X MIN %.0f", (double)emitter->velocity_min.x);
        (void)snprintf(labels[1], 64U, "VELOCITY X MAX %.0f", (double)emitter->velocity_max.x);
        (void)snprintf(labels[2], 64U, "VELOCITY Y MIN %.0f", (double)emitter->velocity_min.y);
        (void)snprintf(labels[3], 64U, "VELOCITY Y MAX %.0f", (double)emitter->velocity_max.y);
        (void)snprintf(labels[4], 64U, "GRAVITY X %.0f", (double)emitter->gravity.x);
        (void)snprintf(labels[5], 64U, "GRAVITY Y %.0f", (double)emitter->gravity.y);
        (void)snprintf(labels[6], 64U, "WIND X %.0f", (double)emitter->wind.x);
        (void)snprintf(labels[7], 64U, "DRAG %.2f", (double)emitter->drag);
    } else if (editor->particle_page == 2U) {
        (void)snprintf(labels[0], 64U, "TURBULENCE %.0f", (double)emitter->turbulence_strength);
        (void)snprintf(labels[1], 64U, "FREQUENCY %.2f", (double)emitter->turbulence_frequency);
        (void)snprintf(labels[2], 64U, "RADIAL %.0f", (double)emitter->radial_force);
        (void)snprintf(labels[3], 64U, "VORTEX %.0f", (double)emitter->vortex_force);
        (void)snprintf(labels[4], 64U, "ATTRACT %.0f", (double)emitter->attractor_strength);
        (void)snprintf(labels[5], 64U, "ATTRACT RADIUS %.0f", (double)emitter->attractor_radius);
        (void)snprintf(labels[6], 64U, "REPULSE %.0f", (double)emitter->repulsor_strength);
        (void)snprintf(labels[7], 64U, "VECTOR FIELD %.0f", (double)emitter->vector_field_strength);
    } else if (editor->particle_page == 3U) {
        (void)snprintf(labels[0], 64U, "PLANE %s", emitter->collision_plane_enabled ? "ON" : "OFF");
        (void)snprintf(labels[1], 64U, "PLANE OFFSET %.0f",
                       (double)emitter->collision_plane_offset);
        (void)snprintf(labels[2], 64U, "BOUNCE %.2f", (double)emitter->collision_restitution);
        (void)snprintf(labels[3], 64U, "FRICTION %.2f", (double)emitter->collision_friction);
        (void)snprintf(labels[4], 64U, "DEPTH %s", emitter->depth_collision_enabled ? "ON" : "OFF");
        (void)snprintf(labels[5], 64U, "DEPTH MIN %.0f", (double)emitter->depth_min);
        (void)snprintf(labels[6], 64U, "DEPTH MAX %.0f", (double)emitter->depth_max);
        (void)snprintf(labels[7], 64U, "DEPTH SPEED %.0f / %.0f",
                       (double)emitter->depth_velocity_min, (double)emitter->depth_velocity_max);
    } else if (editor->particle_page == 4U) {
        (void)snprintf(labels[0], 64U, "OUTPUT %s", output_names[emitter->output]);
        (void)snprintf(labels[1], 64U, "TRAIL LENGTH %.2f", (double)emitter->trail_length);
        (void)snprintf(labels[2], 64U, "TRAIL WIDTH %.2f", (double)emitter->trail_width);
        (void)snprintf(labels[3], 64U, "MESH SCALE %.2f", (double)emitter->mesh_scale);
        (void)snprintf(labels[4], 64U, "DECAL ROT %.2f", (double)emitter->decal_rotation_offset);
        (void)snprintf(labels[5], 64U, "DISTORTION %.2f", (double)emitter->distortion_strength);
        (void)snprintf(labels[6], 64U, "SOFT FADE %.2f", (double)emitter->soft_fade_distance);
        (void)snprintf(labels[7], 64U, "LIGHT RADIUS %.1f", (double)emitter->light_radius);
    } else if (editor->particle_page == 5U) {
        (void)snprintf(labels[0], 64U, "FLIP COLUMNS %u", emitter->flipbook_columns);
        (void)snprintf(labels[1], 64U, "FLIP ROWS %u", emitter->flipbook_rows);
        (void)snprintf(labels[2], 64U, "FLIP FPS %.1f",
                       (double)emitter->flipbook_frames_per_second);
        (void)snprintf(labels[3], 64U, "SUB EMITTER %llu",
                       (unsigned long long)emitter->sub_emitter_id);
        (void)snprintf(labels[4], 64U, "ON BIRTH %u", emitter->sub_emit_birth_count);
        (void)snprintf(labels[5], 64U, "ON DEATH %u", emitter->sub_emit_death_count);
        (void)snprintf(labels[6], 64U, "RECURSION %u", emitter->max_recursion_depth);
        (void)snprintf(labels[7], 64U, "EVENT BURST %u", emitter->event_burst_count);
    } else {
        (void)snprintf(labels[0], 64U, "MAX PARTICLES %u", lod->maximum_particles);
        (void)snprintf(labels[1], 64U, "SPAWN QUALITY %.0f%%",
                       (double)(lod->spawn_rate_scale * 100.0F));
        (void)snprintf(labels[2], 64U, "SIM QUALITY %.0f%%",
                       (double)(lod->simulation_frequency_scale * 100.0F));
        (void)snprintf(labels[3], 64U, "RENDERER %s", lod->renderer_enabled ? "ON" : "OFF");
        (void)snprintf(labels[4], 64U, "DIST START %.0f", (double)lod->distance_start);
        (void)snprintf(labels[5], 64U, "DIST END %.0f", (double)lod->distance_end);
        (void)snprintf(labels[6], 64U, "HOST QUALITY %.0f%%", (double)(lod->host_quality * 100.0F));
        (void)snprintf(labels[7], 64U, "VIEW DIST %.0f", (double)lod->view_distance);
    }
}

static void af_editor_render_inspector(AfEditorState *editor, int width) {
    const float x = (float)width - editor->right_width + 18.0F;
    const AfSceneNode *selected = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    const char *selected_name = af_project_name(&editor->project, editor->selected_id);
    char selection_name[64];
    uint32_t index = 0U;
    if (editor->selected_count > 1U) {
        (void)snprintf(selection_name, sizeof(selection_name), "%u OBJECTS SELECTED",
                       editor->selected_count);
        selected_name = selection_name;
    }
    af_editor_add_text(editor, x, 15.0F, "INSPECTOR", 1.5F, editor->right_width - 36.0F,
                       (AfColor){0.78F, 0.81F, 0.88F, 1.0F}, 100125);
    af_editor_add_text(editor, x, 35.0F, selected_name == NULL ? "NO SELECTION" : selected_name,
                       1.0F, editor->right_width - 36.0F, (AfColor){0.5F, 0.55F, 0.65F, 1.0F},
                       100125);
    if (selected != NULL && selected->type == AF_NODE_IMAGE) {
        af_editor_render_image_inspector(editor, width, selected);
        return;
    }
    af_editor_render_color_picker(editor, x, selected);
    af_editor_add_text(editor, x, AF_EDITOR_INSPECTOR_LABEL_Y, "TRANSFORM & GEOMETRY", 0.62F,
                       editor->right_width - 36.0F, (AfColor){0.48F, 0.55F, 0.68F, 1.0F}, 100125);
    {
        const float ratio_width = 76.0F;
        const float ratio_x = x + editor->right_width - 36.0F - ratio_width;
        const bool hovered = af_editor_hovered_rect(
            editor, ratio_x, AF_EDITOR_INSPECTOR_LABEL_Y - 4.0F, ratio_width, 17.0F);
        af_editor_add_button_rect(
            editor, ratio_x, AF_EDITOR_INSPECTOR_LABEL_Y - 4.0F, ratio_width, 17.0F,
            editor->inspector_ratio_locked ? (AfColor){0.16F, 0.48F, 0.78F, 1.0F}
                                           : (hovered ? (AfColor){0.22F, 0.27F, 0.35F, 1.0F}
                                                      : (AfColor){0.16F, 0.18F, 0.23F, 1.0F}),
            100130);
        af_editor_add_text(editor, ratio_x + 7.0F, AF_EDITOR_INSPECTOR_LABEL_Y + 1.0F,
                           editor->inspector_ratio_locked ? "RATIO ON" : "RATIO OFF", 0.55F,
                           ratio_width - 12.0F, (AfColor){0.82F, 0.88F, 0.97F, 1.0F}, 100140);
    }
    {
        static const char *const labels[5][2] = {
            {"X", "Y"}, {"W", "H"}, {"SX", "SY"}, {"ROT", "OP%"}, {"RAD", "LAYER"}};
        static const AfEditorInspectorField fields[5][2] = {
            {AF_EDITOR_INSPECTOR_POSITION_X, AF_EDITOR_INSPECTOR_POSITION_Y},
            {AF_EDITOR_INSPECTOR_WIDTH, AF_EDITOR_INSPECTOR_HEIGHT},
            {AF_EDITOR_INSPECTOR_SCALE_X, AF_EDITOR_INSPECTOR_SCALE_Y},
            {AF_EDITOR_INSPECTOR_ROTATION, AF_EDITOR_INSPECTOR_OPACITY},
            {AF_EDITOR_INSPECTOR_RADIUS, AF_EDITOR_INSPECTOR_LAYER}};
        const float available = editor->right_width - 36.0F;
        const float field_width = (available - 6.0F) * 0.5F;
        char values[5][2][24];
        for (uint32_t row = 0U; row < 5U; ++row) {
            for (uint32_t column = 0U; column < 2U; ++column) {
                (void)snprintf(values[row][column], sizeof(values[row][column]), "--");
            }
        }
        if (selected != NULL) {
            (void)snprintf(values[0][0], sizeof(values[0][0]), "%.2f",
                           (double)selected->local_transform.position.x);
            (void)snprintf(values[0][1], sizeof(values[0][1]), "%.2f",
                           (double)selected->local_transform.position.y);
            (void)snprintf(values[1][0], sizeof(values[1][0]), "%.2f",
                           (double)selected->resolved_bounds.x);
            (void)snprintf(values[1][1], sizeof(values[1][1]), "%.2f",
                           (double)selected->resolved_bounds.y);
            (void)snprintf(values[2][0], sizeof(values[2][0]), "%.3f",
                           (double)selected->local_transform.scale.x);
            (void)snprintf(values[2][1], sizeof(values[2][1]), "%.3f",
                           (double)selected->local_transform.scale.y);
            (void)snprintf(values[3][0], sizeof(values[3][0]), "%.2f",
                           (double)(selected->local_transform.rotation * 57.2957795131F));
            (void)snprintf(values[3][1], sizeof(values[3][1]), "%.1f",
                           (double)(selected->opacity * 100.0F));
            (void)snprintf(values[4][0], sizeof(values[4][0]), "%.2f",
                           (double)selected->corner_radius);
            (void)snprintf(values[4][1], sizeof(values[4][1]), "%d", selected->layer_order);
        }
        for (uint32_t row = 0U; row < 5U; ++row) {
            const float y =
                AF_EDITOR_INSPECTOR_FIELD_Y + (float)row * AF_EDITOR_INSPECTOR_FIELD_STEP;
            for (uint32_t column = 0U; column < 2U; ++column) {
                af_editor_render_inspector_field(editor, x + (float)column * (field_width + 6.0F),
                                                 y, field_width, labels[row][column],
                                                 values[row][column], fields[row][column]);
            }
        }
    }
    if (selected != NULL) {
        const bool vector_shape =
            selected->type == AF_NODE_PATH || selected->type == AF_NODE_RECTANGLE ||
            selected->type == AF_NODE_ROUNDED_RECTANGLE || selected->type == AF_NODE_ELLIPSE;
        if (vector_shape) {
            if (selected->polygon_sides != 0U) {
                char labels[4][24];
                (void)snprintf(labels[0], sizeof(labels[0]), "PTS -");
                (void)snprintf(labels[1], sizeof(labels[1]), "%u +", selected->polygon_sides);
                (void)snprintf(labels[2], sizeof(labels[2]), "INNER -");
                (void)snprintf(labels[3], sizeof(labels[3]), "%.0f%% +",
                               (double)(selected->polygon_inset * 100.0F));
                const float cell = (editor->right_width - 36.0F) / 4.0F;
                for (uint32_t action = 0U; action < 4U; ++action) {
                    if (action >= 2U && selected->polygon_inset == 0.0F)
                        continue;
                    const float bx = x + (float)action * cell;
                    const bool hover = af_editor_hovered_rect(
                        editor, bx, AF_EDITOR_SHAPE_SETTINGS_Y, cell - 3, 26);
                    af_editor_add_button_rect(editor, bx, AF_EDITOR_SHAPE_SETTINGS_Y, cell - 3, 26,
                                              hover ? (AfColor){0.22F, 0.45F, 0.65F, 1}
                                                    : (AfColor){0.16F, 0.23F, 0.32F, 1},
                                              100120);
                    af_editor_add_text(editor, bx + 4, AF_EDITOR_SHAPE_SETTINGS_Y + 9,
                                       labels[action], 0.58F, cell - 8,
                                       (AfColor){0.78F, 0.88F, 1, 1}, 100135);
                }
            } else if (selected->type != AF_NODE_PATH) {
                af_editor_add_text(
                    editor, x, AF_EDITOR_SHAPE_SETTINGS_Y + 8,
                    selected->type == AF_NODE_ELLIPSE ? "SIZE: W / H   CIRCLE: RATIO ON"
                                                      : "SIZE: W / H   CORNERS: RAD",
                    0.61F, editor->right_width - 36, (AfColor){0.55F, 0.66F, 0.78F, 1}, 100125);
            }
            const bool enabled = editor->selected_count == 1U;
            const bool hovered =
                enabled && af_editor_hovered_rect(editor, x, AF_EDITOR_CONVERT_PATH_Y,
                                                  editor->right_width - 36.0F, 26.0F);
            af_editor_add_button_rect(editor, x, AF_EDITOR_CONVERT_PATH_Y,
                                      editor->right_width - 36.0F, 26.0F,
                                      hovered ? (AfColor){0.22F, 0.48F, 0.74F, 1.0F}
                                              : (AfColor){0.16F, 0.23F, 0.32F, 1.0F},
                                      100120);
            af_editor_add_text(editor, x + 10.0F, AF_EDITOR_CONVERT_PATH_Y + 8.0F,
                               selected->type == AF_NODE_PATH && selected->polygon_sides == 0U
                                   ? "EDIT VECTOR POINTS"
                                   : "CONVERT TO PATH",
                               0.7F, editor->right_width - 56.0F,
                               enabled ? (AfColor){0.78F, 0.88F, 1.0F, 1.0F}
                                       : (AfColor){0.4F, 0.44F, 0.5F, 1.0F},
                               100135);
        }
        const AfStyle *style = af_style_find_node_const(&editor->project.style_data, selected->id);
        const AfParticleEmitter *emitter =
            af_project_find_node_emitter(&editor->project, selected->id);
        char outline_width[32];
        (void)snprintf(outline_width, sizeof(outline_width), style == NULL ? "--" : "%.2f PX",
                       style == NULL ? 0.0 : (double)style->stroke.width);
        af_editor_render_inspector_field(editor, x, AF_EDITOR_STYLE_BAR_Y,
                                         editor->right_width - 36.0F, "STROKE", outline_width,
                                         AF_EDITOR_INSPECTOR_OUTLINE_WIDTH);
        for (index = 0U; index < 6U; ++index) {
            static const char *const style_labels[] = {"OUT", "-", "+", "FILL", "CAP", "JOIN"};
            static const char *const layout_labels[] = {"ANCH", "WIDTH", "HEIGHT",
                                                        "FLOW", "ALIGN", "ASPECT"};
            AfColor button = {0.24F, 0.26F, 0.32F, 1.0F};
            if (style != NULL && ((index == 0U && style->stroke.enabled) ||
                                  (index == 3U && style->fill.type != AF_PAINT_SOLID))) {
                button = (AfColor){0.22F, 0.58F, 0.96F, 1.0F};
            }
            af_editor_add_button_rect(editor, x + (float)index * 38.0F, AF_EDITOR_STYLE_ROW_Y,
                                      34.0F, 28.0F, button, 100120);
            af_editor_add_button_rect(editor, x + (float)index * 38.0F, AF_EDITOR_LAYOUT_ROW_Y,
                                      34.0F, 28.0F, (AfColor){0.28F, 0.3F, 0.37F, 1.0F}, 100120);
            af_editor_add_text(editor, x + (float)index * 38.0F + 4.0F,
                               AF_EDITOR_STYLE_ROW_Y + 9.0F, style_labels[index], 0.65F, 26.0F,
                               (AfColor){0.82F, 0.84F, 0.9F, 1.0F}, 100135);
            af_editor_add_text(editor, x + (float)index * 38.0F + 3.0F,
                               AF_EDITOR_LAYOUT_ROW_Y + 9.0F, layout_labels[index], 0.58F, 27.0F,
                               (AfColor){0.82F, 0.84F, 0.9F, 1.0F}, 100135);
        }
        af_editor_add_text(editor, x, AF_EDITOR_PARTICLE_LABEL_Y, "PARTICLE EMITTER", 0.85F,
                           editor->right_width - 36.0F,
                           emitter == NULL ? (AfColor){0.45F, 0.49F, 0.58F, 1.0F}
                                           : (AfColor){0.45F, 0.78F, 1.0F, 1.0F},
                           100125);
        if (emitter == NULL) {
            af_editor_add_text(editor, x, AF_EDITOR_PARTICLE_LABEL_Y + 21.0F,
                               "RIGHT CLICK > TOGGLE PARTICLES", 0.62F, editor->right_width - 36.0F,
                               (AfColor){0.52F, 0.56F, 0.64F, 1.0F}, 100125);
        } else {
            static const char *const page_names[] = {"EMIT", "MOVE",  "FORCE", "COLL",
                                                     "LOOK", "SPAWN", "LOD"};
            char labels[AF_EDITOR_PARTICLE_ROW_COUNT][64];
            const AfVkVfxMetrics metrics = af_vk_renderer_vfx_metrics(editor->renderer);
            editor->particle_page %= AF_EDITOR_PARTICLE_PAGE_COUNT;
            af_editor_particle_labels(editor, emitter, labels);
            for (uint32_t page = 0U; page < AF_EDITOR_PARTICLE_PAGE_COUNT; ++page) {
                const float page_x = x + (float)page * 32.0F;
                af_editor_add_button_rect(editor, page_x, AF_EDITOR_PARTICLE_PAGE_Y, 30.0F, 26.0F,
                                          page == editor->particle_page
                                              ? (AfColor){0.18F, 0.48F, 0.76F, 1.0F}
                                              : (AfColor){0.2F, 0.22F, 0.27F, 1.0F},
                                          100120);
                af_editor_add_text(editor, page_x + 3.0F, AF_EDITOR_PARTICLE_PAGE_Y + 7.0F,
                                   page_names[page], 0.5F, 24.0F,
                                   (AfColor){0.86F, 0.89F, 0.95F, 1.0F}, 100135);
            }
            for (uint32_t row = 0U; row < AF_EDITOR_PARTICLE_ROW_COUNT; ++row) {
                const float row_y =
                    AF_EDITOR_PARTICLE_ROW_Y + (float)row * AF_EDITOR_PARTICLE_ROW_STEP;
                af_editor_add_button_rect(editor, x, row_y, 31.0F, 26.0F,
                                          (AfColor){0.24F, 0.26F, 0.32F, 1.0F}, 100120);
                af_editor_add_button_rect(editor, x + 34.0F, row_y, 31.0F, 26.0F,
                                          (AfColor){0.24F, 0.26F, 0.32F, 1.0F}, 100120);
                af_editor_add_text(editor, x + 9.0F, row_y + 6.0F, "-", 0.8F, 10.0F,
                                   (AfColor){0.85F, 0.87F, 0.92F, 1.0F}, 100135);
                af_editor_add_text(editor, x + 42.0F, row_y + 6.0F, "+", 0.8F, 12.0F,
                                   (AfColor){0.85F, 0.87F, 0.92F, 1.0F}, 100135);
                af_editor_add_text(editor, x + 72.0F, row_y + 7.0F, labels[row], 0.72F,
                                   editor->right_width - 108.0F,
                                   (AfColor){0.7F, 0.74F, 0.82F, 1.0F}, 100130);
            }
            {
                char timing[96];
                (void)snprintf(timing, sizeof(timing), "VFX CPU %.2f MS  GPU %.2f MS  LOD %.0f%%",
                               (double)metrics.cpu_milliseconds, (double)metrics.gpu_milliseconds,
                               (double)(metrics.lod_factor * 100.0F));
                af_editor_add_text(editor, x,
                                   AF_EDITOR_PARTICLE_ROW_Y +
                                       AF_EDITOR_PARTICLE_ROW_COUNT * AF_EDITOR_PARTICLE_ROW_STEP +
                                       2.0F,
                                   timing, 0.58F, editor->right_width - 36.0F,
                                   (AfColor){0.48F, 0.66F, 0.76F, 1.0F}, 100130);
            }
        }
    }
}

static AfVec2 af_editor_context_origin(const AfEditorState *editor, int width, int height) {
    const AfEditorContextMenuInfo *info = af_editor_context_info(editor->context_menu);
    const float menu_height =
        info == NULL ? 0.0F
                     : AF_EDITOR_CONTEXT_HEADER_HEIGHT +
                           (float)info->item_count * AF_EDITOR_CONTEXT_ITEM_HEIGHT + 8.0F;
    return (AfVec2){
        af_editor_clamp(editor->context_position.x, 6.0F,
                        (float)width - AF_EDITOR_CONTEXT_WIDTH - 6.0F),
        af_editor_clamp(editor->context_position.y, 6.0F, (float)height - menu_height - 6.0F)};
}

static void af_editor_render_context_icon(AfEditorState *editor, const char *label, float x,
                                          float y) {
    const AfColor color = {0.56F, 0.78F, 1.0F, 1.0F};
    const int32_t layer = 200030;
    if (strcmp(label, "ADD RECTANGLE") == 0 || strcmp(label, "ADD RECT CHILD") == 0 ||
        strcmp(label, "MAKE RECTANGLE") == 0) {
        af_editor_add_rect(editor, x, y + 2.0F, 14.0F, 10.0F, color, layer);
    } else if (strcmp(label, "ADD ROUNDED RECT") == 0 || strcmp(label, "MAKE ROUNDED") == 0 ||
               strstr(label, "RADIUS") != NULL) {
        af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y + 2.0F},
                            (AfVec2){14.0F, 10.0F}, color, 3.0F, layer);
    } else if (strcmp(label, "ADD ELLIPSE") == 0 || strcmp(label, "MAKE CIRCLE") == 0) {
        af_editor_add_shape(editor, AF_NODE_ELLIPSE,
                            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 1.0F, .ty = y},
                            (AfVec2){12.0F, 12.0F}, color, 0.0F, layer);
    } else if (strstr(label, "FIT") != NULL) {
        af_editor_add_line(editor, (AfVec2){x, y + 4.0F}, (AfVec2){x, y}, 2.0F, color, layer);
        af_editor_add_line(editor, (AfVec2){x, y}, (AfVec2){x + 4.0F, y}, 2.0F, color, layer);
        af_editor_add_line(editor, (AfVec2){x + 10.0F, y}, (AfVec2){x + 14.0F, y}, 2.0F, color,
                           layer);
        af_editor_add_line(editor, (AfVec2){x + 14.0F, y}, (AfVec2){x + 14.0F, y + 4.0F}, 2.0F,
                           color, layer);
        af_editor_add_line(editor, (AfVec2){x, y + 9.0F}, (AfVec2){x, y + 13.0F}, 2.0F, color,
                           layer);
        af_editor_add_line(editor, (AfVec2){x, y + 13.0F}, (AfVec2){x + 4.0F, y + 13.0F}, 2.0F,
                           color, layer);
        af_editor_add_line(editor, (AfVec2){x + 10.0F, y + 13.0F}, (AfVec2){x + 14.0F, y + 13.0F},
                           2.0F, color, layer);
        af_editor_add_line(editor, (AfVec2){x + 14.0F, y + 13.0F}, (AfVec2){x + 14.0F, y + 9.0F},
                           2.0F, color, layer);
    } else if (strstr(label, "FILL RULE") != NULL) {
        af_editor_add_shape(editor, AF_NODE_ELLIPSE,
                            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y},
                            (AfVec2){13.0F, 13.0F}, color, 0.0F, layer);
        af_editor_add_shape(editor, AF_NODE_ELLIPSE,
                            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 4.0F, .ty = y + 4.0F},
                            (AfVec2){5.0F, 5.0F}, (AfColor){0.12F, 0.18F, 0.26F, 1.0F}, 0.0F,
                            layer + 1);
    } else if (strstr(label, "EDIT POINTS") != NULL) {
        af_editor_add_line(editor, (AfVec2){x + 1.0F, y + 11.0F}, (AfVec2){x + 7.0F, y + 2.0F},
                           1.5F, color, layer);
        af_editor_add_line(editor, (AfVec2){x + 7.0F, y + 2.0F}, (AfVec2){x + 13.0F, y + 9.0F},
                           1.5F, color, layer);
        af_editor_add_rect(editor, x, y + 10.0F, 3.0F, 3.0F, color, layer + 1);
        af_editor_add_rect(editor, x + 6.0F, y + 1.0F, 3.0F, 3.0F, color, layer + 1);
        af_editor_add_rect(editor, x + 12.0F, y + 8.0F, 3.0F, 3.0F, color, layer + 1);
    } else if (strstr(label, "REVERSE") != NULL) {
        af_editor_add_line(editor, (AfVec2){x + 13.0F, y + 3.0F}, (AfVec2){x + 3.0F, y + 3.0F},
                           2.0F, color, layer);
        af_editor_add_line(editor, (AfVec2){x + 3.0F, y + 3.0F}, (AfVec2){x + 7.0F, y}, 2.0F, color,
                           layer);
        af_editor_add_line(editor, (AfVec2){x + 3.0F, y + 3.0F}, (AfVec2){x + 7.0F, y + 6.0F}, 2.0F,
                           color, layer);
    } else if (strstr(label, "PATH") != NULL || strstr(label, "CURVE") != NULL) {
        af_editor_add_line(editor, (AfVec2){x, y + 11.0F}, (AfVec2){x + 5.0F, y + 3.0F}, 2.0F,
                           color, layer);
        af_editor_add_line(editor, (AfVec2){x + 5.0F, y + 3.0F}, (AfVec2){x + 14.0F, y + 9.0F},
                           2.0F, color, layer);
    } else if (strstr(label, "GRID") != NULL) {
        for (uint32_t index = 0U; index < 3U; ++index) {
            af_editor_add_rect(editor, x + (float)index * 5.0F, y, 1.0F, 13.0F, color, layer);
            af_editor_add_rect(editor, x, y + (float)index * 5.0F, 13.0F, 1.0F, color, layer);
        }
    } else if (strstr(label, "SNAP") != NULL) {
        af_editor_add_rect(editor, x, y, 3.0F, 10.0F, color, layer);
        af_editor_add_rect(editor, x + 10.0F, y, 3.0F, 10.0F, color, layer);
        af_editor_add_rect(editor, x + 2.0F, y + 8.0F, 9.0F, 3.0F, color, layer);
    } else if (strstr(label, "DELETE") != NULL) {
        const AfColor danger = {1.0F, 0.45F, 0.42F, 1.0F};
        af_editor_add_line(editor, (AfVec2){x + 1.0F, y + 1.0F}, (AfVec2){x + 12.0F, y + 12.0F},
                           2.0F, danger, layer);
        af_editor_add_line(editor, (AfVec2){x + 12.0F, y + 1.0F}, (AfVec2){x + 1.0F, y + 12.0F},
                           2.0F, danger, layer);
    } else if (strstr(label, "DUPLICATE") != NULL || strstr(label, "COPY") != NULL) {
        af_editor_add_rect(editor, x, y, 10.0F, 9.0F, (AfColor){0.34F, 0.48F, 0.66F, 1.0F}, layer);
        af_editor_add_rect(editor, x + 4.0F, y + 4.0F, 10.0F, 9.0F, color, layer + 1);
    } else if (strstr(label, "RENAME") != NULL) {
        af_editor_add_line(editor, (AfVec2){x + 1.0F, y + 12.0F}, (AfVec2){x + 12.0F, y + 1.0F},
                           3.0F, color, layer);
    } else if (strstr(label, "UNPARENT") != NULL) {
        af_editor_add_shape(editor, AF_NODE_ELLIPSE,
                            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x, .ty = y + 3.0F},
                            (AfVec2){7.0F, 7.0F}, color, 0.0F, layer);
        af_editor_add_shape(editor, AF_NODE_ELLIPSE,
                            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = x + 7.0F, .ty = y + 3.0F},
                            (AfVec2){7.0F, 7.0F}, color, 0.0F, layer);
        af_editor_add_line(editor, (AfVec2){x, y + 13.0F}, (AfVec2){x + 14.0F, y}, 2.0F,
                           (AfColor){1.0F, 0.45F, 0.42F, 1.0F}, layer + 1);
    } else if (strstr(label, "KEYFRAME") != NULL) {
        af_editor_add_shape(editor, AF_NODE_RECTANGLE,
                            (AfAffine2){.m00 = 0.70710678F,
                                        .m01 = -0.70710678F,
                                        .m10 = 0.70710678F,
                                        .m11 = 0.70710678F,
                                        .tx = x + 7.0F,
                                        .ty = y},
                            (AfVec2){9.0F, 9.0F}, (AfColor){1.0F, 0.76F, 0.28F, 1.0F}, 0.0F, layer);
    } else if (strstr(label, "FORWARD") != NULL || strstr(label, "BACKWARD") != NULL) {
        const bool forward = strstr(label, "FORWARD") != NULL;
        const float tip = forward ? y : y + 13.0F;
        const float base = forward ? y + 13.0F : y;
        af_editor_add_line(editor, (AfVec2){x + 7.0F, tip}, (AfVec2){x + 1.0F, base}, 2.0F, color,
                           layer);
        af_editor_add_line(editor, (AfVec2){x + 7.0F, tip}, (AfVec2){x + 13.0F, base}, 2.0F, color,
                           layer);
    } else if (strstr(label, "UNDO") != NULL || strstr(label, "REDO") != NULL) {
        const float direction = strstr(label, "UNDO") != NULL ? -1.0F : 1.0F;
        const float center = x + 7.0F;
        af_editor_add_line(editor, (AfVec2){center - direction * 5.0F, y + 6.0F},
                           (AfVec2){center + direction * 5.0F, y + 2.0F}, 2.0F, color, layer);
        af_editor_add_line(editor, (AfVec2){center - direction * 5.0F, y + 6.0F},
                           (AfVec2){center + direction * 2.0F, y + 12.0F}, 2.0F, color, layer);
    } else if (strstr(label, "SAVE") != NULL || strstr(label, "PASTE") != NULL) {
        af_editor_add_rect(editor, x + 1.0F, y, 12.0F, 13.0F, color, layer);
        af_editor_add_rect(editor, x + 4.0F, y + 2.0F, 6.0F, 4.0F,
                           (AfColor){0.12F, 0.18F, 0.26F, 1.0F}, layer + 1);
    } else {
        af_editor_add_rect(editor, x + 5.0F, y, 3.0F, 13.0F, color, layer);
        af_editor_add_rect(editor, x, y + 5.0F, 13.0F, 3.0F, color, layer);
    }
}

static void af_editor_render_context_menu(AfEditorState *editor, int width, int height) {
    const AfEditorContextMenuInfo *info = af_editor_context_info(editor->context_menu);
    const AfVec2 origin = af_editor_context_origin(editor, width, height);
    if (info == NULL) {
        return;
    }
    const float menu_height = AF_EDITOR_CONTEXT_HEADER_HEIGHT +
                              (float)info->item_count * AF_EDITOR_CONTEXT_ITEM_HEIGHT + 8.0F;
    af_editor_add_shape(
        editor, AF_NODE_ROUNDED_RECTANGLE,
        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = origin.x + 5.0F, .ty = origin.y + 6.0F},
        (AfVec2){AF_EDITOR_CONTEXT_WIDTH, menu_height}, (AfColor){0.0F, 0.0F, 0.0F, 0.45F}, 7.0F,
        200000);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = origin.x, .ty = origin.y},
                        (AfVec2){AF_EDITOR_CONTEXT_WIDTH, menu_height},
                        (AfColor){0.105F, 0.115F, 0.14F, 1.0F}, 7.0F, 200010);
    af_editor_add_rect(editor, origin.x, origin.y + AF_EDITOR_CONTEXT_HEADER_HEIGHT - 1.0F,
                       AF_EDITOR_CONTEXT_WIDTH, 1.0F, (AfColor){0.28F, 0.3F, 0.36F, 1.0F}, 200020);
    af_editor_add_text(editor, origin.x + 13.0F, origin.y + 10.0F, info->title, 1.2F,
                       AF_EDITOR_CONTEXT_WIDTH - 26.0F, (AfColor){0.57F, 0.72F, 0.94F, 1.0F},
                       200030);
    for (uint32_t index = 0U; index < info->item_count; ++index) {
        const float item_y = origin.y + AF_EDITOR_CONTEXT_HEADER_HEIGHT +
                             (float)index * AF_EDITOR_CONTEXT_ITEM_HEIGHT + 4.0F;
        const bool hovered = editor->mouse_position.x >= origin.x &&
                             editor->mouse_position.x <= origin.x + AF_EDITOR_CONTEXT_WIDTH &&
                             editor->mouse_position.y >= item_y &&
                             editor->mouse_position.y < item_y + AF_EDITOR_CONTEXT_ITEM_HEIGHT;
        if (hovered) {
            af_editor_add_rect(editor, origin.x + 4.0F, item_y, AF_EDITOR_CONTEXT_WIDTH - 8.0F,
                               AF_EDITOR_CONTEXT_ITEM_HEIGHT - 1.0F,
                               (AfColor){0.18F, 0.34F, 0.56F, 1.0F}, 200020);
        }
        af_editor_render_context_icon(editor, info->items[index], origin.x + 11.0F, item_y + 7.0F);
        af_editor_add_text(editor, origin.x + 34.0F, item_y + 9.0F, info->items[index], 1.05F,
                           AF_EDITOR_CONTEXT_WIDTH - 45.0F, (AfColor){0.85F, 0.87F, 0.92F, 1.0F},
                           200035);
    }
}

static bool af_editor_point_in_rect(AfVec2 point, float x, float y, float width, float height) {
    return point.x >= x && point.x < x + width && point.y >= y && point.y < y + height;
}

static AfEditorTooltip af_editor_context_tooltip(const char *label, uint64_t id) {
    AfEditorTooltip tooltip = {.id = id,
                               .title = label,
                               .summary = "Run this context action.",
                               .details = "The action applies to the current editor context.",
                               .shortcut = "Right click opens context actions"};
    if (strstr(label, "DUPLICATE") != NULL) {
        tooltip.summary = "Create an independent copy of the selection.";
        tooltip.details = "Copies the object or selected keyframes and assigns stable new IDs.";
        tooltip.shortcut = "Shortcut: Ctrl+D";
    } else if (strstr(label, "RENAME") != NULL) {
        tooltip.summary = "Rename the selected scene object.";
        tooltip.details = "Starts inline name editing; Enter confirms and Escape cancels.";
        tooltip.shortcut = "Shortcut: F2";
    } else if (strstr(label, "UNUSED ASSET") != NULL) {
        tooltip.summary = "Remove an image that has no remaining dependencies.";
        tooltip.details = "Deletion is rejected while a scene object or sequence still uses it.";
        tooltip.shortcut = "Select Users shows current dependencies";
    } else if (strstr(label, "DELETE") != NULL) {
        tooltip.summary = "Delete the selected item safely.";
        tooltip.details = "Deletes the current object or selected keys through undo history.";
        tooltip.shortcut = "Shortcut: Delete";
    } else if (strstr(label, "PARTICLES") != NULL) {
        tooltip.summary = "Attach or remove a GPU particle emitter.";
        tooltip.details = "Uses this object's shape as the emitter source with bounded GPU memory.";
        tooltip.shortcut = "Edit emitter values in the Inspector";
    } else if (strstr(label, "ADD KEYFRAME") != NULL) {
        tooltip.summary = "Key the selected property at the playhead.";
        tooltip.details =
            "Creates a key on the active clip and property track at the current time.";
        tooltip.shortcut = "Shortcut: K";
    } else if (strstr(label, "KEYFRAME") != NULL) {
        tooltip.summary = "Edit the currently selected animation keys.";
        tooltip.details = "The operation is bounded, undoable, and keeps stable keyframe IDs.";
        tooltip.shortcut = "Ctrl+C / Ctrl+V / Delete";
    } else if (strstr(label, "ADD RECTANGLE") != NULL || strstr(label, "RECT CHILD") != NULL) {
        tooltip.summary = "Create a new rectangle object.";
        tooltip.details = "Adds an editable rectangle at the canvas center or under this group.";
        tooltip.shortcut = "Shortcut: 1";
    } else if (strstr(label, "ADD ROUNDED") != NULL) {
        tooltip.summary = "Create a rounded rectangle object.";
        tooltip.details = "Adds a rectangle with an editable corner radius at the canvas center.";
        tooltip.shortcut = "Shortcut: 2";
    } else if (strstr(label, "ADD ELLIPSE") != NULL) {
        tooltip.summary = "Create a new ellipse object.";
        tooltip.details =
            "Adds an editable ellipse at the canvas center; equal bounds make a circle.";
        tooltip.shortcut = "Shortcut: 3";
    } else if (strstr(label, "ADD PATH") != NULL) {
        tooltip.summary = "Create a new vector path.";
        tooltip.details =
            "Adds an editable Bézier path with direct point and control-handle editing.";
        tooltip.shortcut = "Shortcut: 4";
    } else if (strstr(label, "GRID") != NULL) {
        tooltip.summary = "Show or hide the canvas grid.";
        tooltip.details = "The grid is a visual guide and is not included in runtime packages.";
        tooltip.shortcut = "Shortcut: G";
    } else if (strstr(label, "SNAP") != NULL) {
        tooltip.summary = "Toggle position and timeline snapping.";
        tooltip.details = "Rounds object moves and timeline edits to the active grid increments.";
        tooltip.shortcut = "Shortcut: N";
    } else if (strstr(label, "SAVE") != NULL) {
        tooltip.summary = "Save the editable Arcflow project.";
        tooltip.details =
            "Writes scene, animation, style, layout, path, and particle authoring data.";
        tooltip.shortcut = "Shortcut: Ctrl+S";
    } else if (strstr(label, "COMPILE") != NULL) {
        tooltip.summary = "Compile the project into a runtime package.";
        tooltip.details = "Validates authoring data and writes the bounded package used by Player.";
        tooltip.shortcut = "Shortcut: F5";
    } else if (strstr(label, "UNDO") != NULL) {
        tooltip.summary = "Undo the most recent editor command.";
        tooltip.details = "Restores the previous bounded command snapshot without changing IDs.";
        tooltip.shortcut = "Shortcut: Ctrl+Z";
    } else if (strstr(label, "REDO") != NULL) {
        tooltip.summary = "Redo the most recently undone command.";
        tooltip.details = "Reapplies the command at the current history position.";
        tooltip.shortcut = "Shortcut: Ctrl+Y";
    } else if (strstr(label, "FORWARD") != NULL || strstr(label, "BACKWARD") != NULL) {
        tooltip.summary = "Change the object's drawing order.";
        tooltip.details = "Moves the object one layer while preserving its scene hierarchy.";
        tooltip.shortcut = "Shortcuts: [ and ]";
    } else if (strstr(label, "UNPARENT") != NULL) {
        tooltip.summary = "Move the object out of its parent group.";
        tooltip.details = "Reparents to the scene root while rejecting cyclic hierarchies.";
        tooltip.shortcut = "Shortcut: U";
    } else if (strstr(label, "CURVE") != NULL) {
        tooltip.summary = "Show or hide the animation curve editor.";
        tooltip.details = "Displays easing curves and tangent handles for selected cubic keys.";
        tooltip.shortcut = "Shortcut: F6";
    } else if (strstr(label, "CLIP") != NULL) {
        tooltip.summary = "Create or manage an animation clip.";
        tooltip.details = "Clips contain bounded tracks, keys, duration, looping, and events.";
        tooltip.shortcut = "New clip: Alt+C";
    } else if (strstr(label, "RESET") != NULL) {
        tooltip.summary = "Restore default values for this section.";
        tooltip.details = "Creates one undoable command for the selected object's values.";
        tooltip.shortcut = "Undo available with Ctrl+Z";
    } else if (strstr(label, "PATH") != NULL || strstr(label, "POINTS") != NULL ||
               strstr(label, "CONTOUR") != NULL || strstr(label, "FILL RULE") != NULL) {
        tooltip.summary = "Modify this vector path.";
        tooltip.details = "Edits bounded path commands while invalidating only affected geometry.";
        tooltip.shortcut = "Path shortcuts: I, C, O, F, Shift+R";
    }
    return tooltip;
}
