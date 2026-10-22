AfResult af_project_deserialize_versioned(const void *data, size_t size, AfAllocator *allocator,
                                          uint32_t *out_source_format_version,
                                          AfProject *out_project) {
    char *text = NULL;
    char *cursor = NULL;
    char *line = NULL;
    AfProject project = {0};
    AfProjectNodeSnapshot node = {0};
    AfProjectClipSnapshot clip = {0};
    AfProjectTrackSnapshot track = {0};
    AfProjectKeyframeSnapshot keyframe = {0};
    AfProjectPathSnapshot path = {0};
    AfPathCommand path_commands[AF_MAX_PATH_COMMANDS_PER_PATH];
    uint32_t path_command_count = 0U;
    uint32_t fields = 0U;
    uint32_t node_count = 0U;
    bool in_node = false;
    bool in_clip = false;
    bool in_track = false;
    bool in_keyframe = false;
    bool in_path = false;
    bool header_seen = false;
    bool canvas_seen = false;
    bool layout_canvas_seen = false;
    bool vfx_lod_seen = false;
    bool next_id_seen = false;
    bool next_animation_id_seen = false;
    bool next_asset_id_seen = false;
    bool metadata_seen = false;
    uint64_t parsed_next_id = 0U;
    uint64_t parsed_next_animation_id = 0U;
    uint64_t parsed_next_asset_id = 0U;
    uint64_t format_version = 0U;
    uint64_t binding_property_id = 0U;
    uint32_t binding_targets_remaining = 0U;
    AfResult result = AF_RESULT_INVALID_PACKAGE;
    if (out_source_format_version != NULL) {
        *out_source_format_version = 0U;
    }
    if (data == NULL || size == 0U || size > AF_PROJECT_MAX_FILE_SIZE || allocator == NULL ||
        out_project == NULL || memchr(data, '\0', size) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_project = (AfProject){0};
    text = af_allocate(allocator, size + 1U, alignof(char));
    if (text == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memcpy(text, data, size);
    text[size] = '\0';
    cursor = text;
    for (char *scan = text; *scan != '\0'; ++scan) {
        if (*scan == '\n' && node_count < AF_MAX_NODES) {
            ++node_count;
        }
    }
    node_count = node_count > AF_MAX_NODES ? AF_MAX_NODES : node_count;
    result = af_project_create(&project, allocator, node_count == 0U ? 1U : node_count);
    if (result != AF_RESULT_SUCCESS) {
        af_deallocate(allocator, text);
        return result;
    }
    result = AF_RESULT_INVALID_PACKAGE;
    while ((line = af_project_next_line(&cursor)) != NULL) {
        const char *value = NULL;
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        if (!header_seen) {
            value = af_project_value(line, "arcflow_project");
            if (value == NULL || !af_project_parse_u64(value, &format_version)) {
                goto cleanup;
            }
            if (out_source_format_version != NULL && format_version <= UINT32_MAX) {
                *out_source_format_version = (uint32_t)format_version;
            }
            if (format_version == 0U || format_version > AF_PROJECT_FORMAT_VERSION) {
                result = AF_RESULT_UNSUPPORTED_VERSION;
                goto cleanup;
            }
            header_seen = true;
            continue;
        }
        if (binding_targets_remaining != 0U) {
            if (!af_project_parse_binding_target_line(line, &project, binding_property_id)) {
                goto cleanup;
            }
            --binding_targets_remaining;
            continue;
        }
        if (strncmp(line, "binding_property ", 17U) == 0) {
            if (in_node || in_clip || in_track || in_keyframe || in_path ||
                !af_project_parse_binding_property_line(line, &project,
                                                        &binding_targets_remaining)) {
                goto cleanup;
            }
            binding_property_id =
                project.binding_data.properties[project.binding_data.property_count - 1U].id;
            continue;
        }
        if (strncmp(line, "binding_target ", 15U) == 0) {
            goto cleanup;
        }
        if (strncmp(line, "particle ", 9U) == 0) {
            AfParticleEmitter emitter = af_particle_emitter_default(1U, 1U);
            if (in_node || in_clip || in_track || in_keyframe || in_path ||
                project.particle_data.count >= project.particle_data.capacity ||
                !af_project_parse_particle_line(line, &emitter)) {
                goto cleanup;
            }
            project.particle_data.emitters[project.particle_data.count++] = emitter;
            continue;
        }
        if (strncmp(line, "image_asset ", 12U) == 0) {
            if (in_node || in_clip || in_track || in_keyframe || in_path ||
                !af_project_parse_image_asset_line(line, &project)) {
                goto cleanup;
            }
            continue;
        }
        if (strncmp(line, "image_sequence ", 15U) == 0) {
            if (in_node || in_clip || in_track || in_keyframe || in_path ||
                !af_project_parse_image_sequence_line(line, &project)) {
                goto cleanup;
            }
            continue;
        }
        if (strncmp(line, "image_binding ", 14U) == 0) {
            if (in_node || in_clip || in_track || in_keyframe || in_path ||
                !af_project_parse_image_binding_line(line, &project)) {
                goto cleanup;
            }
            continue;
        }
        if (strncmp(line, "vfx_emitter ", 12U) == 0) {
            AfParticleEmitter parsed = {0};
            AfParticleEmitter *emitter = NULL;
            uint64_t emitter_id = 0U;
            if (in_node || in_clip || in_track || in_keyframe || in_path ||
                !af_project_parse_vfx_emitter_line(line, &emitter_id, &parsed) ||
                (emitter = af_project_find_emitter(&project, emitter_id)) == NULL ||
                emitter->event_sequence != 0U) {
                goto cleanup;
            }
            emitter->turbulence_strength = parsed.turbulence_strength;
            emitter->turbulence_frequency = parsed.turbulence_frequency;
            emitter->force_center = parsed.force_center;
            emitter->radial_force = parsed.radial_force;
            emitter->vortex_force = parsed.vortex_force;
            emitter->attractor_position = parsed.attractor_position;
            emitter->attractor_strength = parsed.attractor_strength;
            emitter->attractor_radius = parsed.attractor_radius;
            emitter->repulsor_position = parsed.repulsor_position;
            emitter->repulsor_strength = parsed.repulsor_strength;
            emitter->repulsor_radius = parsed.repulsor_radius;
            emitter->vector_field_direction = parsed.vector_field_direction;
            emitter->vector_field_strength = parsed.vector_field_strength;
            emitter->collision_plane_normal = parsed.collision_plane_normal;
            emitter->collision_plane_offset = parsed.collision_plane_offset;
            emitter->collision_restitution = parsed.collision_restitution;
            emitter->collision_friction = parsed.collision_friction;
            emitter->depth_min = parsed.depth_min;
            emitter->depth_max = parsed.depth_max;
            emitter->depth_velocity_min = parsed.depth_velocity_min;
            emitter->depth_velocity_max = parsed.depth_velocity_max;
            emitter->trail_length = parsed.trail_length;
            emitter->trail_width = parsed.trail_width;
            emitter->mesh_scale = parsed.mesh_scale;
            emitter->decal_rotation_offset = parsed.decal_rotation_offset;
            emitter->distortion_strength = parsed.distortion_strength;
            emitter->soft_fade_distance = parsed.soft_fade_distance;
            emitter->light_radius = parsed.light_radius;
            emitter->sub_emitter_id = parsed.sub_emitter_id;
            emitter->sub_emit_birth_count = parsed.sub_emit_birth_count;
            emitter->sub_emit_death_count = parsed.sub_emit_death_count;
            emitter->max_recursion_depth = parsed.max_recursion_depth;
            emitter->event_burst_count = parsed.event_burst_count;
            emitter->collision_plane_enabled = parsed.collision_plane_enabled;
            emitter->depth_collision_enabled = parsed.depth_collision_enabled;
            emitter->event_sequence = 1U;
            continue;
        }
        if (strncmp(line, "vfx_lod ", 8U) == 0) {
            if (vfx_lod_seen || in_node || in_clip || in_track || in_keyframe || in_path ||
                !af_project_parse_vfx_lod_line(line, &project.particle_data.lod)) {
                goto cleanup;
            }
            vfx_lod_seen = true;
            continue;
        }
        if (strncmp(line, "layout_canvas ", 14U) == 0) {
            if (layout_canvas_seen || in_node || in_clip || in_track || in_keyframe || in_path ||
                !af_project_parse_layout_canvas(line, &project.layout_data)) {
                goto cleanup;
            }
            layout_canvas_seen = true;
            continue;
        }
        if (strncmp(line, "layout ", 7U) == 0) {
            AfLayoutNode parsed_layout = {0};
            if (in_node || in_clip || in_track || in_keyframe || in_path ||
                !af_project_parse_layout_line(line, &parsed_layout) ||
                af_layout_find_const(&project.layout_data, parsed_layout.node_id) != NULL ||
                af_project_set_layout(&project, &parsed_layout) != AF_RESULT_SUCCESS) {
                goto cleanup;
            }
            continue;
        }
        if (strncmp(line, "style ", 6U) == 0) {
            AfStyle parsed_style = {0};
            AfStyle *node_style = NULL;
            const AfStyle *id_style = NULL;
            if (in_node || in_clip || in_track || in_keyframe || in_path ||
                !af_project_parse_style_line(line, &parsed_style)) {
                goto cleanup;
            }
            id_style = af_style_find_const(&project.style_data, parsed_style.id);
            if (parsed_style.node_id == 0U) {
                if (id_style != NULL ||
                    af_project_add_style_preset(&project, &parsed_style) != AF_RESULT_SUCCESS) {
                    goto cleanup;
                }
            } else {
                node_style = af_project_find_node_style(&project, parsed_style.node_id);
                if (node_style == NULL || (id_style != NULL && id_style != node_style)) {
                    goto cleanup;
                }
                *node_style = parsed_style;
            }
            continue;
        }
        if (strcmp(line, "node") == 0) {
            if (in_node || in_clip || in_track || in_keyframe || in_path ||
                project.scene.count >= project.scene.capacity) {
                goto cleanup;
            }
            node = (AfProjectNodeSnapshot){.index = project.scene.count};
            fields = 0U;
            in_node = true;
            continue;
        }
        if (strcmp(line, "end") == 0) {
            const uint32_t required_fields = format_version >= 12U
                                                 ? AF_PROJECT_REQUIRED_FIELDS
                                                 : AF_PROJECT_LEGACY_REQUIRED_FIELDS;
            if (!in_node ||
                (fields & ~(uint32_t)(AF_PROJECT_FIELD_POLYGON | AF_PROJECT_FIELD_TEXT |
                                      AF_PROJECT_FIELD_TEXT_UTF8 | AF_PROJECT_FIELD_TEXT_FORMAT |
                                      AF_PROJECT_FIELD_TEXT_FONT)) != required_fields ||
                ((fields & AF_PROJECT_FIELD_TEXT_FONT) &&
                 (format_version < 15U || !(fields & AF_PROJECT_FIELD_TEXT))) ||
                ((fields & (AF_PROJECT_FIELD_TEXT | AF_PROJECT_FIELD_TEXT_UTF8 |
                            AF_PROJECT_FIELD_TEXT_FORMAT)) != 0U &&
                 (format_version < 14U ||
                  (fields & (AF_PROJECT_FIELD_TEXT | AF_PROJECT_FIELD_TEXT_UTF8 |
                             AF_PROJECT_FIELD_TEXT_FORMAT)) !=
                      (AF_PROJECT_FIELD_TEXT | AF_PROJECT_FIELD_TEXT_UTF8 |
                       AF_PROJECT_FIELD_TEXT_FORMAT))) ||
                (node.text.guide_count != 0U && node.text.mode != AF_PROJECT_TEXT_ON_PATH) ||
                (format_version < 13U && (fields & AF_PROJECT_FIELD_POLYGON) != 0U) ||
                af_project_insert_snapshot(&project, &node) != AF_RESULT_SUCCESS) {
                goto cleanup;
            }
            in_node = false;
            continue;
        }
        if (in_node) {
            if (!af_project_parse_node_field(line, &node, &fields)) {
                goto cleanup;
            }
            continue;
        }
        if (strcmp(line, "path") == 0) {
            if (in_clip || in_track || in_keyframe || in_path ||
                project.path_data.path_count >= project.path_data.path_capacity) {
                goto cleanup;
            }
            path = (AfProjectPathSnapshot){.index = project.path_data.path_count};
            path.path.command_start = project.path_data.command_count;
            path_command_count = 0U;
            fields = 0U;
            in_path = true;
            continue;
        }
        if (strcmp(line, "end_path") == 0) {
            path.path.command_count = path_command_count;
            if (!in_path || fields != AF_PROJECT_PATH_REQUIRED || path_command_count == 0U ||
                af_project_insert_path_snapshot(&project, &path, path_commands) !=
                    AF_RESULT_SUCCESS) {
                goto cleanup;
            }
            in_path = false;
            continue;
        }
        if (in_path) {
            if (!af_project_parse_path_line(line, &path, path_commands, &path_command_count,
                                            &fields)) {
                goto cleanup;
            }
            continue;
        }
        if (strcmp(line, "clip") == 0) {
            if (in_clip || in_track || in_keyframe || in_path ||
                project.clip_count >= project.clip_capacity) {
                goto cleanup;
            }
            clip = (AfProjectClipSnapshot){.index = project.clip_count};
            fields = 0U;
            in_clip = true;
            continue;
        }
        if (strcmp(line, "end_clip") == 0) {
            if (!in_clip || (fields & AF_PROJECT_CLIP_REQUIRED) != AF_PROJECT_CLIP_REQUIRED ||
                af_project_insert_clip_snapshot(&project, &clip) != AF_RESULT_SUCCESS) {
                goto cleanup;
            }
            in_clip = false;
            continue;
        }
        if (in_clip) {
            if (!af_project_parse_clip_field(line, &clip, &fields)) {
                goto cleanup;
            }
            continue;
        }
        if (strcmp(line, "track") == 0) {
            if (in_track || in_keyframe || in_path ||
                project.track_count >= project.track_capacity) {
                goto cleanup;
            }
            track = (AfProjectTrackSnapshot){.index = project.track_count};
            fields = 0U;
            in_track = true;
            continue;
        }
        if (strcmp(line, "end_track") == 0) {
            if (!in_track || (fields & AF_PROJECT_TRACK_REQUIRED) != AF_PROJECT_TRACK_REQUIRED ||
                af_project_insert_track_snapshot(&project, &track) != AF_RESULT_SUCCESS) {
                goto cleanup;
            }
            in_track = false;
            continue;
        }
        if (in_track) {
            if (!af_project_parse_track_field(line, &track, &fields)) {
                goto cleanup;
            }
            continue;
        }
        if (strcmp(line, "keyframe") == 0) {
            if (in_keyframe || in_path || project.keyframe_count >= project.keyframe_capacity) {
                goto cleanup;
            }
            keyframe = (AfProjectKeyframeSnapshot){.index = project.keyframe_count};
            fields = 0U;
            in_keyframe = true;
            continue;
        }
        if (strcmp(line, "end_keyframe") == 0) {
            if (!in_keyframe || fields != AF_PROJECT_KEY_REQUIRED ||
                af_project_insert_keyframe_snapshot(&project, &keyframe) != AF_RESULT_SUCCESS) {
                goto cleanup;
            }
            in_keyframe = false;
            continue;
        }
        if (in_keyframe) {
            if (!af_project_parse_keyframe_field(line, &keyframe, &fields)) {
                goto cleanup;
            }
            continue;
        }
        if ((value = af_project_value(line, "canvas")) != NULL) {
            float values[2] = {0};
            if (canvas_seen || !af_project_parse_floats(value, values, 2U)) {
                goto cleanup;
            }
            project.canvas_size = (AfVec2){values[0], values[1]};
            canvas_seen = true;
        } else if ((value = af_project_value(line, "metadata")) != NULL) {
            unsigned long long revision = 0ULL;
            char trailing = '\0';
            if (metadata_seen || sscanf(value, "%63s %llu %c", project.metadata.project_id,
                                        &revision, &trailing) != 2) {
                goto cleanup;
            }
            project.metadata.revision = (uint64_t)revision;
            project.metadata.change_count = 0U;
            metadata_seen = true;
        } else if ((value = af_project_value(line, "change")) != NULL) {
            AfProjectChange *change = NULL;
            unsigned long long revision = 0ULL;
            char trailing = '\0';
            if (!metadata_seen || project.metadata.change_count >= AF_PROJECT_MAX_CHANGES) {
                goto cleanup;
            }
            change = &project.metadata.changes[project.metadata.change_count];
            if (sscanf(value, "%llu %63s %63s %c", &revision, change->author, change->summary,
                       &trailing) != 3) {
                goto cleanup;
            }
            change->revision = (uint64_t)revision;
            ++project.metadata.change_count;
        } else if ((value = af_project_value(line, "next_id")) != NULL) {
            if (next_id_seen || !af_project_parse_u64(value, &parsed_next_id)) {
                goto cleanup;
            }
            next_id_seen = true;
        } else if ((value = af_project_value(line, "next_animation_id")) != NULL) {
            if (next_animation_id_seen || !af_project_parse_u64(value, &parsed_next_animation_id)) {
                goto cleanup;
            }
            next_animation_id_seen = true;
        } else if ((value = af_project_value(line, "next_asset_id")) != NULL) {
            if (next_asset_id_seen || !af_project_parse_u64(value, &parsed_next_asset_id)) {
                goto cleanup;
            }
            next_asset_id_seen = true;
        }
    }
    if (in_node || in_clip || in_track || in_keyframe || in_path ||
        binding_targets_remaining != 0U || !header_seen || !canvas_seen ||
        (format_version >= 11U && !metadata_seen)) {
        goto cleanup;
    }
    if (next_id_seen && parsed_next_id > project.next_id) {
        project.next_id = parsed_next_id;
    }
    if (next_animation_id_seen && parsed_next_animation_id > project.next_animation_id) {
        project.next_animation_id = parsed_next_animation_id;
    }
    if (next_asset_id_seen && parsed_next_asset_id > project.next_asset_id) {
        project.next_asset_id = parsed_next_asset_id;
    }
    if (!layout_canvas_seen) {
        project.layout_data.reference_size = project.canvas_size;
        project.layout_data.target_size = project.canvas_size;
    }
    for (uint32_t emitter_index = 0U; emitter_index < project.particle_data.count;
         ++emitter_index) {
        project.particle_data.emitters[emitter_index].event_sequence = 0U;
    }
    if (af_project_validate(&project) != AF_RESULT_SUCCESS ||
        af_scene_evaluate_world(&project.scene) != AF_RESULT_SUCCESS) {
        goto cleanup;
    }
    *out_project = project;
    out_project->scene.paths = &out_project->path_data;
    out_project->scene.styles = &out_project->style_data;
    out_project->scene.layouts = &out_project->layout_data;
    out_project->scene.particles = &out_project->particle_data;
    out_project->scene.images = &out_project->image_data;
    project = (AfProject){0};
    result = AF_RESULT_SUCCESS;

cleanup:
    af_project_destroy(&project);
    af_deallocate(allocator, text);
    return result;
}

AfResult af_project_deserialize(const void *data, size_t size, AfAllocator *allocator,
                                AfProject *out_project) {
    return af_project_deserialize_versioned(data, size, allocator, NULL, out_project);
}
