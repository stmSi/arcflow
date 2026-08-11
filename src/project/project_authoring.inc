AfResult af_project_insert_clip_snapshot(AfProject *project,
                                         const AfProjectClipSnapshot *snapshot) {
    uint32_t index = 0U;
    if (project == NULL || snapshot == NULL || snapshot->index > project->clip_count ||
        project->clip_count >= project->clip_capacity || snapshot->clip.id == 0U ||
        af_project_find_clip(project, snapshot->clip.id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (index = project->clip_count; index > snapshot->index; --index) {
        project->clips[index] = project->clips[index - 1U];
    }
    project->clips[snapshot->index] = snapshot->clip;
    ++project->clip_count;
    if (project->next_animation_id <= snapshot->clip.id) {
        project->next_animation_id = snapshot->clip.id + 1U;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_insert_track_snapshot(AfProject *project,
                                          const AfProjectTrackSnapshot *snapshot) {
    uint32_t index = 0U;
    if (project == NULL || snapshot == NULL || snapshot->index > project->track_count ||
        project->track_count >= project->track_capacity || snapshot->track.id == 0U ||
        af_project_find_track(project, snapshot->track.id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (index = project->track_count; index > snapshot->index; --index) {
        project->tracks[index] = project->tracks[index - 1U];
    }
    project->tracks[snapshot->index] = snapshot->track;
    ++project->track_count;
    if (project->next_animation_id <= snapshot->track.id) {
        project->next_animation_id = snapshot->track.id + 1U;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_insert_keyframe_snapshot(AfProject *project,
                                             const AfProjectKeyframeSnapshot *snapshot) {
    uint32_t index = 0U;
    if (project == NULL || snapshot == NULL || snapshot->index > project->keyframe_count ||
        project->keyframe_count >= project->keyframe_capacity || snapshot->keyframe.id == 0U ||
        af_project_find_keyframe(project, snapshot->keyframe.id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (index = project->keyframe_count; index > snapshot->index; --index) {
        project->keyframes[index] = project->keyframes[index - 1U];
    }
    project->keyframes[snapshot->index] = snapshot->keyframe;
    ++project->keyframe_count;
    if (project->next_animation_id <= snapshot->keyframe.id) {
        project->next_animation_id = snapshot->keyframe.id + 1U;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_add_clip(AfProject *project, const AfProjectClip *clip) {
    AfProjectClipSnapshot snapshot = {0};
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    if (project == NULL || clip == NULL) {
        return result;
    }
    snapshot = (AfProjectClipSnapshot){.clip = *clip, .index = project->clip_count};
    result = af_project_insert_clip_snapshot(project, &snapshot);
    if (result == AF_RESULT_SUCCESS && af_project_validate(project) != AF_RESULT_SUCCESS) {
        (void)af_project_remove_clip_exact(project, clip->id);
        result = AF_RESULT_INVALID_ARGUMENT;
    }
    return result;
}

AfResult af_project_add_track(AfProject *project, const AfProjectTrack *track) {
    AfProjectTrackSnapshot snapshot = {0};
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    if (project == NULL || track == NULL) {
        return result;
    }
    snapshot = (AfProjectTrackSnapshot){.track = *track, .index = project->track_count};
    result = af_project_insert_track_snapshot(project, &snapshot);
    if (result == AF_RESULT_SUCCESS && af_project_validate(project) != AF_RESULT_SUCCESS) {
        (void)af_project_remove_track_exact(project, track->id);
        result = AF_RESULT_INVALID_ARGUMENT;
    }
    return result;
}

AfResult af_project_add_keyframe(AfProject *project, const AfProjectKeyframe *keyframe) {
    AfProjectKeyframeSnapshot snapshot = {0};
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    if (project == NULL || keyframe == NULL) {
        return result;
    }
    snapshot = (AfProjectKeyframeSnapshot){.keyframe = *keyframe, .index = project->keyframe_count};
    result = af_project_insert_keyframe_snapshot(project, &snapshot);
    if (result == AF_RESULT_SUCCESS && af_project_validate(project) != AF_RESULT_SUCCESS) {
        (void)af_project_remove_keyframe_exact(project, keyframe->id);
        result = AF_RESULT_INVALID_ARGUMENT;
    }
    return result;
}

AfResult af_project_remove_clip_exact(AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    while (index < project->clip_count && project->clips[index].id != id) {
        ++index;
    }
    if (index == project->clip_count) {
        return AF_RESULT_NOT_FOUND;
    }
    for (; index + 1U < project->clip_count; ++index) {
        project->clips[index] = project->clips[index + 1U];
    }
    --project->clip_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_project_remove_track_exact(AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    while (index < project->track_count && project->tracks[index].id != id) {
        ++index;
    }
    if (index == project->track_count) {
        return AF_RESULT_NOT_FOUND;
    }
    for (; index + 1U < project->track_count; ++index) {
        project->tracks[index] = project->tracks[index + 1U];
    }
    --project->track_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_project_remove_keyframe_exact(AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    while (index < project->keyframe_count && project->keyframes[index].id != id) {
        ++index;
    }
    if (index == project->keyframe_count) {
        return AF_RESULT_NOT_FOUND;
    }
    for (; index + 1U < project->keyframe_count; ++index) {
        project->keyframes[index] = project->keyframes[index + 1U];
    }
    --project->keyframe_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_project_clip_snapshot(const AfProject *project, uint64_t id,
                                  AfProjectClipSnapshot *out_snapshot) {
    uint32_t index = 0U;
    if (project == NULL || out_snapshot == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    while (index < project->clip_count && project->clips[index].id != id) {
        ++index;
    }
    if (index == project->clip_count) {
        return AF_RESULT_NOT_FOUND;
    }
    *out_snapshot = (AfProjectClipSnapshot){.clip = project->clips[index], .index = index};
    return AF_RESULT_SUCCESS;
}

AfResult af_project_track_snapshot(const AfProject *project, uint64_t id,
                                   AfProjectTrackSnapshot *out_snapshot) {
    uint32_t index = 0U;
    if (project == NULL || out_snapshot == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    while (index < project->track_count && project->tracks[index].id != id) {
        ++index;
    }
    if (index == project->track_count) {
        return AF_RESULT_NOT_FOUND;
    }
    *out_snapshot = (AfProjectTrackSnapshot){.track = project->tracks[index], .index = index};
    return AF_RESULT_SUCCESS;
}

AfResult af_project_keyframe_snapshot(const AfProject *project, uint64_t id,
                                      AfProjectKeyframeSnapshot *out_snapshot) {
    uint32_t index = 0U;
    if (project == NULL || out_snapshot == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    while (index < project->keyframe_count && project->keyframes[index].id != id) {
        ++index;
    }
    if (index == project->keyframe_count) {
        return AF_RESULT_NOT_FOUND;
    }
    *out_snapshot =
        (AfProjectKeyframeSnapshot){.keyframe = project->keyframes[index], .index = index};
    return AF_RESULT_SUCCESS;
}

AfResult af_project_allocate_animation_id(AfProject *project, uint64_t *out_id) {
    if (project == NULL || out_id == NULL || project->next_animation_id == 0U ||
        project->next_animation_id >= UINT64_MAX - 1U) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    *out_id = project->next_animation_id++;
    return AF_RESULT_SUCCESS;
}

AfPath *af_project_find_path(AfProject *project, uint64_t path_id) {
    if (project == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < project->path_data.path_count; ++index) {
        if (project->path_data.paths[index].id == path_id) {
            return &project->path_data.paths[index];
        }
    }
    return NULL;
}

AfPath *af_project_find_node_path(AfProject *project, uint64_t node_id) {
    if (project == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < project->path_data.path_count; ++index) {
        if (project->path_data.paths[index].node_id == node_id) {
            return &project->path_data.paths[index];
        }
    }
    return NULL;
}

AfResult af_project_insert_path_snapshot(AfProject *project, const AfProjectPathSnapshot *snapshot,
                                         const AfPathCommand *commands) {
    uint32_t command_count = 0U;
    uint32_t command_start = 0U;
    if (project == NULL || snapshot == NULL || commands == NULL || snapshot->path.id == 0U ||
        snapshot->path.node_id == 0U || snapshot->path.command_count == 0U ||
        snapshot->path.command_count > AF_MAX_PATH_COMMANDS_PER_PATH ||
        snapshot->index > project->path_data.path_count ||
        project->path_data.path_count >= project->path_data.path_capacity ||
        snapshot->path.command_start > project->path_data.command_count ||
        snapshot->path.command_count >
            project->path_data.command_capacity - project->path_data.command_count ||
        af_project_find_path(project, snapshot->path.id) != NULL ||
        af_project_find_node_path(project, snapshot->path.node_id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    command_count = snapshot->path.command_count;
    command_start = snapshot->path.command_start;
    memmove(&project->path_data.commands[command_start + command_count],
            &project->path_data.commands[command_start],
            (size_t)(project->path_data.command_count - command_start) *
                sizeof(*project->path_data.commands));
    memcpy(&project->path_data.commands[command_start], commands,
           (size_t)command_count * sizeof(*commands));
    for (uint32_t index = 0U; index < project->path_data.path_count; ++index) {
        if (project->path_data.paths[index].command_start >= command_start) {
            project->path_data.paths[index].command_start += command_count;
        }
    }
    memmove(&project->path_data.paths[snapshot->index + 1U],
            &project->path_data.paths[snapshot->index],
            (size_t)(project->path_data.path_count - snapshot->index) *
                sizeof(*project->path_data.paths));
    project->path_data.paths[snapshot->index] = snapshot->path;
    ++project->path_data.path_count;
    project->path_data.command_count += command_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_project_add_path(AfProject *project, const AfPath *path,
                             const AfPathCommand *commands) {
    AfProjectPathSnapshot snapshot = {0};
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    if (project == NULL || path == NULL || commands == NULL) {
        return result;
    }
    snapshot = (AfProjectPathSnapshot){.path = *path, .index = project->path_data.path_count};
    snapshot.path.command_start = project->path_data.command_count;
    result = af_project_insert_path_snapshot(project, &snapshot, commands);
    if (result == AF_RESULT_SUCCESS && af_project_validate(project) != AF_RESULT_SUCCESS) {
        (void)af_project_remove_path_exact(project, path->id);
        result = AF_RESULT_INVALID_ARGUMENT;
    }
    return result;
}

AfResult af_project_remove_path_exact(AfProject *project, uint64_t path_id) {
    uint32_t index = 0U;
    uint32_t command_start = 0U;
    uint32_t command_count = 0U;
    if (project == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    while (index < project->path_data.path_count && project->path_data.paths[index].id != path_id) {
        ++index;
    }
    if (index == project->path_data.path_count) {
        return AF_RESULT_NOT_FOUND;
    }
    command_start = project->path_data.paths[index].command_start;
    command_count = project->path_data.paths[index].command_count;
    memmove(&project->path_data.commands[command_start],
            &project->path_data.commands[command_start + command_count],
            (size_t)(project->path_data.command_count - command_start - command_count) *
                sizeof(*project->path_data.commands));
    project->path_data.command_count -= command_count;
    for (uint32_t path_index = 0U; path_index < project->path_data.path_count; ++path_index) {
        if (path_index != index &&
            project->path_data.paths[path_index].command_start > command_start) {
            project->path_data.paths[path_index].command_start -= command_count;
        }
    }
    memmove(&project->path_data.paths[index], &project->path_data.paths[index + 1U],
            (size_t)(project->path_data.path_count - index - 1U) *
                sizeof(*project->path_data.paths));
    --project->path_data.path_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_project_path_snapshot(const AfProject *project, uint64_t path_id,
                                  AfProjectPathSnapshot *out_snapshot, AfPathCommand *out_commands,
                                  uint32_t command_capacity) {
    if (project == NULL || out_snapshot == NULL || out_commands == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < project->path_data.path_count; ++index) {
        const AfPath *path = &project->path_data.paths[index];
        if (path->id != path_id) {
            continue;
        }
        if (path->command_count > command_capacity) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        *out_snapshot = (AfProjectPathSnapshot){.path = *path, .index = index};
        memcpy(out_commands, &project->path_data.commands[path->command_start],
               (size_t)path->command_count * sizeof(*out_commands));
        return AF_RESULT_SUCCESS;
    }
    return AF_RESULT_NOT_FOUND;
}

static float af_project_bezier(float p1, float p2, float t) {
    const float inverse = 1.0F - t;
    return 3.0F * inverse * inverse * t * p1 + 3.0F * inverse * t * t * p2 + t * t * t;
}

static float af_project_cubic_ease(float progress, const float easing[4]) {
    float parameter = 0.0F;
    float low = 0.0F;
    float high = 1.0F;
    uint32_t iteration = 0U;
    if (progress <= 0.0F || progress >= 1.0F) {
        return progress;
    }
    for (iteration = 0U; iteration < 16U; ++iteration) {
        parameter = (low + high) * 0.5F;
        if (af_project_bezier(easing[0], easing[2], parameter) < progress) {
            low = parameter;
        } else {
            high = parameter;
        }
    }
    return af_project_bezier(easing[1], easing[3], (low + high) * 0.5F);
}

static AfResult af_project_sample_track(const AfProject *project, const AfProjectTrack *track,
                                        float time, float value[4]) {
    const AfProjectKeyframe *first = NULL;
    const AfProjectKeyframe *left = NULL;
    const AfProjectKeyframe *right = NULL;
    uint32_t index = 0U;
    float progress = 0.0F;
    for (index = 0U; index < project->keyframe_count; ++index) {
        const AfProjectKeyframe *key = &project->keyframes[index];
        if (key->track_id != track->id) {
            continue;
        }
        if (first == NULL || key->keyframe.time < first->keyframe.time ||
            (key->keyframe.time == first->keyframe.time && key->id < first->id)) {
            first = key;
        }
        if (key->keyframe.time <= time &&
            (left == NULL || key->keyframe.time > left->keyframe.time ||
             (key->keyframe.time == left->keyframe.time && key->id > left->id))) {
            left = key;
        }
        if (key->keyframe.time > time &&
            (right == NULL || key->keyframe.time < right->keyframe.time ||
             (key->keyframe.time == right->keyframe.time && key->id < right->id))) {
            right = key;
        }
    }
    if (first == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    if (left == NULL) {
        left = first;
    }
    if (right == NULL || left->keyframe.interpolation == AF_INTERPOLATION_STEP ||
        right->keyframe.time <= left->keyframe.time) {
        memcpy(value, left->keyframe.value, sizeof(left->keyframe.value));
        return AF_RESULT_SUCCESS;
    }
    progress = (time - left->keyframe.time) / (right->keyframe.time - left->keyframe.time);
    progress = progress < 0.0F ? 0.0F : (progress > 1.0F ? 1.0F : progress);
    if (left->keyframe.interpolation == AF_INTERPOLATION_CUBIC_BEZIER) {
        progress = af_project_cubic_ease(progress, left->keyframe.easing);
    }
    for (index = 0U; index < 4U; ++index) {
        value[index] = left->keyframe.value[index] +
                       (right->keyframe.value[index] - left->keyframe.value[index]) * progress;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_animation_apply(const AfProject *project, uint64_t clip_id, float time,
                                    AfScene *scene) {
    const AfProjectClip *clip = af_project_find_clip_const(project, clip_id);
    uint32_t index = 0U;
    if (clip == NULL || scene == NULL || !isfinite(time)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    time = time < 0.0F ? 0.0F : (time > clip->duration ? clip->duration : time);
    for (index = 0U; index < project->track_count; ++index) {
        const AfProjectTrack *track = &project->tracks[index];
        AfSceneNode *node = NULL;
        float value[4] = {0};
        AfResult result = AF_RESULT_SUCCESS;
        if (track->clip_id != clip_id) {
            continue;
        }
        node = af_scene_find(scene, track->node_id);
        if (node == NULL) {
            return AF_RESULT_NOT_FOUND;
        }
        result = af_project_sample_track(project, track, time, value);
        if (result == AF_RESULT_NOT_FOUND) {
            continue;
        }
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
        switch (track->property) {
        case AF_PROPERTY_POSITION:
            node->local_transform.position = (AfVec2){value[0], value[1]};
            break;
        case AF_PROPERTY_ROTATION:
            node->local_transform.rotation = value[0];
            break;
        case AF_PROPERTY_SCALE:
            node->local_transform.scale = (AfVec2){value[0], value[1]};
            break;
        case AF_PROPERTY_OPACITY:
            node->opacity = value[0];
            break;
        case AF_PROPERTY_FILL_COLOR:
            node->fill = (AfColor){value[0], value[1], value[2], value[3]};
            break;
        case AF_PROPERTY_CORNER_RADIUS:
            node->corner_radius = value[0];
            break;
        case AF_PROPERTY_PATH_POINT:
        case AF_PROPERTY_PATH_CONTROL_IN:
        case AF_PROPERTY_PATH_CONTROL_OUT: {
            AfPathData *paths = (AfPathData *)scene->paths;
            AfPath *path = af_path_find_node(paths, track->node_id);
            AfPathCommand *command = &paths->commands[path->command_start + track->element_index];
            AfVec2 *target =
                track->property == AF_PROPERTY_PATH_POINT
                    ? &command->point
                    : (track->property == AF_PROPERTY_PATH_CONTROL_IN ? &command->control2
                                                                      : &command->control1);
            if (target->x != value[0] || target->y != value[1]) {
                *target = (AfVec2){value[0], value[1]};
                if (af_path_mark_geometry_changed(path) != AF_RESULT_SUCCESS) {
                    return AF_RESULT_LIMIT_EXCEEDED;
                }
            }
            break;
        }
        default:
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    return af_scene_evaluate_world(scene);
}
