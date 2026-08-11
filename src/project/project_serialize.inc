static AfResult af_project_writer_append(AfProjectTextWriter *writer, const char *format, ...) {
    int written = 0;
    va_list arguments;
    if (writer == NULL || format == NULL || writer->length >= writer->capacity) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    va_start(arguments, format);
    written = vsnprintf(writer->data + writer->length, writer->capacity - writer->length, format,
                        arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= writer->capacity - writer->length) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    writer->length += (size_t)written;
    return AF_RESULT_SUCCESS;
}

static const char *af_project_type_name(AfNodeType type) {
    static const char *const names[] = {"group", "rectangle", "rounded_rectangle", "ellipse",
                                        "path"};
    return type <= AF_NODE_PATH ? names[type] : "invalid";
}

static const char *af_project_property_name(AfPropertyType property) {
    static const char *const names[] = {"position",   "rotation",        "scale",
                                        "opacity",    "fill_color",      "corner_radius",
                                        "path_point", "path_control_in", "path_control_out"};
    return property <= AF_PROPERTY_PATH_CONTROL_OUT ? names[property] : "invalid";
}

static const char *af_project_interpolation_name(AfInterpolation interpolation) {
    static const char *const names[] = {"step", "linear", "cubic_bezier"};
    return interpolation <= AF_INTERPOLATION_CUBIC_BEZIER ? names[interpolation] : "invalid";
}

AfResult af_project_serialize(const AfProject *project, AfAllocator *allocator,
                              AfPackageBlob *out_text) {
    size_t capacity = 0U;
    size_t animation_capacity = 0U;
    AfProjectTextWriter writer = {0};
    uint32_t index = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (allocator == NULL || out_text == NULL ||
        af_project_validate(project) != AF_RESULT_SUCCESS ||
        af_size_mul_overflow(project->scene.count, 640U, &capacity) ||
        af_size_mul_overflow(project->clip_count, 256U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->track_count, 256U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->keyframe_count, 384U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->path_data.path_count, 192U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->path_data.command_count, 192U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->style_data.count, 3072U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->layout_data.count, 512U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->particle_data.count, 2048U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_add_overflow(capacity, 256U, &capacity)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_text = (AfPackageBlob){0};
    writer.data = af_allocate(allocator, capacity, alignof(char));
    if (writer.data == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    writer.capacity = capacity;
    result = af_project_writer_append(
        &writer,
        "arcflow_project %u\ncanvas %.9g %.9g\nnext_id %llu\nnext_animation_id %llu\n"
        "layout_canvas %.9g %.9g %.9g %.9g %u %.9g %.9g %.9g %.9g\n"
        "vfx_lod %u %.9g %.9g %.9g %.9g %u\n",
        AF_PROJECT_FORMAT_VERSION, project->canvas_size.x, project->canvas_size.y,
        (unsigned long long)project->next_id, (unsigned long long)project->next_animation_id,
        project->layout_data.reference_size.x, project->layout_data.reference_size.y,
        project->layout_data.target_size.x, project->layout_data.target_size.y,
        (uint32_t)project->layout_data.scale_mode, project->layout_data.safe_area.left,
        project->layout_data.safe_area.top, project->layout_data.safe_area.right,
        project->layout_data.safe_area.bottom, project->particle_data.lod.maximum_particles,
        project->particle_data.lod.spawn_rate_scale,
        project->particle_data.lod.simulation_frequency_scale,
        project->particle_data.lod.distance_start, project->particle_data.lod.distance_end,
        project->particle_data.lod.renderer_enabled ? 1U : 0U);
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->scene.count; ++index) {
        const AfSceneNode *node = &project->scene.nodes[index];
        char parent[32] = {0};
        if (node->parent_id == AF_INVALID_NODE_ID) {
            (void)snprintf(parent, sizeof(parent), "none");
        } else {
            (void)snprintf(parent, sizeof(parent), "%llu", (unsigned long long)node->parent_id);
        }
        result = af_project_writer_append(
            &writer,
            "node\nid %llu\nparent %s\ntype %s\nname %s\nposition %.9g %.9g\nrotation "
            "%.9g\nscale %.9g %.9g\nbounds %.9g %.9g\nfill %.9g %.9g %.9g %.9g\nopacity "
            "%.9g\ncorner_radius %.9g\nlayer %d\nvisible %u\nend\n",
            (unsigned long long)node->id, parent, af_project_type_name(node->type),
            project->names[index], node->local_transform.position.x,
            node->local_transform.position.y, node->local_transform.rotation,
            node->local_transform.scale.x, node->local_transform.scale.y, node->bounds.x,
            node->bounds.y, node->fill.r, node->fill.g, node->fill.b, node->fill.a, node->opacity,
            node->corner_radius, node->layer_order, node->visible ? 1U : 0U);
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->style_data.count; ++index) {
        const AfStyle *style = &project->style_data.styles[index];
        const AfPaint *paints[2] = {&style->fill, &style->stroke_paint};
        result = af_project_writer_append(
            &writer,
            "style %llu %llu %llu %u %.9g %.9g %u %.9g %.9g %u %u %.9g %u %.9g %llu "
            "%llu",
            (unsigned long long)style->id, (unsigned long long)style->node_id,
            (unsigned long long)style->parent_style_id, style->inherit_flags, style->fill_opacity,
            style->stroke_opacity, style->stroke.enabled ? 1U : 0U, style->stroke.width,
            style->stroke.end_width, (uint32_t)style->stroke.cap, (uint32_t)style->stroke.join,
            style->stroke.miter_limit, style->stroke.dash_count, style->stroke.dash_offset,
            (unsigned long long)style->paint_generation,
            (unsigned long long)style->stroke_geometry_generation);
        for (uint32_t dash = 0U; result == AF_RESULT_SUCCESS && dash < style->stroke.dash_count;
             ++dash) {
            result = af_project_writer_append(&writer, " %.9g", style->stroke.dashes[dash]);
        }
        for (uint32_t paint_index = 0U; result == AF_RESULT_SUCCESS && paint_index < 2U;
             ++paint_index) {
            const AfPaint *paint = paints[paint_index];
            result = af_project_writer_append(
                &writer,
                " %u %u %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g "
                "%.9g %.9g %.9g %.9g %.9g",
                (uint32_t)paint->type, paint->stop_count, paint->solid.r, paint->solid.g,
                paint->solid.b, paint->solid.a, paint->start.x, paint->start.y, paint->end.x,
                paint->end.y, paint->start_radius, paint->end_radius, paint->angle,
                paint->transform.m00, paint->transform.m01, paint->transform.m10,
                paint->transform.m11, paint->transform.tx, paint->transform.ty);
            for (uint32_t stop = 0U; result == AF_RESULT_SUCCESS && stop < paint->stop_count;
                 ++stop) {
                const AfGradientStop *gradient_stop = &paint->stops[stop];
                result = af_project_writer_append(&writer, " %.9g %.9g %.9g %.9g %.9g",
                                                  gradient_stop->offset, gradient_stop->color.r,
                                                  gradient_stop->color.g, gradient_stop->color.b,
                                                  gradient_stop->color.a);
            }
        }
        if (result == AF_RESULT_SUCCESS) {
            result = af_project_writer_append(&writer, "\n");
        }
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->layout_data.count; ++index) {
        const AfLayoutNode *layout = &project->layout_data.nodes[index];
        result = af_project_writer_append(
            &writer,
            "layout %llu %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %u %.9g %u %.9g "
            "%.9g %.9g %.9g %.9g %.9g %u %u %u %.9g %.9g %.9g %.9g %.9g\n",
            (unsigned long long)layout->node_id, layout->anchor_min.x, layout->anchor_min.y,
            layout->anchor_max.x, layout->anchor_max.y, layout->pivot.x, layout->pivot.y,
            layout->offset.x, layout->offset.y, (uint32_t)layout->width.mode, layout->width.value,
            (uint32_t)layout->height.mode, layout->height.value, layout->minimum_size.x,
            layout->minimum_size.y, layout->maximum_size.x, layout->maximum_size.y,
            layout->aspect_ratio, (uint32_t)layout->aspect, (uint32_t)layout->direction,
            (uint32_t)layout->child_alignment, layout->spacing, layout->padding.left,
            layout->padding.top, layout->padding.right, layout->padding.bottom);
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->particle_data.count; ++index) {
        const AfParticleEmitter *emitter = &project->particle_data.emitters[index];
        result = af_project_writer_append(
            &writer,
            "particle %llu %llu %llu %u %u %u %u %.9g %u "
            "%.9g %.9g %.9g %.9g %.9g %.9g "
            "%.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g "
            "%.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g "
            "%.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g "
            "%.9g %u %u %.9g %u\n",
            (unsigned long long)emitter->id, (unsigned long long)emitter->node_id,
            (unsigned long long)emitter->path_id, (uint32_t)emitter->type,
            (uint32_t)emitter->output, emitter->capacity, emitter->seed, emitter->spawn_rate,
            emitter->burst_count, emitter->lifetime_min, emitter->lifetime_max,
            emitter->position_min.x, emitter->position_min.y, emitter->position_max.x,
            emitter->position_max.y, emitter->velocity_min.x, emitter->velocity_min.y,
            emitter->velocity_max.x, emitter->velocity_max.y, emitter->rotation_min,
            emitter->rotation_max, emitter->angular_velocity_min, emitter->angular_velocity_max,
            emitter->size_start_min, emitter->size_start_max, emitter->size_end_min,
            emitter->size_end_max, emitter->color_start.r, emitter->color_start.g,
            emitter->color_start.b, emitter->color_start.a, emitter->color_end.r,
            emitter->color_end.g, emitter->color_end.b, emitter->color_end.a, emitter->gravity.x,
            emitter->gravity.y, emitter->wind.x, emitter->wind.y, emitter->drag,
            emitter->flipbook_columns, emitter->flipbook_rows, emitter->flipbook_frames_per_second,
            emitter->enabled ? 1U : 0U);
        if (result == AF_RESULT_SUCCESS) {
            result = af_project_writer_append(
                &writer,
                "vfx_emitter %llu "
                "%.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g "
                "%.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g "
                "%.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g "
                "%u %u %llu %u %u %u %u\n",
                (unsigned long long)emitter->id, emitter->turbulence_strength,
                emitter->turbulence_frequency, emitter->force_center.x, emitter->force_center.y,
                emitter->radial_force, emitter->vortex_force, emitter->attractor_position.x,
                emitter->attractor_position.y, emitter->attractor_strength,
                emitter->attractor_radius, emitter->repulsor_position.x,
                emitter->repulsor_position.y, emitter->repulsor_strength, emitter->repulsor_radius,
                emitter->vector_field_direction.x, emitter->vector_field_direction.y,
                emitter->vector_field_strength, emitter->collision_plane_normal.x,
                emitter->collision_plane_normal.y, emitter->collision_plane_offset,
                emitter->collision_restitution, emitter->collision_friction, emitter->depth_min,
                emitter->depth_max, emitter->depth_velocity_min, emitter->depth_velocity_max,
                emitter->trail_length, emitter->trail_width, emitter->mesh_scale,
                emitter->decal_rotation_offset, emitter->distortion_strength,
                emitter->soft_fade_distance, emitter->light_radius,
                emitter->collision_plane_enabled ? 1U : 0U,
                emitter->depth_collision_enabled ? 1U : 0U,
                (unsigned long long)emitter->sub_emitter_id, emitter->sub_emit_birth_count,
                emitter->sub_emit_death_count, emitter->max_recursion_depth,
                emitter->event_burst_count);
        }
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->path_data.path_count;
         ++index) {
        const AfPath *path = &project->path_data.paths[index];
        result = af_project_writer_append(
            &writer, "path\nid %llu\nnode %llu\nfill_rule %s\ngeneration %llu\n",
            (unsigned long long)path->id, (unsigned long long)path->node_id,
            path->fill_rule == AF_PATH_FILL_EVEN_ODD ? "even_odd" : "non_zero",
            (unsigned long long)path->geometry_generation);
        for (uint32_t command_index = 0U;
             result == AF_RESULT_SUCCESS && command_index < path->command_count; ++command_index) {
            const AfPathCommand *command =
                &project->path_data.commands[path->command_start + command_index];
            if (command->type == AF_PATH_MOVE) {
                result = af_project_writer_append(&writer, "move %.9g %.9g\n", command->point.x,
                                                  command->point.y);
            } else if (command->type == AF_PATH_LINE) {
                result = af_project_writer_append(&writer, "line %.9g %.9g\n", command->point.x,
                                                  command->point.y);
            } else if (command->type == AF_PATH_CUBIC) {
                result = af_project_writer_append(&writer, "cubic %.9g %.9g %.9g %.9g %.9g %.9g\n",
                                                  command->control1.x, command->control1.y,
                                                  command->control2.x, command->control2.y,
                                                  command->point.x, command->point.y);
            } else {
                result = af_project_writer_append(&writer, "close\n");
            }
        }
        if (result == AF_RESULT_SUCCESS) {
            result = af_project_writer_append(&writer, "end_path\n");
        }
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->clip_count; ++index) {
        const AfProjectClip *clip = &project->clips[index];
        result = af_project_writer_append(
            &writer,
            "clip\nid %llu\nname %s\nduration %.9g\nloop %u\nloop_region %.9g %.9g\nend_clip\n",
            (unsigned long long)clip->id, clip->name, clip->duration, clip->loop ? 1U : 0U,
            clip->loop_start, clip->loop_end);
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->track_count; ++index) {
        const AfProjectTrack *track = &project->tracks[index];
        result = af_project_writer_append(
            &writer, "track\nid %llu\nclip %llu\nnode %llu\nproperty %s\nelement %u\nend_track\n",
            (unsigned long long)track->id, (unsigned long long)track->clip_id,
            (unsigned long long)track->node_id, af_project_property_name(track->property),
            track->element_index);
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->keyframe_count; ++index) {
        const AfProjectKeyframe *key = &project->keyframes[index];
        result = af_project_writer_append(
            &writer,
            "keyframe\nid %llu\ntrack %llu\ntime %.9g\nvalue %.9g %.9g %.9g %.9g\n"
            "interpolation %s\neasing %.9g %.9g %.9g %.9g\nend_keyframe\n",
            (unsigned long long)key->id, (unsigned long long)key->track_id, key->keyframe.time,
            key->keyframe.value[0], key->keyframe.value[1], key->keyframe.value[2],
            key->keyframe.value[3], af_project_interpolation_name(key->keyframe.interpolation),
            key->keyframe.easing[0], key->keyframe.easing[1], key->keyframe.easing[2],
            key->keyframe.easing[3]);
    }
    if (result != AF_RESULT_SUCCESS) {
        af_deallocate(allocator, writer.data);
        return result;
    }
    out_text->data = (uint8_t *)writer.data;
    out_text->size = writer.length;
    return AF_RESULT_SUCCESS;
}
