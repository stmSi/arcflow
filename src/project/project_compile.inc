AfResult af_project_save_file(const AfProject *project, const char *path, AfAllocator *allocator) {
    AfPackageBlob text = {0};
    AfResult result = AF_RESULT_SUCCESS;
    if (path == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_project_serialize(project, allocator, &text);
    if (result == AF_RESULT_SUCCESS) {
        result = af_file_write(path, text.data, text.size);
    }
    af_package_blob_destroy(allocator, &text);
    return result;
}

AfResult af_project_load_file(const char *path, AfAllocator *allocator, AfProject *out_project) {
    AfFileData file = {0};
    AfResult result = af_file_read_limited(path, AF_PROJECT_MAX_FILE_SIZE, allocator, &file);
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_deserialize(file.data, file.size, allocator, out_project);
    }
    af_file_data_destroy(allocator, &file);
    if (result == AF_RESULT_SUCCESS) {
        uint32_t missing_count = 0U;
        (void)af_project_reimport_images(out_project, path, &missing_count);
    }
    return result;
}

static int af_project_keyframe_compare(const void *left_value, const void *right_value) {
    const AfProjectKeyframe *left = left_value;
    const AfProjectKeyframe *right = right_value;
    if (left->track_id != right->track_id) {
        return left->track_id < right->track_id ? -1 : 1;
    }
    if (left->keyframe.time != right->keyframe.time) {
        return left->keyframe.time < right->keyframe.time ? -1 : 1;
    }
    if (left->id == right->id) {
        return 0;
    }
    return left->id < right->id ? -1 : 1;
}

AfResult af_project_compile(const AfProject *project, AfAllocator *allocator,
                            AfPackageBlob *out_package) {
    AfSceneNode *ir_nodes = NULL;
    AfAnimationClip *ir_clips = NULL;
    AfAnimationTrack *ir_tracks = NULL;
    AfKeyframe *ir_keyframes = NULL;
    AfProjectKeyframe *ordered_keys = NULL;
    AfScene ir_scene = {0};
    AfAnimationData ir_animation = {0};
    AfPackageSource source = {0};
    AfImageData ir_images = {0};
    AfResult result = AF_RESULT_SUCCESS;
    if (allocator == NULL || out_package == NULL ||
        af_project_validate(project) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    ir_nodes =
        af_allocate_array(allocator, project->scene.count, sizeof(*ir_nodes), alignof(AfSceneNode));
    ir_clips = af_allocate_array(allocator, project->clip_count, sizeof(*ir_clips),
                                 alignof(AfAnimationClip));
    ir_tracks = af_allocate_array(allocator, project->track_count, sizeof(*ir_tracks),
                                  alignof(AfAnimationTrack));
    ir_keyframes = af_allocate_array(allocator, project->keyframe_count, sizeof(*ir_keyframes),
                                     alignof(AfKeyframe));
    ordered_keys = af_allocate_array(allocator, project->keyframe_count, sizeof(*ordered_keys),
                                     alignof(AfProjectKeyframe));
    if ((project->scene.count != 0U && ir_nodes == NULL) ||
        (project->clip_count != 0U && ir_clips == NULL) ||
        (project->track_count != 0U && ir_tracks == NULL) ||
        (project->keyframe_count != 0U && (ir_keyframes == NULL || ordered_keys == NULL))) {
        af_deallocate(allocator, ordered_keys);
        af_deallocate(allocator, ir_keyframes);
        af_deallocate(allocator, ir_tracks);
        af_deallocate(allocator, ir_clips);
        af_deallocate(allocator, ir_nodes);
        return AF_RESULT_OUT_OF_MEMORY;
    }
    if (project->scene.count != 0U) {
        memcpy(ir_nodes, project->scene.nodes, (size_t)project->scene.count * sizeof(*ir_nodes));
    }
    ir_scene = (AfScene){.nodes = ir_nodes,
                         .count = project->scene.count,
                         .capacity = project->scene.count,
                         .paths = &project->path_data,
                         .styles = &project->style_data,
                         .layouts = &project->layout_data,
                         .particles = &project->particle_data};
    result = af_image_data_clone(&project->image_data, allocator, &ir_images);
    if (result == AF_RESULT_SUCCESS) {
        result = af_image_build_atlases(&ir_images);
    }
    if (result == AF_RESULT_SUCCESS) {
        ir_scene.images = &ir_images;
    }
    if (project->keyframe_count != 0U) {
        memcpy(ordered_keys, project->keyframes,
               (size_t)project->keyframe_count * sizeof(*ordered_keys));
        qsort(ordered_keys, project->keyframe_count, sizeof(*ordered_keys),
              af_project_keyframe_compare);
    }
    {
        uint32_t clip_index = 0U;
        uint32_t track_write = 0U;
        uint32_t key_write = 0U;
        for (clip_index = 0U; clip_index < project->clip_count; ++clip_index) {
            const AfProjectClip *source_clip = &project->clips[clip_index];
            uint32_t track_index = 0U;
            const uint32_t track_start = track_write;
            for (track_index = 0U; track_index < project->track_count; ++track_index) {
                const AfProjectTrack *source_track = &project->tracks[track_index];
                uint32_t key_index = 0U;
                const uint32_t key_start = key_write;
                if (source_track->clip_id != source_clip->id) {
                    continue;
                }
                for (key_index = 0U; key_index < project->keyframe_count; ++key_index) {
                    if (ordered_keys[key_index].track_id == source_track->id) {
                        ir_keyframes[key_write++] = ordered_keys[key_index].keyframe;
                    }
                }
                ir_tracks[track_write++] =
                    (AfAnimationTrack){.id = source_track->id,
                                       .node_id = source_track->node_id,
                                       .property = source_track->property,
                                       .keyframe_start = key_start,
                                       .keyframe_count = key_write - key_start,
                                       .element_index = source_track->element_index};
            }
            ir_clips[clip_index] = (AfAnimationClip){.id = source_clip->id,
                                                     .duration = source_clip->duration,
                                                     .track_start = track_start,
                                                     .track_count = track_write - track_start,
                                                     .loop = source_clip->loop};
        }
        ir_animation = (AfAnimationData){.clips = ir_clips,
                                         .clip_count = project->clip_count,
                                         .tracks = ir_tracks,
                                         .track_count = track_write,
                                         .keyframes = ir_keyframes,
                                         .keyframe_count = key_write};
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_scene_validate(&ir_scene);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_animation_validate(&ir_animation, &ir_scene);
    }
    if (result == AF_RESULT_SUCCESS) {
        source = (AfPackageSource){.scene = &ir_scene, .animation = &ir_animation};
        result = af_package_write(&source, allocator, out_package);
    }
    af_deallocate(allocator, ordered_keys);
    af_deallocate(allocator, ir_keyframes);
    af_deallocate(allocator, ir_tracks);
    af_deallocate(allocator, ir_clips);
    af_deallocate(allocator, ir_nodes);
    af_image_data_destroy(&ir_images);
    return result;
}
