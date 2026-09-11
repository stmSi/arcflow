#include "animation/animation.h"

#include <math.h>
#include <stddef.h>

static float af_clamp01(float value) { return value < 0.0F ? 0.0F : (value > 1.0F ? 1.0F : value); }

static float af_bezier(float p1, float p2, float t) {
    const float inverse = 1.0F - t;
    return 3.0F * inverse * inverse * t * p1 + 3.0F * inverse * t * t * p2 + t * t * t;
}

static float af_cubic_ease(float progress, const float easing[4]) {
    float parameter = 0.0F;
    float low = 0.0F;
    float high = 1.0F;
    uint32_t iteration = 0U;
    if (progress <= 0.0F || progress >= 1.0F) {
        return progress;
    }
    for (iteration = 0U; iteration < 16U; ++iteration) {
        parameter = (low + high) * 0.5F;
        if (af_bezier(easing[0], easing[2], parameter) < progress) {
            low = parameter;
        } else {
            high = parameter;
        }
    }
    return af_bezier(easing[1], easing[3], (low + high) * 0.5F);
}

static bool af_animation_path_property(AfPropertyType property) {
    return property >= AF_PROPERTY_PATH_POINT && property <= AF_PROPERTY_PATH_CONTROL_OUT;
}

static bool af_animation_track_target_valid(const AfAnimationTrack *track, const AfScene *scene) {
    const AfSceneNode *node = af_scene_find_const(scene, track->node_id);
    const AfPath *path = NULL;
    const AfPathCommand *command = NULL;
    if (node == NULL) {
        return false;
    }
    if (!af_animation_path_property(track->property)) {
        return track->element_index == 0U;
    }
    path = af_path_find_node_const(scene->paths, track->node_id);
    if (node->type != AF_NODE_PATH || path == NULL || track->element_index >= path->command_count) {
        return false;
    }
    command = &scene->paths->commands[path->command_start + track->element_index];
    if (command->type == AF_PATH_CLOSE) {
        return false;
    }
    return track->property == AF_PROPERTY_PATH_POINT || command->type == AF_PATH_CUBIC;
}

AfResult af_animation_validate(const AfAnimationData *animation, const AfScene *scene) {
    uint32_t clip_index = 0U;
    uint32_t track_index = 0U;
    if (animation == NULL || scene == NULL || animation->clip_count > AF_MAX_CLIPS ||
        animation->track_count > AF_MAX_TRACKS || animation->keyframe_count > AF_MAX_KEYFRAMES ||
        (animation->clip_count != 0U && animation->clips == NULL) ||
        (animation->track_count != 0U && animation->tracks == NULL) ||
        (animation->keyframe_count != 0U && animation->keyframes == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (clip_index = 0U; clip_index < animation->clip_count; ++clip_index) {
        const AfAnimationClip *clip = &animation->clips[clip_index];
        uint32_t other = 0U;
        if (clip->id == 0U || clip->duration < 0.0F || clip->track_start > animation->track_count ||
            clip->track_count > animation->track_count - clip->track_start) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (other = clip_index + 1U; other < animation->clip_count; ++other) {
            if (clip->id == animation->clips[other].id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    for (track_index = 0U; track_index < animation->track_count; ++track_index) {
        const AfAnimationTrack *track = &animation->tracks[track_index];
        uint32_t key_index = 0U;
        uint32_t other = 0U;
        float last_time = -INFINITY;
        if (track->id == 0U || track->property > AF_PROPERTY_PATH_CONTROL_OUT ||
            !af_animation_track_target_valid(track, scene) ||
            track->keyframe_start > animation->keyframe_count ||
            track->keyframe_count > animation->keyframe_count - track->keyframe_start) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (other = track_index + 1U; other < animation->track_count; ++other) {
            if (track->id == animation->tracks[other].id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        for (key_index = 0U; key_index < track->keyframe_count; ++key_index) {
            const AfKeyframe *key = &animation->keyframes[track->keyframe_start + key_index];
            uint32_t component = 0U;
            if (!isfinite(key->time) || key->time < last_time ||
                key->interpolation > AF_INTERPOLATION_CUBIC_BEZIER) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            for (component = 0U; component < 4U; ++component) {
                if (!isfinite(key->value[component]) || !isfinite(key->easing[component])) {
                    return AF_RESULT_INVALID_ARGUMENT;
                }
            }
            if (key->interpolation == AF_INTERPOLATION_CUBIC_BEZIER &&
                (key->easing[0] < 0.0F || key->easing[0] > 1.0F || key->easing[2] < 0.0F ||
                 key->easing[2] > 1.0F)) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            last_time = key->time;
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_animation_sample_track(const AfAnimationData *animation, const AfAnimationTrack *track,
                                   float time, float out_value[4]) {
    const AfKeyframe *keys = NULL;
    uint32_t left_index = 0U;
    uint32_t component = 0U;
    float progress = 0.0F;
    if (animation == NULL || track == NULL || out_value == NULL ||
        track->keyframe_start > animation->keyframe_count ||
        track->keyframe_count > animation->keyframe_count - track->keyframe_start) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (track->keyframe_count == 0U) {
        return AF_RESULT_NOT_FOUND;
    }
    keys = &animation->keyframes[track->keyframe_start];
    if (time < keys[0].time) {
        for (component = 0U; component < 4U; ++component) {
            out_value[component] = keys[0].value[component];
        }
        return AF_RESULT_SUCCESS;
    }
    /* Upper bound preserves the last key at duplicate timestamps, including stepped tracks. */
    uint32_t end = track->keyframe_count;
    while (left_index + 1U < end) {
        const uint32_t middle = left_index + (end - left_index) / 2U;
        if (keys[middle].time <= time) {
            left_index = middle;
        } else {
            end = middle;
        }
    }
    if (left_index + 1U == track->keyframe_count ||
        keys[left_index].interpolation == AF_INTERPOLATION_STEP) {
        for (component = 0U; component < 4U; ++component) {
            out_value[component] = keys[left_index].value[component];
        }
        return AF_RESULT_SUCCESS;
    }
    if (keys[left_index + 1U].time <= keys[left_index].time) {
        progress = 1.0F;
    } else {
        progress = af_clamp01((time - keys[left_index].time) /
                              (keys[left_index + 1U].time - keys[left_index].time));
    }
    if (keys[left_index].interpolation == AF_INTERPOLATION_CUBIC_BEZIER) {
        progress = af_cubic_ease(progress, keys[left_index].easing);
    }
    for (component = 0U; component < 4U; ++component) {
        out_value[component] =
            keys[left_index].value[component] +
            (keys[left_index + 1U].value[component] - keys[left_index].value[component]) * progress;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_animation_apply(const AfAnimationData *animation, const AfAnimationClip *clip,
                            float time, AfScene *scene) {
    uint32_t track_index = 0U;
    if (animation == NULL || clip == NULL || scene == NULL ||
        clip->track_start > animation->track_count ||
        clip->track_count > animation->track_count - clip->track_start) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (track_index = 0U; track_index < clip->track_count; ++track_index) {
        const AfAnimationTrack *track = &animation->tracks[clip->track_start + track_index];
        AfSceneNode *node = af_scene_find(scene, track->node_id);
        float value[4] = {0};
        AfResult result = AF_RESULT_SUCCESS;
        if (node == NULL) {
            return AF_RESULT_NOT_FOUND;
        }
        result = af_animation_sample_track(animation, track, time, value);
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
        case AF_PROPERTY_SKEW:
            node->local_transform.skew = (AfVec2){value[0], value[1]};
            break;
        case AF_PROPERTY_OPACITY:
            node->opacity = af_clamp01(value[0]);
            break;
        case AF_PROPERTY_FILL_COLOR:
            node->fill = (AfColor){value[0], value[1], value[2], value[3]};
            break;
        case AF_PROPERTY_CORNER_RADIUS:
            node->corner_radius = value[0] < 0.0F ? 0.0F : value[0];
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
        }
    }
    return af_scene_evaluate_world(scene);
}

void af_playback_play(AfPlayback *playback) {
    if (playback != NULL) {
        playback->playing = true;
    }
}

void af_playback_pause(AfPlayback *playback) {
    if (playback != NULL) {
        playback->playing = false;
    }
}

void af_playback_stop(AfPlayback *playback) {
    if (playback != NULL) {
        playback->playing = false;
        playback->time = 0.0F;
    }
}

void af_playback_seek(AfPlayback *playback, const AfAnimationClip *clip, float time) {
    if (playback == NULL || clip == NULL) {
        return;
    }
    playback->time = time < 0.0F ? 0.0F : (time > clip->duration ? clip->duration : time);
}

void af_playback_update(AfPlayback *playback, const AfAnimationClip *clip, float delta_time) {
    float next = 0.0F;
    if (playback == NULL || clip == NULL || !playback->playing || clip->duration <= 0.0F) {
        return;
    }
    next = playback->time + delta_time * playback->speed * (float)playback->direction;
    if (clip->loop) {
        next = fmodf(next, clip->duration);
        if (next < 0.0F) {
            next += clip->duration;
        }
    } else if (next >= clip->duration) {
        next = clip->duration;
        playback->playing = false;
    } else if (next <= 0.0F) {
        next = 0.0F;
        playback->playing = false;
    }
    playback->time = next;
}
