AfResult af_package_decode(const void *data, size_t size, AfAllocator *allocator,
                           AfLoadedPackage *out_package) {
    AfSectionView sections[AF_PACKAGE_MAX_SECTIONS] = {0};
    uint32_t section_count = 0U;
    const uint8_t *bytes = data;
    const AfSectionView *nodes = NULL;
    const AfSectionView *clips = NULL;
    const AfSectionView *tracks = NULL;
    const AfSectionView *keys = NULL;
    const AfSectionView *paths = NULL;
    const AfSectionView *path_commands = NULL;
    const AfSectionView *styles = NULL;
    const AfSectionView *layouts = NULL;
    const AfSectionView *components = NULL;
    const AfSectionView *state_machines = NULL;
    const AfSectionView *particles = NULL;
    const AfSectionView *images = NULL;
    const AfSectionView *bindings = NULL;
    uint32_t index = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (allocator == NULL || out_package == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_package = (AfLoadedPackage){0};
    result = af_package_validate(data, size);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    result = af_read_sections(bytes, size, sections, &section_count);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    nodes = af_find_section(sections, section_count, AF_SECTION_NODES);
    clips = af_find_section(sections, section_count, AF_SECTION_CLIPS);
    tracks = af_find_section(sections, section_count, AF_SECTION_TRACKS);
    keys = af_find_section(sections, section_count, AF_SECTION_KEYFRAMES);
    paths = af_find_section(sections, section_count, AF_SECTION_PATHS);
    path_commands = af_find_section(sections, section_count, AF_SECTION_PATH_COMMANDS);
    styles = af_find_section(sections, section_count, AF_SECTION_STYLES);
    layouts = af_find_section(sections, section_count, AF_SECTION_LAYOUTS);
    components = af_find_section(sections, section_count, AF_SECTION_COMPONENTS);
    state_machines = af_find_section(sections, section_count, AF_SECTION_STATE_MACHINES);
    particles = af_find_section(sections, section_count, AF_SECTION_PARTICLES);
    images = af_find_section(sections, section_count, AF_SECTION_IMAGES);
    bindings = af_find_section(sections, section_count, AF_SECTION_BINDINGS);
    out_package->scene.nodes =
        af_allocate_array(allocator, nodes->count, sizeof(AfSceneNode), alignof(AfSceneNode));
    out_package->base_nodes =
        af_allocate_array(allocator, nodes->count, sizeof(AfSceneNode), alignof(AfSceneNode));
    out_package->animation.clips = af_allocate_array(
        allocator, clips->count, sizeof(AfAnimationClip), alignof(AfAnimationClip));
    out_package->animation.tracks = af_allocate_array(
        allocator, tracks->count, sizeof(AfAnimationTrack), alignof(AfAnimationTrack));
    out_package->animation.keyframes =
        af_allocate_array(allocator, keys->count, sizeof(AfKeyframe), alignof(AfKeyframe));
    out_package->path_data.paths =
        af_allocate_array(allocator, paths->count, sizeof(AfPath), alignof(AfPath));
    out_package->path_data.commands = af_allocate_array(
        allocator, path_commands->count, sizeof(AfPathCommand), alignof(AfPathCommand));
    out_package->style_data.styles =
        af_allocate_array(allocator, styles->count, sizeof(AfStyle), alignof(AfStyle));
    out_package->layout_data.nodes =
        af_allocate_array(allocator, layouts->count, sizeof(AfLayoutNode), alignof(AfLayoutNode));
    out_package->particle_data.emitters = af_allocate_array(
        allocator, particles->count, sizeof(AfParticleEmitter), alignof(AfParticleEmitter));
    if ((nodes->count != 0U &&
         (out_package->scene.nodes == NULL || out_package->base_nodes == NULL)) ||
        (clips->count != 0U && out_package->animation.clips == NULL) ||
        (tracks->count != 0U && out_package->animation.tracks == NULL) ||
        (keys->count != 0U && out_package->animation.keyframes == NULL) ||
        (paths->count != 0U && out_package->path_data.paths == NULL) ||
        (path_commands->count != 0U && out_package->path_data.commands == NULL) ||
        (styles->count != 0U && out_package->style_data.styles == NULL) ||
        (layouts->count != 0U && out_package->layout_data.nodes == NULL) ||
        (particles->count != 0U && out_package->particle_data.emitters == NULL)) {
        af_loaded_package_destroy(allocator, out_package);
        return AF_RESULT_OUT_OF_MEMORY;
    }
    result =
        af_decode_components(bytes + components->offset, allocator, &out_package->component_data);
    if (result != AF_RESULT_SUCCESS) {
        af_loaded_package_destroy(allocator, out_package);
        return result;
    }
    result = af_decode_state_machines(bytes + state_machines->offset, allocator,
                                      &out_package->state_machine_data);
    if (result != AF_RESULT_SUCCESS) {
        af_loaded_package_destroy(allocator, out_package);
        return result;
    }
    result = af_decode_images(bytes + images->offset, allocator, &out_package->image_data);
    if (result != AF_RESULT_SUCCESS) {
        af_loaded_package_destroy(allocator, out_package);
        return result;
    }
    result = af_decode_bindings(bytes + bindings->offset, allocator, &out_package->binding_data);
    if (result != AF_RESULT_SUCCESS) {
        af_loaded_package_destroy(allocator, out_package);
        return result;
    }
    out_package->scene.count = nodes->count;
    out_package->scene.capacity = nodes->count;
    out_package->animation.clip_count = clips->count;
    out_package->animation.track_count = tracks->count;
    out_package->animation.keyframe_count = keys->count;
    out_package->path_data.path_count = paths->count;
    out_package->path_data.path_capacity = paths->count;
    out_package->path_data.command_count = path_commands->count;
    out_package->path_data.command_capacity = path_commands->count;
    out_package->style_data.count = styles->count;
    out_package->style_data.capacity = styles->count;
    out_package->layout_data.count = layouts->count;
    out_package->layout_data.capacity = layouts->count;
    out_package->particle_data.count = particles->count;
    out_package->particle_data.capacity = particles->count;
    out_package->particle_data.lod = (AfVfxLodSettings){
        .maximum_particles = af_read_u32(bytes + particles->offset),
        .spawn_rate_scale = af_read_f32(bytes + particles->offset + 4U),
        .simulation_frequency_scale = af_read_f32(bytes + particles->offset + 8U),
        .distance_start = af_read_f32(bytes + particles->offset + 12U),
        .distance_end = af_read_f32(bytes + particles->offset + 16U),
        .host_quality = 1.0F,
        .renderer_enabled = af_read_u32(bytes + particles->offset + 20U) != 0U};
    out_package->layout_data.reference_size =
        (AfVec2){af_read_f32(bytes + layouts->offset), af_read_f32(bytes + layouts->offset + 4U)};
    out_package->layout_data.target_size = (AfVec2){af_read_f32(bytes + layouts->offset + 8U),
                                                    af_read_f32(bytes + layouts->offset + 12U)};
    out_package->layout_data.scale_mode =
        (AfCanvasScaleMode)af_read_u32(bytes + layouts->offset + 16U);
    out_package->layout_data.safe_area =
        (AfInsets){.left = af_read_f32(bytes + layouts->offset + 20U),
                   .top = af_read_f32(bytes + layouts->offset + 24U),
                   .right = af_read_f32(bytes + layouts->offset + 28U),
                   .bottom = af_read_f32(bytes + layouts->offset + 32U)};
    out_package->scene.paths = &out_package->path_data;
    out_package->scene.styles = &out_package->style_data;
    out_package->scene.layouts = &out_package->layout_data;
    out_package->scene.particles = &out_package->particle_data;
    out_package->scene.images = &out_package->image_data;
    for (index = 0U; index < nodes->count; ++index) {
        const uint8_t *record = bytes + nodes->offset + (uint64_t)index * AF_NODE_RECORD_SIZE;
        AfSceneNode *node = &out_package->scene.nodes[index];
        *node = (AfSceneNode){
            .id = af_read_u64(record),
            .parent_id = af_read_u64(record + 8U),
            .type = (AfNodeType)af_read_u32(record + 16U),
            .local_transform = {.position = {af_read_f32(record + 24U), af_read_f32(record + 28U)},
                                .rotation = af_read_f32(record + 32U),
                                .scale = {af_read_f32(record + 36U), af_read_f32(record + 40U)},
                                .skew = {af_read_f32(record + 44U), af_read_f32(record + 48U)}},
            .bounds = {af_read_f32(record + 52U), af_read_f32(record + 56U)},
            .fill = {af_read_f32(record + 60U), af_read_f32(record + 64U),
                     af_read_f32(record + 68U), af_read_f32(record + 72U)},
            .opacity = af_read_f32(record + 76U),
            .corner_radius = af_read_f32(record + 80U),
            .layer_order = (int32_t)af_read_u32(record + 84U),
            .visible = af_read_u32(record + 20U) != 0U};
    }
    if (nodes->count != 0U) {
        memcpy(out_package->base_nodes, out_package->scene.nodes,
               (size_t)nodes->count * sizeof(AfSceneNode));
    }
    for (index = 0U; index < clips->count; ++index) {
        const uint8_t *record = bytes + clips->offset + (uint64_t)index * AF_CLIP_RECORD_SIZE;
        out_package->animation.clips[index] =
            (AfAnimationClip){.id = af_read_u64(record),
                              .duration = af_read_f32(record + 8U),
                              .track_start = af_read_u32(record + 12U),
                              .track_count = af_read_u32(record + 16U),
                              .loop = af_read_u32(record + 20U) != 0U};
    }
    for (index = 0U; index < tracks->count; ++index) {
        const uint8_t *record = bytes + tracks->offset + (uint64_t)index * AF_TRACK_RECORD_SIZE;
        out_package->animation.tracks[index] =
            (AfAnimationTrack){.id = af_read_u64(record),
                               .node_id = af_read_u64(record + 8U),
                               .property = (AfPropertyType)af_read_u32(record + 16U),
                               .keyframe_start = af_read_u32(record + 20U),
                               .keyframe_count = af_read_u32(record + 24U),
                               .element_index = af_read_u32(record + 28U)};
    }
    for (index = 0U; index < keys->count; ++index) {
        const uint8_t *record = bytes + keys->offset + (uint64_t)index * AF_KEYFRAME_RECORD_SIZE;
        uint32_t component = 0U;
        AfKeyframe *key = &out_package->animation.keyframes[index];
        key->time = af_read_f32(record);
        key->interpolation = (AfInterpolation)af_read_u32(record + 20U);
        for (component = 0U; component < 4U; ++component) {
            key->value[component] = af_read_f32(record + 4U + component * 4U);
            key->easing[component] = af_read_f32(record + 24U + component * 4U);
        }
    }
    for (index = 0U; index < paths->count; ++index) {
        const uint8_t *record = bytes + paths->offset + (uint64_t)index * AF_PATH_RECORD_SIZE;
        out_package->path_data.paths[index] =
            (AfPath){.id = af_read_u64(record),
                     .node_id = af_read_u64(record + 8U),
                     .command_start = af_read_u32(record + 16U),
                     .command_count = af_read_u32(record + 20U),
                     .fill_rule = (AfPathFillRule)af_read_u32(record + 24U),
                     .geometry_generation = af_read_u64(record + 32U)};
    }
    for (index = 0U; index < path_commands->count; ++index) {
        const uint8_t *record =
            bytes + path_commands->offset + (uint64_t)index * AF_PATH_COMMAND_RECORD_SIZE;
        out_package->path_data.commands[index] =
            (AfPathCommand){.type = (AfPathCommandType)af_read_u32(record),
                            .point = {af_read_f32(record + 8U), af_read_f32(record + 12U)},
                            .control1 = {af_read_f32(record + 16U), af_read_f32(record + 20U)},
                            .control2 = {af_read_f32(record + 24U), af_read_f32(record + 28U)}};
    }
    for (index = 0U; index < styles->count; ++index) {
        out_package->style_data.styles[index] =
            af_read_style(bytes + styles->offset + (uint64_t)index * AF_STYLE_RECORD_SIZE);
    }
    for (index = 0U; index < layouts->count; ++index) {
        out_package->layout_data.nodes[index] =
            af_read_layout(bytes + layouts->offset + AF_LAYOUT_HEADER_SIZE +
                           (uint64_t)index * AF_LAYOUT_RECORD_SIZE);
    }
    for (index = 0U; index < particles->count; ++index) {
        out_package->particle_data.emitters[index] =
            af_read_particle(bytes + particles->offset + AF_PARTICLE_HEADER_SIZE +
                             (uint64_t)index * AF_PARTICLE_RECORD_SIZE);
    }
    result = af_scene_evaluate_world(&out_package->scene);
    if (result == AF_RESULT_SUCCESS) {
        const AfBindingValidationContext context = {.scene = &out_package->scene,
                                                    .animation = &out_package->animation,
                                                    .state_machines =
                                                        &out_package->state_machine_data,
                                                    .components = &out_package->component_data,
                                                    .images = &out_package->image_data,
                                                    .particles = &out_package->particle_data};
        result = af_binding_data_validate(&out_package->binding_data, &context);
    }
    if (result != AF_RESULT_SUCCESS) {
        af_loaded_package_destroy(allocator, out_package);
    }
    return result;
}

void af_loaded_package_destroy(AfAllocator *allocator, AfLoadedPackage *package) {
    if (package == NULL) {
        return;
    }
    af_deallocate(allocator, package->scene.nodes);
    af_deallocate(allocator, package->base_nodes);
    af_deallocate(allocator, package->animation.clips);
    af_deallocate(allocator, package->animation.tracks);
    af_deallocate(allocator, package->animation.keyframes);
    af_deallocate(allocator, package->path_data.paths);
    af_deallocate(allocator, package->path_data.commands);
    af_deallocate(allocator, package->style_data.styles);
    af_deallocate(allocator, package->layout_data.nodes);
    af_deallocate(allocator, package->component_data.definitions);
    af_deallocate(allocator, package->component_data.references);
    af_deallocate(allocator, package->component_data.properties);
    af_deallocate(allocator, package->component_data.instances);
    af_deallocate(allocator, package->component_data.overrides);
    af_deallocate(allocator, package->state_machine_data.machines);
    af_deallocate(allocator, package->state_machine_data.states);
    af_deallocate(allocator, package->state_machine_data.transitions);
    af_deallocate(allocator, package->state_machine_data.inputs);
    af_deallocate(allocator, package->state_machine_data.conditions);
    af_deallocate(allocator, package->particle_data.emitters);
    af_deallocate(allocator, package->binding_data.targets);
    af_deallocate(allocator, package->binding_data.properties);
    af_image_data_destroy(&package->image_data);
    *package = (AfLoadedPackage){0};
}
