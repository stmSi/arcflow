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
            key->keyframe.value[0] = color.r;
            key->keyframe.value[1] = color.g;
            key->keyframe.value[2] = color.b;
            key->keyframe.value[3] = color.a;
        } else {
            AfStyle *style = af_project_find_node_style(&editor->project, node->id);
            node->fill = color;
            if (style != NULL) {
                style->fill = af_paint_solid(color);
                (void)af_style_mark_paint_changed(style);
            }
        }
    }
    if (editor->transform_drag_key_count != 0U) {
        af_editor_update_preview(editor);
    }
    af_editor_mark_dirty(editor);
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
    color = node->fill;
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
    (void)af_editor_commit_selection_modify(editor, AF_EDITOR_MERGE_STYLE, false);
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
        if (editor->snapping) {
            position.x = roundf(position.x / editor->grid_step) * editor->grid_step;
            position.y = roundf(position.y / editor->grid_step) * editor->grid_step;
        }
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
    AfProjectNodeSnapshot before = {0};
    AfSceneNode *parent = af_scene_find(&editor->project.scene, parent_id);
    if (editor->selected_id == 0U || editor->selected_id == parent_id ||
        (parent_id != AF_INVALID_NODE_ID && (parent == NULL || parent->type != AF_NODE_GROUP)) ||
        af_project_snapshot(&editor->project, editor->selected_id, &before) != AF_RESULT_SUCCESS) {
        return;
    }
    if (af_scene_reparent(&editor->project.scene, editor->selected_id, parent_id) ==
        AF_RESULT_SUCCESS) {
        (void)af_scene_evaluate_world(&editor->project.scene);
        (void)af_editor_commit_modify(editor, &before, AF_EDITOR_MERGE_PARENT, false);
        af_editor_status(editor,
                         parent_id == AF_INVALID_NODE_ID ? "Node unparented" : "Node parented");
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
    if (af_editor_handle_image_inspector(editor, width, x, y)) {
        return;
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
        editor->selected_property = AF_PROPERTY_FILL_COLOR;
        af_editor_begin_transform_key_drag(editor, AF_PROPERTY_FILL_COLOR);
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
    for (int row = 0; row < 4; ++row) {
        const float row_y =
            AF_EDITOR_INSPECTOR_SCALAR_Y + (float)row * AF_EDITOR_INSPECTOR_SCALAR_STEP;
        if (y >= row_y && y <= row_y + 28.0F) {
            const bool decrease = x >= panel_x && x <= panel_x + 34.0F;
            const bool increase = x >= panel_x + 38.0F && x <= panel_x + 72.0F;
            const float amount = decrease ? -1.0F : 1.0F;
            if (!decrease && !increase) {
                return;
            }
            if (row == 0) {
                af_editor_modify_scalar(editor, AF_EDITOR_MERGE_OPACITY, amount * 0.05F, 0);
            } else if (row == 1) {
                af_editor_modify_scalar(editor, AF_EDITOR_MERGE_RADIUS, amount * 2.0F, 1);
            } else if (row == 2) {
                af_editor_modify_scalar(editor, AF_EDITOR_MERGE_LAYER, amount, 2);
            } else {
                af_editor_modify_scalar(editor, AF_EDITOR_MERGE_TRANSFORM, amount * 0.05F, 3);
            }
            return;
        }
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
                *style = af_style_default(style->id, node->id, defaults.fill);
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
            editor->selected_path_command = 0U;
            af_editor_status(editor, "Path points ready for editing");
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
            if (row < 5U && asset_index < editor->project.image_data.asset_count) {
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
        const float hierarchy_top = AF_EDITOR_TOOLBAR_HEIGHT + 8.0F;
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
    } else if (y < AF_EDITOR_TOOLBAR_HEIGHT) {
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
