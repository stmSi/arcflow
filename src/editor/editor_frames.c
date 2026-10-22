#include "editor/editor_frames.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

uint32_t af_editor_frame_rate(const AfProjectClip *clip) {
    return clip != NULL && clip->frame_rate ? clip->frame_rate : 24U;
}

const AfProjectTrack *af_editor_frame_at(const AfProject *project, uint64_t clip_id,
                                         uint32_t frame) {
    for (uint32_t i = 0; i < project->track_count; ++i) {
        const AfProjectTrack *track = &project->tracks[i];
        if (track->clip_id == clip_id && track->cel_length && frame >= track->cel_frame &&
            frame - track->cel_frame < track->cel_length)
            return track;
    }
    return NULL;
}

const AfProjectTrack *af_editor_frame_owner(const AfProject *project, uint64_t node_id) {
    for (uint32_t depth = 0; node_id && depth < project->scene.count; ++depth) {
        for (uint32_t i = 0; i < project->track_count; ++i)
            if (project->tracks[i].cel_length && project->tracks[i].node_id == node_id)
                return &project->tracks[i];
        const AfSceneNode *node = af_scene_find_const(&project->scene, node_id);
        if (node == NULL)
            break;
        node_id = node->parent_id;
    }
    return NULL;
}

static bool af_frame_space(const AfProject *project, uint64_t clip_id, uint64_t ignore,
                           uint32_t frame, uint32_t length) {
    if (!length || frame > 10368000U || length > 10368000U - frame)
        return false;
    for (uint32_t i = 0; i < project->track_count; ++i) {
        const AfProjectTrack *track = &project->tracks[i];
        if (track->clip_id == clip_id && track->id != ignore && track->cel_length &&
            frame < track->cel_frame + track->cel_length && track->cel_frame < frame + length)
            return false;
    }
    return true;
}

static AfResult af_frame_keys(AfProject *project, const AfProjectTrack *track, uint32_t fps,
                              AfEditorCommand *command) {
    const uint32_t count = track->cel_frame ? 3U : 2U;
    if (command->keyframe_after_count + count > AF_EDITOR_COMMAND_MAX_KEYFRAMES)
        return AF_RESULT_LIMIT_EXCEEDED;
    for (uint32_t i = 0; i < count; ++i) {
        AfProjectKeyframeSnapshot *key = &command->keyframe_after[command->keyframe_after_count++];
        AfResult result = af_project_allocate_animation_id(project, &key->keyframe.id);
        if (result != AF_RESULT_SUCCESS)
            return result;
        key->keyframe.track_id = track->id;
        key->keyframe.keyframe.interpolation = AF_INTERPOLATION_STEP;
        key->keyframe.keyframe.time =
            i == count - 1 ? (float)(track->cel_frame + track->cel_length) / (float)fps
                           : (i == count - 2 ? (float)track->cel_frame / (float)fps : 0);
        key->keyframe.keyframe.value[0] = i == count - 2 ? 1.0F : 0.0F;
        key->index = project->keyframe_count - command->keyframe_before_count +
                     command->keyframe_after_count - 1;
    }
    return AF_RESULT_SUCCESS;
}

static void af_frame_extend_clip(AfProjectClip *clip, uint32_t end) {
    float duration = (float)end / (float)af_editor_frame_rate(clip);
    if (duration > clip->duration) {
        if (clip->loop_end == clip->duration)
            clip->loop_end = duration;
        clip->duration = duration;
    }
}

AfResult af_editor_command_frame(AfProject *project, uint64_t clip_id, uint32_t frame,
                                 uint64_t source_node, AfEditorCommand *command) {
    const AfProjectClip *clip = af_project_find_clip_const(project, clip_id);
    if (clip == NULL || !af_frame_space(project, clip_id, 0, frame, 1))
        return AF_RESULT_INVALID_ARGUMENT;
    memset(command, 0, sizeof(*command));
    command->type = AF_EDITOR_COMMAND_CREATE;
    AfProjectNodeSnapshot sources[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint32_t count = 1;
    if (source_node) {
        const AfProjectTrack *source = af_editor_frame_owner(project, source_node);
        if (!source || source->node_id != source_node)
            return AF_RESULT_INVALID_ARGUMENT;
        AfResult result = af_project_collect_subtree(project, source_node, sources,
                                                     AF_EDITOR_COMMAND_MAX_SNAPSHOTS, &count);
        if (result != AF_RESULT_SUCCESS)
            return result;
    }
    uint64_t ids[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    for (uint32_t i = 0; i < count; ++i) {
        AfResult result = af_project_allocate_id(project, &ids[i]);
        if (result != AF_RESULT_SUCCESS)
            return result;
    }
    command->after_count = count;
    for (uint32_t i = 0; i < count; ++i) {
        AfProjectNodeSnapshot *copy = &command->after[i];
        if (source_node)
            *copy = sources[i];
        else {
            copy->node = (AfSceneNode){.type = AF_NODE_GROUP,
                                       .visible = true,
                                       .local_transform = {.scale = {1, 1}},
                                       .opacity = 0};
            copy->style = af_style_default(ids[i], ids[i], (AfColor){1, 1, 1, 1});
        }
        /* Particle simulations are not drawing cels. */
        if (copy->has_emitter)
            return AF_RESULT_INVALID_ARGUMENT;
        copy->index = project->scene.count + i;
        copy->node.id = ids[i];
        copy->style.id = ids[i];
        copy->style.node_id = ids[i];
        if (source_node && copy->style.parent_style_id)
            for (uint32_t j = 0; j < count; ++j)
                if (copy->style.parent_style_id == sources[j].style.id) {
                    copy->style.parent_style_id = ids[j];
                    break;
                }
        copy->layout.node_id = ids[i];
        copy->image_binding.node_id = ids[i];
        if (i == 0) {
            copy->node.parent_id = AF_INVALID_NODE_ID;
            copy->node.opacity = 0;
            snprintf(copy->name, sizeof(copy->name), "Drawing %u", frame + 1);
        } else {
            for (uint32_t j = 0; j < count; ++j)
                if (sources[i].node.parent_id == sources[j].node.id) {
                    copy->node.parent_id = ids[j];
                    break;
                }
        }
        const AfPath *path =
            source_node ? af_path_find_node_const(&project->path_data, sources[i].node.id) : NULL;
        if (path) {
            if (command->path_after_count >= AF_EDITOR_COMMAND_MAX_PATHS ||
                path->command_count >
                    AF_EDITOR_COMMAND_MAX_PATH_COMMANDS - command->path_command_after_count)
                return AF_RESULT_LIMIT_EXCEEDED;
            AfProjectPathSnapshot *out = &command->path_after[command->path_after_count];
            *out = (AfProjectPathSnapshot){.path = *path,
                                           .index = project->path_data.path_count +
                                                    command->path_after_count,
                                           .command_offset = command->path_command_after_count};
            out->path.id = out->path.node_id = ids[i];
            out->path.command_start =
                project->path_data.command_count + command->path_command_after_count;
            memcpy(&command->path_command_after[command->path_command_after_count],
                   &project->path_data.commands[path->command_start],
                   path->command_count * sizeof(AfPathCommand));
            command->path_command_after_count += path->command_count;
            ++command->path_after_count;
        }
    }
    command->target_id = ids[0];
    command->clip_before_count = command->clip_after_count = 1;
    (void)af_project_clip_snapshot(project, clip_id, &command->clip_before[0]);
    command->clip_after[0] = command->clip_before[0];
    af_frame_extend_clip(&command->clip_after[0].clip, frame + 1);
    if (command->clip_after[0].clip.duration > 86400)
        return AF_RESULT_LIMIT_EXCEEDED;
    command->track_after_count = 1;
    AfProjectTrackSnapshot *track = &command->track_after[0];
    track->index = project->track_count;
    track->track = (AfProjectTrack){.clip_id = clip_id,
                                    .node_id = ids[0],
                                    .property = AF_PROPERTY_OPACITY,
                                    .cel_frame = frame,
                                    .cel_length = 1};
    AfResult result = af_project_allocate_animation_id(project, &track->track.id);
    if (result == AF_RESULT_SUCCESS)
        result = af_frame_keys(project, &track->track, af_editor_frame_rate(clip), command);
    return result;
}

static AfResult af_frame_capture_keys(const AfProject *project, uint64_t track_id,
                                      AfEditorCommand *command) {
    for (uint32_t i = 0; i < project->keyframe_count; ++i) {
        if (project->keyframes[i].track_id != track_id)
            continue;
        if (command->keyframe_before_count == AF_EDITOR_COMMAND_MAX_KEYFRAMES)
            return AF_RESULT_LIMIT_EXCEEDED;
        command->keyframe_before[command->keyframe_before_count++] =
            (AfProjectKeyframeSnapshot){.keyframe = project->keyframes[i], .index = i};
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_command_frame_hold(AfProject *project, uint64_t track_id, uint32_t length,
                                      AfEditorCommand *command) {
    const AfProjectTrack *track = af_project_find_track_const(project, track_id);
    if (!track || !track->cel_length ||
        !af_frame_space(project, track->clip_id, track_id, track->cel_frame, length))
        return AF_RESULT_INVALID_ARGUMENT;
    memset(command, 0, sizeof(*command));
    command->type = AF_EDITOR_COMMAND_ANIMATION;
    command->target_id = track_id;
    command->track_before_count = command->track_after_count = 1;
    (void)af_project_track_snapshot(project, track_id, &command->track_before[0]);
    command->track_after[0] = command->track_before[0];
    command->track_after[0].track.cel_length = length;
    command->clip_before_count = command->clip_after_count = 1;
    (void)af_project_clip_snapshot(project, track->clip_id, &command->clip_before[0]);
    command->clip_after[0] = command->clip_before[0];
    af_frame_extend_clip(&command->clip_after[0].clip, track->cel_frame + length);
    if (command->clip_after[0].clip.duration > 86400)
        return AF_RESULT_LIMIT_EXCEEDED;
    AfResult result = af_frame_capture_keys(project, track_id, command);
    if (result == AF_RESULT_SUCCESS)
        result = af_frame_keys(project, &command->track_after[0].track,
                               af_editor_frame_rate(&command->clip_after[0].clip), command);
    return result;
}

AfResult af_editor_command_frame_rate(AfProject *project, uint64_t clip_id, uint32_t fps,
                                      AfEditorCommand *command) {
    if (!fps || fps > 120 || !af_project_find_clip_const(project, clip_id))
        return AF_RESULT_INVALID_ARGUMENT;
    memset(command, 0, sizeof(*command));
    command->type = AF_EDITOR_COMMAND_ANIMATION;
    command->target_id = clip_id;
    command->clip_before_count = command->clip_after_count = 1;
    (void)af_project_clip_snapshot(project, clip_id, &command->clip_before[0]);
    command->clip_after[0] = command->clip_before[0];
    command->clip_after[0].clip.frame_rate = fps;
    /* Capture every old key first so insertion indices remain valid. */
    for (uint32_t i = 0; i < project->track_count; ++i) {
        const AfProjectTrack *track = &project->tracks[i];
        if (track->clip_id != clip_id || !track->cel_length)
            continue;
        AfResult result = af_frame_capture_keys(project, track->id, command);
        if (result != AF_RESULT_SUCCESS)
            return result;
    }
    for (uint32_t i = 0; i < project->track_count; ++i) {
        const AfProjectTrack *track = &project->tracks[i];
        if (track->clip_id != clip_id || !track->cel_length)
            continue;
        af_frame_extend_clip(&command->clip_after[0].clip, track->cel_frame + track->cel_length);
        AfResult result = af_frame_keys(project, track, fps, command);
        if (result != AF_RESULT_SUCCESS)
            return result;
    }
    return command->clip_after[0].clip.duration <= 86400 ? AF_RESULT_SUCCESS
                                                         : AF_RESULT_LIMIT_EXCEEDED;
}
