static void af_editor_modify_scalar(AfEditorState *editor, AfEditorMergeKey merge_key, float amount,
                                    int property) {
    AfPropertyType animation_property = AF_PROPERTY_POSITION;
    bool animatable = true;
    if (editor->selected_id == 0U) {
        return;
    }
    if (property == 0) {
        animation_property = AF_PROPERTY_OPACITY;
    } else if (property == 1) {
        animation_property = AF_PROPERTY_CORNER_RADIUS;
    } else if (property == 2) {
        animatable = false;
    } else if (property == 3) {
        animation_property = AF_PROPERTY_ROTATION;
    } else if (property == 4) {
        animation_property = AF_PROPERTY_SCALE;
    } else {
        return;
    }
    if (animatable) {
        editor->selected_property = animation_property;
    }
    af_editor_begin_selection_modify(editor, false);
    if (animatable) {
        af_editor_begin_transform_key_drag(editor, animation_property);
    }
    for (uint32_t index = 0U; index < editor->selection_drag_count; ++index) {
        const uint64_t node_id = editor->selection_drag_before[index].node.id;
        const int32_t key_index =
            animatable ? af_editor_transform_drag_key_index(editor, node_id) : -1;
        AfProjectKeyframe *key =
            key_index < 0
                ? NULL
                : af_project_find_keyframe(&editor->project,
                                           editor->transform_drag_key_ids[(uint32_t)key_index]);
        AfSceneNode *node = af_scene_find(&editor->project.scene, node_id);
        if (node == NULL) {
            continue;
        }
        switch (property) {
        case 0:
            if (key != NULL) {
                key->keyframe.value[0] = af_editor_clamp(
                    editor->transform_drag_key_starts[(uint32_t)key_index][0] + amount, 0.0F, 1.0F);
            } else {
                node->opacity = af_editor_clamp(node->opacity + amount, 0.0F, 1.0F);
            }
            break;
        case 1:
            if (key != NULL) {
                key->keyframe.value[0] =
                    fmaxf(0.0F, editor->transform_drag_key_starts[(uint32_t)key_index][0] + amount);
            } else {
                node->corner_radius = fmaxf(0.0F, node->corner_radius + amount);
            }
            break;
        case 2:
            if ((amount > 0.0F && node->layer_order < INT32_MAX) ||
                (amount < 0.0F && node->layer_order > INT32_MIN)) {
                node->layer_order += amount > 0.0F ? 1 : -1;
            }
            break;
        case 3:
            if (key != NULL) {
                key->keyframe.value[0] =
                    editor->transform_drag_key_starts[(uint32_t)key_index][0] + amount;
            } else {
                node->local_transform.rotation += amount;
            }
            break;
        case 4:
            if (key != NULL) {
                key->keyframe.value[0] = fmaxf(
                    0.05F, editor->transform_drag_key_starts[(uint32_t)key_index][0] + amount);
                key->keyframe.value[1] = fmaxf(
                    0.05F, editor->transform_drag_key_starts[(uint32_t)key_index][1] + amount);
            } else {
                node->local_transform.scale.x =
                    fmaxf(0.05F, node->local_transform.scale.x + amount);
                node->local_transform.scale.y =
                    fmaxf(0.05F, node->local_transform.scale.y + amount);
            }
            break;
        }
    }
    (void)af_scene_evaluate_world(&editor->project.scene);
    if (editor->transform_drag_key_count != 0U) {
        af_editor_update_preview(editor);
    }
    if (!af_editor_transform_key_drag_covers_selection(editor)) {
        (void)af_editor_commit_selection_modify(editor, merge_key, true);
    }
    (void)af_editor_commit_transform_key_drag(editor, true);
}

static AfColor af_editor_inspector_color(const AfEditorState *editor, const AfSceneNode *node) {
    const AfStyle *style =
        node == NULL ? NULL : af_style_find_node_const(&editor->project.style_data, node->id);
    if (node) {
        for (uint32_t i = 0; i < editor->project.track_count; ++i) {
            const AfProjectTrack *track = &editor->project.tracks[i];
            if (track->node_id == node->id && track->clip_id == editor->selected_clip_id &&
                ((af_property_is_gradient(track->property) &&
                  (track->element_index >= AF_ANIMATION_OUTLINE_ELEMENT) ==
                      editor->color_target_outline) ||
                 (track->property == AF_PROPERTY_OUTLINE_COLOR && editor->color_target_outline))) {
                const AfStyle *animated =
                    af_style_find_node_const(&editor->preview_style_data, node->id);
                if (animated)
                    style = animated;
                break;
            }
        }
    }
    const AfPaint *paint = af_editor_target_paint(editor, style);
    if (paint && paint->type != AF_PAINT_SOLID && paint->stop_count)
        return paint->stops[af_editor_gradient_stop(editor, paint)].color;
    if (editor->color_target_outline && paint)
        return paint->solid;
    return node == NULL ? (AfColor){0.5F, 0.5F, 0.5F, 1.0F} : node->fill;
}

static void af_editor_apply_fill(AfEditorState *editor, AfColor color) {
    for (uint32_t index = 0U; index < editor->selected_count; ++index) {
        const uint64_t node_id = editor->selected_ids[index];
        const int32_t key_index = af_editor_transform_drag_key_index(editor, node_id);
        AfProjectKeyframe *key =
            key_index < 0
                ? NULL
                : af_project_find_keyframe(&editor->project,
                                           editor->transform_drag_key_ids[(uint32_t)key_index]);
        AfSceneNode *node = af_scene_find(&editor->project.scene, node_id);
        if (node == NULL) {
            continue;
        }
        if (key != NULL) {
            memcpy(key->keyframe.value, &color, sizeof(color));
        } else {
            AfStyle *style = af_project_find_node_style(&editor->project, node->id);
            if (!editor->color_target_outline)
                node->fill = color;
            if (style != NULL) {
                AfPaint *paint = editor->color_target_outline ? &style->stroke_paint : &style->fill;
                if (paint->type != AF_PAINT_SOLID && paint->stop_count)
                    paint->stops[af_editor_gradient_stop(editor, paint)].color = color;
                else
                    *paint = af_paint_solid(color);
                style->inherit_flags &=
                    ~(uint32_t)(editor->color_target_outline ? AF_STYLE_INHERIT_STROKE_PAINT
                                                             : AF_STYLE_INHERIT_FILL);
                (void)af_style_mark_paint_changed(style);
            }
        }
    }
    if (editor->transform_drag_key_count != 0U) {
        af_editor_update_preview(editor);
    }
    af_editor_mark_dirty(editor);
}

static AfEditorInspectorField af_editor_inspector_field_at(const AfEditorState *editor, int width,
                                                           float x, float y) {
    const float panel_x = (float)width - editor->right_width + 18.0F;
    const float available = editor->right_width - 36.0F;
    const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    if (node != NULL && y >= AF_EDITOR_PARTICLE_ROW_Y &&
        y < AF_EDITOR_PARTICLE_ROW_Y + AF_EDITOR_PARTICLE_ROW_COUNT * AF_EDITOR_PARTICLE_ROW_STEP &&
        x >= panel_x && x < panel_x + available) {
        const uint32_t row =
            (uint32_t)((y - AF_EDITOR_PARTICLE_ROW_Y) / AF_EDITOR_PARTICLE_ROW_STEP);
        uint32_t value_row = row;
        if (((editor->particle_page == 1 && row == 6) ||
             (editor->particle_page == 3 && row == 7)) &&
            x >= panel_x + (available - 6) * .5F + 6)
            value_row = 8;
        const AfEditorInspectorField field =
            (AfEditorInspectorField)(AF_EDITOR_INSPECTOR_PARTICLE_FIRST +
                                     editor->particle_page * 9 + value_row);
        float field_x, field_width;
        if (af_editor_particle_field_rect(editor, width, field, &field_x, &field_width) &&
            x >= field_x && x < field_x + field_width &&
            y < AF_EDITOR_PARTICLE_ROW_Y + (float)row * AF_EDITOR_PARTICLE_ROW_STEP +
                    AF_EDITOR_INSPECTOR_FIELD_HEIGHT)
            return field;
    }
    if (node == NULL || node->type == AF_NODE_IMAGE || x < panel_x || x >= panel_x + available) {
        return AF_EDITOR_INSPECTOR_NONE;
    }
    if (y >= AF_EDITOR_COLOR_HEX_Y &&
        y < AF_EDITOR_COLOR_HEX_Y + AF_EDITOR_INSPECTOR_FIELD_HEIGHT) {
        return AF_EDITOR_INSPECTOR_COLOR_HEX;
    }
    if (y >= AF_EDITOR_COLOR_COMPONENT_Y &&
        y < AF_EDITOR_COLOR_COMPONENT_Y + 2.0F * AF_EDITOR_INSPECTOR_FIELD_STEP) {
        const float field_width = (available - 6.0F) * 0.5F;
        const uint32_t column = x >= panel_x + field_width + 6.0F ? 1U : 0U;
        const uint32_t row =
            (uint32_t)((y - AF_EDITOR_COLOR_COMPONENT_Y) / AF_EDITOR_INSPECTOR_FIELD_STEP);
        const float component_x = panel_x + (float)column * (field_width + 6.0F);
        const float row_y =
            AF_EDITOR_COLOR_COMPONENT_Y + (float)row * AF_EDITOR_INSPECTOR_FIELD_STEP;
        if (x >= component_x && x < component_x + field_width &&
            y < row_y + AF_EDITOR_INSPECTOR_FIELD_HEIGHT) {
            return (AfEditorInspectorField)(AF_EDITOR_INSPECTOR_COLOR_R + row * 2U + column);
        }
        return AF_EDITOR_INSPECTOR_NONE;
    }
    if (y >= AF_EDITOR_INSPECTOR_FIELD_Y &&
        y < AF_EDITOR_INSPECTOR_FIELD_Y + 5.0F * AF_EDITOR_INSPECTOR_FIELD_STEP) {
        static const AfEditorInspectorField fields[5][2] = {
            {AF_EDITOR_INSPECTOR_POSITION_X, AF_EDITOR_INSPECTOR_POSITION_Y},
            {AF_EDITOR_INSPECTOR_WIDTH, AF_EDITOR_INSPECTOR_HEIGHT},
            {AF_EDITOR_INSPECTOR_SCALE_X, AF_EDITOR_INSPECTOR_SCALE_Y},
            {AF_EDITOR_INSPECTOR_ROTATION, AF_EDITOR_INSPECTOR_OPACITY},
            {AF_EDITOR_INSPECTOR_RADIUS, AF_EDITOR_INSPECTOR_LAYER}};
        const float field_width = (available - 6.0F) * 0.5F;
        const uint32_t row =
            (uint32_t)((y - AF_EDITOR_INSPECTOR_FIELD_Y) / AF_EDITOR_INSPECTOR_FIELD_STEP);
        const float row_y =
            AF_EDITOR_INSPECTOR_FIELD_Y + (float)row * AF_EDITOR_INSPECTOR_FIELD_STEP;
        const uint32_t column = x >= panel_x + field_width + 6.0F ? 1U : 0U;
        const float field_x = panel_x + (float)column * (field_width + 6.0F);
        if (row < 5U && y < row_y + AF_EDITOR_INSPECTOR_FIELD_HEIGHT && x >= field_x &&
            x < field_x + field_width) {
            return fields[row][column];
        }
    }
    if (y >= AF_EDITOR_STYLE_BAR_Y &&
        y < AF_EDITOR_STYLE_BAR_Y + AF_EDITOR_INSPECTOR_FIELD_HEIGHT) {
        return AF_EDITOR_INSPECTOR_OUTLINE_WIDTH;
    }
    return AF_EDITOR_INSPECTOR_NONE;
}

static bool af_editor_inspector_field_rect(const AfEditorState *editor, int width,
                                           AfEditorInspectorField field, float *out_x,
                                           float *out_width) {
    if (field >= AF_EDITOR_INSPECTOR_PARTICLE_FIRST) {
        float x, w;
        if (!af_editor_particle_field_rect(editor, width, field, &x, &w))
            return false;
        if (out_x != NULL)
            *out_x = x;
        if (out_width != NULL)
            *out_width = w;
        return true;
    }
    const float panel_x = (float)width - editor->right_width + 18.0F;
    const float available = editor->right_width - 36.0F;
    if ((field >= AF_EDITOR_INSPECTOR_POSITION_X && field <= AF_EDITOR_INSPECTOR_LAYER) ||
        (field >= AF_EDITOR_INSPECTOR_COLOR_R && field <= AF_EDITOR_INSPECTOR_COLOR_A)) {
        const AfEditorInspectorField first = field <= AF_EDITOR_INSPECTOR_COLOR_A
                                                 ? AF_EDITOR_INSPECTOR_COLOR_R
                                                 : AF_EDITOR_INSPECTOR_POSITION_X;
        const uint32_t offset = (uint32_t)(field - first);
        const uint32_t column = offset % 2U;
        const float field_width = (available - 6.0F) * 0.5F;
        if (out_x != NULL) {
            *out_x = panel_x + (float)column * (field_width + 6.0F);
        }
        if (out_width != NULL) {
            *out_width = field_width;
        }
        return true;
    }
    if (field == AF_EDITOR_INSPECTOR_COLOR_HEX || field == AF_EDITOR_INSPECTOR_OUTLINE_WIDTH) {
        if (out_x != NULL) {
            *out_x = panel_x;
        }
        if (out_width != NULL) {
            *out_width = available;
        }
        return true;
    }
    return false;
}

static bool af_editor_inspector_ratio_hit(const AfEditorState *editor, int width, float x,
                                          float y) {
    const float available = editor->right_width - 36.0F;
    const float ratio_width = 76.0F;
    const float ratio_x = (float)width - editor->right_width + 18.0F + available - ratio_width;
    return x >= ratio_x && x < ratio_x + ratio_width && y >= AF_EDITOR_INSPECTOR_LABEL_Y - 4.0F &&
           y < AF_EDITOR_INSPECTOR_LABEL_Y + 13.0F;
}

static int af_editor_inspector_step_at(const AfEditorState *editor, int width,
                                       AfEditorInspectorField field, float x) {
    float field_x = 0.0F;
    float field_width = 0.0F;
    if (!af_editor_inspector_field_rect(editor, width, field, &field_x, &field_width)) {
        return 0;
    }
    if (x >= field_x + field_width - 40.0F && x < field_x + field_width - 21.0F) {
        return -1;
    }
    return x >= field_x + field_width - 19.0F && x < field_x + field_width ? 1 : 0;
}

static bool af_editor_inspector_numeric_value(const AfEditorState *editor,
                                              AfEditorInspectorField field, float *out_value) {
    if (field >= AF_EDITOR_INSPECTOR_PARTICLE_FIRST) {
        AfEditorParticleParameter parameter;
        if (out_value == NULL || !af_editor_particle_parameter(editor, field, &parameter))
            return false;
        *out_value = (float)af_editor_particle_value(&parameter);
        return true;
    }
    const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    if (node == NULL || out_value == NULL) {
        return false;
    }
    if (field >= AF_EDITOR_INSPECTOR_COLOR_HEX && field <= AF_EDITOR_INSPECTOR_COLOR_A) {
        const AfColor color = af_editor_inspector_color(editor, node);
        if (field == AF_EDITOR_INSPECTOR_COLOR_HEX) {
            const uint32_t red = (uint32_t)roundf(af_editor_clamp(color.r, 0.0F, 1.0F) * 255.0F);
            const uint32_t green = (uint32_t)roundf(af_editor_clamp(color.g, 0.0F, 1.0F) * 255.0F);
            const uint32_t blue = (uint32_t)roundf(af_editor_clamp(color.b, 0.0F, 1.0F) * 255.0F);
            *out_value = (float)((red << 16U) | (green << 8U) | blue);
        } else {
            const float components[] = {color.r, color.g, color.b, color.a};
            *out_value = roundf(components[field - AF_EDITOR_INSPECTOR_COLOR_R] * 255.0F);
        }
    } else if (field == AF_EDITOR_INSPECTOR_POSITION_X) {
        *out_value = node->local_transform.position.x;
    } else if (field == AF_EDITOR_INSPECTOR_POSITION_Y) {
        *out_value = node->local_transform.position.y;
    } else if (field == AF_EDITOR_INSPECTOR_WIDTH) {
        *out_value = node->resolved_bounds.x;
    } else if (field == AF_EDITOR_INSPECTOR_HEIGHT) {
        *out_value = node->resolved_bounds.y;
    } else if (field == AF_EDITOR_INSPECTOR_SCALE_X) {
        *out_value = node->local_transform.scale.x;
    } else if (field == AF_EDITOR_INSPECTOR_SCALE_Y) {
        *out_value = node->local_transform.scale.y;
    } else if (field == AF_EDITOR_INSPECTOR_ROTATION) {
        *out_value = node->local_transform.rotation * 57.2957795131F;
    } else if (field == AF_EDITOR_INSPECTOR_OPACITY) {
        *out_value = node->opacity * 100.0F;
    } else if (field == AF_EDITOR_INSPECTOR_RADIUS) {
        *out_value = node->corner_radius;
    } else if (field == AF_EDITOR_INSPECTOR_LAYER) {
        *out_value = (float)node->layer_order;
    } else if (field == AF_EDITOR_INSPECTOR_OUTLINE_WIDTH) {
        const AfStyle *style =
            af_style_find_node_const(&editor->project.style_data, editor->selected_id);
        if (style == NULL) {
            return false;
        }
        *out_value = style->stroke.width;
    } else {
        return false;
    }
    return true;
}

static float af_editor_inspector_step_size(AfEditorInspectorField field) {
    if (field == AF_EDITOR_INSPECTOR_SCALE_X || field == AF_EDITOR_INSPECTOR_SCALE_Y) {
        return 0.05F;
    }
    if (field == AF_EDITOR_INSPECTOR_OPACITY) {
        return 5.0F;
    }
    return 1.0F;
}

static float af_editor_inspector_scrub_sensitivity(AfEditorInspectorField field) {
    if (field >= AF_EDITOR_INSPECTOR_COLOR_HEX && field <= AF_EDITOR_INSPECTOR_COLOR_A) {
        return 1.0F;
    }
    if (field == AF_EDITOR_INSPECTOR_SCALE_X || field == AF_EDITOR_INSPECTOR_SCALE_Y) {
        return 0.005F;
    }
    if (field == AF_EDITOR_INSPECTOR_OPACITY) {
        return 0.25F;
    }
    if (field == AF_EDITOR_INSPECTOR_LAYER) {
        return 0.2F;
    }
    return 0.25F;
}

static void af_editor_begin_inspector_edit(AfEditorState *editor, AfEditorInspectorField field) {
    const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    const AfColor color = af_editor_inspector_color(editor, node);
    if (field == AF_EDITOR_INSPECTOR_NONE || node == NULL) {
        return;
    }
    if (editor->rename_active) {
        af_editor_finish_rename(editor, true);
    }
    editor->inspector_field = field;
    editor->inspector_replace = true;
    if (field >= AF_EDITOR_INSPECTOR_PARTICLE_FIRST) {
        AfEditorParticleParameter parameter;
        if (!af_editor_particle_parameter(editor, field, &parameter))
            return;
        const uint32_t offset = (uint32_t)(field - AF_EDITOR_INSPECTOR_PARTICLE_FIRST);
        const uint32_t row = offset % 9U == 8U ? (offset / 9U == 1U ? 6U : 7U) : offset % 9U;
        const float y = AF_EDITOR_PARTICLE_ROW_Y + (float)row * AF_EDITOR_PARTICLE_ROW_STEP;
        AfPlatformWindowMetrics metrics;
        if (af_platform_window_metrics(&editor->window, &metrics) == AF_RESULT_SUCCESS) {
            const float bottom = (float)metrics.pixel_height - editor->status_height;
            if (y - editor->inspector_scroll + AF_EDITOR_INSPECTOR_FIELD_HEIGHT > bottom)
                editor->inspector_scroll = y + AF_EDITOR_INSPECTOR_FIELD_HEIGHT - bottom;
            else if (y - editor->inspector_scroll < 40)
                editor->inspector_scroll = fmaxf(y - 40, 0);
        }
        (void)snprintf(editor->inspector_buffer, sizeof(editor->inspector_buffer),
                       parameter.integer ? "%.0f" : "%.6g", af_editor_particle_value(&parameter));
    } else if (field == AF_EDITOR_INSPECTOR_COLOR_HEX) {
        af_editor_inspector_format_hex_color(color, editor->inspector_buffer,
                                             sizeof(editor->inspector_buffer));
    } else if (field >= AF_EDITOR_INSPECTOR_COLOR_R && field <= AF_EDITOR_INSPECTOR_COLOR_A) {
        const float components[4] = {color.r, color.g, color.b, color.a};
        (void)snprintf(editor->inspector_buffer, sizeof(editor->inspector_buffer), "%u",
                       (uint32_t)roundf(components[field - AF_EDITOR_INSPECTOR_COLOR_R] * 255.0F));
    } else if (field == AF_EDITOR_INSPECTOR_POSITION_X || field == AF_EDITOR_INSPECTOR_POSITION_Y) {
        const float value = field == AF_EDITOR_INSPECTOR_POSITION_X
                                ? node->local_transform.position.x
                                : node->local_transform.position.y;
        (void)snprintf(editor->inspector_buffer, sizeof(editor->inspector_buffer), "%.6g",
                       (double)value);
    } else if (field == AF_EDITOR_INSPECTOR_WIDTH || field == AF_EDITOR_INSPECTOR_HEIGHT) {
        const float value =
            field == AF_EDITOR_INSPECTOR_WIDTH ? node->resolved_bounds.x : node->resolved_bounds.y;
        (void)snprintf(editor->inspector_buffer, sizeof(editor->inspector_buffer), "%.6g",
                       (double)value);
    } else if (field == AF_EDITOR_INSPECTOR_SCALE_X || field == AF_EDITOR_INSPECTOR_SCALE_Y) {
        const float value = field == AF_EDITOR_INSPECTOR_SCALE_X ? node->local_transform.scale.x
                                                                 : node->local_transform.scale.y;
        (void)snprintf(editor->inspector_buffer, sizeof(editor->inspector_buffer), "%.6g",
                       (double)value);
    } else if (field == AF_EDITOR_INSPECTOR_ROTATION) {
        (void)snprintf(editor->inspector_buffer, sizeof(editor->inspector_buffer), "%.6g",
                       (double)(node->local_transform.rotation * 57.2957795131F));
    } else if (field == AF_EDITOR_INSPECTOR_OPACITY) {
        (void)snprintf(editor->inspector_buffer, sizeof(editor->inspector_buffer), "%.6g",
                       (double)(node->opacity * 100.0F));
    } else if (field == AF_EDITOR_INSPECTOR_RADIUS) {
        (void)snprintf(editor->inspector_buffer, sizeof(editor->inspector_buffer), "%.6g",
                       (double)node->corner_radius);
    } else if (field == AF_EDITOR_INSPECTOR_OUTLINE_WIDTH) {
        const AfStyle *style =
            af_style_find_node_const(&editor->project.style_data, editor->selected_id);
        (void)snprintf(editor->inspector_buffer, sizeof(editor->inspector_buffer), "%.6g",
                       style == NULL ? 0.0 : (double)style->stroke.width);
    } else {
        (void)snprintf(editor->inspector_buffer, sizeof(editor->inspector_buffer), "%d",
                       node->layer_order);
    }
    (void)af_platform_text_input_set(&editor->window, true);
    af_editor_status(editor, "Type a value · Enter apply · Esc cancel · Tab next");
}

static bool af_editor_inspector_animation_property(AfEditorInspectorField field,
                                                   AfPropertyType *out_property) {
    if (out_property == NULL) {
        return false;
    }
    if (field == AF_EDITOR_INSPECTOR_POSITION_X || field == AF_EDITOR_INSPECTOR_POSITION_Y) {
        *out_property = AF_PROPERTY_POSITION;
    } else if (field == AF_EDITOR_INSPECTOR_SCALE_X || field == AF_EDITOR_INSPECTOR_SCALE_Y) {
        *out_property = AF_PROPERTY_SCALE;
    } else if (field == AF_EDITOR_INSPECTOR_ROTATION) {
        *out_property = AF_PROPERTY_ROTATION;
    } else if (field == AF_EDITOR_INSPECTOR_OPACITY) {
        *out_property = AF_PROPERTY_OPACITY;
    } else if (field == AF_EDITOR_INSPECTOR_RADIUS) {
        *out_property = AF_PROPERTY_CORNER_RADIUS;
    } else if (field >= AF_EDITOR_INSPECTOR_COLOR_HEX && field <= AF_EDITOR_INSPECTOR_COLOR_A) {
        *out_property = AF_PROPERTY_FILL_COLOR;
    } else {
        return false;
    }
    return true;
}

static AfEditorMergeKey af_editor_inspector_merge_key(AfEditorInspectorField field) {
    if (field == AF_EDITOR_INSPECTOR_OPACITY) {
        return AF_EDITOR_MERGE_OPACITY;
    }
    if (field == AF_EDITOR_INSPECTOR_RADIUS) {
        return AF_EDITOR_MERGE_RADIUS;
    }
    if (field == AF_EDITOR_INSPECTOR_LAYER) {
        return AF_EDITOR_MERGE_LAYER;
    }
    if (field >= AF_EDITOR_INSPECTOR_COLOR_HEX && field <= AF_EDITOR_INSPECTOR_COLOR_A) {
        return AF_EDITOR_MERGE_FILL;
    }
    if (field == AF_EDITOR_INSPECTOR_OUTLINE_WIDTH) {
        return AF_EDITOR_MERGE_STYLE;
    }
    return AF_EDITOR_MERGE_TRANSFORM;
}

static bool af_editor_apply_inspector_color(AfEditorState *editor, AfColor color,
                                            bool allow_merge) {
    const AfPropertyType color_property = af_editor_color_property(editor);
    if (editor->selected_id == 0U) {
        return false;
    }
    af_editor_begin_selection_modify(editor, false);
    editor->selected_property = color_property;
    af_editor_begin_transform_key_drag(editor, color_property);
    af_editor_apply_fill(editor, color);
    (void)af_scene_evaluate_world(&editor->project.scene);
    af_editor_update_preview(editor);
    if (!af_editor_transform_key_drag_covers_selection(editor)) {
        (void)af_editor_commit_selection_modify(
            editor, editor->color_target_outline ? AF_EDITOR_MERGE_STYLE : AF_EDITOR_MERGE_FILL,
            allow_merge);
    }
    (void)af_editor_commit_transform_key_drag(editor, allow_merge);
    af_editor_status(editor,
                     editor->color_target_outline ? "Outline color entered" : "Fill color entered");
    return true;
}

static bool af_editor_update_polygon(AfEditorState *editor, AfVec2 size, uint32_t sides,
                                     float inset, bool allow_merge) {
    AfEditorCommand command = {0};
    AfResult result = af_editor_command_polygon(&editor->project, editor->selected_id, size, sides,
                                                inset, &command);
    if (result == AF_RESULT_SUCCESS)
        result = af_editor_history_execute(&editor->history, &editor->project, &command, false,
                                           allow_merge);
    if (result != AF_RESULT_SUCCESS) {
        af_editor_status(editor, "Shape settings could not be changed");
        return false;
    }
    af_editor_mark_dirty(editor);
    af_editor_update_preview(editor);
    af_editor_status(editor, "Shape updated · Convert to path unlocks individual vector points");
    return true;
}

static bool af_editor_apply_inspector_number(AfEditorState *editor, AfEditorInspectorField field,
                                             float value, int32_t integer, bool allow_merge) {
    if (field >= AF_EDITOR_INSPECTOR_PARTICLE_FIRST)
        return af_editor_apply_particle_number(editor, field, (double)value, allow_merge);
    AfPropertyType property = AF_PROPERTY_POSITION;
    const bool animatable =
        af_editor_inspector_animation_property(field, &property) &&
        !(field >= AF_EDITOR_INSPECTOR_COLOR_HEX && field <= AF_EDITOR_INSPECTOR_COLOR_A);
    if (editor->selected_id == 0U) {
        return false;
    }
    if (field >= AF_EDITOR_INSPECTOR_COLOR_HEX && field <= AF_EDITOR_INSPECTOR_COLOR_A) {
        const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->selected_id);
        AfColor color = af_editor_inspector_color(editor, node);
        if (field == AF_EDITOR_INSPECTOR_COLOR_HEX) {
            const uint32_t rgb = (uint32_t)roundf(af_editor_clamp(value, 0.0F, 16777215.0F));
            color.r = (float)((rgb >> 16U) & 255U) / 255.0F;
            color.g = (float)((rgb >> 8U) & 255U) / 255.0F;
            color.b = (float)(rgb & 255U) / 255.0F;
        } else {
            float *components[] = {&color.r, &color.g, &color.b, &color.a};
            *components[field - AF_EDITOR_INSPECTOR_COLOR_R] =
                roundf(af_editor_clamp(value, 0.0F, 255.0F)) / 255.0F;
        }
        return af_editor_apply_inspector_color(editor, color, allow_merge);
    }
    if (field == AF_EDITOR_INSPECTOR_WIDTH || field == AF_EDITOR_INSPECTOR_HEIGHT) {
        const AfSceneNode *shape = af_scene_find_const(&editor->project.scene, editor->selected_id);
        if (shape != NULL && shape->polygon_sides != 0U) {
            if (editor->selected_count != 1U)
                return false;
            AfVec2 size = shape->bounds;
            const float next = af_editor_clamp(value, 0.1F, 1000000.0F);
            if (field == AF_EDITOR_INSPECTOR_WIDTH) {
                if (editor->inspector_ratio_locked)
                    size.y *= next / size.x;
                size.x = next;
            } else {
                if (editor->inspector_ratio_locked)
                    size.x *= next / size.y;
                size.y = next;
            }
            return af_editor_update_polygon(editor, size, shape->polygon_sides,
                                            shape->polygon_inset, allow_merge);
        }
    }
    af_editor_begin_selection_modify(editor, false);
    if (animatable) {
        editor->selected_property = property;
        af_editor_begin_transform_key_drag(editor, property);
    }
    for (uint32_t index = 0U; index < editor->selection_drag_count; ++index) {
        const uint64_t node_id = editor->selection_drag_before[index].node.id;
        AfSceneNode *node = af_scene_find(&editor->project.scene, node_id);
        const int32_t key_index =
            animatable ? af_editor_transform_drag_key_index(editor, node_id) : -1;
        AfProjectKeyframe *key =
            key_index < 0
                ? NULL
                : af_project_find_keyframe(&editor->project,
                                           editor->transform_drag_key_ids[(uint32_t)key_index]);
        if (node == NULL) {
            continue;
        }
        if (field == AF_EDITOR_INSPECTOR_POSITION_X || field == AF_EDITOR_INSPECTOR_POSITION_Y) {
            const uint32_t component = field == AF_EDITOR_INSPECTOR_POSITION_X ? 0U : 1U;
            if (key != NULL) {
                key->keyframe.value[component] = value;
            } else if (component == 0U) {
                node->local_transform.position.x = value;
            } else {
                node->local_transform.position.y = value;
            }
        } else if (field == AF_EDITOR_INSPECTOR_SCALE_X || field == AF_EDITOR_INSPECTOR_SCALE_Y) {
            const uint32_t component = field == AF_EDITOR_INSPECTOR_SCALE_X ? 0U : 1U;
            const uint32_t other_component = component == 0U ? 1U : 0U;
            const float scale = fmaxf(0.05F, value);
            if (key != NULL) {
                key->keyframe.value[component] = scale;
                if (editor->inspector_ratio_locked) {
                    const float current =
                        editor->transform_drag_key_starts[(uint32_t)key_index][component];
                    const float other =
                        editor->transform_drag_key_starts[(uint32_t)key_index][other_component];
                    const float ratio = scale / fmaxf(fabsf(current), 0.0001F);
                    key->keyframe.value[other_component] = fmaxf(0.05F, other * ratio);
                }
            } else if (component == 0U) {
                if (editor->inspector_ratio_locked) {
                    const float ratio =
                        scale / fmaxf(fabsf(node->local_transform.scale.x), 0.0001F);
                    node->local_transform.scale.y =
                        fmaxf(0.05F, node->local_transform.scale.y * ratio);
                }
                node->local_transform.scale.x = scale;
            } else {
                if (editor->inspector_ratio_locked) {
                    const float ratio =
                        scale / fmaxf(fabsf(node->local_transform.scale.y), 0.0001F);
                    node->local_transform.scale.x =
                        fmaxf(0.05F, node->local_transform.scale.x * ratio);
                }
                node->local_transform.scale.y = scale;
            }
        } else if (field == AF_EDITOR_INSPECTOR_ROTATION) {
            const float radians = value * 0.0174532925199F;
            if (key != NULL) {
                key->keyframe.value[0] = radians;
            } else {
                node->local_transform.rotation = radians;
            }
        } else if (field == AF_EDITOR_INSPECTOR_OPACITY) {
            const float opacity = af_editor_clamp(value * 0.01F, 0.0F, 1.0F);
            if (key != NULL) {
                key->keyframe.value[0] = opacity;
            } else {
                node->opacity = opacity;
            }
        } else if (field == AF_EDITOR_INSPECTOR_RADIUS) {
            const float radius = af_editor_clamp(value, 0.0F, 1000000.0F);
            if (key != NULL) {
                key->keyframe.value[0] = radius;
            } else {
                node->corner_radius = radius;
            }
        } else if (field == AF_EDITOR_INSPECTOR_LAYER) {
            node->layer_order = af_editor_clamp_layer(integer);
        } else if (field == AF_EDITOR_INSPECTOR_OUTLINE_WIDTH) {
            AfStyle *style = af_project_find_node_style(&editor->project, node_id);
            if (style != NULL) {
                style->stroke.width = af_editor_clamp(value, 0.0F, 1000.0F);
                style->stroke.end_width = style->stroke.width;
                (void)af_style_mark_stroke_geometry_changed(style);
            }
        } else if (field == AF_EDITOR_INSPECTOR_WIDTH || field == AF_EDITOR_INSPECTOR_HEIGHT) {
            const bool horizontal = field == AF_EDITOR_INSPECTOR_WIDTH;
            const float size = af_editor_clamp(value, 0.1F, 1000000.0F);
            const AfSceneNode *preview_node = af_scene_find_const(&editor->preview_scene, node_id);
            const float current_width =
                preview_node == NULL ? node->bounds.x : preview_node->resolved_bounds.x;
            const float current_height =
                preview_node == NULL ? node->bounds.y : preview_node->resolved_bounds.y;
            float other_size = horizontal ? current_height : current_width;
            const AfLayoutNode *existing =
                af_layout_find_const(&editor->project.layout_data, node_id);
            if (editor->inspector_ratio_locked) {
                const float current = horizontal ? current_width : current_height;
                other_size = af_editor_clamp(other_size * size / fmaxf(fabsf(current), 0.0001F),
                                             0.1F, 1000000.0F);
            }
            if (horizontal) {
                node->bounds.x = size;
                if (editor->inspector_ratio_locked) {
                    node->bounds.y = other_size;
                }
            } else {
                node->bounds.y = size;
                if (editor->inspector_ratio_locked) {
                    node->bounds.x = other_size;
                }
            }
            if (existing != NULL) {
                AfLayoutNode layout = *existing;
                AfLayoutValue *dimension = horizontal ? &layout.width : &layout.height;
                AfLayoutValue *other_dimension = horizontal ? &layout.height : &layout.width;
                bool layout_changed = false;
                if (dimension->mode != AF_LAYOUT_VALUE_INTRINSIC) {
                    *dimension = (AfLayoutValue){.mode = AF_LAYOUT_VALUE_FIXED, .value = size};
                    layout_changed = true;
                }
                if (editor->inspector_ratio_locked &&
                    other_dimension->mode != AF_LAYOUT_VALUE_INTRINSIC) {
                    *other_dimension =
                        (AfLayoutValue){.mode = AF_LAYOUT_VALUE_FIXED, .value = other_size};
                    layout_changed = true;
                }
                if (layout_changed) {
                    (void)af_project_set_layout(&editor->project, &layout);
                }
            }
        }
    }
    (void)af_scene_evaluate_world(&editor->project.scene);
    af_editor_update_preview(editor);
    if (!animatable || !af_editor_transform_key_drag_covers_selection(editor)) {
        (void)af_editor_commit_selection_modify(editor, af_editor_inspector_merge_key(field),
                                                allow_merge);
    }
    if (animatable) {
        (void)af_editor_commit_transform_key_drag(editor, allow_merge);
    }
    af_editor_status(editor, "Inspector value entered");
    return true;
}

static bool af_editor_step_inspector_value(AfEditorState *editor, AfEditorInspectorField field,
                                           int direction) {
    float value = 0.0F;
    float multiplier = 1.0F;
    const SDL_Keymod modifiers = SDL_GetModState();
    if (direction == 0 || !af_editor_inspector_numeric_value(editor, field, &value)) {
        return false;
    }
    if ((modifiers & SDL_KMOD_SHIFT) != 0U && field > AF_EDITOR_INSPECTOR_COLOR_A &&
        field != AF_EDITOR_INSPECTOR_LAYER) {
        multiplier = 0.1F;
    } else if ((modifiers & SDL_KMOD_CTRL) != 0U) {
        multiplier = 10.0F;
    }
    if (field >= AF_EDITOR_INSPECTOR_PARTICLE_FIRST) {
        AfEditorParticleParameter parameter;
        if (!af_editor_particle_parameter(editor, field, &parameter))
            return false;
        double number = af_editor_particle_value(&parameter) +
                        (double)direction * parameter.step * (double)multiplier;
        if (parameter.integer)
            number = round(number);
        return af_editor_apply_particle_number(editor, field, number, false);
    }
    value += (float)direction * af_editor_inspector_step_size(field) * multiplier;
    if (field == AF_EDITOR_INSPECTOR_LAYER) {
        const int32_t integer = (int32_t)roundf(af_editor_clamp(value, -10000.0F, 10000.0F));
        return af_editor_apply_inspector_number(editor, field, (float)integer, integer, false);
    }
    return af_editor_apply_inspector_number(editor, field, value, 0, false);
}

static void af_editor_begin_inspector_scrub(AfEditorState *editor, AfEditorInspectorField field,
                                            float x, float y) {
    float value = 0.0F;
    if (!af_editor_inspector_numeric_value(editor, field, &value)) {
        return;
    }
    if (field >= AF_EDITOR_INSPECTOR_PARTICLE_FIRST) {
        AfEditorParticleParameter parameter;
        if (!af_editor_particle_parameter(editor, field, &parameter))
            return;
        editor->inspector_particle_scrub_start_value = af_editor_particle_value(&parameter);
    }
    editor->drag_mode = AF_EDITOR_DRAG_INSPECTOR_SCRUB;
    editor->drag_start_mouse = (AfVec2){x, y};
    editor->inspector_scrub_field = field;
    editor->inspector_scrub_start_value = value;
    editor->inspector_scrub_changed = false;
    af_editor_status(editor, "Drag horizontally to adjust · click to type");
}

static void af_editor_update_inspector_scrub(AfEditorState *editor, float x) {
    const float distance = x - editor->drag_start_mouse.x;
    float multiplier = 1.0F;
    float value = 0.0F;
    const bool allow_merge = editor->inspector_scrub_changed;
    const SDL_Keymod modifiers = SDL_GetModState();
    if (!editor->inspector_scrub_changed && fabsf(distance) < 3.0F) {
        return;
    }
    if ((modifiers & SDL_KMOD_SHIFT) != 0U &&
        editor->inspector_scrub_field != AF_EDITOR_INSPECTOR_LAYER) {
        multiplier = 0.1F;
    } else if ((modifiers & SDL_KMOD_CTRL) != 0U) {
        multiplier = 10.0F;
    }
    if (editor->inspector_scrub_field >= AF_EDITOR_INSPECTOR_PARTICLE_FIRST) {
        AfEditorParticleParameter parameter;
        if (!af_editor_particle_parameter(editor, editor->inspector_scrub_field, &parameter))
            return;
        double number = editor->inspector_particle_scrub_start_value +
                        (double)distance * parameter.step * .125 * (double)multiplier;
        if (parameter.integer)
            number = round(number);
        if (af_editor_apply_particle_number(editor, editor->inspector_scrub_field, number,
                                            allow_merge))
            editor->inspector_scrub_changed = true;
        return;
    }
    value = editor->inspector_scrub_start_value +
            distance * af_editor_inspector_scrub_sensitivity(editor->inspector_scrub_field) *
                multiplier;
    if (editor->inspector_scrub_field == AF_EDITOR_INSPECTOR_LAYER) {
        const int32_t integer = (int32_t)roundf(af_editor_clamp(value, -10000.0F, 10000.0F));
        (void)af_editor_apply_inspector_number(editor, editor->inspector_scrub_field,
                                               (float)integer, integer, allow_merge);
    } else {
        (void)af_editor_apply_inspector_number(editor, editor->inspector_scrub_field, value, 0,
                                               allow_merge);
    }
    editor->inspector_scrub_changed = true;
    af_editor_status(editor, "Value scrubbed · Shift fine · Ctrl coarse");
}

static bool af_editor_finish_inspector_edit(AfEditorState *editor, bool commit) {
    const AfEditorInspectorField field = editor->inspector_field;
    bool applied = false;
    if (field == AF_EDITOR_INSPECTOR_NONE) {
        return true;
    }
    if (!commit) {
        editor->inspector_field = AF_EDITOR_INSPECTOR_NONE;
        editor->inspector_buffer[0] = '\0';
        editor->inspector_replace = false;
        (void)af_platform_text_input_set(&editor->window, false);
        af_editor_status(editor, "Inspector edit cancelled");
        return true;
    }
    if (field >= AF_EDITOR_INSPECTOR_PARTICLE_FIRST) {
        char *end = NULL;
        const double number = strtod(editor->inspector_buffer, &end);
        if (end != editor->inspector_buffer && *end == '\0' && isfinite(number))
            applied = af_editor_apply_particle_number(editor, field, number, false);
    } else if (field == AF_EDITOR_INSPECTOR_COLOR_HEX) {
        AfColor color = {0};
        if (af_editor_inspector_parse_hex_color(editor->inspector_buffer, &color)) {
            applied = af_editor_apply_inspector_color(editor, color, false);
        }
    } else if (field >= AF_EDITOR_INSPECTOR_COLOR_R && field <= AF_EDITOR_INSPECTOR_COLOR_A) {
        float component = 0.0F;
        if (af_editor_inspector_parse_number(editor->inspector_buffer, &component)) {
            const AfSceneNode *node =
                af_scene_find_const(&editor->preview_scene, editor->selected_id);
            AfColor color = af_editor_inspector_color(editor, node);
            float *components[4] = {&color.r, &color.g, &color.b, &color.a};
            *components[field - AF_EDITOR_INSPECTOR_COLOR_R] =
                af_editor_clamp(component / 255.0F, 0.0F, 1.0F);
            applied = af_editor_apply_inspector_color(editor, color, false);
        }
    } else if (field == AF_EDITOR_INSPECTOR_LAYER) {
        int32_t integer = 0;
        if (af_editor_inspector_parse_integer(editor->inspector_buffer, &integer)) {
            applied =
                af_editor_apply_inspector_number(editor, field, (float)integer, integer, false);
        }
    } else {
        float value = 0.0F;
        if (af_editor_inspector_parse_number(editor->inspector_buffer, &value)) {
            applied = af_editor_apply_inspector_number(editor, field, value, 0, false);
        }
    }
    if (!applied) {
        af_editor_status(editor, "Invalid value · correct it or press Esc");
        return false;
    }
    editor->inspector_field = AF_EDITOR_INSPECTOR_NONE;
    editor->inspector_buffer[0] = '\0';
    editor->inspector_replace = false;
    (void)af_platform_text_input_set(&editor->window, false);
    return true;
}

static void af_editor_append_inspector_text(AfEditorState *editor, const char *text) {
    const size_t incoming = text == NULL ? 0U : strlen(text);
    size_t current = strlen(editor->inspector_buffer);
    if (editor->inspector_field == AF_EDITOR_INSPECTOR_NONE || incoming == 0U) {
        return;
    }
    if (editor->inspector_replace) {
        editor->inspector_buffer[0] = '\0';
        current = 0U;
        editor->inspector_replace = false;
    }
    if (current + incoming >= sizeof(editor->inspector_buffer)) {
        return;
    }
    for (size_t index = 0U; index < incoming; ++index) {
        const unsigned char character = (unsigned char)text[index];
        const bool color = editor->inspector_field == AF_EDITOR_INSPECTOR_COLOR_HEX;
        const bool integer = editor->inspector_field == AF_EDITOR_INSPECTOR_LAYER;
        const bool accepted = color ? (isxdigit(character) != 0 || character == '#')
                                    : (isdigit(character) != 0 || character == '-' ||
                                       character == '+' || (!integer && character == '.') ||
                                       (!integer && (character == 'e' || character == 'E')));
        if (!accepted) {
            return;
        }
    }
    memcpy(editor->inspector_buffer + current, text, incoming + 1U);
}

static AfEditorInspectorField af_editor_next_inspector_field(const AfEditorState *editor,
                                                             AfEditorInspectorField field,
                                                             bool backwards) {
    if (field >= AF_EDITOR_INSPECTOR_PARTICLE_FIRST) {
        const int page = ((int)field - AF_EDITOR_INSPECTOR_PARTICLE_FIRST) / 9;
        const int base = AF_EDITOR_INSPECTOR_PARTICLE_FIRST + page * 9;
        const int order[9] = {0, 1, 2, 3, 4, 5, 6, page == 1 ? 8 : 7, page == 1 ? 7 : 8};
        int position = 0;
        while (position < 9 && order[position] != (int)field - base)
            ++position;
        for (int i = 0; i < 9; ++i) {
            position = (position + (backwards ? 8 : 1)) % 9;
            const int row = order[position];
            AfEditorParticleParameter parameter;
            const AfEditorInspectorField next = (AfEditorInspectorField)(base + row);
            if (af_editor_particle_parameter(editor, next, &parameter))
                return next;
        }
        return AF_EDITOR_INSPECTOR_NONE;
    }
    const int first = AF_EDITOR_INSPECTOR_COLOR_HEX;
    const int last = AF_EDITOR_INSPECTOR_OUTLINE_WIDTH;
    int next = (int)field + (backwards ? -1 : 1);
    if (next < first) {
        next = last;
    } else if (next > last) {
        next = first;
    }
    return (AfEditorInspectorField)next;
}

static void af_editor_update_color_picker(AfEditorState *editor, int width, float x, float y) {
    const AfSceneNode *node = af_scene_find_const(&editor->preview_scene, editor->selected_id);
    const float panel_x = (float)width - editor->right_width + 18.0F;
    float hue = editor->color_hue;
    float saturation = 0.0F;
    float value = 0.0F;
    AfColor color;
    if (node == NULL) {
        return;
    }
    color = af_editor_inspector_color(editor, node);
    af_editor_color_hsv(color, &hue, &saturation, &value);
    if (saturation <= 0.0001F) {
        hue = editor->color_hue;
    }
    if (editor->drag_mode == AF_EDITOR_DRAG_COLOR_SV) {
        saturation = af_editor_clamp((x - panel_x) / AF_EDITOR_COLOR_SV_WIDTH, 0.0F, 1.0F);
        value = 1.0F -
                af_editor_clamp((y - AF_EDITOR_COLOR_SV_Y) / AF_EDITOR_COLOR_SV_HEIGHT, 0.0F, 1.0F);
    } else if (editor->drag_mode == AF_EDITOR_DRAG_COLOR_HUE) {
        hue = af_editor_clamp((y - AF_EDITOR_COLOR_SV_Y) / AF_EDITOR_COLOR_SV_HEIGHT, 0.0F, 1.0F);
        editor->color_hue = hue;
    } else if (editor->drag_mode == AF_EDITOR_DRAG_COLOR_ALPHA) {
        color.a = af_editor_clamp(
            (x - panel_x) / (AF_EDITOR_COLOR_HUE_X + AF_EDITOR_COLOR_HUE_WIDTH), 0.0F, 1.0F);
    }
    {
        AfColor result = af_editor_hsv_color(hue, saturation, value, color.a);
        af_editor_apply_fill(editor, result);
    }
}

static void af_editor_apply_style_action(AfSceneNode *node, AfStyle *style, uint32_t action) {
    if (action == 0U) {
        style->stroke.enabled = !style->stroke.enabled;
        (void)af_style_mark_stroke_geometry_changed(style);
    } else if (action == 1U || action == 2U) {
        const float amount = action == 1U ? -1.0F : 1.0F;
        style->stroke.width = af_editor_clamp(style->stroke.width + amount, 0.0F, 1000.0F);
        style->stroke.end_width = style->stroke.width;
        (void)af_style_mark_stroke_geometry_changed(style);
    } else if (action == 3U) {
        const AfPaintType next = (AfPaintType)(((uint32_t)style->fill.type + 1U) % 4U);
        if (next == AF_PAINT_SOLID) {
            style->fill = af_paint_solid(node->fill);
        } else {
            style->fill = (AfPaint){.type = next,
                                    .solid = node->fill,
                                    .start = {0.0F, 0.0F},
                                    .end = {node->bounds.x, node->bounds.y},
                                    .end_radius = fmaxf(node->bounds.x, node->bounds.y),
                                    .transform = {.m00 = 1.0F, .m11 = 1.0F},
                                    .stops = {{.offset = 0.0F, .color = node->fill},
                                              {.offset = 1.0F,
                                               .color = {1.0F - node->fill.r, 1.0F - node->fill.g,
                                                         1.0F - node->fill.b, node->fill.a}}},
                                    .stop_count = 2U};
        }
        (void)af_style_mark_paint_changed(style);
    } else if (action == 4U) {
        style->stroke.cap = (AfStrokeCap)(((uint32_t)style->stroke.cap + 1U) % 3U);
        (void)af_style_mark_stroke_geometry_changed(style);
    } else if (action == 5U) {
        style->stroke.join = (AfStrokeJoin)(((uint32_t)style->stroke.join + 1U) % 3U);
        (void)af_style_mark_stroke_geometry_changed(style);
    }
}

static void af_editor_modify_style(AfEditorState *editor, uint32_t action) {
    if (editor->selected_id == 0U) {
        return;
    }
    af_editor_begin_selection_modify(editor, false);
    for (uint32_t index = 0U; index < editor->selection_drag_count; ++index) {
        const uint64_t id = editor->selection_drag_before[index].node.id;
        AfSceneNode *node = af_scene_find(&editor->project.scene, id);
        AfStyle *style = af_project_find_node_style(&editor->project, id);
        if (node != NULL && style != NULL) {
            af_editor_apply_style_action(node, style, action);
        }
    }
    if (af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_STYLE, false) ==
        AF_RESULT_SUCCESS) {
        const AfStyle *style =
            af_style_find_node_const(&editor->project.style_data, editor->selected_id);
        char status[96];
        editor->color_target_outline = action != 3U;
        if (style != NULL && action == 0U) {
            (void)snprintf(status, sizeof(status), "Outline %s at %.1f px",
                           style->stroke.enabled ? "enabled" : "disabled",
                           (double)style->stroke.width);
        } else if (style != NULL && (action == 1U || action == 2U)) {
            (void)snprintf(status, sizeof(status), "Outline width %.1f px",
                           (double)style->stroke.width);
        } else {
            (void)snprintf(status, sizeof(status), "Vector style updated");
        }
        af_editor_status(editor, status);
    }
}

static AfLayoutNode af_editor_next_layout(const AfSceneNode *node, const AfLayoutNode *existing,
                                          uint32_t action) {
    AfLayoutNode layout = existing == NULL ? af_layout_node_default(node->id) : *existing;
    if (action == 0U) {
        static const AfVec2 anchors[3] = {{0.0F, 0.0F}, {0.5F, 0.5F}, {1.0F, 1.0F}};
        uint32_t next = 0U;
        if (layout.anchor_min.x == 0.0F) {
            next = 1U;
        } else if (layout.anchor_min.x == 0.5F) {
            next = 2U;
        }
        layout.anchor_min = anchors[next];
        layout.anchor_max = anchors[next];
        layout.pivot = anchors[next];
    } else if (action == 1U) {
        layout.width.mode = (AfLayoutValueMode)(((uint32_t)layout.width.mode + 1U) % 3U);
        layout.width.value = layout.width.mode == AF_LAYOUT_VALUE_PERCENT ? 0.5F : node->bounds.x;
    } else if (action == 2U) {
        layout.height.mode = (AfLayoutValueMode)(((uint32_t)layout.height.mode + 1U) % 3U);
        layout.height.value = layout.height.mode == AF_LAYOUT_VALUE_PERCENT ? 0.5F : node->bounds.y;
    } else if (action == 3U) {
        layout.direction = (AfLayoutDirection)(((uint32_t)layout.direction + 1U) % 3U);
        layout.spacing = layout.direction == AF_LAYOUT_DIRECTION_NONE ? 0.0F : 8.0F;
    } else if (action == 4U) {
        layout.child_alignment = (AfLayoutAlignment)(((uint32_t)layout.child_alignment + 1U) % 4U);
    } else if (action == 5U) {
        layout.aspect = (AfAspectConstraint)(((uint32_t)layout.aspect + 1U) % 4U);
        layout.aspect_ratio = node->bounds.y > 0.0F ? node->bounds.x / node->bounds.y : 1.0F;
    }
    return layout;
}

static void af_editor_modify_layout(AfEditorState *editor, uint32_t action) {
    if (editor->selected_id == 0U) {
        return;
    }
    af_editor_begin_selection_modify(editor, false);
    for (uint32_t index = 0U; index < editor->selection_drag_count; ++index) {
        const uint64_t id = editor->selection_drag_before[index].node.id;
        const AfSceneNode *node = af_scene_find_const(&editor->project.scene, id);
        const AfLayoutNode *existing = af_layout_find_const(&editor->project.layout_data, id);
        if (node != NULL) {
            const AfLayoutNode layout = af_editor_next_layout(node, existing, action);
            if (af_project_set_layout(&editor->project, &layout) != AF_RESULT_SUCCESS) {
                return;
            }
        }
    }
    (void)af_scene_evaluate_world(&editor->project.scene);
    (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_STYLE, false);
}

static void af_editor_nudge(AfEditorState *editor, float x, float y) {
    if (editor->selected_id == 0U) {
        return;
    }
    editor->selected_property = AF_PROPERTY_POSITION;
    af_editor_begin_selection_modify(editor, true);
    af_editor_begin_transform_key_drag(editor, AF_PROPERTY_POSITION);
    for (uint32_t index = 0U; index < editor->selection_drag_count; ++index) {
        const uint64_t node_id = editor->selection_drag_before[index].node.id;
        AfSceneNode *node = af_scene_find(&editor->project.scene, node_id);
        const int32_t key_drag_index = af_editor_transform_drag_key_index(editor, node_id);
        AfProjectKeyframe *position_key =
            key_drag_index < 0
                ? NULL
                : af_project_find_keyframe(
                      &editor->project, editor->transform_drag_key_ids[(uint32_t)key_drag_index]);
        AfVec2 position = {0};
        if (node == NULL) {
            continue;
        }
        position = key_drag_index < 0
                       ? node->local_transform.position
                       : (AfVec2){editor->transform_drag_key_starts[(uint32_t)key_drag_index][0],
                                  editor->transform_drag_key_starts[(uint32_t)key_drag_index][1]};
        position.x += x;
        position.y += y;
        if (position_key != NULL) {
            position_key->keyframe.value[0] = position.x;
            position_key->keyframe.value[1] = position.y;
        } else {
            node->local_transform.position = position;
        }
    }
    (void)af_scene_evaluate_world(&editor->project.scene);
    if (editor->transform_drag_key_count != 0U) {
        af_editor_update_preview(editor);
    }
    if (!af_editor_transform_key_drag_covers_selection(editor)) {
        (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_TRANSFORM, true);
    }
    (void)af_editor_commit_transform_key_drag(editor, true);
}

static void af_editor_reparent(AfEditorState *editor, uint64_t parent_id) {
    AfEditorCommand command = {0};
    AfResult result =
        af_editor_command_reparent(&editor->project, editor->selected_id, parent_id, &command);
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_editor_history_execute(&editor->history, &editor->project, &command, false, false);
    }
    if (result == AF_RESULT_SUCCESS) {
        af_editor_mark_dirty(editor);
        af_editor_status(editor,
                         parent_id == AF_INVALID_NODE_ID ? "Node unparented" : "Node parented");
    } else {
        af_editor_status(editor, "Cannot move object into this parent");
    }
}

static void af_editor_adjust_particle(AfEditorState *editor, AfParticleEmitter *emitter,
                                      uint32_t row, bool decrease) {
    const float sign = decrease ? -1.0F : 1.0F;
    AfVfxLodSettings *lod = &editor->project.particle_data.lod;
    if (editor->particle_page == 0U) {
        if (row == 0U) {
            emitter->enabled = !emitter->enabled;
        } else if (row == 1U) {
            emitter->spawn_rate = fmaxf(0.0F, emitter->spawn_rate + sign * 8.0F);
        } else if (row == 2U) {
            emitter->lifetime_min =
                af_editor_clamp(emitter->lifetime_min + sign * 0.1F, 0.1F, emitter->lifetime_max);
        } else if (row == 3U) {
            emitter->lifetime_max =
                fmaxf(emitter->lifetime_min, emitter->lifetime_max + sign * 0.1F);
        } else if (row == 4U) {
            const uint32_t step = 256U;
            const uint32_t total = af_particle_total_capacity(&editor->project.particle_data);
            if (decrease) {
                emitter->capacity = emitter->capacity > step ? emitter->capacity - step : 1U;
            } else if (total <= AF_MAX_PARTICLES - step &&
                       emitter->capacity <= AF_MAX_PARTICLES_PER_EMITTER - step) {
                emitter->capacity += step;
            }
            emitter->burst_count =
                emitter->burst_count > emitter->capacity ? emitter->capacity : emitter->burst_count;
            emitter->event_burst_count = emitter->event_burst_count > emitter->capacity
                                             ? emitter->capacity
                                             : emitter->event_burst_count;
        } else if (row == 5U) {
            int type = (int)emitter->type + (decrease ? -1 : 1);
            type = type < 0 ? (int)AF_PARTICLE_EMITTER_VECTOR_PATH
                            : (type > (int)AF_PARTICLE_EMITTER_VECTOR_PATH ? 0 : type);
            const AfPath *path =
                af_path_find_node_const(&editor->project.path_data, editor->selected_id);
            if (type == (int)AF_PARTICLE_EMITTER_VECTOR_PATH && path == NULL) {
                type = decrease ? (int)AF_PARTICLE_EMITTER_CIRCLE : (int)AF_PARTICLE_EMITTER_POINT;
            }
            emitter->type = (AfParticleEmitterType)type;
            emitter->path_id = emitter->type == AF_PARTICLE_EMITTER_VECTOR_PATH ? path->id : 0U;
        } else if (row == 6U) {
            const uint32_t step = 16U;
            emitter->burst_count =
                decrease ? (emitter->burst_count > step ? emitter->burst_count - step : 0U)
                         : (emitter->burst_count <=
                                    emitter->capacity -
                                        (emitter->capacity < step ? emitter->capacity : step)
                                ? emitter->burst_count +
                                      (emitter->capacity < step ? emitter->capacity : step)
                                : emitter->capacity);
        } else {
            emitter->seed = decrease ? emitter->seed - 1U : emitter->seed + 1U;
        }
    } else if (editor->particle_page == 1U) {
        float *value = row == 0U   ? &emitter->velocity_min.x
                       : row == 1U ? &emitter->velocity_max.x
                       : row == 2U ? &emitter->velocity_min.y
                       : row == 3U ? &emitter->velocity_max.y
                       : row == 4U ? &emitter->gravity.x
                       : row == 5U ? &emitter->gravity.y
                       : row == 6U ? &emitter->wind.x
                                   : &emitter->drag;
        *value += sign * (row == 7U ? 0.05F : 10.0F);
        if (row == 0U) {
            emitter->velocity_min.x = fminf(emitter->velocity_min.x, emitter->velocity_max.x);
        } else if (row == 1U) {
            emitter->velocity_max.x = fmaxf(emitter->velocity_max.x, emitter->velocity_min.x);
        } else if (row == 2U) {
            emitter->velocity_min.y = fminf(emitter->velocity_min.y, emitter->velocity_max.y);
        } else if (row == 3U) {
            emitter->velocity_max.y = fmaxf(emitter->velocity_max.y, emitter->velocity_min.y);
        } else if (row == 7U) {
            emitter->drag = fmaxf(0.0F, emitter->drag);
        }
    } else if (editor->particle_page == 2U) {
        float *value = row == 0U   ? &emitter->turbulence_strength
                       : row == 1U ? &emitter->turbulence_frequency
                       : row == 2U ? &emitter->radial_force
                       : row == 3U ? &emitter->vortex_force
                       : row == 4U ? &emitter->attractor_strength
                       : row == 5U ? &emitter->attractor_radius
                       : row == 6U ? &emitter->repulsor_strength
                                   : &emitter->vector_field_strength;
        *value += sign * (row == 1U ? 0.01F : (row == 5U ? 10.0F : 20.0F));
        if (row == 1U) {
            *value = af_editor_clamp(*value, 0.0F, 100.0F);
        } else if (row == 5U) {
            *value = fmaxf(0.0F, *value);
        } else {
            *value = af_editor_clamp(*value, -AF_MAX_PARTICLE_FORCE, AF_MAX_PARTICLE_FORCE);
        }
        if (emitter->attractor_strength != 0.0F && emitter->attractor_radius <= 0.0F) {
            emitter->attractor_radius = 100.0F;
        }
        if (emitter->repulsor_strength != 0.0F && emitter->repulsor_radius <= 0.0F) {
            emitter->repulsor_radius = 100.0F;
        }
        if (emitter->vector_field_strength != 0.0F && emitter->vector_field_direction.x == 0.0F &&
            emitter->vector_field_direction.y == 0.0F) {
            emitter->vector_field_direction = (AfVec2){1.0F, 0.0F};
        }
    } else if (editor->particle_page == 3U) {
        if (row == 0U) {
            emitter->collision_plane_enabled = !emitter->collision_plane_enabled;
            emitter->collision_plane_normal = (AfVec2){0.0F, -1.0F};
        } else if (row == 1U) {
            emitter->collision_plane_offset += sign * 10.0F;
        } else if (row == 2U) {
            emitter->collision_restitution =
                af_editor_clamp(emitter->collision_restitution + sign * 0.05F, 0.0F, 1.0F);
        } else if (row == 3U) {
            emitter->collision_friction =
                af_editor_clamp(emitter->collision_friction + sign * 0.05F, 0.0F, 1.0F);
        } else if (row == 4U) {
            emitter->depth_collision_enabled = !emitter->depth_collision_enabled;
        } else if (row == 5U) {
            emitter->depth_min = fminf(emitter->depth_min + sign * 0.1F, emitter->depth_max);
        } else if (row == 6U) {
            emitter->depth_max = fmaxf(emitter->depth_min, emitter->depth_max + sign * 0.1F);
        } else {
            emitter->depth_velocity_min += sign * 0.1F;
            emitter->depth_velocity_max += sign * 0.1F;
        }
    } else if (editor->particle_page == 4U) {
        if (row == 0U) {
            int output = (int)emitter->output + (decrease ? -1 : 1);
            output = output < 0 ? (int)AF_PARTICLE_OUTPUT_LIGHT_PROXY
                                : (output > (int)AF_PARTICLE_OUTPUT_LIGHT_PROXY ? 0 : output);
            emitter->output = (AfParticleOutputType)output;
        } else {
            float *value = row == 1U   ? &emitter->trail_length
                           : row == 2U ? &emitter->trail_width
                           : row == 3U ? &emitter->mesh_scale
                           : row == 4U ? &emitter->decal_rotation_offset
                           : row == 5U ? &emitter->distortion_strength
                           : row == 6U ? &emitter->soft_fade_distance
                                       : &emitter->light_radius;
            *value += sign * (row <= 2U || row == 7U ? 1.0F : 0.1F);
            if (row != 4U && row != 5U) {
                *value = fmaxf(row == 3U ? 0.1F : 0.0F, *value);
            }
        }
    } else if (editor->particle_page == 5U) {
        if (row <= 2U) {
            if (row == 0U) {
                emitter->flipbook_columns = decrease && emitter->flipbook_columns > 1U
                                                ? emitter->flipbook_columns - 1U
                                                : (!decrease && emitter->flipbook_columns < 64U
                                                       ? emitter->flipbook_columns + 1U
                                                       : emitter->flipbook_columns);
            } else if (row == 1U) {
                emitter->flipbook_rows =
                    decrease && emitter->flipbook_rows > 1U
                        ? emitter->flipbook_rows - 1U
                        : (!decrease && emitter->flipbook_rows < 64U ? emitter->flipbook_rows + 1U
                                                                     : emitter->flipbook_rows);
            } else {
                emitter->flipbook_frames_per_second =
                    fmaxf(0.0F, emitter->flipbook_frames_per_second + sign);
            }
        } else if (row == 3U) {
            uint32_t current = 0U;
            uint32_t candidate_count = 1U;
            uint64_t candidates[AF_MAX_PARTICLE_EMITTERS + 1U] = {0U};
            for (uint32_t index = 0U; index < editor->project.particle_data.count; ++index) {
                const uint64_t id = editor->project.particle_data.emitters[index].id;
                if (id != emitter->id) {
                    candidates[candidate_count++] = id;
                }
            }
            for (uint32_t index = 0U; index < candidate_count; ++index) {
                if (candidates[index] == emitter->sub_emitter_id) {
                    current = index;
                }
            }
            current = decrease ? (current == 0U ? candidate_count - 1U : current - 1U)
                               : (current + 1U) % candidate_count;
            emitter->sub_emitter_id = candidates[current];
            if (emitter->sub_emitter_id == 0U) {
                emitter->sub_emit_birth_count = 0U;
                emitter->sub_emit_death_count = 0U;
            } else if (emitter->max_recursion_depth == 0U) {
                emitter->max_recursion_depth = 1U;
            }
        } else if (row == 4U || row == 5U) {
            uint32_t *value =
                row == 4U ? &emitter->sub_emit_birth_count : &emitter->sub_emit_death_count;
            if (emitter->sub_emitter_id != 0U) {
                *value = decrease
                             ? (*value > 0U ? *value - 1U : 0U)
                             : (*value < AF_MAX_PARTICLE_SUB_EMIT_COUNT ? *value + 1U : *value);
            }
        } else if (row == 6U) {
            emitter->max_recursion_depth =
                decrease
                    ? (emitter->max_recursion_depth > 0U ? emitter->max_recursion_depth - 1U : 0U)
                    : (emitter->max_recursion_depth < AF_MAX_PARTICLE_RECURSION_DEPTH
                           ? emitter->max_recursion_depth + 1U
                           : emitter->max_recursion_depth);
            if (emitter->sub_emitter_id != 0U && emitter->max_recursion_depth == 0U) {
                emitter->max_recursion_depth = 1U;
            }
        } else {
            const uint32_t step = 16U;
            emitter->event_burst_count =
                decrease
                    ? (emitter->event_burst_count > step ? emitter->event_burst_count - step : 0U)
                    : (emitter->event_burst_count <=
                               emitter->capacity -
                                   (emitter->capacity < step ? emitter->capacity : step)
                           ? emitter->event_burst_count +
                                 (emitter->capacity < step ? emitter->capacity : step)
                           : emitter->capacity);
            emitter->event_sequence =
                emitter->event_sequence == UINT32_MAX ? 1U : emitter->event_sequence + 1U;
        }
    } else {
        if (row == 0U) {
            const uint32_t step = 1024U;
            lod->maximum_particles =
                decrease ? (lod->maximum_particles > step ? lod->maximum_particles - step : 1U)
                         : (lod->maximum_particles <= AF_MAX_PARTICLES - step
                                ? lod->maximum_particles + step
                                : AF_MAX_PARTICLES);
        } else if (row == 1U) {
            lod->spawn_rate_scale =
                af_editor_clamp(lod->spawn_rate_scale + sign * 0.1F, 0.0F, 1.0F);
        } else if (row == 2U) {
            lod->simulation_frequency_scale =
                af_editor_clamp(lod->simulation_frequency_scale + sign * 0.1F, 0.1F, 1.0F);
        } else if (row == 3U) {
            lod->renderer_enabled = !lod->renderer_enabled;
        } else if (row == 4U) {
            lod->distance_start =
                fmaxf(0.0F, fminf(lod->distance_end, lod->distance_start + sign * 10.0F));
        } else if (row == 5U) {
            lod->distance_end = fmaxf(lod->distance_start, lod->distance_end + sign * 10.0F);
        } else if (row == 6U) {
            lod->host_quality = af_editor_clamp(lod->host_quality + sign * 0.1F, 0.0F, 1.0F);
        } else {
            lod->view_distance = fmaxf(0.0F, lod->view_distance + sign * 10.0F);
        }
    }
}

static bool af_editor_handle_image_inspector(AfEditorState *editor, int width, float x, float y) {
    const float panel_x = (float)width - editor->right_width + 18.0F;
    const AfSceneNode *node = af_scene_find_const(&editor->project.scene, editor->selected_id);
    AfImageBinding *binding =
        af_image_find_binding(&editor->project.image_data, editor->selected_id);
    AfImageAsset *asset =
        binding == NULL ? NULL : af_image_find(&editor->project.image_data, binding->asset_id);
    const AfImageSequence *sequence =
        binding == NULL
            ? NULL
            : af_image_find_sequence_const(&editor->project.image_data, binding->sequence_id);
    if (asset == NULL && sequence != NULL && sequence->frame_count != 0U) {
        asset = af_image_find(&editor->project.image_data,
                              editor->project.image_data.sequence_frames[sequence->frame_start]);
    }
    if (node == NULL || node->type != AF_NODE_IMAGE || binding == NULL || asset == NULL) {
        return false;
    }
    if (x < panel_x || x > (float)width - 18.0F) {
        return true;
    }
    if (y >= 150.0F && y < 150.0F + 5.0F * 38.0F) {
        const uint32_t row = (uint32_t)((y - 150.0F) / 38.0F);
        if (y - (150.0F + (float)row * 38.0F) > 32.0F) {
            return true;
        }
        if (row == 0U) {
            asset->trim_enabled = !asset->trim_enabled;
            if (asset->trim_enabled) {
                (void)af_image_trim(asset);
            } else {
                asset->trim_x = 0U;
                asset->trim_y = 0U;
                asset->trim_width = asset->width;
                asset->trim_height = asset->height;
            }
        } else if (row == 1U) {
            asset->mipmaps_enabled = !asset->mipmaps_enabled;
        } else if (row == 2U) {
            asset->color_space = asset->color_space == AF_IMAGE_COLOR_SRGB ? AF_IMAGE_COLOR_LINEAR
                                                                           : AF_IMAGE_COLOR_SRGB;
        } else if (row == 3U) {
            asset->compression = asset->compression == AF_IMAGE_COMPRESSION_NONE
                                     ? AF_IMAGE_COMPRESSION_LOSSLESS
                                     : AF_IMAGE_COMPRESSION_NONE;
        } else {
            binding->display_mode = binding->display_mode == AF_IMAGE_DISPLAY_STRETCH
                                        ? AF_IMAGE_DISPLAY_NINE_SLICE
                                        : AF_IMAGE_DISPLAY_STRETCH;
        }
        ++editor->project.image_data.generation;
        (void)af_image_build_atlases(&editor->project.image_data);
        af_editor_mark_dirty(editor);
        af_editor_status(editor, "Image settings updated");
        return true;
    }
    if (y >= 375.0F && y < 375.0F + 4.0F * 36.0F) {
        const uint32_t row = (uint32_t)((y - 375.0F) / 36.0F);
        const bool decrease = x >= panel_x && x <= panel_x + 34.0F;
        const bool increase = x >= panel_x + 38.0F && x <= panel_x + 72.0F;
        uint16_t *value = row == 0U   ? &binding->borders.left
                          : row == 1U ? &binding->borders.top
                          : row == 2U ? &binding->borders.right
                                      : &binding->borders.bottom;
        const uint32_t maximum = row == 0U || row == 2U ? asset->width : asset->height;
        if (y - (375.0F + (float)row * 36.0F) > 28.0F || (!decrease && !increase)) {
            return true;
        }
        if (decrease) {
            *value = *value == 0U ? 0U : (uint16_t)(*value - 1U);
        } else if (*value < UINT16_MAX && *value < maximum) {
            *value = (uint16_t)(*value + 1U);
        }
        asset->sprite_borders = binding->borders;
        binding->display_mode = AF_IMAGE_DISPLAY_NINE_SLICE;
        ++editor->project.image_data.generation;
        (void)af_image_build_atlases(&editor->project.image_data);
        af_editor_mark_dirty(editor);
        af_editor_status(editor, "Nine-slice border updated");
        return true;
    }
    if (y >= 548.0F && y < 580.0F) {
        if (sequence == NULL) {
            (void)af_editor_create_flipbook(editor);
        } else {
            (void)af_editor_remove_flipbook(editor);
        }
        return true;
    }
    if (sequence != NULL && y >= 586.0F && y < 614.0F) {
        const bool decrease = x >= panel_x && x <= panel_x + 34.0F;
        const bool increase = x >= panel_x + 38.0F && x <= panel_x + 72.0F;
        if (!decrease && !increase) {
            return true;
        }
        binding->frames_per_second =
            af_editor_clamp(binding->frames_per_second + (decrease ? -1.0F : 1.0F), 1.0F, 120.0F);
        ++editor->project.image_data.generation;
        af_editor_mark_dirty(editor);
        af_editor_status(editor, "Flipbook speed updated");
        return true;
    }
    if (sequence != NULL && y >= 622.0F && y < 652.0F) {
        binding->loop = !binding->loop;
        ++editor->project.image_data.generation;
        af_editor_mark_dirty(editor);
        af_editor_status(editor,
                         binding->loop ? "Flipbook looping enabled" : "Flipbook looping disabled");
        return true;
    }
    return true;
}

static void af_editor_handle_inspector(AfEditorState *editor, int width, float x, float y) {
    const float panel_x = (float)width - editor->right_width + 18.0F;
    AfParticleEmitter *emitter =
        af_project_find_node_emitter(&editor->project, editor->selected_id);
    const AfEditorInspectorField field = af_editor_inspector_field_at(editor, width, x, y);
    if (af_editor_handle_image_inspector(editor, width, x, y)) {
        return;
    }
    if (editor->selected_count == 1U && y >= AF_EDITOR_SHAPE_SETTINGS_Y &&
        y <= AF_EDITOR_SHAPE_SETTINGS_Y + 26.0F && x >= panel_x &&
        x < panel_x + editor->right_width - 36.0F) {
        const AfSceneNode *shape = af_scene_find_const(&editor->project.scene, editor->selected_id);
        if (shape != NULL && shape->polygon_sides != 0U) {
            const uint32_t action =
                (uint32_t)((x - panel_x) / ((editor->right_width - 36.0F) / 4.0F));
            uint32_t sides = shape->polygon_sides;
            float inset = shape->polygon_inset;
            if (action == 0U && sides > 3U)
                --sides;
            if (action == 1U && sides < 64U)
                ++sides;
            if (action >= 2U && inset > 0.0F)
                inset = af_editor_clamp(inset + (action == 2U ? -0.05F : 0.05F), 0.05F, 0.95F);
            if (sides != shape->polygon_sides || inset != shape->polygon_inset)
                (void)af_editor_update_polygon(editor, shape->bounds, sides, inset, false);
            return;
        }
    }
    if (x >= panel_x && x <= panel_x + editor->right_width - 36.0F &&
        y >= AF_EDITOR_CONVERT_PATH_Y && y <= AF_EDITOR_CONVERT_PATH_Y + 26.0F) {
        af_editor_convert_selected_to_path(editor);
        return;
    }
    if (af_editor_inspector_ratio_hit(editor, width, x, y) &&
        af_scene_find_const(&editor->preview_scene, editor->selected_id) != NULL) {
        editor->inspector_ratio_locked = !editor->inspector_ratio_locked;
        af_editor_status(editor, editor->inspector_ratio_locked
                                     ? "Ratio lock enabled for size and scale"
                                     : "Ratio lock disabled");
        return;
    }
    if (field != AF_EDITOR_INSPECTOR_NONE) {
        const int step = af_editor_inspector_step_at(editor, width, field, x);
        if (step != 0) {
            if (af_editor_step_inspector_value(editor, field, step)) {
                af_editor_status(editor, step > 0 ? "Value increased" : "Value decreased");
            }
        } else {
            af_editor_begin_inspector_scrub(editor, field, x, y);
        }
        return;
    }
    if (af_scene_find_const(&editor->project.scene, editor->selected_id) != NULL &&
        y >= AF_EDITOR_COLOR_TARGET_Y &&
        y <= AF_EDITOR_COLOR_TARGET_Y + AF_EDITOR_COLOR_TARGET_HEIGHT) {
        if (x >= panel_x && x <= panel_x + 72.0F) {
            editor->color_target_outline = false;
            af_editor_status(editor, "Editing fill color");
            return;
        }
        if (x >= panel_x + 78.0F && x <= panel_x + 174.0F) {
            editor->color_target_outline = true;
            af_editor_status(editor, "Editing outline color");
            return;
        }
    }
    if (af_scene_find_const(&editor->project.scene, editor->selected_id) != NULL &&
        ((x >= panel_x && x <= panel_x + AF_EDITOR_COLOR_SV_WIDTH && y >= AF_EDITOR_COLOR_SV_Y &&
          y <= AF_EDITOR_COLOR_SV_Y + AF_EDITOR_COLOR_SV_HEIGHT) ||
         (x >= panel_x + AF_EDITOR_COLOR_HUE_X &&
          x <= panel_x + AF_EDITOR_COLOR_HUE_X + AF_EDITOR_COLOR_HUE_WIDTH &&
          y >= AF_EDITOR_COLOR_SV_Y && y <= AF_EDITOR_COLOR_SV_Y + AF_EDITOR_COLOR_SV_HEIGHT) ||
         (x >= panel_x && x <= panel_x + AF_EDITOR_COLOR_HUE_X + AF_EDITOR_COLOR_HUE_WIDTH &&
          y >= AF_EDITOR_COLOR_ALPHA_Y &&
          y <= AF_EDITOR_COLOR_ALPHA_Y + AF_EDITOR_COLOR_ALPHA_HEIGHT))) {
        af_editor_begin_selection_modify(editor, false);
        if (editor->selection_drag_count == 0U) {
            return;
        }
        editor->selected_property = af_editor_color_property(editor);
        af_editor_begin_transform_key_drag(editor, editor->selected_property);
        if (y >= AF_EDITOR_COLOR_ALPHA_Y) {
            editor->drag_mode = AF_EDITOR_DRAG_COLOR_ALPHA;
        } else if (x >= panel_x + AF_EDITOR_COLOR_HUE_X) {
            editor->drag_mode = AF_EDITOR_DRAG_COLOR_HUE;
        } else {
            editor->drag_mode = AF_EDITOR_DRAG_COLOR_SV;
        }
        af_editor_update_color_picker(editor, width, x, y);
        return;
    }
    if (emitter != NULL && y >= AF_EDITOR_PARTICLE_PAGE_Y &&
        y < AF_EDITOR_PARTICLE_PAGE_Y + 26.0F && x >= panel_x) {
        const uint32_t page = (uint32_t)((x - panel_x) / 32.0F);
        if (page < AF_EDITOR_PARTICLE_PAGE_COUNT) {
            editor->particle_page = page;
            af_editor_status(editor, "Particle editor page selected");
        }
        return;
    }
    if (emitter != NULL && y >= AF_EDITOR_PARTICLE_ROW_Y &&
        y < AF_EDITOR_PARTICLE_ROW_Y + AF_EDITOR_PARTICLE_ROW_COUNT * AF_EDITOR_PARTICLE_ROW_STEP) {
        AfProjectNodeSnapshot before = {0};
        const uint32_t row =
            (uint32_t)((y - AF_EDITOR_PARTICLE_ROW_Y) / AF_EDITOR_PARTICLE_ROW_STEP);
        AfEditorParticleParameter parameter;
        if (af_editor_particle_parameter(
                editor,
                (AfEditorInspectorField)(AF_EDITOR_INSPECTOR_PARTICLE_FIRST +
                                         editor->particle_page * 9 + row),
                &parameter))
            return;
        const bool decrease = x >= panel_x && x <= panel_x + 31.0F;
        const bool increase = x >= panel_x + 34.0F && x <= panel_x + 65.0F;
        if ((decrease || increase) && row < AF_EDITOR_PARTICLE_ROW_COUNT &&
            af_project_snapshot(&editor->project, editor->selected_id, &before) ==
                AF_RESULT_SUCCESS) {
            af_editor_adjust_particle(editor, emitter, row, decrease);
            if (af_editor_commit_modify(editor, &before, AF_EDITOR_MERGE_PARTICLE, true) ==
                AF_RESULT_SUCCESS) {
                af_editor_mark_dirty(editor);
                af_editor_status(editor, "Particle emitter updated");
            }
        }
        return;
    }
    if (y >= AF_EDITOR_STYLE_ROW_Y && y <= AF_EDITOR_STYLE_ROW_Y + 28.0F) {
        const int action = (int)((x - panel_x) / 38.0F);
        const float action_x = panel_x + (float)action * 38.0F;
        if (action >= 0 && action < 6 && x >= action_x && x <= action_x + 34.0F) {
            if (action == 3) {
                int pixel_width, pixel_height;
                if (SDL_GetWindowSizeInPixels(editor->window.window, &pixel_width, &pixel_height))
                    af_editor_choice_open(editor, 1, width, pixel_height, action_x, y + 28);
            } else
                af_editor_modify_style(editor, (uint32_t)action);
        }
        return;
    }
    if (y >= AF_EDITOR_LAYOUT_ROW_Y && y <= AF_EDITOR_LAYOUT_ROW_Y + 28.0F) {
        const int action = (int)((x - panel_x) / 38.0F);
        const float action_x = panel_x + (float)action * 38.0F;
        if (action >= 0 && action < 6 && x >= action_x && x <= action_x + 34.0F) {
            af_editor_modify_layout(editor, (uint32_t)action);
        }
        return;
    }
}

static void af_editor_reset_selected(AfEditorState *editor, bool appearance) {
    if (editor->selected_id == 0U) {
        return;
    }
    af_editor_begin_selection_modify(editor, false);
    for (uint32_t index = 0U; index < editor->selection_drag_count; ++index) {
        AfSceneNode *node =
            af_scene_find(&editor->project.scene, editor->selection_drag_before[index].node.id);
        if (node == NULL) {
            continue;
        }
        if (appearance) {
            AfStyle *style = af_project_find_node_style(&editor->project, node->id);
            const AfSceneNode defaults = af_editor_default_node(node->type, node->id, (AfVec2){0});
            node->opacity = 1.0F;
            node->fill = defaults.fill;
            node->corner_radius = defaults.corner_radius;
            if (style != NULL) {
                *style = af_editor_default_style(style->id, &defaults);
            }
        } else {
            node->local_transform = (AfTransform2D){.scale = {1.0F, 1.0F}};
        }
    }
    (void)af_scene_evaluate_world(&editor->project.scene);
    if (af_editor_commit_selection_modify(
            editor, appearance ? AF_EDITOR_MERGE_STYLE : AF_EDITOR_MERGE_TRANSFORM, false) ==
        AF_RESULT_SUCCESS) {
        af_editor_status(editor, appearance ? "Appearance reset" : "Transform reset");
    }
}

static void af_editor_convert_selected(AfEditorState *editor, AfNodeType type) {
    AfProjectNodeSnapshot before = {0};
    AfSceneNode *node = af_scene_find(&editor->project.scene, editor->selected_id);
    if (node == NULL || node->type == AF_NODE_GROUP || node->type == AF_NODE_PATH ||
        af_project_snapshot(&editor->project, node->id, &before) != AF_RESULT_SUCCESS) {
        return;
    }
    node->type = type;
    node->corner_radius =
        type == AF_NODE_ROUNDED_RECTANGLE ? fmaxf(node->corner_radius, 18.0F) : 0.0F;
    if (af_editor_commit_modify(editor, &before, AF_EDITOR_MERGE_STYLE, false) ==
        AF_RESULT_SUCCESS) {
        af_editor_status(editor, type == AF_NODE_ROUNDED_RECTANGLE
                                     ? "Converted to rounded rectangle"
                                     : "Converted to rectangle");
    }
}

static void af_editor_make_selected_circle(AfEditorState *editor) {
    AfProjectNodeSnapshot before = {0};
    AfSceneNode *node = af_scene_find(&editor->project.scene, editor->selected_id);
    if (node == NULL || node->type != AF_NODE_ELLIPSE ||
        af_project_snapshot(&editor->project, node->id, &before) != AF_RESULT_SUCCESS) {
        return;
    }
    const float size = fminf(node->bounds.x, node->bounds.y);
    node->local_transform.position.x += (node->bounds.x - size) * 0.5F;
    node->local_transform.position.y += (node->bounds.y - size) * 0.5F;
    node->bounds = (AfVec2){size, size};
    (void)af_scene_evaluate_world(&editor->project.scene);
    if (af_editor_commit_modify(editor, &before, AF_EDITOR_MERGE_TRANSFORM, false) ==
        AF_RESULT_SUCCESS) {
        af_editor_status(editor, "Ellipse made circular");
    }
}

static void af_editor_fit_selected_to_canvas(AfEditorState *editor, int width, int height) {
    AfProjectNodeSnapshot before = {0};
    AfSceneNode *node = af_scene_find(&editor->project.scene, editor->selected_id);
    AfAffine2 parent_inverse = {.m00 = 1.0F, .m11 = 1.0F};
    float canvas_x = 0.0F;
    float canvas_y = 0.0F;
    float canvas_width = 0.0F;
    float canvas_height = 0.0F;
    AfVec2 corners[4];
    float minimum_x = 0.0F;
    float minimum_y = 0.0F;
    float maximum_x = 0.0F;
    float maximum_y = 0.0F;
    if (node == NULL || (node->type != AF_NODE_RECTANGLE && node->type != AF_NODE_ELLIPSE) ||
        af_project_snapshot(&editor->project, node->id, &before) != AF_RESULT_SUCCESS) {
        return;
    }
    if (node->parent_id != AF_INVALID_NODE_ID) {
        const AfSceneNode *parent = af_scene_find_const(&editor->project.scene, node->parent_id);
        if (parent == NULL || !af_affine_inverse(parent->world_transform, &parent_inverse)) {
            return;
        }
    }
    af_editor_layout(editor, width, height, &canvas_x, &canvas_y, &canvas_width, &canvas_height);
    corners[0] = af_editor_screen_to_project(editor, width, height,
                                             (AfVec2){canvas_x + 24.0F, canvas_y + 24.0F});
    corners[1] = af_editor_screen_to_project(
        editor, width, height, (AfVec2){canvas_x + canvas_width - 24.0F, canvas_y + 24.0F});
    corners[2] = af_editor_screen_to_project(
        editor, width, height,
        (AfVec2){canvas_x + canvas_width - 24.0F, canvas_y + canvas_height - 24.0F});
    corners[3] = af_editor_screen_to_project(
        editor, width, height, (AfVec2){canvas_x + 24.0F, canvas_y + canvas_height - 24.0F});
    for (uint32_t index = 0U; index < 4U; ++index) {
        corners[index] = af_affine_transform_point(parent_inverse, corners[index]);
        if (index == 0U) {
            minimum_x = maximum_x = corners[index].x;
            minimum_y = maximum_y = corners[index].y;
        } else {
            minimum_x = fminf(minimum_x, corners[index].x);
            minimum_y = fminf(minimum_y, corners[index].y);
            maximum_x = fmaxf(maximum_x, corners[index].x);
            maximum_y = fmaxf(maximum_y, corners[index].y);
        }
    }
    node->local_transform =
        (AfTransform2D){.position = {minimum_x, minimum_y}, .scale = {1.0F, 1.0F}};
    node->bounds = (AfVec2){maximum_x - minimum_x, maximum_y - minimum_y};
    (void)af_scene_evaluate_world(&editor->project.scene);
    if (af_editor_commit_modify(editor, &before, AF_EDITOR_MERGE_TRANSFORM, false) ==
        AF_RESULT_SUCCESS) {
        af_editor_status(editor, "Object fitted to visible canvas");
    }
}

static void af_editor_toggle_selected_particles(AfEditorState *editor) {
    const AfSceneNode *node = af_scene_find_const(&editor->project.scene, editor->selected_id);
    AfParticleEmitter *existing =
        af_project_find_node_emitter(&editor->project, editor->selected_id);
    AfProjectNodeSnapshot before = {0};
    AfResult result = AF_RESULT_SUCCESS;
    if (node == NULL ||
        af_project_snapshot(&editor->project, node->id, &before) != AF_RESULT_SUCCESS) {
        return;
    }
    if (existing != NULL) {
        result = af_project_remove_emitter(&editor->project, existing->id);
        af_editor_status(editor, result == AF_RESULT_SUCCESS ? "Particle emitter removed"
                                                             : "Particle emitter removal failed");
    } else {
        uint64_t id = 0U;
        result = af_project_allocate_animation_id(&editor->project, &id);
        if (result == AF_RESULT_SUCCESS) {
            AfParticleEmitter emitter = af_particle_emitter_default(id, node->id);
            if (node->type == AF_NODE_RECTANGLE || node->type == AF_NODE_ROUNDED_RECTANGLE) {
                emitter.type = AF_PARTICLE_EMITTER_RECTANGLE;
            } else if (node->type == AF_NODE_ELLIPSE) {
                emitter.type = AF_PARTICLE_EMITTER_CIRCLE;
            } else if (node->type == AF_NODE_PATH) {
                const AfPath *path = af_path_find_node_const(&editor->project.path_data, node->id);
                emitter.type = AF_PARTICLE_EMITTER_VECTOR_PATH;
                emitter.path_id = path == NULL ? 0U : path->id;
            }
            emitter.output = AF_PARTICLE_OUTPUT_UI_BILLBOARD;
            result = af_project_add_emitter(&editor->project, &emitter);
        }
        af_editor_status(editor, result == AF_RESULT_SUCCESS ? "Particle emitter added"
                                                             : "Particle emitter creation failed");
    }
    if (result == AF_RESULT_SUCCESS) {
        (void)af_editor_commit_modify(editor, &before, AF_EDITOR_MERGE_PARTICLE, false);
        af_editor_mark_dirty(editor);
    }
}

static void af_editor_execute_particle_context_item(AfEditorState *editor, uint32_t item) {
    AfParticleEmitter *emitter =
        af_project_find_node_emitter(&editor->project, editor->selected_id);
    AfProjectNodeSnapshot before = {0};
    if (emitter == NULL) {
        return;
    }
    if (item == 5U) {
        editor->particle_page = 6U;
        af_editor_status(editor, "VFX LOD controls opened");
        return;
    }
    if (item == 6U) {
        af_editor_toggle_selected_particles(editor);
        return;
    }
    if (af_project_snapshot(&editor->project, editor->selected_id, &before) != AF_RESULT_SUCCESS) {
        return;
    }
    if (item == 0U) {
        const uint64_t id = emitter->id;
        const uint64_t node_id = emitter->node_id;
        const AfSceneNode *node = af_scene_find_const(&editor->project.scene, node_id);
        *emitter = af_particle_emitter_default(id, node_id);
        if (node != NULL &&
            (node->type == AF_NODE_RECTANGLE || node->type == AF_NODE_ROUNDED_RECTANGLE)) {
            emitter->type = AF_PARTICLE_EMITTER_RECTANGLE;
        } else if (node != NULL && node->type == AF_NODE_ELLIPSE) {
            emitter->type = AF_PARTICLE_EMITTER_CIRCLE;
        } else if (node != NULL && node->type == AF_NODE_PATH) {
            const AfPath *path = af_path_find_node_const(&editor->project.path_data, node_id);
            emitter->type = AF_PARTICLE_EMITTER_VECTOR_PATH;
            emitter->path_id = path == NULL ? 0U : path->id;
        }
    } else if (item == 1U) {
        emitter->event_burst_count = emitter->burst_count != 0U
                                         ? emitter->burst_count
                                         : (emitter->capacity < 16U ? emitter->capacity : 16U);
        emitter->event_sequence =
            emitter->event_sequence == UINT32_MAX ? 1U : emitter->event_sequence + 1U;
    } else if (item == 2U) {
        emitter->enabled = !emitter->enabled;
    } else if (item == 3U) {
        emitter->output = (AfParticleOutputType)(((uint32_t)emitter->output + 1U) %
                                                 ((uint32_t)AF_PARTICLE_OUTPUT_LIGHT_PROXY + 1U));
    } else if (item == 4U) {
        const uint32_t previous_page = editor->particle_page;
        editor->particle_page = 0U;
        af_editor_adjust_particle(editor, emitter, 5U, false);
        editor->particle_page = previous_page;
    }
    if (af_editor_commit_modify(editor, &before, AF_EDITOR_MERGE_PARTICLE, false) ==
        AF_RESULT_SUCCESS) {
        af_editor_mark_dirty(editor);
        af_editor_status(editor, "Particle context action applied");
    }
}

static void af_editor_execute_object_context_item(AfEditorState *editor, uint32_t item, int width,
                                                  int height) {
    const AfEditorContextMenu menu = editor->context_menu;
    if (menu == AF_EDITOR_CONTEXT_IMAGE) {
        if (item == 0U) {
            (void)af_editor_duplicate_selection(editor);
        } else if (item == 1U) {
            af_editor_begin_rename(editor);
        } else if (item == 2U) {
            const AfImageBinding *binding =
                af_image_find_binding_const(&editor->project.image_data, editor->selected_id);
            const AfImageSequence *sequence =
                binding == NULL ? NULL
                                : af_image_find_sequence_const(&editor->project.image_data,
                                                               binding->sequence_id);
            editor->selected_asset_id =
                binding == NULL ? 0U
                                : (binding->asset_id != 0U
                                       ? binding->asset_id
                                       : (sequence == NULL || sequence->frame_count == 0U
                                              ? 0U
                                              : editor->project.image_data
                                                    .sequence_frames[sequence->frame_start]));
            editor->asset_browser_visible = true;
            af_editor_status(editor, "Image asset shown in browser");
        } else if (item == 3U) {
            const AfImageBinding *binding =
                af_image_find_binding_const(&editor->project.image_data, editor->selected_id);
            const AfImageAsset *asset =
                binding == NULL
                    ? NULL
                    : af_image_find_const(&editor->project.image_data, binding->asset_id);
            if (asset != NULL && asset->missing) {
                editor->selected_asset_id = asset->id;
                af_editor_show_project_dialog(editor, AF_EDITOR_DIALOG_REPAIR_IMAGE);
            } else {
                (void)af_editor_reimport_images(editor);
            }
        } else if (item == 4U) {
            (void)af_editor_create_flipbook(editor);
        } else if (item == 5U) {
            af_editor_toggle_image_nine_slice(editor);
        } else if (item == 6U) {
            (void)af_editor_delete_selection(editor);
        }
        return;
    }
    if (menu == AF_EDITOR_CONTEXT_ASSET) {
        if (item == 0U) {
            af_editor_select_asset_users(editor, editor->selected_asset_id);
        } else if (item == 1U) {
            (void)af_editor_create_flipbook(editor);
        } else if (item == 2U) {
            const AfImageAsset *asset =
                af_image_find_const(&editor->project.image_data, editor->selected_asset_id);
            if (asset != NULL && asset->missing) {
                af_editor_show_project_dialog(editor, AF_EDITOR_DIALOG_REPAIR_IMAGE);
            } else {
                (void)af_editor_reimport_images(editor);
            }
        } else if (item == 3U) {
            if (af_image_build_atlases(&editor->project.image_data) == AF_RESULT_SUCCESS) {
                af_editor_status(editor, "Atlases packed deterministically");
            }
        } else if (item == 4U) {
            const AfResult result =
                af_image_remove(&editor->project.image_data, editor->selected_asset_id);
            if (result == AF_RESULT_SUCCESS) {
                editor->selected_asset_id = 0U;
                if (editor->asset_scroll >= editor->project.image_data.asset_count &&
                    editor->asset_scroll > 0U) {
                    --editor->asset_scroll;
                }
                (void)af_image_build_atlases(&editor->project.image_data);
                af_editor_mark_dirty(editor);
                af_editor_status(editor, "Unused image asset deleted");
            } else {
                af_editor_status(editor, "Asset is still used by a scene object or sequence");
            }
        }
        return;
    }
    if (item == 0U) {
        (void)af_editor_duplicate_selection(editor);
        return;
    }
    if (item == 1U) {
        af_editor_begin_rename(editor);
        return;
    }
    if (item == 4U) {
        af_editor_toggle_selected_particles(editor);
        return;
    }
    if (item == 7U &&
        (menu == AF_EDITOR_CONTEXT_RECTANGLE || menu == AF_EDITOR_CONTEXT_ROUNDED_RECTANGLE ||
         menu == AF_EDITOR_CONTEXT_ELLIPSE)) {
        af_editor_convert_selected_to_path(editor);
        return;
    }
    if (menu == AF_EDITOR_CONTEXT_RECTANGLE) {
        if (item == 2U) {
            af_editor_convert_selected(editor, AF_NODE_ROUNDED_RECTANGLE);
        } else if (item == 3U) {
            af_editor_fit_selected_to_canvas(editor, width, height);
        } else if (item == 5U) {
            af_editor_modify_scalar(editor, AF_EDITOR_MERGE_LAYER, 1.0F, 2);
        } else if (item == 6U) {
            (void)af_editor_delete_selection(editor);
        }
    } else if (menu == AF_EDITOR_CONTEXT_ROUNDED_RECTANGLE) {
        if (item == 2U) {
            af_editor_modify_scalar(editor, AF_EDITOR_MERGE_RADIUS, 4.0F, 1);
        } else if (item == 3U) {
            af_editor_convert_selected(editor, AF_NODE_RECTANGLE);
        } else if (item == 5U) {
            af_editor_modify_scalar(editor, AF_EDITOR_MERGE_LAYER, 1.0F, 2);
        } else if (item == 6U) {
            (void)af_editor_delete_selection(editor);
        }
    } else if (menu == AF_EDITOR_CONTEXT_ELLIPSE) {
        if (item == 2U) {
            af_editor_make_selected_circle(editor);
        } else if (item == 3U) {
            af_editor_fit_selected_to_canvas(editor, width, height);
        } else if (item == 5U) {
            af_editor_modify_scalar(editor, AF_EDITOR_MERGE_LAYER, 1.0F, 2);
        } else if (item == 6U) {
            (void)af_editor_delete_selection(editor);
        }
    } else if (menu == AF_EDITOR_CONTEXT_PATH) {
        if (item == 2U) {
            (void)af_editor_toggle_path_fill_rule(editor);
        } else if (item == 3U) {
            af_editor_convert_selected_to_path(editor);
        } else if (item == 5U) {
            (void)af_editor_reverse_path_contour(editor);
        } else if (item == 6U) {
            (void)af_editor_delete_selection(editor);
        }
    } else if (menu == AF_EDITOR_CONTEXT_GROUP) {
        if (item == 2U) {
            const uint64_t parent_id = editor->selected_id;
            if (af_editor_create_primitive(editor, AF_NODE_RECTANGLE, width, height) ==
                AF_RESULT_SUCCESS) {
                af_editor_reparent(editor, parent_id);
                af_editor_status(editor, "Rectangle child added to group");
            }
        } else if (item == 3U) {
            af_editor_reparent(editor, AF_INVALID_NODE_ID);
        } else if (item == 5U) {
            (void)af_editor_delete_selection(editor);
        }
    }
}

static void af_editor_execute_context_item(AfEditorState *editor, uint32_t item, int width,
                                           int height) {
    if (editor->context_menu == AF_EDITOR_CONTEXT_HIERARCHY) {
        if (item == 0U) {
            (void)af_editor_duplicate_selection(editor);
        } else if (item == 1U) {
            af_editor_begin_rename(editor);
        } else if (item == 2U) {
            af_editor_reparent(editor, AF_INVALID_NODE_ID);
        } else if (item == 3U) {
            (void)af_editor_delete_selection(editor);
        } else if (item == 4U) {
            af_editor_group_selection(editor);
        }
    } else if (editor->context_menu == AF_EDITOR_CONTEXT_CANVAS) {
        if (item <= 2U) {
            static const AfNodeType types[] = {AF_NODE_RECTANGLE, AF_NODE_ROUNDED_RECTANGLE,
                                               AF_NODE_ELLIPSE};
            (void)af_editor_create_primitive(editor, types[item], width, height);
        } else if (item == 3U) {
            (void)af_editor_create_path(editor, width, height);
        } else if (item == 4U) {
            editor->grid_visible = !editor->grid_visible;
            af_editor_status(editor, editor->grid_visible ? "Grid enabled" : "Grid disabled");
        } else if (item == 5U) {
            editor->snapping = !editor->snapping;
            af_editor_status(editor, editor->snapping ? "Snapping enabled" : "Snapping disabled");
        }
    } else if (editor->context_menu == AF_EDITOR_CONTEXT_PARTICLE) {
        af_editor_execute_particle_context_item(editor, item);
    } else if (editor->context_menu == AF_EDITOR_CONTEXT_INSPECTOR) {
        if (item == 0U) {
            af_editor_reset_selected(editor, false);
        } else if (item == 1U) {
            af_editor_reset_selected(editor, true);
        } else if (item == 2U) {
            af_editor_modify_scalar(editor, AF_EDITOR_MERGE_LAYER, 1.0F, 2);
        } else if (item == 3U) {
            af_editor_modify_scalar(editor, AF_EDITOR_MERGE_LAYER, -1.0F, 2);
        } else if (item == 4U) {
            (void)af_editor_duplicate_selection(editor);
        } else if (item == 5U) {
            (void)af_editor_delete_selection(editor);
        }
    } else if (editor->context_menu == AF_EDITOR_CONTEXT_TIMELINE) {
        editor->timeline_focus = true;
        if (item == 0U) {
            (void)af_editor_add_keyframe(editor);
        } else if (item == 1U) {
            (void)af_editor_delete_keyframes(editor);
        } else if (item == 2U) {
            af_editor_copy_keyframes(editor);
        } else if (item == 3U) {
            (void)af_editor_paste_keyframes(editor, false);
        } else if (item == 4U) {
            editor->curve_visible = !editor->curve_visible;
        } else if (item == 5U) {
            (void)af_editor_new_clip(editor);
        }
    } else if (editor->context_menu == AF_EDITOR_CONTEXT_TOOLBAR) {
        if (item == 0U) {
            (void)af_editor_save(editor);
        } else if (item == 1U) {
            (void)af_editor_compile(editor);
        } else if (item == 2U) {
            editor->grid_visible = !editor->grid_visible;
        } else if (item == 3U) {
            editor->snapping = !editor->snapping;
        } else if (item == 4U) {
            if (af_editor_history_undo(&editor->history, &editor->project) == AF_RESULT_SUCCESS) {
                af_editor_mark_dirty(editor);
                af_editor_reconcile_selection(editor);
                af_editor_status(editor, "Undo");
            }
        } else if (item == 5U && af_editor_history_redo(&editor->history, &editor->project) ==
                                     AF_RESULT_SUCCESS) {
            af_editor_mark_dirty(editor);
            af_editor_reconcile_selection(editor);
            af_editor_status(editor, "Redo");
        }
    } else {
        af_editor_execute_object_context_item(editor, item, width, height);
    }
}

static int32_t af_editor_context_item_at(const AfEditorState *editor, int width, int height,
                                         float x, float y) {
    const AfEditorContextMenuInfo *info = af_editor_context_info(editor->context_menu);
    const AfVec2 origin = af_editor_context_origin(editor, width, height);
    const float first_y = origin.y + AF_EDITOR_CONTEXT_HEADER_HEIGHT + 4.0F;
    if (info == NULL || x < origin.x || x > origin.x + AF_EDITOR_CONTEXT_WIDTH || y < first_y) {
        return -1;
    }
    const uint32_t item = (uint32_t)((y - first_y) / AF_EDITOR_CONTEXT_ITEM_HEIGHT);
    return item < info->item_count ? (int32_t)item : -1;
}

static void af_editor_open_context_menu(AfEditorState *editor, int width, int height, float x,
                                        float y) {
    const float timeline_top = af_editor_timeline_top(editor, height);
    editor->context_position = (AfVec2){x, y};
    editor->mouse_position = editor->context_position;
    if (x < editor->left_width) {
        if (editor->asset_browser_visible && y >= af_editor_asset_browser_top(editor, height)) {
            const float row_start = af_editor_asset_browser_top(editor, height) + 48.0F;
            const uint32_t row = y < row_start ? UINT32_MAX : (uint32_t)((y - row_start) / 27.0F);
            const uint32_t asset_index = editor->asset_scroll + row;
            if (row < af_editor_asset_visible_rows(editor) &&
                asset_index < editor->project.image_data.asset_count) {
                editor->context_menu = AF_EDITOR_CONTEXT_ASSET;
                editor->selected_asset_id = editor->project.image_data.assets[asset_index].id;
                for (uint32_t binding = 0U; binding < editor->project.image_data.binding_count;
                     ++binding) {
                    if (af_editor_binding_uses_asset(&editor->project.image_data,
                                                     &editor->project.image_data.bindings[binding],
                                                     editor->selected_asset_id)) {
                        af_editor_select_only(editor,
                                              editor->project.image_data.bindings[binding].node_id);
                        break;
                    }
                }
            } else {
                editor->context_menu = AF_EDITOR_CONTEXT_NONE;
            }
            return;
        }
        const float hierarchy_top = AF_EDITOR_HIERARCHY_TOP;
        editor->context_menu = AF_EDITOR_CONTEXT_HIERARCHY;
        if (y >= hierarchy_top) {
            const uint32_t row = (uint32_t)((y - hierarchy_top) / AF_EDITOR_ROW_HEIGHT);
            const uint32_t hierarchy_index = editor->hierarchy_scroll + row;
            if (hierarchy_index < editor->hierarchy_count) {
                const uint64_t clicked =
                    editor->project.scene.nodes[editor->hierarchy_order[hierarchy_index]].id;
                if (!af_editor_selection_contains(editor, clicked)) {
                    af_editor_select_only(editor, clicked);
                } else {
                    editor->selected_id = clicked;
                }
                af_editor_reconcile_selection(editor);
                editor->context_menu = af_editor_context_for_node(
                    af_scene_find_const(&editor->project.scene, editor->selected_id));
            }
        }
    } else if (x >= (float)width - editor->right_width) {
        editor->context_menu =
            y >= AF_EDITOR_PARTICLE_LABEL_Y && af_project_find_node_emitter_const(
                                                   &editor->project, editor->selected_id) != NULL
                ? AF_EDITOR_CONTEXT_PARTICLE
                : AF_EDITOR_CONTEXT_INSPECTOR;
    } else if (y < editor->toolbar_height) {
        editor->context_menu = AF_EDITOR_CONTEXT_TOOLBAR;
    } else if (y >= timeline_top) {
        editor->context_menu = AF_EDITOR_CONTEXT_TIMELINE;
        editor->timeline_focus = true;
    } else {
        const uint64_t hit = af_editor_hit_test(
            editor, af_editor_screen_to_project(editor, width, height, (AfVec2){x, y}));
        if (hit != 0U) {
            if (!af_editor_selection_contains(editor, hit)) {
                af_editor_select_only(editor, hit);
            } else {
                editor->selected_id = hit;
            }
            af_editor_reconcile_selection(editor);
            editor->context_menu = af_editor_context_for_node(
                af_scene_find_const(&editor->project.scene, editor->selected_id));
        } else {
            editor->context_menu = AF_EDITOR_CONTEXT_CANVAS;
        }
    }
}
