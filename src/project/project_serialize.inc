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
    static const char *const names[] = {"group",   "rectangle", "rounded_rectangle",
                                        "ellipse", "path",      "image"};
    return type <= AF_NODE_IMAGE ? names[type] : "invalid";
}

static const char *af_project_property_name(AfPropertyType property) {
    static const char *const names[] = {"position",        "rotation",        "scale", "opacity",
                                        "fill_color",      "corner_radius",   "skew",  "path_point",
                                        "path_control_in", "path_control_out"};
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
    uint32_t text_count = 0U;
    if (project != NULL && project->texts != NULL &&
        project->scene.count <= project->scene.capacity && project->scene.capacity <= AF_MAX_NODES)
        for (uint32_t i = 0; i < project->scene.count; ++i)
            text_count += project->texts[i].mode != AF_PROJECT_TEXT_NONE;
    if (allocator == NULL || out_text == NULL ||
        af_project_validate(project) != AF_RESULT_SUCCESS ||
        af_size_mul_overflow(project->scene.count, 640U, &capacity) ||
        af_size_mul_overflow(text_count, 8192U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
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
        af_size_mul_overflow(project->image_data.asset_count, 1024U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->image_data.binding_count, 256U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->image_data.sequence_count, 256U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->image_data.sequence_frame_count, 32U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->binding_data.property_count, 2048U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->binding_data.target_count, 192U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->metadata.change_count, 160U, &animation_capacity) ||
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
        "next_asset_id %llu\nmetadata %s %llu\n"
        "layout_canvas %.9g %.9g %.9g %.9g %u %.9g %.9g %.9g %.9g\n"
        "vfx_lod %u %.9g %.9g %.9g %.9g %u\n",
        AF_PROJECT_FORMAT_VERSION, project->canvas_size.x, project->canvas_size.y,
        (unsigned long long)project->next_id, (unsigned long long)project->next_animation_id,
        (unsigned long long)project->next_asset_id, project->metadata.project_id,
        (unsigned long long)project->metadata.revision, project->layout_data.reference_size.x,
        project->layout_data.reference_size.y, project->layout_data.target_size.x,
        project->layout_data.target_size.y, (uint32_t)project->layout_data.scale_mode,
        project->layout_data.safe_area.left, project->layout_data.safe_area.top,
        project->layout_data.safe_area.right, project->layout_data.safe_area.bottom,
        project->particle_data.lod.maximum_particles, project->particle_data.lod.spawn_rate_scale,
        project->particle_data.lod.simulation_frequency_scale,
        project->particle_data.lod.distance_start, project->particle_data.lod.distance_end,
        project->particle_data.lod.renderer_enabled ? 1U : 0U);
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->metadata.change_count;
         ++index) {
        const AfProjectChange *change = &project->metadata.changes[index];
        result = af_project_writer_append(&writer, "change %llu %s %s\n",
                                          (unsigned long long)change->revision, change->author,
                                          change->summary);
    }
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
            "%.9g\nscale %.9g %.9g\nskew %.9g %.9g\nbounds %.9g %.9g\nfill %.9g %.9g %.9g "
            "%.9g\nopacity "
            "%.9g\ncorner_radius %.9g\nlayer %d\nvisible %u\npolygon %u %.9g\n",
            (unsigned long long)node->id, parent, af_project_type_name(node->type),
            project->names[index], node->local_transform.position.x,
            node->local_transform.position.y, node->local_transform.rotation,
            node->local_transform.scale.x, node->local_transform.scale.y,
            node->local_transform.skew.x, node->local_transform.skew.y, node->bounds.x,
            node->bounds.y, node->fill.r, node->fill.g, node->fill.b, node->fill.a, node->opacity,
            node->corner_radius, node->layer_order, node->visible ? 1U : 0U, node->polygon_sides,
            node->polygon_inset);
        const AfProjectText *text = &project->texts[index];
        if (result == AF_RESULT_SUCCESS && text->mode != AF_PROJECT_TEXT_NONE) {
            result = af_project_writer_append(
                &writer, "text_settings %u %.9g %.9g %.9g\ntext_utf8 ", (unsigned int)text->mode,
                (double)text->font_size, (double)text->width, (double)text->offset);
            for (size_t i = 0; result == AF_RESULT_SUCCESS && text->utf8[i]; ++i)
                result = af_project_writer_append(&writer, "%02x",
                                                  (unsigned int)(unsigned char)text->utf8[i]);
            if (result == AF_RESULT_SUCCESS)
                result = af_project_writer_append(&writer, "\n");
            if (result == AF_RESULT_SUCCESS)
                result = af_project_writer_append(
                    &writer, "text_format %u %u %u %u %.9g %.9g\n", text->font_family,
                    text->bold ? 1U : 0U, text->italic ? 1U : 0U, text->alignment,
                    (double)text->letter_spacing, (double)text->line_spacing);
            for (uint32_t i = 0; result == AF_RESULT_SUCCESS && i < text->guide_count; ++i) {
                const AfPathCommand c = text->guide[i];
                result = af_project_writer_append(
                    &writer, "text_guide %u %.9g %.9g %.9g %.9g %.9g %.9g\n", (unsigned int)c.type,
                    (double)c.point.x, (double)c.point.y, (double)c.control1.x,
                    (double)c.control1.y, (double)c.control2.x, (double)c.control2.y);
            }
        }
        if (result == AF_RESULT_SUCCESS)
            result = af_project_writer_append(&writer, "end\n");
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
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->image_data.asset_count;
         ++index) {
        const AfImageAsset *asset = &project->image_data.assets[index];
        char encoded_path[AF_IMAGE_PATH_CAPACITY * 3U];
        char encoded_name[AF_IMAGE_NAME_CAPACITY * 3U];
        if (!af_project_image_encode_token(asset->source_path, encoded_path,
                                           sizeof(encoded_path)) ||
            !af_project_image_encode_token(asset->name, encoded_name, sizeof(encoded_name))) {
            result = AF_RESULT_INVALID_ARGUMENT;
        } else {
            result = af_project_writer_append(
                &writer,
                "image_asset %llu %llu %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %s "
                "%s\n",
                (unsigned long long)asset->id, (unsigned long long)asset->content_hash,
                (uint32_t)asset->source_format, (uint32_t)asset->color_space,
                (uint32_t)asset->compression, asset->width, asset->height, asset->trim_x,
                asset->trim_y, asset->trim_width, asset->trim_height, asset->sprite_borders.left,
                asset->sprite_borders.top, asset->sprite_borders.right,
                asset->sprite_borders.bottom, asset->trim_enabled ? 1U : 0U,
                asset->mipmaps_enabled ? 1U : 0U, encoded_path, encoded_name);
        }
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->image_data.sequence_count;
         ++index) {
        const AfImageSequence *sequence = &project->image_data.sequences[index];
        char encoded_name[AF_IMAGE_NAME_CAPACITY * 3U];
        if (!af_project_image_encode_token(sequence->name, encoded_name, sizeof(encoded_name))) {
            result = AF_RESULT_INVALID_ARGUMENT;
        } else {
            result = af_project_writer_append(&writer, "image_sequence %llu %.9g %u %s %u",
                                              (unsigned long long)sequence->id,
                                              sequence->frames_per_second, sequence->loop ? 1U : 0U,
                                              encoded_name, sequence->frame_count);
        }
        for (uint32_t frame = 0U; result == AF_RESULT_SUCCESS && frame < sequence->frame_count;
             ++frame) {
            result = af_project_writer_append(&writer, " %llu",
                                              (unsigned long long)project->image_data
                                                  .sequence_frames[sequence->frame_start + frame]);
        }
        if (result == AF_RESULT_SUCCESS) {
            result = af_project_writer_append(&writer, "\n");
        }
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->image_data.binding_count;
         ++index) {
        const AfImageBinding *binding = &project->image_data.bindings[index];
        result = af_project_writer_append(
            &writer, "image_binding %llu %llu %llu %u %u %u %u %u %.9g %u\n",
            (unsigned long long)binding->node_id, (unsigned long long)binding->asset_id,
            (unsigned long long)binding->sequence_id, (uint32_t)binding->display_mode,
            binding->borders.left, binding->borders.top, binding->borders.right,
            binding->borders.bottom, binding->frames_per_second, binding->loop ? 1U : 0U);
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->binding_data.property_count;
         ++index) {
        const AfExposedProperty *property = &project->binding_data.properties[index];
        float scalar = 0.0F;
        int32_t integer = 0;
        uint32_t boolean = 0U;
        AfColor color = {0};
        uint64_t resource = 0U;
        const char *text_value = "";
        char name[AF_BINDING_NAME_CAPACITY * 3U];
        char text[AF_BINDING_TEXT_CAPACITY * 3U];
        char prefix[AF_BINDING_AFFIX_CAPACITY * 3U];
        char suffix[AF_BINDING_AFFIX_CAPACITY * 3U];
        char singular[AF_BINDING_PLURAL_CAPACITY * 3U];
        char plural[AF_BINDING_PLURAL_CAPACITY * 3U];
        if (property->type == AF_BINDING_VALUE_SCALAR) {
            scalar = property->default_value.scalar;
        } else if (property->type == AF_BINDING_VALUE_INTEGER) {
            integer = property->default_value.integer;
        } else if (property->type == AF_BINDING_VALUE_BOOLEAN) {
            boolean = property->default_value.boolean ? 1U : 0U;
        } else if (property->type == AF_BINDING_VALUE_COLOR) {
            color = property->default_value.color;
        } else if (property->type == AF_BINDING_VALUE_TEXT) {
            text_value = property->default_value.text;
        } else if (property->type == AF_BINDING_VALUE_IMAGE) {
            resource = property->default_value.image_id;
        } else if (property->type == AF_BINDING_VALUE_VISIBILITY) {
            boolean = property->default_value.visibility ? 1U : 0U;
        } else if (property->type == AF_BINDING_VALUE_PROGRESS) {
            scalar = property->default_value.progress;
        } else if (property->type == AF_BINDING_VALUE_ANIMATION) {
            resource = property->default_value.animation_id;
        }
        if (!af_project_binding_encode_token(property->debug_name, name, sizeof(name)) ||
            !af_project_binding_encode_token(text_value, text, sizeof(text)) ||
            !af_project_binding_encode_token(property->format.prefix, prefix, sizeof(prefix)) ||
            !af_project_binding_encode_token(property->format.suffix, suffix, sizeof(suffix)) ||
            !af_project_binding_encode_token(property->format.singular, singular,
                                             sizeof(singular)) ||
            !af_project_binding_encode_token(property->format.plural, plural, sizeof(plural))) {
            result = AF_RESULT_INVALID_ARGUMENT;
        } else {
            result = af_project_writer_append(
                &writer,
                "binding_property %llu %u %u %u %u %.9g %d %u %.9g %.9g %.9g %.9g %llu "
                "%s %s %s %s %s %s\n",
                (unsigned long long)property->id, (uint32_t)property->type, property->target_count,
                (uint32_t)property->format.mode, property->format.precision, scalar, integer,
                boolean, color.r, color.g, color.b, color.a, (unsigned long long)resource, name,
                text, prefix, suffix, singular, plural);
        }
        for (uint32_t target_index = 0U;
             result == AF_RESULT_SUCCESS && target_index < property->target_count; ++target_index) {
            const AfBindingTarget *target =
                &project->binding_data.targets[property->target_start + target_index];
            result = af_project_writer_append(
                &writer, "binding_target %llu %u %u %llu %llu %u\n",
                (unsigned long long)property->id, (uint32_t)target->type,
                (uint32_t)target->property, (unsigned long long)target->object_id,
                (unsigned long long)target->member_id, target->text_capacity);
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
