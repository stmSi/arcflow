static void af_editor_render_hierarchy(AfEditorState *editor, int height) {
    uint32_t visible_rows = 0U;
    uint32_t row = 0U;
    const float top = AF_EDITOR_TOOLBAR_HEIGHT + 8.0F;
    af_editor_add_text(editor, 14.0F, 15.0F, "SCENE HIERARCHY", 1.5F, editor->left_width - 28.0F,
                       (AfColor){0.78F, 0.81F, 0.88F, 1.0F}, 100125);
    af_editor_add_text(editor, 14.0F, 30.0F, "SHIFT RANGE / CTRL TOGGLE / RIGHT CLICK", 0.6F,
                       editor->left_width - 28.0F, (AfColor){0.43F, 0.47F, 0.56F, 1.0F}, 100125);
    if ((float)height > top + AF_EDITOR_STATUS_HEIGHT) {
        visible_rows =
            (uint32_t)(((float)height - top - AF_EDITOR_STATUS_HEIGHT - editor->timeline_height) /
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
        }
        af_editor_add_rect(editor, 6.0F, y, editor->left_width - 12.0F, AF_EDITOR_ROW_HEIGHT - 2.0F,
                           row_color, 100110);
        af_editor_add_shape(
            editor,
            node->type == AF_NODE_GROUP || node->type == AF_NODE_PATH ? AF_NODE_ROUNDED_RECTANGLE
                                                                      : node->type,
            (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = indent, .ty = y + 6.0F},
            (AfVec2){15.0F, 15.0F},
            node->type == AF_NODE_GROUP ? (AfColor){0.72F, 0.58F, 0.22F, 1.0F} : node->fill,
            node->type == AF_NODE_GROUP ? 3.0F : node->corner_radius, 100120);
        af_editor_add_text(editor, indent + 23.0F, y + 10.0F, editor->project.names[node_index],
                           1.2F, editor->left_width - indent - 34.0F,
                           (AfColor){0.78F, 0.81F, 0.88F, 1.0F}, 100125);
    }
}

static void af_editor_render_toolbar(AfEditorState *editor) {
    const float start = editor->left_width + 14.0F;
    const AfColor button = {0.18F, 0.2F, 0.25F, 1.0F};
    af_editor_add_rect(editor, start, 6.0F, 34.0F, 32.0F, button, 100110);
    af_editor_add_rect(editor, start + 8.0F, 13.0F, 18.0F, 17.0F,
                       (AfColor){0.22F, 0.58F, 0.96F, 1.0F}, 100120);
    af_editor_add_rect(editor, start + 42.0F, 6.0F, 34.0F, 32.0F, button, 100110);
    af_editor_add_shape(editor, AF_NODE_ROUNDED_RECTANGLE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = start + 50.0F, .ty = 13.0F},
                        (AfVec2){18.0F, 17.0F}, (AfColor){0.94F, 0.35F, 0.27F, 1.0F}, 5.0F, 100120);
    af_editor_add_rect(editor, start + 84.0F, 6.0F, 34.0F, 32.0F, button, 100110);
    af_editor_add_shape(editor, AF_NODE_ELLIPSE,
                        (AfAffine2){.m00 = 1.0F, .m11 = 1.0F, .tx = start + 92.0F, .ty = 13.0F},
                        (AfVec2){18.0F, 17.0F}, (AfColor){0.32F, 0.86F, 0.54F, 1.0F}, 0.0F, 100120);
    af_editor_add_rect(editor, start + 126.0F, 6.0F, 34.0F, 32.0F, button, 100110);
    af_editor_add_line(editor, (AfVec2){start + 133.0F, 28.0F}, (AfVec2){start + 141.0F, 14.0F},
                       3.0F, (AfColor){0.78F, 0.42F, 0.96F, 1.0F}, 100120);
    af_editor_add_line(editor, (AfVec2){start + 141.0F, 14.0F}, (AfVec2){start + 153.0F, 26.0F},
                       3.0F, (AfColor){0.78F, 0.42F, 0.96F, 1.0F}, 100120);
    af_editor_add_text(editor, start + 5.0F, 30.0F, "RECT", 0.65F, 26.0F,
                       (AfColor){0.8F, 0.82F, 0.88F, 1.0F}, 100135);
    af_editor_add_text(editor, start + 45.0F, 30.0F, "ROUND", 0.58F, 28.0F,
                       (AfColor){0.8F, 0.82F, 0.88F, 1.0F}, 100135);
    af_editor_add_text(editor, start + 89.0F, 30.0F, "OVAL", 0.65F, 26.0F,
                       (AfColor){0.8F, 0.82F, 0.88F, 1.0F}, 100135);
    af_editor_add_text(editor, start + 130.0F, 30.0F, "PATH", 0.65F, 27.0F,
                       (AfColor){0.8F, 0.82F, 0.88F, 1.0F}, 100135);
    af_editor_add_rect(editor, start + 172.0F, 12.0F, 48.0F, 5.0F,
                       editor->grid_visible ? (AfColor){0.35F, 0.7F, 0.98F, 1.0F}
                                            : (AfColor){0.4F, 0.42F, 0.48F, 1.0F},
                       100120);
    af_editor_add_rect(editor, start + 172.0F, 25.0F, 48.0F, 5.0F,
                       editor->snapping ? (AfColor){0.96F, 0.55F, 0.25F, 1.0F}
                                        : (AfColor){0.4F, 0.42F, 0.48F, 1.0F},
                       100120);
    af_editor_add_text(editor, start + 225.0F, 10.0F, "GRID", 0.8F, 34.0F,
                       (AfColor){0.69F, 0.72F, 0.8F, 1.0F}, 100130);
    af_editor_add_text(editor, start + 225.0F, 24.0F, "SNAP", 0.8F, 34.0F,
                       (AfColor){0.69F, 0.72F, 0.8F, 1.0F}, 100130);
}

static void af_editor_render_color_picker(AfEditorState *editor, float x,
                                          const AfSceneNode *selected) {
    const AfColor color = selected == NULL ? (AfColor){0.5F, 0.5F, 0.5F, 1.0F} : selected->fill;
    float hue = editor->color_hue;
    float saturation = 0.0F;
    float value = 0.0F;
    char first_values[32];
    char second_values[32];
    af_editor_color_hsv(color, &hue, &saturation, &value);
    if (saturation > 0.0001F) {
        editor->color_hue = hue;
    } else {
        hue = editor->color_hue;
    }
    af_editor_add_text(editor, x, 58.0F, "COLOR PICKER", 0.9F, 100.0F,
                       (AfColor){0.61F, 0.65F, 0.73F, 1.0F}, 100125);
    af_editor_add_text(editor, x + 106.0F, 59.0F, "SV / HUE / ALPHA", 0.55F, 118.0F,
                       (AfColor){0.46F, 0.5F, 0.58F, 1.0F}, 100125);
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
    (void)snprintf(first_values, sizeof(first_values), "R%03u G%03u",
                   (uint32_t)roundf(color.r * 255.0F), (uint32_t)roundf(color.g * 255.0F));
    (void)snprintf(second_values, sizeof(second_values), "B%03u A%03u",
                   (uint32_t)roundf(color.b * 255.0F), (uint32_t)roundf(color.a * 255.0F));
    af_editor_add_text(editor, x + 176.0F, 121.0F, first_values, 0.58F, 56.0F,
                       (AfColor){0.69F, 0.72F, 0.8F, 1.0F}, 100130);
    af_editor_add_text(editor, x + 176.0F, 140.0F, second_values, 0.58F, 56.0F,
                       (AfColor){0.69F, 0.72F, 0.8F, 1.0F}, 100130);
    af_editor_add_text(editor, x + 176.0F, 173.0F, "DRAG", 0.58F, 48.0F,
                       (AfColor){0.46F, 0.5F, 0.58F, 1.0F}, 100130);
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
    const AfSceneNode *selected = af_scene_find_const(&editor->project.scene, editor->selected_id);
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
    af_editor_render_color_picker(editor, x, selected);
    for (index = 0U; index < 4U; ++index) {
        const float y =
            AF_EDITOR_INSPECTOR_SCALAR_Y + (float)index * AF_EDITOR_INSPECTOR_SCALAR_STEP;
        char value[64];
        af_editor_add_rect(editor, x, y, 30.0F, 26.0F, (AfColor){0.24F, 0.26F, 0.32F, 1.0F},
                           100120);
        af_editor_add_rect(editor, x + 40.0F, y, 30.0F, 26.0F, (AfColor){0.24F, 0.26F, 0.32F, 1.0F},
                           100120);
        af_editor_add_rect(editor, x + 8.0F, y + 12.0F, 14.0F, 2.0F,
                           (AfColor){0.75F, 0.78F, 0.84F, 1.0F}, 100130);
        af_editor_add_rect(editor, x + 48.0F, y + 12.0F, 14.0F, 2.0F,
                           (AfColor){0.75F, 0.78F, 0.84F, 1.0F}, 100130);
        af_editor_add_rect(editor, x + 54.0F, y + 6.0F, 2.0F, 14.0F,
                           (AfColor){0.75F, 0.78F, 0.84F, 1.0F}, 100130);
        if (selected == NULL) {
            (void)snprintf(value, sizeof(value), "%s",
                           index == 0U   ? "OPACITY"
                           : index == 1U ? "RADIUS"
                           : index == 2U ? "LAYER"
                                         : "ROTATION");
        } else if (index == 0U) {
            (void)snprintf(value, sizeof(value), "OPACITY %.0f%%",
                           (double)(selected->opacity * 100.0F));
        } else if (index == 1U) {
            (void)snprintf(value, sizeof(value), "RADIUS %.1f", (double)selected->corner_radius);
        } else if (index == 2U) {
            (void)snprintf(value, sizeof(value), "LAYER %d", selected->layer_order);
        } else {
            (void)snprintf(value, sizeof(value), "ROTATION %.2f",
                           (double)selected->local_transform.rotation);
        }
        af_editor_add_text(editor, x + 82.0F, y + 8.0F, value, 1.0F, editor->right_width - 118.0F,
                           (AfColor){0.7F, 0.73F, 0.81F, 1.0F}, 100130);
    }
    if (selected != NULL) {
        const AfStyle *style = af_style_find_node_const(&editor->project.style_data, selected->id);
        const AfParticleEmitter *emitter =
            af_project_find_node_emitter(&editor->project, selected->id);
        af_editor_add_rect(editor, x, AF_EDITOR_STYLE_BAR_Y, editor->right_width - 36.0F, 12.0F,
                           (AfColor){0.18F, 0.2F, 0.24F, 1.0F}, 100120);
        af_editor_add_rect(editor, x, AF_EDITOR_STYLE_BAR_Y,
                           (editor->right_width - 36.0F) * selected->opacity, 12.0F, selected->fill,
                           100130);
        for (index = 0U; index < 6U; ++index) {
            static const char *const style_labels[] = {"ON", "-W", "+W", "FILL", "CAP", "JOIN"};
            static const char *const layout_labels[] = {"ANCH", "WIDTH", "HEIGHT",
                                                        "FLOW", "ALIGN", "ASPECT"};
            AfColor button = {0.24F, 0.26F, 0.32F, 1.0F};
            if (style != NULL && ((index == 0U && style->stroke.enabled) ||
                                  (index == 3U && style->fill.type != AF_PAINT_SOLID))) {
                button = (AfColor){0.22F, 0.58F, 0.96F, 1.0F};
            }
            af_editor_add_rect(editor, x + (float)index * 38.0F, AF_EDITOR_STYLE_ROW_Y, 32.0F,
                               28.0F, button, 100120);
            af_editor_add_rect(editor, x + (float)index * 38.0F, AF_EDITOR_LAYOUT_ROW_Y, 32.0F,
                               28.0F, (AfColor){0.28F, 0.3F, 0.37F, 1.0F}, 100120);
            af_editor_add_text(editor, x + (float)index * 38.0F + 4.0F,
                               AF_EDITOR_STYLE_ROW_Y + 9.0F, style_labels[index], 0.65F, 26.0F,
                               (AfColor){0.82F, 0.84F, 0.9F, 1.0F}, 100135);
            af_editor_add_text(editor, x + (float)index * 38.0F + 3.0F,
                               AF_EDITOR_LAYOUT_ROW_Y + 9.0F, layout_labels[index], 0.58F, 27.0F,
                               (AfColor){0.82F, 0.84F, 0.9F, 1.0F}, 100135);
        }
        af_editor_add_text(editor, x, AF_EDITOR_STYLE_LABEL_Y, "STYLE", 0.8F, 48.0F,
                           (AfColor){0.55F, 0.59F, 0.68F, 1.0F}, 100125);
        af_editor_add_text(editor, x + 62.0F, AF_EDITOR_STYLE_LABEL_Y, "LAYOUT CONTROLS BELOW",
                           0.55F, editor->right_width - 98.0F, (AfColor){0.45F, 0.49F, 0.58F, 1.0F},
                           100125);
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
                af_editor_add_rect(editor, page_x, AF_EDITOR_PARTICLE_PAGE_Y, 29.0F, 23.0F,
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
                af_editor_add_rect(editor, x, row_y, 28.0F, 24.0F,
                                   (AfColor){0.24F, 0.26F, 0.32F, 1.0F}, 100120);
                af_editor_add_rect(editor, x + 34.0F, row_y, 28.0F, 24.0F,
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
