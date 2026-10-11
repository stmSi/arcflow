bool af_project_text_valid(const AfProjectText *text) {
    if (!text || text->mode > AF_PROJECT_TEXT_ON_PATH)
        return false;
    if (text->mode == AF_PROJECT_TEXT_NONE)
        return true;
    if (!memchr(text->utf8, '\0', sizeof(text->utf8)) || !text->utf8[0] ||
        !isfinite(text->font_size) || text->font_size < 1 || text->font_size > 1000 ||
        !isfinite(text->width) || text->width < 1 || text->width > 1000000 ||
        !isfinite(text->offset) || fabsf(text->offset) > 1000000 ||
        text->guide_count > AF_PROJECT_TEXT_GUIDE_COMMANDS)
        return false;
    if (!memchr(text->font_name, '\0', sizeof(text->font_name)) ||
        (text->font_name[0] &&
         !af_text_utf8_valid(text->font_name, (uint32_t)strlen(text->font_name))))
        return false;
    if (text->font_family > 2U || text->alignment > 2U || !isfinite(text->letter_spacing) ||
        fabsf(text->letter_spacing) > 1000 || !isfinite(text->line_spacing) ||
        text->line_spacing < 0 || text->line_spacing > 10)
        return false;
    if (!af_text_utf8_valid(text->utf8, (uint32_t)strlen(text->utf8)))
        return false;
    if (text->mode != AF_PROJECT_TEXT_ON_PATH)
        return text->guide_count == 0;
    if (text->guide_count < 2)
        return false;
    AfPath path = {
        .id = 1, .node_id = 1, .geometry_generation = 1, .command_count = text->guide_count};
    const AfPathData data = {.paths = &path,
                             .path_count = 1,
                             .path_capacity = 1,
                             .commands = (AfPathCommand *)text->guide,
                             .command_count = text->guide_count,
                             .command_capacity = AF_PROJECT_TEXT_GUIDE_COMMANDS};
    if (af_path_data_validate(&data) != AF_RESULT_SUCCESS)
        return false;
    for (uint32_t i = 1; i < text->guide_count; ++i)
        if (text->guide[i].type == AF_PATH_MOVE)
            return false;
    return true;
}

static bool af_project_name_valid(const char *name) {
    size_t length = 0U;
    if (name == NULL || name[0] == '\0') {
        return false;
    }
    length = strlen(name);
    if (length >= AF_PROJECT_NAME_CAPACITY) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        const unsigned char byte = (unsigned char)name[index];
        if (byte < 32U || byte == 127U) {
            return false;
        }
    }
    return true;
}

AfResult af_project_create(AfProject *project, AfAllocator *allocator, uint32_t capacity) {
    AfSceneNode *nodes = NULL;
    AfProjectText *texts = NULL;
    char (*names)[AF_PROJECT_NAME_CAPACITY] = NULL;
    AfProjectClip *clips = NULL;
    AfProjectTrack *tracks = NULL;
    AfProjectKeyframe *keyframes = NULL;
    AfPath *paths = NULL;
    AfPathCommand *path_commands = NULL;
    AfStyle *styles = NULL;
    AfLayoutNode *layouts = NULL;
    AfParticleEmitter *emitters = NULL;
    AfExposedProperty *properties = NULL;
    AfBindingTarget *binding_targets = NULL;
    uint64_t *validation_ids = NULL;
    if (project == NULL || allocator == NULL || capacity == 0U || capacity > AF_MAX_NODES) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *project = (AfProject){0};
    project->allocator = allocator;
    texts = af_allocate_array(allocator, capacity, sizeof(*texts), alignof(AfProjectText));
    nodes = af_allocate_array(allocator, capacity, sizeof(*nodes), alignof(AfSceneNode));
    names = af_allocate_array(allocator, capacity, sizeof(*names), alignof(char));
    clips =
        af_allocate_array(allocator, AF_PROJECT_MAX_CLIPS, sizeof(*clips), alignof(AfProjectClip));
    tracks = af_allocate_array(allocator, AF_PROJECT_MAX_TRACKS, sizeof(*tracks),
                               alignof(AfProjectTrack));
    keyframes = af_allocate_array(allocator, AF_PROJECT_MAX_KEYFRAMES, sizeof(*keyframes),
                                  alignof(AfProjectKeyframe));
    paths = af_allocate_array(allocator, AF_MAX_PATHS, sizeof(*paths), alignof(AfPath));
    path_commands = af_allocate_array(allocator, AF_MAX_PATH_COMMANDS, sizeof(*path_commands),
                                      alignof(AfPathCommand));
    styles = af_allocate_array(allocator, AF_MAX_STYLES, sizeof(*styles), alignof(AfStyle));
    layouts =
        af_allocate_array(allocator, AF_MAX_LAYOUT_NODES, sizeof(*layouts), alignof(AfLayoutNode));
    emitters = af_allocate_array(allocator, AF_MAX_PARTICLE_EMITTERS, sizeof(*emitters),
                                 alignof(AfParticleEmitter));
    properties = af_allocate_array(allocator, AF_MAX_EXPOSED_PROPERTIES, sizeof(*properties),
                                   alignof(AfExposedProperty));
    binding_targets = af_allocate_array(allocator, AF_MAX_BINDING_TARGETS, sizeof(*binding_targets),
                                        alignof(AfBindingTarget));
    validation_ids = af_allocate_array(allocator, AF_PROJECT_VALIDATION_ID_CAPACITY,
                                       sizeof(*validation_ids), alignof(uint64_t));
    if (texts == NULL || nodes == NULL || names == NULL || clips == NULL || tracks == NULL ||
        keyframes == NULL || paths == NULL || path_commands == NULL || styles == NULL ||
        layouts == NULL || emitters == NULL || properties == NULL || binding_targets == NULL ||
        validation_ids == NULL) {
        af_deallocate(allocator, validation_ids);
        af_deallocate(allocator, binding_targets);
        af_deallocate(allocator, properties);
        af_deallocate(allocator, path_commands);
        af_deallocate(allocator, styles);
        af_deallocate(allocator, layouts);
        af_deallocate(allocator, emitters);
        af_deallocate(allocator, paths);
        af_deallocate(allocator, keyframes);
        af_deallocate(allocator, tracks);
        af_deallocate(allocator, clips);
        af_deallocate(allocator, names);
        af_deallocate(allocator, texts);
        af_deallocate(allocator, nodes);
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memset(texts, 0, (size_t)capacity * sizeof(*texts));
    memset(nodes, 0, (size_t)capacity * sizeof(*nodes));
    memset(names, 0, (size_t)capacity * sizeof(*names));
    memset(clips, 0, AF_PROJECT_MAX_CLIPS * sizeof(*clips));
    memset(tracks, 0, AF_PROJECT_MAX_TRACKS * sizeof(*tracks));
    memset(keyframes, 0, AF_PROJECT_MAX_KEYFRAMES * sizeof(*keyframes));
    memset(paths, 0, AF_MAX_PATHS * sizeof(*paths));
    memset(path_commands, 0, AF_MAX_PATH_COMMANDS * sizeof(*path_commands));
    memset(styles, 0, AF_MAX_STYLES * sizeof(*styles));
    memset(layouts, 0, AF_MAX_LAYOUT_NODES * sizeof(*layouts));
    memset(emitters, 0, AF_MAX_PARTICLE_EMITTERS * sizeof(*emitters));
    memset(properties, 0, AF_MAX_EXPOSED_PROPERTIES * sizeof(*properties));
    memset(binding_targets, 0, AF_MAX_BINDING_TARGETS * sizeof(*binding_targets));
    memset(validation_ids, 0, AF_PROJECT_VALIDATION_ID_CAPACITY * sizeof(*validation_ids));
    af_scene_init(&project->scene, nodes, capacity);
    af_project_metadata_default(&project->metadata);
    project->names = names;
    project->texts = texts;
    project->canvas_size = (AfVec2){1280.0F, 720.0F};
    project->next_id = 1U;
    project->clips = clips;
    project->clip_capacity = AF_PROJECT_MAX_CLIPS;
    project->tracks = tracks;
    project->track_capacity = AF_PROJECT_MAX_TRACKS;
    project->keyframes = keyframes;
    project->keyframe_capacity = AF_PROJECT_MAX_KEYFRAMES;
    project->next_animation_id = 1U;
    project->next_asset_id = 1U;
    project->path_data = (AfPathData){.paths = paths,
                                      .path_capacity = AF_MAX_PATHS,
                                      .commands = path_commands,
                                      .command_capacity = AF_MAX_PATH_COMMANDS};
    project->scene.paths = &project->path_data;
    project->style_data = (AfStyleData){.styles = styles, .capacity = AF_MAX_STYLES};
    project->scene.styles = &project->style_data;
    project->layout_data = (AfLayoutData){.nodes = layouts,
                                          .capacity = AF_MAX_LAYOUT_NODES,
                                          .reference_size = project->canvas_size,
                                          .target_size = project->canvas_size,
                                          .scale_mode = AF_CANVAS_SCALE_FIT};
    project->scene.layouts = &project->layout_data;
    project->particle_data = (AfParticleData){
        .emitters = emitters, .capacity = AF_MAX_PARTICLE_EMITTERS, .lod = af_vfx_lod_default()};
    project->scene.particles = &project->particle_data;
    project->binding_data = (AfBindingData){.properties = properties,
                                            .property_capacity = AF_MAX_EXPOSED_PROPERTIES,
                                            .targets = binding_targets,
                                            .target_capacity = AF_MAX_BINDING_TARGETS};
    if (af_image_data_create(&project->image_data, allocator) != AF_RESULT_SUCCESS) {
        af_project_destroy(project);
        return AF_RESULT_OUT_OF_MEMORY;
    }
    project->scene.images = &project->image_data;
    project->validation_ids = validation_ids;
    return AF_RESULT_SUCCESS;
}

void af_project_destroy(AfProject *project) {
    if (project == NULL) {
        return;
    }
    if (project->allocator != NULL) {
        af_image_data_destroy(&project->image_data);
        af_deallocate(project->allocator, project->binding_data.targets);
        af_deallocate(project->allocator, project->binding_data.properties);
        af_deallocate(project->allocator, project->validation_ids);
        af_deallocate(project->allocator, project->path_data.commands);
        af_deallocate(project->allocator, project->style_data.styles);
        af_deallocate(project->allocator, project->layout_data.nodes);
        af_deallocate(project->allocator, project->particle_data.emitters);
        af_deallocate(project->allocator, project->path_data.paths);
        af_deallocate(project->allocator, project->keyframes);
        af_deallocate(project->allocator, project->tracks);
        af_deallocate(project->allocator, project->clips);
        af_deallocate(project->allocator, project->texts);
        af_deallocate(project->allocator, project->names);
        af_deallocate(project->allocator, project->scene.nodes);
    }
    *project = (AfProject){0};
}

static bool af_project_validation_insert(const AfProject *project, uint64_t id) {
    uint32_t slot = (uint32_t)((id * UINT64_C(11400714819323198485)) &
                               (uint64_t)(AF_PROJECT_VALIDATION_ID_CAPACITY - 1U));
    for (uint32_t probe = 0U; probe < AF_PROJECT_VALIDATION_ID_CAPACITY; ++probe) {
        if (project->validation_ids[slot] == 0U) {
            project->validation_ids[slot] = id;
            return true;
        }
        if (project->validation_ids[slot] == id) {
            return false;
        }
        slot = (slot + 1U) & (AF_PROJECT_VALIDATION_ID_CAPACITY - 1U);
    }
    return false;
}

uint32_t af_project_find_index(const AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return UINT32_MAX;
    }
    for (index = 0U; index < project->scene.count; ++index) {
        if (project->scene.nodes[index].id == id) {
            return index;
        }
    }
    return UINT32_MAX;
}

static bool af_project_track_target_valid(const AfProject *project, const AfProjectTrack *track) {
    return af_animation_target_valid(&project->scene, track->node_id, track->property,
                                      track->element_index);
}

const char *af_project_name(const AfProject *project, uint64_t id) {
    const uint32_t index = af_project_find_index(project, id);
    return index == UINT32_MAX ? NULL : project->names[index];
}

AfResult af_project_validate(const AfProject *project) {
    uint32_t index = 0U;
    uint64_t maximum_id = 0U;
    uint64_t maximum_animation_id = 0U;
    uint64_t maximum_asset_id = 0U;
    if (project == NULL || project->names == NULL || project->texts == NULL ||
        project->scene.nodes == NULL || project->allocator == NULL ||
        !isfinite(project->canvas_size.x) || !isfinite(project->canvas_size.y) ||
        project->canvas_size.x <= 0.0F || project->canvas_size.y <= 0.0F ||
        project->canvas_size.x > 1000000.0F || project->canvas_size.y > 1000000.0F ||
        project->next_id == 0U || project->next_id == AF_INVALID_NODE_ID ||
        project->clips == NULL || project->tracks == NULL || project->keyframes == NULL ||
        project->validation_ids == NULL || project->clip_count > project->clip_capacity ||
        project->clip_capacity > AF_PROJECT_MAX_CLIPS ||
        project->track_count > project->track_capacity ||
        project->track_capacity > AF_PROJECT_MAX_TRACKS ||
        project->keyframe_count > project->keyframe_capacity ||
        project->keyframe_capacity > AF_PROJECT_MAX_KEYFRAMES || project->next_animation_id == 0U ||
        project->next_animation_id == UINT64_MAX || project->next_asset_id == 0U ||
        project->next_asset_id == UINT64_MAX ||
        af_project_metadata_validate(&project->metadata) != AF_RESULT_SUCCESS ||
        af_scene_validate(&project->scene) != AF_RESULT_SUCCESS ||
        af_path_data_validate(&project->path_data) != AF_RESULT_SUCCESS ||
        af_style_data_validate(&project->style_data) != AF_RESULT_SUCCESS ||
        af_particle_data_validate(&project->particle_data, &project->scene) != AF_RESULT_SUCCESS ||
        af_image_data_validate(&project->image_data, true) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (af_layout_validate(&project->layout_data, &project->scene) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    memset(project->validation_ids, 0,
           AF_PROJECT_VALIDATION_ID_CAPACITY * sizeof(*project->validation_ids));
    for (index = 0U; index < project->scene.count; ++index) {
        if (!af_project_text_valid(&project->texts[index]) ||
            (project->texts[index].mode != AF_PROJECT_TEXT_NONE &&
             project->scene.nodes[index].type != AF_NODE_PATH) ||
            !af_project_name_valid(project->names[index]) ||
            project->scene.nodes[index].id >= AF_INVALID_NODE_ID - 1U) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        if (project->scene.nodes[index].id > maximum_id) {
            maximum_id = project->scene.nodes[index].id;
        }
    }
    for (index = 0U; index < project->style_data.count; ++index) {
        const AfStyle *style = &project->style_data.styles[index];
        if (style->node_id != 0U && af_scene_find_const(&project->scene, style->node_id) == NULL) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (index = 0U; index < project->scene.count; ++index) {
        if (af_style_find_node_const(&project->style_data, project->scene.nodes[index].id) ==
            NULL) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    if (project->next_id <= maximum_id) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < project->path_data.path_count; ++index) {
        const AfPath *path = &project->path_data.paths[index];
        const AfSceneNode *node = af_scene_find_const(&project->scene, path->node_id);
        if (node == NULL || node->type != AF_NODE_PATH) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (index = 0U; index < project->scene.count; ++index) {
        const AfSceneNode *node = &project->scene.nodes[index];
        if (node->type == AF_NODE_PATH &&
            af_path_find_node_const(&project->path_data, node->id) == NULL) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        if (!isfinite(node->polygon_inset) ||
            (node->polygon_sides == 0U && node->polygon_inset != 0.0F) ||
            (node->polygon_sides != 0U &&
             (node->type != AF_NODE_PATH || node->polygon_sides < 3U || node->polygon_sides > 64U ||
              node->polygon_inset < 0.0F || node->polygon_inset > 0.95F))) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (index = 0U; index < project->image_data.binding_count; ++index) {
        const AfImageBinding *binding = &project->image_data.bindings[index];
        const AfSceneNode *node = af_scene_find_const(&project->scene, binding->node_id);
        if (node == NULL || node->type != AF_NODE_IMAGE) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (index = 0U; index < project->image_data.asset_count; ++index) {
        maximum_asset_id = project->image_data.assets[index].id > maximum_asset_id
                               ? project->image_data.assets[index].id
                               : maximum_asset_id;
    }
    for (index = 0U; index < project->image_data.sequence_count; ++index) {
        maximum_asset_id = project->image_data.sequences[index].id > maximum_asset_id
                               ? project->image_data.sequences[index].id
                               : maximum_asset_id;
    }
    if (project->next_asset_id <= maximum_asset_id) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < project->scene.count; ++index) {
        if (project->scene.nodes[index].type == AF_NODE_IMAGE &&
            af_image_find_binding_const(&project->image_data, project->scene.nodes[index].id) ==
                NULL) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (index = 0U; index < project->clip_count; ++index) {
        const AfProjectClip *clip = &project->clips[index];
        if (clip->id == 0U || clip->id >= UINT64_MAX - 1U || !af_project_name_valid(clip->name) ||
            !af_project_validation_insert(project, clip->id) || !isfinite(clip->duration) ||
            !isfinite(clip->loop_start) || !isfinite(clip->loop_end) || clip->duration <= 0.0F ||
            clip->duration > 86400.0F || clip->loop_start < 0.0F ||
            clip->loop_end < clip->loop_start || clip->loop_end > clip->duration) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        maximum_animation_id = clip->id > maximum_animation_id ? clip->id : maximum_animation_id;
    }
    for (index = 0U; index < project->track_count; ++index) {
        const AfProjectTrack *track = &project->tracks[index];
        uint32_t other = 0U;
        if (track->id == 0U || track->id >= UINT64_MAX - 1U ||
            !af_project_validation_insert(project, track->id) ||
            af_project_find_clip_const(project, track->clip_id) == NULL ||
            track->property > AF_PROPERTY_LAST ||
            !af_project_track_target_valid(project, track)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (other = index + 1U; other < project->track_count; ++other) {
            const AfProjectTrack *candidate = &project->tracks[other];
            if (track->clip_id == candidate->clip_id && track->node_id == candidate->node_id &&
                track->property == candidate->property &&
                track->element_index == candidate->element_index) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        maximum_animation_id = track->id > maximum_animation_id ? track->id : maximum_animation_id;
    }
    for (index = 0U; index < project->keyframe_count; ++index) {
        const AfProjectKeyframe *key = &project->keyframes[index];
        const AfProjectTrack *track = af_project_find_track_const(project, key->track_id);
        const AfProjectClip *clip =
            track == NULL ? NULL : af_project_find_clip_const(project, track->clip_id);
        uint32_t component = 0U;
        if (key->id == 0U || key->id >= UINT64_MAX - 1U || track == NULL || clip == NULL ||
            !af_project_validation_insert(project, key->id) || !isfinite(key->keyframe.time) ||
            key->keyframe.time < 0.0F || key->keyframe.time > clip->duration ||
            key->keyframe.interpolation > AF_INTERPOLATION_CUBIC_BEZIER) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (component = 0U; component < 4U; ++component) {
            if (!isfinite(key->keyframe.value[component]) ||
                !isfinite(key->keyframe.easing[component])) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        if (key->keyframe.interpolation == AF_INTERPOLATION_CUBIC_BEZIER &&
            (key->keyframe.easing[0] < 0.0F || key->keyframe.easing[0] > 1.0F ||
             key->keyframe.easing[2] < 0.0F || key->keyframe.easing[2] > 1.0F)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        maximum_animation_id = key->id > maximum_animation_id ? key->id : maximum_animation_id;
    }
    for (index = 0U; index < project->particle_data.count; ++index) {
        const uint64_t id = project->particle_data.emitters[index].id;
        if (!af_project_validation_insert(project, id)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        maximum_animation_id = id > maximum_animation_id ? id : maximum_animation_id;
    }
    if (project->next_animation_id <= maximum_animation_id) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    {
        const AfBindingValidationContext context = {.scene = &project->scene,
                                                    .images = &project->image_data,
                                                    .particles = &project->particle_data};
        if (af_binding_data_validate(&project->binding_data, &context) != AF_RESULT_SUCCESS) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_set_name(AfProject *project, uint64_t id, const char *name) {
    const uint32_t index = af_project_find_index(project, id);
    if (index == UINT32_MAX) {
        return AF_RESULT_NOT_FOUND;
    }
    if (!af_project_name_valid(name)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    (void)snprintf(project->names[index], AF_PROJECT_NAME_CAPACITY, "%s", name);
    return AF_RESULT_SUCCESS;
}

AfResult af_project_add_node(AfProject *project, const AfSceneNode *node, const char *name) {
    AfResult result = AF_RESULT_SUCCESS;
    if (project == NULL || node == NULL || !af_project_name_valid(name) ||
        node->id >= AF_INVALID_NODE_ID - 1U ||
        project->style_data.count >= project->style_data.capacity ||
        af_style_find_const(&project->style_data, node->id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_scene_add_node(&project->scene, node);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    project->texts[project->scene.count - 1U] = (AfProjectText){0};
    (void)snprintf(project->names[project->scene.count - 1U], AF_PROJECT_NAME_CAPACITY, "%s", name);
    if (af_scene_validate(&project->scene) != AF_RESULT_SUCCESS) {
        --project->scene.count;
        return AF_RESULT_INVALID_ARGUMENT;
    }
    project->style_data.styles[project->style_data.count++] =
        af_style_default(node->id, node->id, node->fill);
    if (project->next_id <= node->id) {
        project->next_id = node->id + 1U;
    }
    return AF_RESULT_SUCCESS;
}

static bool af_project_snapshot_image_valid(const AfProject *project,
                                            const AfProjectNodeSnapshot *snapshot) {
    const AfImageBinding *binding = &snapshot->image_binding;
    if (!snapshot->has_image_binding) {
        return true;
    }
    return snapshot->node.type == AF_NODE_IMAGE && binding->node_id == snapshot->node.id &&
           project->image_data.binding_count < project->image_data.binding_capacity &&
           af_image_find_binding_const(&project->image_data, binding->node_id) == NULL &&
           binding->display_mode <= AF_IMAGE_DISPLAY_NINE_SLICE &&
           (binding->asset_id == 0U) != (binding->sequence_id == 0U) &&
           (binding->asset_id == 0U ||
            af_image_find_const(&project->image_data, binding->asset_id) != NULL) &&
           (binding->sequence_id == 0U ||
            af_image_find_sequence_const(&project->image_data, binding->sequence_id) != NULL) &&
           isfinite(binding->frames_per_second) && binding->frames_per_second >= 0.0F &&
           binding->frames_per_second <= 1000.0F;
}

AfResult af_project_insert_snapshot(AfProject *project, const AfProjectNodeSnapshot *snapshot) {
    uint32_t index = 0U;
    AfStyle style;
    if (project == NULL || snapshot == NULL || !af_project_text_valid(&snapshot->text) ||
        (snapshot->text.mode != AF_PROJECT_TEXT_NONE && snapshot->node.type != AF_NODE_PATH) ||
        !af_project_name_valid(snapshot->name) || snapshot->index > project->scene.count ||
        project->scene.count >= project->scene.capacity ||
        af_project_find_index(project, snapshot->node.id) != UINT32_MAX ||
        snapshot->node.id == 0U || snapshot->node.id >= AF_INVALID_NODE_ID - 1U ||
        project->style_data.count >= project->style_data.capacity ||
        (snapshot->has_layout && project->layout_data.count >= project->layout_data.capacity) ||
        (snapshot->has_emitter &&
         (project->particle_data.count >= project->particle_data.capacity ||
          snapshot->emitter.node_id != snapshot->node.id ||
          af_project_find_emitter(project, snapshot->emitter.id) != NULL ||
          af_project_find_node_emitter(project, snapshot->node.id) != NULL)) ||
        !af_project_snapshot_image_valid(project, snapshot)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    style = snapshot->style;
    if (af_style_validate(&style) != AF_RESULT_SUCCESS || style.node_id != snapshot->node.id ||
        af_style_find_const(&project->style_data, style.id) != NULL) {
        style = af_style_default(snapshot->node.id, snapshot->node.id, snapshot->node.fill);
    }
    for (index = project->scene.count; index > snapshot->index; --index) {
        project->texts[index] = project->texts[index - 1U];
        project->scene.nodes[index] = project->scene.nodes[index - 1U];
        memcpy(project->names[index], project->names[index - 1U], AF_PROJECT_NAME_CAPACITY);
    }
    project->texts[snapshot->index] = snapshot->text;
    project->scene.nodes[snapshot->index] = snapshot->node;
    memcpy(project->names[snapshot->index], snapshot->name, AF_PROJECT_NAME_CAPACITY);
    ++project->scene.count;
    project->style_data.styles[project->style_data.count++] = style;
    if (snapshot->has_layout) {
        AfLayoutNode layout = snapshot->layout;
        layout.node_id = snapshot->node.id;
        project->layout_data.nodes[project->layout_data.count++] = layout;
    }
    if (snapshot->has_emitter) {
        project->particle_data.emitters[project->particle_data.count++] = snapshot->emitter;
        if (project->next_animation_id <= snapshot->emitter.id) {
            project->next_animation_id = snapshot->emitter.id + 1U;
        }
    }
    if (snapshot->has_image_binding) {
        AfImageBinding binding = snapshot->image_binding;
        binding.node_id = snapshot->node.id;
        if (af_image_set_binding(&project->image_data, &binding) != AF_RESULT_SUCCESS) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    if (project->next_id <= snapshot->node.id) {
        project->next_id = snapshot->node.id + 1U;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_remove_node_exact(AfProject *project, uint64_t id) {
    uint32_t index = af_project_find_index(project, id);
    if (index == UINT32_MAX) {
        return AF_RESULT_NOT_FOUND;
    }
    for (; index + 1U < project->scene.count; ++index) {
        project->texts[index] = project->texts[index + 1U];
        project->scene.nodes[index] = project->scene.nodes[index + 1U];
        memcpy(project->names[index], project->names[index + 1U], AF_PROJECT_NAME_CAPACITY);
    }
    --project->scene.count;
    for (uint32_t style_index = 0U; style_index < project->style_data.count; ++style_index) {
        if (project->style_data.styles[style_index].node_id == id) {
            memmove(&project->style_data.styles[style_index],
                    &project->style_data.styles[style_index + 1U],
                    (size_t)(project->style_data.count - style_index - 1U) * sizeof(AfStyle));
            --project->style_data.count;
            break;
        }
    }
    for (uint32_t layout_index = 0U; layout_index < project->layout_data.count; ++layout_index) {
        if (project->layout_data.nodes[layout_index].node_id == id) {
            memmove(&project->layout_data.nodes[layout_index],
                    &project->layout_data.nodes[layout_index + 1U],
                    (size_t)(project->layout_data.count - layout_index - 1U) *
                        sizeof(AfLayoutNode));
            --project->layout_data.count;
            break;
        }
    }
    for (uint32_t emitter_index = 0U; emitter_index < project->particle_data.count;) {
        if (project->particle_data.emitters[emitter_index].node_id == id) {
            memmove(&project->particle_data.emitters[emitter_index],
                    &project->particle_data.emitters[emitter_index + 1U],
                    (size_t)(project->particle_data.count - emitter_index - 1U) *
                        sizeof(AfParticleEmitter));
            --project->particle_data.count;
        } else {
            ++emitter_index;
        }
    }
    (void)af_image_remove_binding(&project->image_data, id);
    return AF_RESULT_SUCCESS;
}

static bool af_project_is_descendant(const AfProject *project, uint64_t candidate, uint64_t root) {
    uint32_t depth = 0U;
    uint64_t current = candidate;
    while (current != AF_INVALID_NODE_ID && depth++ <= AF_MAX_HIERARCHY_DEPTH) {
        const AfSceneNode *node = af_scene_find_const(&project->scene, current);
        if (current == root) {
            return true;
        }
        if (node == NULL) {
            return false;
        }
        current = node->parent_id;
    }
    return false;
}

AfResult af_project_collect_subtree(const AfProject *project, uint64_t id,
                                    AfProjectNodeSnapshot *snapshots, uint32_t capacity,
                                    uint32_t *out_count) {
    uint32_t count = 0U;
    uint32_t index = 0U;
    if (project == NULL || snapshots == NULL || out_count == NULL ||
        af_project_find_index(project, id) == UINT32_MAX) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < project->scene.count; ++index) {
        if (af_project_is_descendant(project, project->scene.nodes[index].id, id)) {
            if (count >= capacity) {
                return AF_RESULT_LIMIT_EXCEEDED;
            }
            snapshots[count] =
                (AfProjectNodeSnapshot){.node = project->scene.nodes[index], .index = index};
            snapshots[count].style =
                *af_style_find_node_const(&project->style_data, project->scene.nodes[index].id);
            {
                const AfLayoutNode *layout =
                    af_layout_find_const(&project->layout_data, project->scene.nodes[index].id);
                if (layout != NULL) {
                    snapshots[count].layout = *layout;
                    snapshots[count].has_layout = true;
                }
            }
            {
                const AfParticleEmitter *emitter =
                    af_project_find_node_emitter_const(project, project->scene.nodes[index].id);
                if (emitter != NULL) {
                    snapshots[count].emitter = *emitter;
                    snapshots[count].has_emitter = true;
                }
            }
            {
                const AfImageBinding *binding = af_image_find_binding_const(
                    &project->image_data, project->scene.nodes[index].id);
                if (binding != NULL) {
                    snapshots[count].image_binding = *binding;
                    snapshots[count].has_image_binding = true;
                }
            }
            snapshots[count].text = project->texts[index];
            memcpy(snapshots[count].name, project->names[index], AF_PROJECT_NAME_CAPACITY);
            ++count;
        }
    }
    *out_count = count;
    return AF_RESULT_SUCCESS;
}

AfResult af_project_delete_subtree(AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    uint32_t write_index = 0U;
    if (project == NULL || af_project_find_index(project, id) == UINT32_MAX) {
        return AF_RESULT_NOT_FOUND;
    }
    for (index = 0U; index < project->scene.count; ++index) {
        if (af_project_is_descendant(project, project->scene.nodes[index].id, id)) {
            continue;
        }
        if (write_index != index) {
            project->texts[write_index] = project->texts[index];
            project->scene.nodes[write_index] = project->scene.nodes[index];
            memcpy(project->names[write_index], project->names[index], AF_PROJECT_NAME_CAPACITY);
        }
        ++write_index;
    }
    project->scene.count = write_index;
    for (index = 0U; index < project->style_data.count;) {
        const uint64_t node_id = project->style_data.styles[index].node_id;
        if (node_id != 0U && af_scene_find_const(&project->scene, node_id) == NULL) {
            memmove(&project->style_data.styles[index], &project->style_data.styles[index + 1U],
                    (size_t)(project->style_data.count - index - 1U) * sizeof(AfStyle));
            --project->style_data.count;
        } else {
            ++index;
        }
    }
    for (index = 0U; index < project->layout_data.count;) {
        if (af_scene_find_const(&project->scene, project->layout_data.nodes[index].node_id) ==
            NULL) {
            memmove(&project->layout_data.nodes[index], &project->layout_data.nodes[index + 1U],
                    (size_t)(project->layout_data.count - index - 1U) * sizeof(AfLayoutNode));
            --project->layout_data.count;
        } else {
            ++index;
        }
    }
    for (index = 0U; index < project->particle_data.count;) {
        if (af_scene_find_const(&project->scene, project->particle_data.emitters[index].node_id) ==
            NULL) {
            memmove(&project->particle_data.emitters[index],
                    &project->particle_data.emitters[index + 1U],
                    (size_t)(project->particle_data.count - index - 1U) *
                        sizeof(AfParticleEmitter));
            --project->particle_data.count;
        } else {
            ++index;
        }
    }
    for (index = 0U; index < project->image_data.binding_count;) {
        if (af_scene_find_const(&project->scene, project->image_data.bindings[index].node_id) ==
            NULL) {
            const uint64_t node_id = project->image_data.bindings[index].node_id;
            (void)af_image_remove_binding(&project->image_data, node_id);
        } else {
            ++index;
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_snapshot(const AfProject *project, uint64_t id,
                             AfProjectNodeSnapshot *out_snapshot) {
    const uint32_t index = af_project_find_index(project, id);
    if (out_snapshot == NULL || index == UINT32_MAX) {
        return AF_RESULT_NOT_FOUND;
    }
    *out_snapshot = (AfProjectNodeSnapshot){.node = project->scene.nodes[index], .index = index};
    out_snapshot->text = project->texts[index];
    out_snapshot->vfx_lod = project->particle_data.lod;
    out_snapshot->style = *af_style_find_node_const(&project->style_data, id);
    {
        const AfLayoutNode *layout = af_layout_find_const(&project->layout_data, id);
        if (layout != NULL) {
            out_snapshot->layout = *layout;
            out_snapshot->has_layout = true;
        }
    }
    {
        const AfParticleEmitter *emitter = af_project_find_node_emitter_const(project, id);
        if (emitter != NULL) {
            out_snapshot->emitter = *emitter;
            out_snapshot->has_emitter = true;
        }
    }
    {
        const AfImageBinding *binding = af_image_find_binding_const(&project->image_data, id);
        if (binding != NULL) {
            out_snapshot->image_binding = *binding;
            out_snapshot->has_image_binding = true;
        }
    }
    memcpy(out_snapshot->name, project->names[index], AF_PROJECT_NAME_CAPACITY);
    return AF_RESULT_SUCCESS;
}

AfStyle *af_project_find_node_style(AfProject *project, uint64_t node_id) {
    if (project == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < project->style_data.count; ++index) {
        if (project->style_data.styles[index].node_id == node_id) {
            return &project->style_data.styles[index];
        }
    }
    return NULL;
}

AfResult af_project_add_style_preset(AfProject *project, const AfStyle *style) {
    uint32_t preset_count = 0U;
    if (project == NULL || style == NULL || style->node_id != 0U ||
        project->style_data.count >= project->style_data.capacity ||
        af_style_validate(style) != AF_RESULT_SUCCESS ||
        af_style_find_const(&project->style_data, style->id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < project->style_data.count; ++index) {
        preset_count += project->style_data.styles[index].node_id == 0U ? 1U : 0U;
    }
    if (preset_count >= AF_MAX_STYLE_PRESETS) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    project->style_data.styles[project->style_data.count++] = *style;
    return AF_RESULT_SUCCESS;
}

AfResult af_project_set_layout(AfProject *project, const AfLayoutNode *layout) {
    if (project == NULL || layout == NULL ||
        af_scene_find_const(&project->scene, layout->node_id) == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < project->layout_data.count; ++index) {
        if (project->layout_data.nodes[index].node_id == layout->node_id) {
            const AfLayoutNode previous = project->layout_data.nodes[index];
            project->layout_data.nodes[index] = *layout;
            if (af_layout_validate(&project->layout_data, &project->scene) != AF_RESULT_SUCCESS) {
                project->layout_data.nodes[index] = previous;
                return AF_RESULT_INVALID_ARGUMENT;
            }
            return AF_RESULT_SUCCESS;
        }
    }
    if (project->layout_data.count >= project->layout_data.capacity) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    project->layout_data.nodes[project->layout_data.count++] = *layout;
    if (af_layout_validate(&project->layout_data, &project->scene) != AF_RESULT_SUCCESS) {
        --project->layout_data.count;
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_clear_layout(AfProject *project, uint64_t node_id) {
    if (project == NULL || af_scene_find_const(&project->scene, node_id) == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < project->layout_data.count; ++index) {
        if (project->layout_data.nodes[index].node_id == node_id) {
            memmove(&project->layout_data.nodes[index], &project->layout_data.nodes[index + 1U],
                    (size_t)(project->layout_data.count - index - 1U) * sizeof(AfLayoutNode));
            --project->layout_data.count;
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_NOT_FOUND;
}

AfParticleEmitter *af_project_find_emitter(AfProject *project, uint64_t id) {
    if (project == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < project->particle_data.count; ++index) {
        if (project->particle_data.emitters[index].id == id) {
            return &project->particle_data.emitters[index];
        }
    }
    return NULL;
}

AfParticleEmitter *af_project_find_node_emitter(AfProject *project, uint64_t node_id) {
    if (project == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < project->particle_data.count; ++index) {
        if (project->particle_data.emitters[index].node_id == node_id) {
            return &project->particle_data.emitters[index];
        }
    }
    return NULL;
}

const AfParticleEmitter *af_project_find_node_emitter_const(const AfProject *project,
                                                            uint64_t node_id) {
    if (project == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < project->particle_data.count; ++index) {
        if (project->particle_data.emitters[index].node_id == node_id) {
            return &project->particle_data.emitters[index];
        }
    }
    return NULL;
}

AfResult af_project_add_emitter(AfProject *project, const AfParticleEmitter *emitter) {
    if (project == NULL || emitter == NULL ||
        project->particle_data.count >= project->particle_data.capacity ||
        af_project_find_emitter(project, emitter->id) != NULL ||
        af_project_find_node_emitter(project, emitter->node_id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    project->particle_data.emitters[project->particle_data.count++] = *emitter;
    if (af_particle_data_validate(&project->particle_data, &project->scene) != AF_RESULT_SUCCESS) {
        --project->particle_data.count;
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (project->next_animation_id <= emitter->id) {
        project->next_animation_id = emitter->id + 1U;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_remove_emitter(AfProject *project, uint64_t id) {
    if (project == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < project->particle_data.count; ++index) {
        if (project->particle_data.emitters[index].id == id) {
            memmove(&project->particle_data.emitters[index],
                    &project->particle_data.emitters[index + 1U],
                    (size_t)(project->particle_data.count - index - 1U) *
                        sizeof(AfParticleEmitter));
            --project->particle_data.count;
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_NOT_FOUND;
}

AfResult af_project_allocate_id(AfProject *project, uint64_t *out_id) {
    if (project == NULL || out_id == NULL || project->next_id == 0U ||
        project->next_id >= AF_INVALID_NODE_ID - 1U) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    *out_id = project->next_id++;
    return AF_RESULT_SUCCESS;
}

AfProjectClip *af_project_find_clip(AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return NULL;
    }
    for (index = 0U; index < project->clip_count; ++index) {
        if (project->clips[index].id == id) {
            return &project->clips[index];
        }
    }
    return NULL;
}

const AfProjectClip *af_project_find_clip_const(const AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return NULL;
    }
    for (index = 0U; index < project->clip_count; ++index) {
        if (project->clips[index].id == id) {
            return &project->clips[index];
        }
    }
    return NULL;
}

AfProjectTrack *af_project_find_track(AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return NULL;
    }
    for (index = 0U; index < project->track_count; ++index) {
        if (project->tracks[index].id == id) {
            return &project->tracks[index];
        }
    }
    return NULL;
}

const AfProjectTrack *af_project_find_track_const(const AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return NULL;
    }
    for (index = 0U; index < project->track_count; ++index) {
        if (project->tracks[index].id == id) {
            return &project->tracks[index];
        }
    }
    return NULL;
}

AfProjectKeyframe *af_project_find_keyframe(AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return NULL;
    }
    for (index = 0U; index < project->keyframe_count; ++index) {
        if (project->keyframes[index].id == id) {
            return &project->keyframes[index];
        }
    }
    return NULL;
}

const AfProjectKeyframe *af_project_find_keyframe_const(const AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return NULL;
    }
    for (index = 0U; index < project->keyframe_count; ++index) {
        if (project->keyframes[index].id == id) {
            return &project->keyframes[index];
        }
    }
    return NULL;
}
