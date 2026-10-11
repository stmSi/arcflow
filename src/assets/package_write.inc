AfResult af_package_write(const AfPackageSource *source, AfAllocator *allocator,
                          AfPackageBlob *out_blob) {
    uint64_t offsets[AF_SECTION_COUNT] = {0};
    uint64_t sizes[AF_SECTION_COUNT] = {0};
    uint64_t total_size = AF_PACKAGE_HEADER_SIZE + AF_SECTION_COUNT * AF_PACKAGE_SECTION_SIZE;
    uint8_t *bytes = NULL;
    uint32_t index = 0U;
    const AfPathData empty_paths = {0};
    const AfStyleData empty_styles = {0};
    const AfComponentData empty_components = {0};
    const AfStateMachineData empty_state_machines = {0};
    AfParticleData empty_particles = {0};
    const AfImageData empty_images = {0};
    const AfBindingData empty_bindings = {0};
    AfImagePackagePlan image_plan = {0};
    const AfLayoutData default_layouts = {.reference_size = {1280.0F, 720.0F},
                                          .target_size = {1280.0F, 720.0F},
                                          .scale_mode = AF_CANVAS_SCALE_FIT};
    const AfPathData *path_data = NULL;
    const AfStyleData *style_data = NULL;
    const AfLayoutData *layout_data = NULL;
    const AfComponentData *component_data = NULL;
    const AfStateMachineData *state_machine_data = NULL;
    const AfParticleData *particle_data = NULL;
    const AfImageData *image_data = NULL;
    const AfBindingData *binding_data = NULL;
    AfBindingValidationContext binding_context = {0};
    AfResult result = AF_RESULT_SUCCESS;
    if (source == NULL || source->scene == NULL || source->animation == NULL || allocator == NULL ||
        out_blob == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    empty_particles.lod = af_vfx_lod_default();
    *out_blob = (AfPackageBlob){0};
    path_data = source->scene->paths == NULL ? &empty_paths : source->scene->paths;
    style_data = source->scene->styles == NULL ? &empty_styles : source->scene->styles;
    layout_data = source->scene->layouts == NULL ? &default_layouts : source->scene->layouts;
    component_data = source->components == NULL ? &empty_components : source->components;
    state_machine_data =
        source->state_machines == NULL ? &empty_state_machines : source->state_machines;
    particle_data = source->scene->particles == NULL ? &empty_particles : source->scene->particles;
    image_data = source->scene->images == NULL ? &empty_images : source->scene->images;
    binding_data = source->bindings == NULL ? &empty_bindings : source->bindings;
    binding_context = (AfBindingValidationContext){.scene = source->scene,
                                                   .animation = source->animation,
                                                   .state_machines = state_machine_data,
                                                   .components = component_data,
                                                   .images = image_data,
                                                   .particles = particle_data};
    result = af_scene_validate(source->scene);
    if (result != AF_RESULT_SUCCESS ||
        af_animation_validate(source->animation, source->scene) != AF_RESULT_SUCCESS ||
        af_path_data_validate(path_data) != AF_RESULT_SUCCESS ||
        af_style_data_validate(style_data) != AF_RESULT_SUCCESS ||
        af_layout_validate(layout_data, source->scene) != AF_RESULT_SUCCESS ||
        af_component_validate(component_data, source->scene, source->animation) !=
            AF_RESULT_SUCCESS ||
        af_state_machine_validate(state_machine_data, source->animation, component_data) !=
            AF_RESULT_SUCCESS ||
        af_particle_data_validate(particle_data, source->scene) != AF_RESULT_SUCCESS ||
        (source->scene->images != NULL &&
         af_image_data_validate(image_data, false) != AF_RESULT_SUCCESS) ||
        (image_data->asset_count != 0U && image_data->atlas_count == 0U) ||
        af_binding_data_validate(binding_data, &binding_context) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_image_package_plan_create(image_data, allocator, &image_plan);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    sizes[0] = (uint64_t)source->scene->count * AF_NODE_RECORD_SIZE;
    sizes[1] = (uint64_t)source->animation->clip_count * AF_CLIP_RECORD_SIZE;
    sizes[2] = (uint64_t)source->animation->track_count * AF_TRACK_RECORD_SIZE;
    sizes[3] = (uint64_t)source->animation->keyframe_count * AF_KEYFRAME_RECORD_SIZE;
    sizes[4] = 1U;
    sizes[5] = (uint64_t)path_data->path_count * AF_PATH_RECORD_SIZE;
    sizes[6] = (uint64_t)path_data->command_count * AF_PATH_COMMAND_RECORD_SIZE;
    sizes[7] = (uint64_t)style_data->count * AF_STYLE_RECORD_SIZE;
    sizes[8] = AF_LAYOUT_HEADER_SIZE + (uint64_t)layout_data->count * AF_LAYOUT_RECORD_SIZE;
    sizes[9] = af_component_section_size(component_data);
    sizes[10] = af_state_machine_section_size(state_machine_data);
    sizes[11] = AF_PARTICLE_HEADER_SIZE + (uint64_t)particle_data->count * AF_PARTICLE_RECORD_SIZE;
    sizes[12] = af_image_section_size(image_data, &image_plan);
    sizes[13] = af_binding_section_size(binding_data);
    for (index = 0U; index < AF_SECTION_COUNT; ++index) {
        offsets[index] = af_align_u64(total_size, 8U);
        if (af_u64_add_overflow(offsets[index], sizes[index], &total_size) ||
            total_size > AF_PACKAGE_MAX_SIZE) {
            af_image_package_plan_destroy(&image_plan);
            return AF_RESULT_LIMIT_EXCEEDED;
        }
    }
    bytes = af_allocate(allocator, (size_t)total_size, 8U);
    if (bytes == NULL) {
        af_image_package_plan_destroy(&image_plan);
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memset(bytes, 0, (size_t)total_size);
    memcpy(bytes, AF_PACKAGE_MAGIC, 8U);
    af_write_u32(bytes + 8U, AF_PACKAGE_FORMAT_VERSION);
    af_write_u32(bytes + 12U, AF_RUNTIME_VERSION);
    af_write_u32(bytes + 16U, AF_PACKAGE_ENDIAN_MARKER);
    af_write_u32(bytes + 20U, AF_PACKAGE_HEADER_SIZE);
    af_write_u64(bytes + 24U, total_size);
    af_write_u64(bytes + 32U, AF_PACKAGE_HEADER_SIZE);
    af_write_u32(bytes + 40U, AF_SECTION_COUNT);
    af_write_u32(bytes + 44U, 0U);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE, AF_SECTION_NODES, offsets[0], sizes[0], 8U,
                     source->scene->count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + AF_PACKAGE_SECTION_SIZE, AF_SECTION_CLIPS,
                     offsets[1], sizes[1], 8U, source->animation->clip_count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 2U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_TRACKS, offsets[2], sizes[2], 8U, source->animation->track_count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 3U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_KEYFRAMES, offsets[3], sizes[3], 8U,
                     source->animation->keyframe_count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 4U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_STRINGS, offsets[4], sizes[4], 1U, 1U);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 5U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_PATHS, offsets[5], sizes[5], 8U, path_data->path_count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 6U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_PATH_COMMANDS, offsets[6], sizes[6], 8U, path_data->command_count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 7U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_STYLES, offsets[7], sizes[7], 8U, style_data->count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 8U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_LAYOUTS, offsets[8], sizes[8], 8U, layout_data->count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 9U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_COMPONENTS, offsets[9], sizes[9], 8U,
                     component_data->definition_count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 10U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_STATE_MACHINES, offsets[10], sizes[10], 8U,
                     state_machine_data->machine_count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 11U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_PARTICLES, offsets[11], sizes[11], 8U, particle_data->count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 12U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_IMAGES, offsets[12], sizes[12], 8U, image_data->asset_count);
    af_write_section(bytes + AF_PACKAGE_HEADER_SIZE + 13U * AF_PACKAGE_SECTION_SIZE,
                     AF_SECTION_BINDINGS, offsets[13], sizes[13], 8U, binding_data->property_count);
    for (index = 0U; index < source->scene->count; ++index) {
        const AfSceneNode *node = &source->scene->nodes[index];
        uint8_t *record = bytes + offsets[0] + (uint64_t)index * AF_NODE_RECORD_SIZE;
        af_write_u64(record, node->id);
        af_write_u64(record + 8U, node->parent_id);
        af_write_u32(record + 16U, (uint32_t)node->type);
        af_write_u32(record + 20U, node->visible ? 1U : 0U);
        af_write_f32(record + 24U, node->local_transform.position.x);
        af_write_f32(record + 28U, node->local_transform.position.y);
        af_write_f32(record + 32U, node->local_transform.rotation);
        af_write_f32(record + 36U, node->local_transform.scale.x);
        af_write_f32(record + 40U, node->local_transform.scale.y);
        af_write_f32(record + 44U, node->local_transform.skew.x);
        af_write_f32(record + 48U, node->local_transform.skew.y);
        af_write_f32(record + 52U, node->bounds.x);
        af_write_f32(record + 56U, node->bounds.y);
        af_write_f32(record + 60U, node->fill.r);
        af_write_f32(record + 64U, node->fill.g);
        af_write_f32(record + 68U, node->fill.b);
        af_write_f32(record + 72U, node->fill.a);
        af_write_f32(record + 76U, node->opacity);
        af_write_f32(record + 80U, node->corner_radius);
        af_write_u32(record + 84U, (uint32_t)node->layer_order);
    }
    for (index = 0U; index < source->animation->clip_count; ++index) {
        const AfAnimationClip *clip = &source->animation->clips[index];
        uint8_t *record = bytes + offsets[1] + (uint64_t)index * AF_CLIP_RECORD_SIZE;
        af_write_u64(record, clip->id);
        af_write_f32(record + 8U, clip->duration);
        af_write_u32(record + 12U, clip->track_start);
        af_write_u32(record + 16U, clip->track_count);
        af_write_u32(record + 20U, clip->loop ? 1U : 0U);
    }
    for (index = 0U; index < source->animation->track_count; ++index) {
        const AfAnimationTrack *track = &source->animation->tracks[index];
        uint8_t *record = bytes + offsets[2] + (uint64_t)index * AF_TRACK_RECORD_SIZE;
        af_write_u64(record, track->id);
        af_write_u64(record + 8U, track->node_id);
        af_write_u32(record + 16U, (uint32_t)track->property);
        af_write_u32(record + 20U, track->keyframe_start);
        af_write_u32(record + 24U, track->keyframe_count);
        af_write_u32(record + 28U, track->element_index);
    }
    for (index = 0U; index < source->animation->keyframe_count; ++index) {
        const AfKeyframe *key = &source->animation->keyframes[index];
        uint8_t *record = bytes + offsets[3] + (uint64_t)index * AF_KEYFRAME_RECORD_SIZE;
        uint32_t component = 0U;
        af_write_f32(record, key->time);
        for (component = 0U; component < 4U; ++component) {
            af_write_f32(record + 4U + component * 4U, key->value[component]);
            af_write_f32(record + 24U + component * 4U, key->easing[component]);
        }
        af_write_u32(record + 20U, (uint32_t)key->interpolation);
    }
    bytes[offsets[4]] = 0U;
    for (index = 0U; index < path_data->path_count; ++index) {
        const AfPath *path = &path_data->paths[index];
        uint8_t *record = bytes + offsets[5] + (uint64_t)index * AF_PATH_RECORD_SIZE;
        af_write_u64(record, path->id);
        af_write_u64(record + 8U, path->node_id);
        af_write_u32(record + 16U, path->command_start);
        af_write_u32(record + 20U, path->command_count);
        af_write_u32(record + 24U, (uint32_t)path->fill_rule);
        af_write_u64(record + 32U, path->geometry_generation);
    }
    for (index = 0U; index < path_data->command_count; ++index) {
        const AfPathCommand *command = &path_data->commands[index];
        uint8_t *record = bytes + offsets[6] + (uint64_t)index * AF_PATH_COMMAND_RECORD_SIZE;
        af_write_u32(record, (uint32_t)command->type);
        af_write_u32(record + 4U, (uint32_t)command->handle_mode);
        af_write_f32(record + 8U, command->point.x);
        af_write_f32(record + 12U, command->point.y);
        af_write_f32(record + 16U, command->control1.x);
        af_write_f32(record + 20U, command->control1.y);
        af_write_f32(record + 24U, command->control2.x);
        af_write_f32(record + 28U, command->control2.y);
    }
    for (index = 0U; index < style_data->count; ++index) {
        af_write_style(bytes + offsets[7] + (uint64_t)index * AF_STYLE_RECORD_SIZE,
                       &style_data->styles[index]);
    }
    af_write_f32(bytes + offsets[8], layout_data->reference_size.x);
    af_write_f32(bytes + offsets[8] + 4U, layout_data->reference_size.y);
    af_write_f32(bytes + offsets[8] + 8U, layout_data->target_size.x);
    af_write_f32(bytes + offsets[8] + 12U, layout_data->target_size.y);
    af_write_u32(bytes + offsets[8] + 16U, (uint32_t)layout_data->scale_mode);
    af_write_f32(bytes + offsets[8] + 20U, layout_data->safe_area.left);
    af_write_f32(bytes + offsets[8] + 24U, layout_data->safe_area.top);
    af_write_f32(bytes + offsets[8] + 28U, layout_data->safe_area.right);
    af_write_f32(bytes + offsets[8] + 32U, layout_data->safe_area.bottom);
    for (index = 0U; index < layout_data->count; ++index) {
        af_write_layout(bytes + offsets[8] + AF_LAYOUT_HEADER_SIZE +
                            (uint64_t)index * AF_LAYOUT_RECORD_SIZE,
                        &layout_data->nodes[index]);
    }
    af_write_components(bytes + offsets[9], component_data);
    af_write_state_machines(bytes + offsets[10], state_machine_data);
    {
        const AfVfxLodSettings lod =
            particle_data->lod.maximum_particles == 0U ? af_vfx_lod_default() : particle_data->lod;
        af_write_u32(bytes + offsets[11], lod.maximum_particles);
        af_write_f32(bytes + offsets[11] + 4U, lod.spawn_rate_scale);
        af_write_f32(bytes + offsets[11] + 8U, lod.simulation_frequency_scale);
        af_write_f32(bytes + offsets[11] + 12U, lod.distance_start);
        af_write_f32(bytes + offsets[11] + 16U, lod.distance_end);
        af_write_u32(bytes + offsets[11] + 20U, lod.renderer_enabled ? 1U : 0U);
    }
    for (index = 0U; index < particle_data->count; ++index) {
        af_write_particle(bytes + offsets[11] + AF_PARTICLE_HEADER_SIZE +
                              (uint64_t)index * AF_PARTICLE_RECORD_SIZE,
                          &particle_data->emitters[index]);
    }
    af_write_images(bytes + offsets[12], image_data, &image_plan);
    af_write_bindings(bytes + offsets[13], binding_data, source->strip_debug_names);
    af_image_package_plan_destroy(&image_plan);
    af_write_u32(bytes + 48U, af_checksum(bytes + AF_PACKAGE_HEADER_SIZE,
                                          (size_t)total_size - AF_PACKAGE_HEADER_SIZE));
    if (af_package_validate(bytes, (size_t)total_size) != AF_RESULT_SUCCESS) {
        af_deallocate(allocator, bytes);
        return AF_RESULT_INVALID_ARGUMENT;
    }
    out_blob->data = bytes;
    out_blob->size = (size_t)total_size;
    return AF_RESULT_SUCCESS;
}

void af_package_blob_destroy(AfAllocator *allocator, AfPackageBlob *blob) {
    if (blob != NULL) {
        af_deallocate(allocator, blob->data);
        *blob = (AfPackageBlob){0};
    }
}
