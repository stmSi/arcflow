#include "animation/advanced_animation.h"

#include <math.h>
#include <string.h>

static float af_adv_clamp(float value, float minimum, float maximum) {
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

static AfAnimationClip *af_adv_clip(AfAnimationData *animation, uint64_t id) {
    if (animation == NULL || id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < animation->clip_count; ++index) {
        if (animation->clips[index].id == id) {
            return &animation->clips[index];
        }
    }
    return NULL;
}

static const AfAnimationClip *af_adv_clip_const(const AfAnimationData *animation, uint64_t id) {
    return af_adv_clip((AfAnimationData *)animation, id);
}

static bool af_adv_nested_visit(const AfAdvancedAnimationData *advanced, uint64_t clip_id,
                                uint64_t *stack, uint32_t depth) {
    if (depth >= AF_MAX_NESTED_TIMELINE_DEPTH) {
        return false;
    }
    for (uint32_t index = 0U; index < depth; ++index) {
        if (stack[index] == clip_id) {
            return false;
        }
    }
    stack[depth] = clip_id;
    for (uint32_t index = 0U; index < advanced->nested_count; ++index) {
        if (advanced->nested[index].owner_clip_id == clip_id &&
            !af_adv_nested_visit(advanced, advanced->nested[index].child_clip_id, stack,
                                 depth + 1U)) {
            return false;
        }
    }
    return true;
}

AfResult af_advanced_animation_validate(const AfAdvancedAnimationData *advanced,
                                        const AfAnimationData *animation) {
    uint64_t stack[AF_MAX_NESTED_TIMELINE_DEPTH] = {0};
    if (advanced == NULL || animation == NULL || advanced->marker_count > AF_MAX_CLIP_MARKERS ||
        advanced->region_count > AF_MAX_TIMELINE_REGIONS ||
        advanced->nested_count > AF_MAX_NESTED_TIMELINES ||
        advanced->layer_count > AF_MAX_ANIMATION_LAYERS ||
        advanced->speed_point_count > AF_MAX_SPEED_POINTS ||
        advanced->marker_count > advanced->marker_capacity ||
        advanced->region_count > advanced->region_capacity ||
        advanced->nested_count > advanced->nested_capacity ||
        advanced->layer_count > advanced->layer_capacity ||
        advanced->speed_point_count > advanced->speed_point_capacity ||
        (advanced->marker_count != 0U && advanced->markers == NULL) ||
        (advanced->region_count != 0U && advanced->regions == NULL) ||
        (advanced->nested_count != 0U && advanced->nested == NULL) ||
        (advanced->layer_count != 0U && advanced->layers == NULL) ||
        (advanced->speed_point_count != 0U && advanced->speed_points == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < advanced->marker_count; ++index) {
        const AfClipMarker *marker = &advanced->markers[index];
        const AfAnimationClip *clip = af_adv_clip_const(animation, marker->clip_id);
        if (clip == NULL || marker->event_id == 0U || !isfinite(marker->time) ||
            marker->time < 0.0F || marker->time > clip->duration) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (uint32_t index = 0U; index < advanced->region_count; ++index) {
        const AfTimelineRegion *region = &advanced->regions[index];
        const AfAnimationClip *clip = af_adv_clip_const(animation, region->clip_id);
        if (clip == NULL || region->id == 0U || !isfinite(region->start_time) ||
            !isfinite(region->end_time) || region->start_time < 0.0F ||
            region->start_time >= region->end_time || region->end_time > clip->duration) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (uint32_t index = 0U; index < advanced->nested_count; ++index) {
        const AfNestedTimeline *nested = &advanced->nested[index];
        const AfAnimationClip *owner = af_adv_clip_const(animation, nested->owner_clip_id);
        const AfAnimationClip *child = af_adv_clip_const(animation, nested->child_clip_id);
        if (owner == NULL || child == NULL || owner == child || !isfinite(nested->start_time) ||
            !isfinite(nested->duration) || !isfinite(nested->child_start_time) ||
            !isfinite(nested->child_end_time) || !isfinite(nested->speed) ||
            nested->start_time < 0.0F || nested->duration <= 0.0F ||
            nested->start_time + nested->duration > owner->duration ||
            nested->child_start_time < 0.0F || nested->child_start_time > nested->child_end_time ||
            nested->child_end_time > child->duration || nested->speed == 0.0F) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (uint32_t index = 0U; index < animation->clip_count; ++index) {
        if (!af_adv_nested_visit(advanced, animation->clips[index].id, stack, 0U)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (uint32_t index = 0U; index < advanced->layer_count; ++index) {
        const AfAnimationLayer *layer = &advanced->layers[index];
        if (af_adv_clip_const(animation, layer->clip_id) == NULL || !isfinite(layer->time) ||
            !isfinite(layer->weight) || layer->weight < 0.0F || layer->weight > 1.0F ||
            !isfinite(layer->transition_duration) || layer->transition_duration < 0.0F) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (uint32_t index = 0U; index < advanced->speed_point_count; ++index) {
        const AfSpeedPoint *point = &advanced->speed_points[index];
        const AfAnimationClip *clip = af_adv_clip_const(animation, point->clip_id);
        if (clip == NULL || !isfinite(point->time) || !isfinite(point->speed) ||
            point->time < 0.0F || point->time > clip->duration || point->speed == 0.0F) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_advanced_clip_duplicate(AfAnimationData *animation, uint32_t clip_capacity,
                                    uint32_t track_capacity, uint32_t keyframe_capacity,
                                    uint64_t source_clip_id, uint64_t new_clip_id,
                                    uint64_t first_track_id) {
    const AfAnimationClip *source = af_adv_clip_const(animation, source_clip_id);
    AfAnimationClip copy = {0};
    uint32_t key_count = 0U;
    if (animation == NULL || source == NULL || new_clip_id == 0U || first_track_id == 0U ||
        animation->clip_count > clip_capacity || animation->track_count > track_capacity ||
        animation->keyframe_count > keyframe_capacity ||
        af_adv_clip(animation, new_clip_id) != NULL || animation->clip_count >= clip_capacity ||
        source->track_count > track_capacity - animation->track_count) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    for (uint32_t index = 0U; index < source->track_count; ++index) {
        key_count += animation->tracks[source->track_start + index].keyframe_count;
    }
    if (key_count > keyframe_capacity - animation->keyframe_count) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    copy = *source;
    copy.id = new_clip_id;
    copy.track_start = animation->track_count;
    for (uint32_t index = 0U; index < source->track_count; ++index) {
        const AfAnimationTrack *source_track = &animation->tracks[source->track_start + index];
        AfAnimationTrack *track = &animation->tracks[animation->track_count++];
        *track = *source_track;
        track->id = first_track_id + index;
        track->keyframe_start = animation->keyframe_count;
        if (source_track->keyframe_count != 0U) {
            memcpy(&animation->keyframes[animation->keyframe_count],
                   &animation->keyframes[source_track->keyframe_start],
                   (size_t)source_track->keyframe_count * sizeof(AfKeyframe));
            animation->keyframe_count += source_track->keyframe_count;
        }
    }
    animation->clips[animation->clip_count++] = copy;
    return AF_RESULT_SUCCESS;
}

AfResult af_advanced_clip_trim(AfAnimationData *animation, uint64_t clip_id, float start_time,
                               float end_time) {
    AfAnimationClip *clip = af_adv_clip(animation, clip_id);
    if (clip == NULL || !isfinite(start_time) || !isfinite(end_time) || start_time < 0.0F ||
        start_time >= end_time || end_time > clip->duration) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < clip->track_count; ++index) {
        AfAnimationTrack *track = &animation->tracks[clip->track_start + index];
        for (uint32_t key = 0U; key < track->keyframe_count; ++key) {
            AfKeyframe *frame = &animation->keyframes[track->keyframe_start + key];
            frame->time = af_adv_clamp(frame->time, start_time, end_time) - start_time;
        }
    }
    clip->duration = end_time - start_time;
    return AF_RESULT_SUCCESS;
}

AfResult af_advanced_clip_split(AfAnimationData *animation, uint32_t clip_capacity,
                                uint32_t track_capacity, uint32_t keyframe_capacity,
                                uint64_t clip_id, uint64_t new_clip_id, uint64_t first_track_id,
                                float split_time) {
    AfAnimationClip *clip = af_adv_clip(animation, clip_id);
    float duration = 0.0F;
    AfResult result = AF_RESULT_SUCCESS;
    if (clip == NULL || !isfinite(split_time) || split_time <= 0.0F ||
        split_time >= clip->duration) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    duration = clip->duration;
    result = af_advanced_clip_duplicate(animation, clip_capacity, track_capacity, keyframe_capacity,
                                        clip_id, new_clip_id, first_track_id);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    clip = af_adv_clip(animation, clip_id);
    result = af_advanced_clip_trim(animation, clip_id, 0.0F, split_time);
    if (result == AF_RESULT_SUCCESS) {
        result = af_advanced_clip_trim(animation, new_clip_id, split_time, duration);
    }
    return result;
}

AfResult af_advanced_retime_keys(AfAnimationData *animation, const uint32_t *key_indices,
                                 uint32_t key_count, float pivot, float scale, bool reverse) {
    if (animation == NULL || (key_count != 0U && key_indices == NULL) || !isfinite(pivot) ||
        !isfinite(scale) || scale <= 0.0F) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < key_count; ++index) {
        if (key_indices[index] >= animation->keyframe_count) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (uint32_t index = 0U; index < key_count; ++index) {
        AfKeyframe *key = &animation->keyframes[key_indices[index]];
        const float delta = key->time - pivot;
        key->time = pivot + (reverse ? -delta : delta) * scale;
    }
    for (uint32_t track_index = 0U; track_index < animation->track_count; ++track_index) {
        AfAnimationTrack *track = &animation->tracks[track_index];
        for (uint32_t key_index = 1U; key_index < track->keyframe_count; ++key_index) {
            AfKeyframe value = animation->keyframes[track->keyframe_start + key_index];
            uint32_t position = key_index;
            while (position > 0U &&
                   animation->keyframes[track->keyframe_start + position - 1U].time > value.time) {
                animation->keyframes[track->keyframe_start + position] =
                    animation->keyframes[track->keyframe_start + position - 1U];
                --position;
            }
            animation->keyframes[track->keyframe_start + position] = value;
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_advanced_snapshot_capture(const AfAnimationData *animation,
                                      AfAnimationEditSnapshot *snapshot) {
    if (animation == NULL || snapshot == NULL || animation->clip_count > snapshot->clip_capacity ||
        animation->track_count > snapshot->track_capacity ||
        animation->keyframe_count > snapshot->keyframe_capacity ||
        (animation->clip_count != 0U && snapshot->clips == NULL) ||
        (animation->track_count != 0U && snapshot->tracks == NULL) ||
        (animation->keyframe_count != 0U && snapshot->keyframes == NULL)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memcpy(snapshot->clips, animation->clips,
           (size_t)animation->clip_count * sizeof(AfAnimationClip));
    memcpy(snapshot->tracks, animation->tracks,
           (size_t)animation->track_count * sizeof(AfAnimationTrack));
    memcpy(snapshot->keyframes, animation->keyframes,
           (size_t)animation->keyframe_count * sizeof(AfKeyframe));
    snapshot->clip_count = animation->clip_count;
    snapshot->track_count = animation->track_count;
    snapshot->keyframe_count = animation->keyframe_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_advanced_snapshot_restore(AfAnimationData *animation,
                                      const AfAnimationEditSnapshot *snapshot) {
    if (animation == NULL || snapshot == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    memcpy(animation->clips, snapshot->clips,
           (size_t)snapshot->clip_count * sizeof(AfAnimationClip));
    memcpy(animation->tracks, snapshot->tracks,
           (size_t)snapshot->track_count * sizeof(AfAnimationTrack));
    memcpy(animation->keyframes, snapshot->keyframes,
           (size_t)snapshot->keyframe_count * sizeof(AfKeyframe));
    animation->clip_count = snapshot->clip_count;
    animation->track_count = snapshot->track_count;
    animation->keyframe_count = snapshot->keyframe_count;
    return AF_RESULT_SUCCESS;
}

static float af_adv_cubic(float a, float b, float t) {
    const float inverse = 1.0F - t;
    return 3.0F * inverse * inverse * t * a + 3.0F * inverse * t * t * b + t * t * t;
}

static float af_adv_custom(const float easing[4], float progress) {
    float low = 0.0F;
    float high = 1.0F;
    for (uint32_t index = 0U; index < 16U; ++index) {
        const float middle = (low + high) * 0.5F;
        if (af_adv_cubic(easing[0], easing[2], middle) < progress) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return af_adv_cubic(easing[1], easing[3], (low + high) * 0.5F);
}

float af_advanced_easing_evaluate(AfEasingPreset preset, const float custom[4], float progress) {
    const float t = af_adv_clamp(progress, 0.0F, 1.0F);
    const float pi = 3.14159265358979323846F;
    if (preset == AF_EASING_EASE_IN) {
        return t * t;
    }
    if (preset == AF_EASING_EASE_OUT) {
        return 1.0F - (1.0F - t) * (1.0F - t);
    }
    if (preset == AF_EASING_EASE_IN_OUT) {
        return t < 0.5F ? 2.0F * t * t : 1.0F - powf(-2.0F * t + 2.0F, 2.0F) * 0.5F;
    }
    if (preset == AF_EASING_BACK || preset == AF_EASING_OVERSHOOT) {
        const float strength = preset == AF_EASING_BACK ? 1.70158F : 2.5F;
        const float shifted = t - 1.0F;
        return 1.0F + (strength + 1.0F) * shifted * shifted * shifted +
               strength * shifted * shifted;
    }
    if (preset == AF_EASING_BOUNCE) {
        const float n = 7.5625F;
        const float d = 2.75F;
        if (t < 1.0F / d) {
            return n * t * t;
        }
        if (t < 2.0F / d) {
            const float value = t - 1.5F / d;
            return n * value * value + 0.75F;
        }
        if (t < 2.5F / d) {
            const float value = t - 2.25F / d;
            return n * value * value + 0.9375F;
        }
        const float value = t - 2.625F / d;
        return n * value * value + 0.984375F;
    }
    if (preset == AF_EASING_ELASTIC) {
        return t == 0.0F || t == 1.0F
                   ? t
                   : powf(2.0F, -10.0F * t) * sinf((t * 10.0F - 0.75F) * (2.0F * pi / 3.0F)) + 1.0F;
    }
    return custom == NULL ? t : af_adv_custom(custom, t);
}

AfResult af_advanced_map_nested_time(const AfAdvancedAnimationData *advanced,
                                     const AfAnimationData *animation, uint64_t owner_clip_id,
                                     uint64_t child_clip_id, float owner_time, bool reverse,
                                     float *out_child_time) {
    if (advanced == NULL || animation == NULL || out_child_time == NULL || !isfinite(owner_time)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < advanced->nested_count; ++index) {
        const AfNestedTimeline *nested = &advanced->nested[index];
        if (nested->owner_clip_id == owner_clip_id && nested->child_clip_id == child_clip_id) {
            float normalized = (owner_time - nested->start_time) / nested->duration;
            float speed = nested->speed;
            for (uint32_t point = 0U; point < advanced->speed_point_count; ++point) {
                if (advanced->speed_points[point].clip_id == owner_clip_id &&
                    advanced->speed_points[point].time <= owner_time) {
                    speed = advanced->speed_points[point].speed;
                }
            }
            normalized = af_adv_clamp(normalized * fabsf(speed), 0.0F, 1.0F);
            if (reverse != (speed < 0.0F)) {
                normalized = 1.0F - normalized;
            }
            if (nested->hold) {
                normalized = floorf(normalized * 12.0F) / 12.0F;
            }
            *out_child_time = nested->child_start_time +
                              (nested->child_end_time - nested->child_start_time) * normalized;
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_NOT_FOUND;
}

static AfResult af_adv_apply_value(AfScene *scene, AfSceneNode *node, const AfAnimationTrack *track,
                                   const float value[4], float weight, bool additive) {
    const AfPropertyType property = track->property;
    if (property == AF_PROPERTY_POSITION) {
        node->local_transform.position.x =
            additive ? node->local_transform.position.x + value[0] * weight
                     : node->local_transform.position.x * (1.0F - weight) + value[0] * weight;
        node->local_transform.position.y =
            additive ? node->local_transform.position.y + value[1] * weight
                     : node->local_transform.position.y * (1.0F - weight) + value[1] * weight;
    } else if (property == AF_PROPERTY_ROTATION) {
        node->local_transform.rotation =
            additive ? node->local_transform.rotation + value[0] * weight
                     : node->local_transform.rotation * (1.0F - weight) + value[0] * weight;
    } else if (property == AF_PROPERTY_SCALE) {
        node->local_transform.scale.x =
            additive ? node->local_transform.scale.x + (value[0] - 1.0F) * weight
                     : node->local_transform.scale.x * (1.0F - weight) + value[0] * weight;
        node->local_transform.scale.y =
            additive ? node->local_transform.scale.y + (value[1] - 1.0F) * weight
                     : node->local_transform.scale.y * (1.0F - weight) + value[1] * weight;
    } else if (property == AF_PROPERTY_SKEW) {
        node->local_transform.skew.x =
            additive ? node->local_transform.skew.x + value[0] * weight
                     : node->local_transform.skew.x * (1.0F - weight) + value[0] * weight;
        node->local_transform.skew.y =
            additive ? node->local_transform.skew.y + value[1] * weight
                     : node->local_transform.skew.y * (1.0F - weight) + value[1] * weight;
    } else if (property == AF_PROPERTY_OPACITY) {
        node->opacity = af_adv_clamp(additive ? node->opacity + value[0] * weight
                                              : node->opacity * (1.0F - weight) + value[0] * weight,
                                     0.0F, 1.0F);
    } else if (property == AF_PROPERTY_FILL_COLOR) {
        float *components[4] = {&node->fill.r, &node->fill.g, &node->fill.b, &node->fill.a};
        for (uint32_t index = 0U; index < 4U; ++index) {
            *components[index] = af_adv_clamp(additive ? *components[index] + value[index] * weight
                                                       : *components[index] * (1.0F - weight) +
                                                             value[index] * weight,
                                              0.0F, 1.0F);
        }
    } else if (property == AF_PROPERTY_CORNER_RADIUS) {
        node->corner_radius =
            fmaxf(0.0F, additive ? node->corner_radius + value[0] * weight
                                 : node->corner_radius * (1.0F - weight) + value[0] * weight);
    } else if (property >= AF_PROPERTY_PATH_POINT && property <= AF_PROPERTY_PATH_CONTROL_OUT) {
        AfPathData *paths = (AfPathData *)scene->paths;
        AfPath *path = paths == NULL ? NULL : af_path_find_node(paths, track->node_id);
        AfPathCommand *command = NULL;
        AfVec2 *target = NULL;
        if (path == NULL || track->element_index >= path->command_count) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        command = &paths->commands[path->command_start + track->element_index];
        if (command->type == AF_PATH_CLOSE ||
            (property != AF_PROPERTY_PATH_POINT && command->type != AF_PATH_CUBIC)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        target = property == AF_PROPERTY_PATH_POINT
                     ? &command->point
                     : (property == AF_PROPERTY_PATH_CONTROL_IN ? &command->control2
                                                                : &command->control1);
        target->x = additive ? target->x + value[0] * weight
                             : target->x * (1.0F - weight) + value[0] * weight;
        target->y = additive ? target->y + value[1] * weight
                             : target->y * (1.0F - weight) + value[1] * weight;
        if (af_path_mark_geometry_changed(path) != AF_RESULT_SUCCESS) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_advanced_apply_layers(const AfAdvancedAnimationData *advanced,
                                  const AfAnimationData *animation, AfScene *scene) {
    if (advanced == NULL || animation == NULL || scene == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (int32_t order = INT32_MIN; order < INT32_MAX;) {
        int32_t next_order = INT32_MAX;
        for (uint32_t layer_index = 0U; layer_index < advanced->layer_count; ++layer_index) {
            const AfAnimationLayer *layer = &advanced->layers[layer_index];
            const AfAnimationClip *clip = af_adv_clip_const(animation, layer->clip_id);
            if (!layer->enabled || layer->order <= order) {
                continue;
            }
            if (layer->order < next_order) {
                next_order = layer->order;
            }
            if (clip == NULL || layer->order != next_order) {
                continue;
            }
        }
        if (next_order == INT32_MAX) {
            break;
        }
        for (uint32_t layer_index = 0U; layer_index < advanced->layer_count; ++layer_index) {
            const AfAnimationLayer *layer = &advanced->layers[layer_index];
            const AfAnimationClip *clip = af_adv_clip_const(animation, layer->clip_id);
            if (!layer->enabled || layer->order != next_order || clip == NULL) {
                continue;
            }
            const float transition_weight =
                layer->transition_duration > 0.0F
                    ? af_adv_clamp(layer->time / layer->transition_duration, 0.0F, 1.0F)
                    : 1.0F;
            for (uint32_t track_index = 0U; track_index < clip->track_count; ++track_index) {
                const AfAnimationTrack *track = &animation->tracks[clip->track_start + track_index];
                AfSceneNode *node = af_scene_find(scene, track->node_id);
                float value[4] = {0};
                if (node != NULL && af_animation_sample_track(animation, track, layer->time,
                                                              value) == AF_RESULT_SUCCESS) {
                    const AfResult result =
                        af_adv_apply_value(scene, node, track, value,
                                           layer->weight * transition_weight, layer->additive);
                    if (result != AF_RESULT_SUCCESS) {
                        return result;
                    }
                }
            }
        }
        order = next_order;
    }
    return af_scene_evaluate_world(scene);
}

AfResult af_advanced_motion_path(const AfAnimationData *animation, uint64_t clip_id,
                                 uint64_t node_id, uint32_t sample_count,
                                 AfMotionPathPoint *out_points, uint32_t capacity,
                                 uint32_t *out_count) {
    const AfAnimationClip *clip = af_adv_clip_const(animation, clip_id);
    const AfAnimationTrack *track = NULL;
    if (clip == NULL || out_points == NULL || out_count == NULL || sample_count < 2U ||
        sample_count > AF_MAX_MOTION_PATH_POINTS || sample_count > capacity) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < clip->track_count; ++index) {
        const AfAnimationTrack *candidate = &animation->tracks[clip->track_start + index];
        if (candidate->node_id == node_id && candidate->property == AF_PROPERTY_POSITION) {
            track = candidate;
            break;
        }
    }
    if (track == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    for (uint32_t index = 0U; index < sample_count; ++index) {
        float value[4] = {0};
        const float time = clip->duration * (float)index / (float)(sample_count - 1U);
        AfResult result = af_animation_sample_track(animation, track, time, value);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
        out_points[index] = (AfMotionPathPoint){.time = time, .position = {value[0], value[1]}};
    }
    *out_count = sample_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_advanced_motion_path_edit(AfAnimationData *animation, uint64_t track_id,
                                      uint32_t key_offset, AfVec2 position) {
    if (animation == NULL || !isfinite(position.x) || !isfinite(position.y)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < animation->track_count; ++index) {
        AfAnimationTrack *track = &animation->tracks[index];
        if (track->id == track_id && track->property == AF_PROPERTY_POSITION) {
            if (key_offset >= track->keyframe_count) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            animation->keyframes[track->keyframe_start + key_offset].value[0] = position.x;
            animation->keyframes[track->keyframe_start + key_offset].value[1] = position.y;
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_NOT_FOUND;
}

AfResult af_advanced_ghost_frames(float current_time, float frame_step, uint32_t before,
                                  uint32_t after, float *out_times, uint32_t capacity,
                                  uint32_t *out_count) {
    const uint32_t count = before + after;
    if (!isfinite(current_time) || !isfinite(frame_step) || frame_step <= 0.0F ||
        count > AF_MAX_GHOST_FRAMES || count > capacity || out_times == NULL || out_count == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    uint32_t output = 0U;
    for (uint32_t index = before; index > 0U; --index) {
        out_times[output++] = fmaxf(0.0F, current_time - (float)index * frame_step);
    }
    for (uint32_t index = 1U; index <= after; ++index) {
        out_times[output++] = current_time + (float)index * frame_step;
    }
    *out_count = output;
    return AF_RESULT_SUCCESS;
}
