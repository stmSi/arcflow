static float af_editor_clamp(float value, float minimum, float maximum) {
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

static AfColor af_editor_hsv_color(float hue, float saturation, float value, float alpha) {
    const float wrapped = hue - floorf(hue);
    const float sector_value = wrapped * 6.0F;
    const int sector = (int)floorf(sector_value);
    const float fraction = sector_value - (float)sector;
    const float low = value * (1.0F - saturation);
    const float falling = value * (1.0F - saturation * fraction);
    const float rising = value * (1.0F - saturation * (1.0F - fraction));
    switch (sector % 6) {
    case 0:
        return (AfColor){value, rising, low, alpha};
    case 1:
        return (AfColor){falling, value, low, alpha};
    case 2:
        return (AfColor){low, value, rising, alpha};
    case 3:
        return (AfColor){low, falling, value, alpha};
    case 4:
        return (AfColor){rising, low, value, alpha};
    default:
        return (AfColor){value, low, falling, alpha};
    }
}

static void af_editor_color_hsv(AfColor color, float *out_hue, float *out_saturation,
                                float *out_value) {
    const float maximum = fmaxf(color.r, fmaxf(color.g, color.b));
    const float minimum = fminf(color.r, fminf(color.g, color.b));
    const float range = maximum - minimum;
    float hue = 0.0F;
    if (range > 0.00001F) {
        if (maximum == color.r) {
            hue = (color.g - color.b) / range;
        } else if (maximum == color.g) {
            hue = 2.0F + (color.b - color.r) / range;
        } else {
            hue = 4.0F + (color.r - color.g) / range;
        }
        hue /= 6.0F;
        if (hue < 0.0F) {
            hue += 1.0F;
        }
    }
    *out_hue = hue;
    *out_saturation = maximum <= 0.00001F ? 0.0F : range / maximum;
    *out_value = maximum;
}

static bool af_editor_selection_contains(const AfEditorState *editor, uint64_t id) {
    for (uint32_t index = 0U; index < editor->selected_count; ++index) {
        if (editor->selected_ids[index] == id) {
            return true;
        }
    }
    return false;
}

static void af_editor_select_only(AfEditorState *editor, uint64_t id) {
    editor->selected_count = id == 0U ? 0U : 1U;
    editor->selected_id = id;
    if (id != 0U) {
        editor->selected_ids[0] = id;
    }
}

static void af_editor_selection_add(AfEditorState *editor, uint64_t id) {
    if (id == 0U || af_editor_selection_contains(editor, id)) {
        return;
    }
    if (editor->selected_count < AF_EDITOR_COMMAND_MAX_SNAPSHOTS) {
        editor->selected_ids[editor->selected_count++] = id;
        editor->selected_id = id;
    }
}

static void af_editor_selection_toggle(AfEditorState *editor, uint64_t id) {
    for (uint32_t index = 0U; index < editor->selected_count; ++index) {
        if (editor->selected_ids[index] == id) {
            memmove(&editor->selected_ids[index], &editor->selected_ids[index + 1U],
                    (size_t)(editor->selected_count - index - 1U) *
                        sizeof(editor->selected_ids[0]));
            --editor->selected_count;
            editor->selected_id = editor->selected_count == 0U
                                      ? 0U
                                      : editor->selected_ids[editor->selected_count - 1U];
            return;
        }
    }
    af_editor_selection_add(editor, id);
}

static bool af_editor_selection_has_selected_ancestor(const AfEditorState *editor, uint64_t id) {
    const AfSceneNode *node = af_scene_find_const(&editor->project.scene, id);
    uint32_t depth = 0U;
    while (node != NULL && node->parent_id != AF_INVALID_NODE_ID &&
           depth++ < AF_MAX_HIERARCHY_DEPTH) {
        if (af_editor_selection_contains(editor, node->parent_id)) {
            return true;
        }
        node = af_scene_find_const(&editor->project.scene, node->parent_id);
    }
    return false;
}

static void af_editor_begin_selection_modify(AfEditorState *editor, bool top_level_only) {
    editor->selection_drag_count = 0U;
    for (uint32_t index = 0U; index < editor->selected_count; ++index) {
        const uint64_t id = editor->selected_ids[index];
        if ((!top_level_only || !af_editor_selection_has_selected_ancestor(editor, id)) &&
            af_project_snapshot(&editor->project, id,
                                &editor->selection_drag_before[editor->selection_drag_count]) ==
                AF_RESULT_SUCCESS) {
            ++editor->selection_drag_count;
        }
    }
    if (editor->selection_drag_count == 0U && editor->selected_id != 0U &&
        af_project_snapshot(&editor->project, editor->selected_id,
                            &editor->selection_drag_before[0]) == AF_RESULT_SUCCESS) {
        editor->selection_drag_count = 1U;
    }
}

static AfResult af_editor_commit_selection_modify(AfEditorState *editor, AfEditorMergeKey merge_key,
                                                  bool allow_merge) {
    AfEditorCommand command = {.type = AF_EDITOR_COMMAND_MODIFY,
                               .merge_key = merge_key,
                               .target_id = editor->selected_id,
                               .before_count = editor->selection_drag_count,
                               .after_count = editor->selection_drag_count};
    AfResult result = AF_RESULT_SUCCESS;
    if (editor->selection_drag_count == 0U) {
        return AF_RESULT_NOT_FOUND;
    }
    memcpy(command.before, editor->selection_drag_before,
           (size_t)editor->selection_drag_count * sizeof(command.before[0]));
    for (uint32_t index = 0U; index < editor->selection_drag_count; ++index) {
        result = af_project_snapshot(&editor->project, editor->selection_drag_before[index].node.id,
                                     &command.after[index]);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
    }
    result = af_editor_history_execute(&editor->history, &editor->project, &command, true,
                                       allow_merge && editor->selection_drag_count == 1U);
    if (result == AF_RESULT_SUCCESS) {
        editor->dirty = true;
    }
    return result;
}

static int32_t af_editor_clamp_layer(int32_t value) {
    if (value < -10000) {
        return -10000;
    }
    return value > 10000 ? 10000 : value;
}

static void af_editor_status(AfEditorState *editor, const char *message) {
    (void)snprintf(editor->status, sizeof(editor->status), "%s", message);
}

static AfSceneNode af_editor_default_node(AfNodeType type, uint64_t id, AfVec2 position) {
    const bool rounded = type == AF_NODE_ROUNDED_RECTANGLE;
    return (AfSceneNode){.id = id,
                         .parent_id = AF_INVALID_NODE_ID,
                         .type = type,
                         .local_transform = {.position = position, .scale = {1.0F, 1.0F}},
                         .bounds = type == AF_NODE_ELLIPSE ? (AfVec2){120.0F, 120.0F}
                                                           : (AfVec2){180.0F, 110.0F},
                         .fill = {0.22F, 0.58F, 0.96F, 1.0F},
                         .opacity = 1.0F,
                         .corner_radius = rounded ? 24.0F : 0.0F,
                         .visible = true};
}

static AfResult af_editor_default_project(AfEditorState *editor) {
    AfSceneNode root = {.id = 1U,
                        .parent_id = AF_INVALID_NODE_ID,
                        .type = AF_NODE_GROUP,
                        .local_transform = {.scale = {1.0F, 1.0F}},
                        .opacity = 1.0F,
                        .visible = true};
    AfSceneNode rectangle = af_editor_default_node(AF_NODE_RECTANGLE, 2U, (AfVec2){160.0F, 150.0F});
    AfSceneNode rounded =
        af_editor_default_node(AF_NODE_ROUNDED_RECTANGLE, 3U, (AfVec2){480.0F, 270.0F});
    AfSceneNode ellipse = af_editor_default_node(AF_NODE_ELLIPSE, 4U, (AfVec2){820.0F, 390.0F});
    AfResult result = AF_RESULT_SUCCESS;
    AfProjectClip clip = {.id = 1U,
                          .name = "Main Clip",
                          .duration = 4.0F,
                          .loop_start = 0.0F,
                          .loop_end = 4.0F,
                          .loop = true};
    rectangle.parent_id = root.id;
    rectangle.fill = (AfColor){0.18F, 0.56F, 0.96F, 1.0F};
    rounded.parent_id = root.id;
    rounded.fill = (AfColor){0.94F, 0.35F, 0.27F, 1.0F};
    rounded.layer_order = 1;
    ellipse.parent_id = root.id;
    ellipse.fill = (AfColor){0.32F, 0.86F, 0.54F, 1.0F};
    ellipse.layer_order = 2;
    result = af_project_add_node(&editor->project, &root, "Scene");
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_node(&editor->project, &rectangle, "Rectangle");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_node(&editor->project, &rounded, "Rounded Rectangle");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_node(&editor->project, &ellipse, "Ellipse");
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_add_clip(&editor->project, &clip);
    }
    if (result == AF_RESULT_SUCCESS) {
        uint64_t emitter_id = 0U;
        result = af_project_allocate_animation_id(&editor->project, &emitter_id);
        if (result == AF_RESULT_SUCCESS) {
            AfParticleEmitter emitter = af_particle_emitter_default(emitter_id, ellipse.id);
            emitter.type = AF_PARTICLE_EMITTER_CIRCLE;
            emitter.output = AF_PARTICLE_OUTPUT_TRAIL;
            emitter.turbulence_strength = 120.0F;
            emitter.vortex_force = 70.0F;
            result = af_project_add_emitter(&editor->project, &emitter);
        }
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_scene_evaluate_world(&editor->project.scene);
    }
    return result;
}

static void af_editor_derive_package_path(AfEditorState *editor) {
    const char *extension = strrchr(editor->project_path, '.');
    size_t prefix = strlen(editor->project_path);
    if (extension != NULL && strcmp(extension, ".afproj") == 0) {
        prefix = (size_t)(extension - editor->project_path);
    }
    if (prefix > sizeof(editor->package_path) - 5U) {
        prefix = sizeof(editor->package_path) - 5U;
    }
    memcpy(editor->package_path, editor->project_path, prefix);
    memcpy(editor->package_path + prefix, ".afp", 5U);
}

static AfResult af_editor_save(AfEditorState *editor) {
    AfResult result =
        af_project_save_file(&editor->project, editor->project_path, &editor->allocator);
    if (result == AF_RESULT_SUCCESS) {
        editor->dirty = false;
        af_editor_status(editor, "Project saved");
    } else {
        af_editor_status(editor, "Project save failed");
    }
    return result;
}

static AfResult af_editor_compile(AfEditorState *editor) {
    AfPackageBlob package = {0};
    AfResult result = af_project_compile(&editor->project, &editor->allocator, &package);
    if (result == AF_RESULT_SUCCESS) {
        result = af_file_write(editor->package_path, package.data, package.size);
    }
    af_package_blob_destroy(&editor->allocator, &package);
    af_editor_status(editor, result == AF_RESULT_SUCCESS ? "Runtime package compiled"
                                                         : "Runtime package compilation failed");
    return result;
}

static void af_editor_hierarchy_visit(AfEditorState *editor, uint64_t parent_id, uint8_t depth) {
    uint32_t index = 0U;
    for (index = 0U; index < editor->project.scene.count; ++index) {
        const AfSceneNode *node = &editor->project.scene.nodes[index];
        if (node->parent_id != parent_id || editor->hierarchy_count >= AF_EDITOR_PROJECT_CAPACITY) {
            continue;
        }
        editor->hierarchy_order[editor->hierarchy_count] = index;
        editor->hierarchy_depth[editor->hierarchy_count] = depth;
        ++editor->hierarchy_count;
        if (depth + 1U < AF_MAX_HIERARCHY_DEPTH) {
            af_editor_hierarchy_visit(editor, node->id, (uint8_t)(depth + 1U));
        }
    }
}

static void af_editor_build_hierarchy(AfEditorState *editor) {
    editor->hierarchy_count = 0U;
    af_editor_hierarchy_visit(editor, AF_INVALID_NODE_ID, 0U);
    if (editor->hierarchy_scroll > editor->hierarchy_count) {
        editor->hierarchy_scroll = editor->hierarchy_count;
    }
}

static void af_editor_hierarchy_select_range(AfEditorState *editor, uint32_t clicked_index,
                                             bool additive) {
    uint32_t anchor_index = UINT32_MAX;
    if (clicked_index >= editor->hierarchy_count) {
        return;
    }
    for (uint32_t index = 0U; index < editor->hierarchy_count; ++index) {
        if (editor->project.scene.nodes[editor->hierarchy_order[index]].id ==
            editor->hierarchy_anchor_id) {
            anchor_index = index;
            break;
        }
    }
    if (anchor_index == UINT32_MAX) {
        anchor_index = clicked_index;
    }
    if (!additive) {
        af_editor_select_only(editor, 0U);
    }
    const uint32_t first = anchor_index < clicked_index ? anchor_index : clicked_index;
    const uint32_t last = anchor_index > clicked_index ? anchor_index : clicked_index;
    for (uint32_t index = first; index <= last; ++index) {
        af_editor_selection_add(editor,
                                editor->project.scene.nodes[editor->hierarchy_order[index]].id);
    }
    editor->selected_id = editor->project.scene.nodes[editor->hierarchy_order[clicked_index]].id;
    af_editor_status(editor, editor->selected_count == AF_EDITOR_COMMAND_MAX_SNAPSHOTS &&
                                     last - first + 1U > AF_EDITOR_COMMAND_MAX_SNAPSHOTS
                                 ? "Selection reached the bounded 64-object limit"
                                 : "Hierarchy range selected");
}
