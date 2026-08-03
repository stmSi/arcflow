#include "deformation/deformation.h"

#include <math.h>
#include <string.h>

static AfPath *af_deform_path(AfPathData *paths, uint64_t id) {
    for (uint32_t index = 0U; paths != NULL && index < paths->path_count; ++index) {
        if (paths->paths[index].id == id) {
            return &paths->paths[index];
        }
    }
    return NULL;
}

static AfStyle *af_deform_style(AfStyleData *styles, uint64_t id) {
    for (uint32_t index = 0U; styles != NULL && index < styles->count; ++index) {
        if (styles->styles[index].id == id) {
            return &styles->styles[index];
        }
    }
    return NULL;
}

static AfCompositionMask *af_deform_mask(AfCompositionData *composition, uint64_t id) {
    for (uint32_t index = 0U; composition != NULL && index < composition->mask_count; ++index) {
        if (composition->masks[index].id == id) {
            return &composition->masks[index];
        }
    }
    return NULL;
}

static bool af_deform_range(uint32_t start, uint32_t count, uint32_t total) {
    return start <= total && count <= total - start;
}

AfResult af_shape_animation_validate(const AfShapeAnimationData *animation, const AfPathData *paths,
                                     const AfStyleData *styles,
                                     const AfCompositionData *composition) {
    if (animation == NULL || paths == NULL || styles == NULL || composition == NULL ||
        animation->track_count > AF_MAX_SHAPE_ANIMATION_TRACKS ||
        animation->sample_count > AF_MAX_SHAPE_ANIMATION_SAMPLES ||
        animation->track_count > animation->track_capacity ||
        animation->sample_count > animation->sample_capacity ||
        (animation->track_count != 0U && animation->tracks == NULL) ||
        (animation->sample_count != 0U && animation->samples == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < animation->track_count; ++index) {
        const AfShapeAnimationTrack *track = &animation->tracks[index];
        float last_time = -INFINITY;
        if (track->target_id == 0U || track->property > AF_SHAPE_PROPERTY_MASK_ALPHA ||
            !af_deform_range(track->sample_start, track->sample_count, animation->sample_count) ||
            track->sample_count == 0U) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        if (track->property == AF_SHAPE_PROPERTY_PATH_POINT) {
            const AfPath *path = af_path_find_const(paths, track->target_id);
            if (path == NULL || track->element_index >= path->command_count ||
                paths->commands[path->command_start + track->element_index].type == AF_PATH_CLOSE) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        } else if (track->property == AF_SHAPE_PROPERTY_MASK_ALPHA) {
            if (af_deform_mask((AfCompositionData *)composition, track->target_id) == NULL) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        } else {
            const AfStyle *style = af_style_find_const(styles, track->target_id);
            if (style == NULL || (track->property == AF_SHAPE_PROPERTY_GRADIENT_STOP &&
                                  track->element_index >= style->fill.stop_count)) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        for (uint32_t sample_index = 0U; sample_index < track->sample_count; ++sample_index) {
            const AfShapeAnimationSample *sample =
                &animation->samples[track->sample_start + sample_index];
            if (!isfinite(sample->time) || sample->time < last_time) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            for (uint32_t component = 0U; component < 8U; ++component) {
                if (!isfinite(sample->value[component])) {
                    return AF_RESULT_INVALID_ARGUMENT;
                }
            }
            last_time = sample->time;
        }
    }
    return AF_RESULT_SUCCESS;
}

static void af_shape_sample(const AfShapeAnimationData *animation,
                            const AfShapeAnimationTrack *track, float time, float value[8]) {
    const AfShapeAnimationSample *samples = &animation->samples[track->sample_start];
    uint32_t left = 0U;
    while (left + 1U < track->sample_count && samples[left + 1U].time <= time) {
        ++left;
    }
    float amount = 0.0F;
    if (left + 1U < track->sample_count && samples[left + 1U].time > samples[left].time) {
        amount = fminf(
            fmaxf((time - samples[left].time) / (samples[left + 1U].time - samples[left].time),
                  0.0F),
            1.0F);
    }
    for (uint32_t component = 0U; component < 8U; ++component) {
        value[component] =
            left + 1U < track->sample_count
                ? samples[left].value[component] +
                      (samples[left + 1U].value[component] - samples[left].value[component]) *
                          amount
                : samples[left].value[component];
    }
}

static bool af_vec_changed(AfVec2 first, AfVec2 second) {
    return first.x != second.x || first.y != second.y;
}

AfResult af_shape_animation_apply(const AfShapeAnimationData *animation, float time,
                                  AfPathData *paths, AfStyleData *styles,
                                  AfCompositionData *composition) {
    if (animation == NULL || paths == NULL || styles == NULL || composition == NULL ||
        !isfinite(time)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < animation->track_count; ++index) {
        const AfShapeAnimationTrack *track = &animation->tracks[index];
        float value[8] = {0};
        af_shape_sample(animation, track, time, value);
        if (track->property == AF_SHAPE_PROPERTY_PATH_POINT) {
            AfPath *path = af_deform_path(paths, track->target_id);
            AfPathCommand *command = &paths->commands[path->command_start + track->element_index];
            const AfVec2 point = {value[0], value[1]};
            const AfVec2 control1 = {value[2], value[3]};
            const AfVec2 control2 = {value[4], value[5]};
            if (af_vec_changed(command->point, point) ||
                af_vec_changed(command->control1, control1) ||
                af_vec_changed(command->control2, control2)) {
                command->point = point;
                command->control1 = control1;
                command->control2 = control2;
                if (af_path_mark_geometry_changed(path) != AF_RESULT_SUCCESS) {
                    return AF_RESULT_LIMIT_EXCEEDED;
                }
            }
        } else if (track->property == AF_SHAPE_PROPERTY_MASK_ALPHA) {
            AfCompositionMask *mask = af_deform_mask(composition, track->target_id);
            mask->alpha = fminf(fmaxf(value[0], 0.0F), 1.0F);
        } else {
            AfStyle *style = af_deform_style(styles, track->target_id);
            if (track->property == AF_SHAPE_PROPERTY_STROKE_WIDTH) {
                const float width = fmaxf(value[0], 0.0F);
                if (style->stroke.width != width) {
                    style->stroke.width = width;
                    if (af_style_mark_stroke_geometry_changed(style) != AF_RESULT_SUCCESS) {
                        return AF_RESULT_LIMIT_EXCEEDED;
                    }
                }
            } else if (track->property == AF_SHAPE_PROPERTY_GRADIENT_STOP) {
                AfGradientStop *stop = &style->fill.stops[track->element_index];
                const AfGradientStop sampled = {.offset = fminf(fmaxf(value[0], 0.0F), 1.0F),
                                                .color = {value[1], value[2], value[3], value[4]}};
                if (memcmp(stop, &sampled, sizeof(sampled)) != 0) {
                    *stop = sampled;
                    if (af_style_mark_paint_changed(style) != AF_RESULT_SUCCESS) {
                        return AF_RESULT_LIMIT_EXCEEDED;
                    }
                }
            } else {
                const AfAffine2 sampled = {.m00 = value[0],
                                           .m01 = value[1],
                                           .m10 = value[2],
                                           .m11 = value[3],
                                           .tx = value[4],
                                           .ty = value[5]};
                if (memcmp(&style->fill.transform, &sampled, sizeof(sampled)) != 0) {
                    style->fill.transform = sampled;
                    if (af_style_mark_paint_changed(style) != AF_RESULT_SUCCESS) {
                        return AF_RESULT_LIMIT_EXCEEDED;
                    }
                }
            }
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_shape_morph_validate(const AfPathData *paths, uint64_t base_path_id,
                                 uint64_t target_path_id) {
    const AfPath *base = af_path_find_const(paths, base_path_id);
    const AfPath *target = af_path_find_const(paths, target_path_id);
    if (base == NULL || target == NULL || base == target ||
        base->command_count != target->command_count || base->fill_rule != target->fill_rule) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < base->command_count; ++index) {
        if (paths->commands[base->command_start + index].type !=
            paths->commands[target->command_start + index].type) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    return AF_RESULT_SUCCESS;
}

static AfVec2 af_deform_lerp(AfVec2 first, AfVec2 second, float amount) {
    return (AfVec2){first.x + (second.x - first.x) * amount,
                    first.y + (second.y - first.y) * amount};
}

AfResult af_shape_morph_update(const AfPathData *paths, uint64_t base_path_id,
                               uint64_t target_path_id, float amount, AfMorphCache *cache) {
    const AfPath *base = af_path_find_const(paths, base_path_id);
    const AfPath *target = af_path_find_const(paths, target_path_id);
    if (cache == NULL || !isfinite(amount) || amount < 0.0F || amount > 1.0F ||
        af_shape_morph_validate(paths, base_path_id, target_path_id) != AF_RESULT_SUCCESS ||
        base->command_count > cache->command_capacity ||
        (base->command_count != 0U && cache->commands == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (cache->base_generation == base->geometry_generation &&
        cache->target_generation == target->geometry_generation && cache->amount == amount) {
        return AF_RESULT_SUCCESS;
    }
    for (uint32_t index = 0U; index < base->command_count; ++index) {
        const AfPathCommand *first = &paths->commands[base->command_start + index];
        const AfPathCommand *second = &paths->commands[target->command_start + index];
        cache->commands[index] =
            (AfPathCommand){.type = first->type,
                            .point = af_deform_lerp(first->point, second->point, amount),
                            .control1 = af_deform_lerp(first->control1, second->control1, amount),
                            .control2 = af_deform_lerp(first->control2, second->control2, amount)};
    }
    cache->command_count = base->command_count;
    cache->base_generation = base->geometry_generation;
    cache->target_generation = target->geometry_generation;
    cache->amount = amount;
    if (cache->geometry_generation == UINT64_MAX) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    ++cache->geometry_generation;
    ++cache->rebuild_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_trim_path(const AfPathData *paths, uint64_t path_id, float start, float end,
                      float tolerance, AfPathTessellationScratch *scratch, AfVec2 *out_points,
                      uint32_t capacity, uint32_t *out_count) {
    const AfPath *path = af_path_find_const(paths, path_id);
    AfFlattenedPath flattened = {0};
    float total = 0.0F;
    float cursor = 0.0F;
    uint32_t output = 0U;
    if (path == NULL || scratch == NULL || out_points == NULL || out_count == NULL ||
        !isfinite(start) || !isfinite(end) || start < 0.0F || start >= end || end > 1.0F) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    flattened = (AfFlattenedPath){.points = scratch->points,
                                  .point_capacity = AF_MAX_PATH_FLATTENED_POINTS,
                                  .contours = scratch->contours,
                                  .contour_capacity = AF_MAX_PATH_CONTOURS};
    if (af_path_flatten(paths, path, tolerance, &flattened) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t contour = 0U; contour < flattened.contour_count; ++contour) {
        const AfPathContour *range = &flattened.contours[contour];
        for (uint32_t point = 1U; point < range->point_count; ++point) {
            const AfVec2 a = flattened.points[range->point_start + point - 1U];
            const AfVec2 b = flattened.points[range->point_start + point];
            total += hypotf(b.x - a.x, b.y - a.y);
        }
        if (range->closed && range->point_count > 1U) {
            const AfVec2 a = flattened.points[range->point_start + range->point_count - 1U];
            const AfVec2 b = flattened.points[range->point_start];
            total += hypotf(b.x - a.x, b.y - a.y);
        }
    }
    if (total <= 0.0F) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    const float trim_start = total * start;
    const float trim_end = total * end;
    for (uint32_t contour = 0U; contour < flattened.contour_count; ++contour) {
        const AfPathContour *range = &flattened.contours[contour];
        const uint32_t segment_count =
            range->point_count > 0U ? range->point_count - 1U + (range->closed ? 1U : 0U) : 0U;
        for (uint32_t segment = 0U; segment < segment_count; ++segment) {
            const AfVec2 a = flattened.points[range->point_start + segment];
            const AfVec2 b =
                flattened.points[range->point_start + ((segment + 1U) % range->point_count)];
            const float length = hypotf(b.x - a.x, b.y - a.y);
            const float segment_end = cursor + length;
            if (length > 0.0F && segment_end >= trim_start && cursor <= trim_end) {
                const float local_start = fminf(fmaxf((trim_start - cursor) / length, 0.0F), 1.0F);
                const float local_end = fminf(fmaxf((trim_end - cursor) / length, 0.0F), 1.0F);
                const AfVec2 first = af_deform_lerp(a, b, local_start);
                const AfVec2 last = af_deform_lerp(a, b, local_end);
                if (output == 0U || af_vec_changed(out_points[output - 1U], first)) {
                    if (output >= capacity) {
                        return AF_RESULT_LIMIT_EXCEEDED;
                    }
                    out_points[output++] = first;
                }
                if (output >= capacity) {
                    return AF_RESULT_LIMIT_EXCEEDED;
                }
                out_points[output++] = last;
            }
            cursor = segment_end;
        }
    }
    *out_count = output;
    return output >= 2U ? AF_RESULT_SUCCESS : AF_RESULT_NOT_FOUND;
}

static const AfShapeKey *af_shape_key(const AfShapeKeyData *data, uint64_t id) {
    for (uint32_t index = 0U; data != NULL && index < data->key_count; ++index) {
        if (data->keys[index].id == id) {
            return &data->keys[index];
        }
    }
    return NULL;
}

AfResult af_deform_mesh_validate(const AfDeformMesh *mesh, const AfShapeKeyData *shape_keys) {
    if (mesh == NULL || shape_keys == NULL || mesh->vertex_count > AF_MAX_MESH_VERTICES ||
        mesh->index_count > AF_MAX_MESH_INDICES || shape_keys->key_count > AF_MAX_SHAPE_KEYS ||
        shape_keys->delta_count > AF_MAX_SHAPE_KEY_DELTAS ||
        mesh->vertex_count > mesh->vertex_capacity || mesh->index_count > mesh->index_capacity ||
        shape_keys->key_count > shape_keys->key_capacity ||
        shape_keys->delta_count > shape_keys->delta_capacity ||
        (mesh->vertex_count != 0U && mesh->vertices == NULL) ||
        (mesh->index_count != 0U && mesh->indices == NULL) ||
        (shape_keys->key_count != 0U && shape_keys->keys == NULL) ||
        (shape_keys->delta_count != 0U && shape_keys->deltas == NULL) ||
        mesh->index_count % 3U != 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < mesh->vertex_count; ++index) {
        if (!isfinite(mesh->vertices[index].x) || !isfinite(mesh->vertices[index].y)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (uint32_t index = 0U; index < mesh->index_count; ++index) {
        if (mesh->indices[index] >= mesh->vertex_count) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (uint32_t index = 0U; index < shape_keys->key_count; ++index) {
        const AfShapeKey *key = &shape_keys->keys[index];
        if (key->id == 0U ||
            !af_deform_range(key->vertex_start, key->vertex_count, mesh->vertex_count) ||
            !af_deform_range(key->delta_start, key->vertex_count, shape_keys->delta_count)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t other = index + 1U; other < shape_keys->key_count; ++other) {
            if (key->id == shape_keys->keys[other].id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    for (uint32_t index = 0U; index < shape_keys->delta_count; ++index) {
        if (!isfinite(shape_keys->deltas[index].x) || !isfinite(shape_keys->deltas[index].y)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_deform_mesh_apply(const AfDeformMesh *mesh, const AfShapeKeyData *shape_keys,
                              const AfShapeKeyInfluence *influences, uint32_t influence_count,
                              AfVec2 *out_vertices, uint32_t capacity) {
    if (af_deform_mesh_validate(mesh, shape_keys) != AF_RESULT_SUCCESS ||
        influence_count > AF_MAX_SHAPE_KEY_INFLUENCES ||
        (influence_count != 0U && influences == NULL) || out_vertices == NULL ||
        capacity < mesh->vertex_count) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    memcpy(out_vertices, mesh->vertices, (size_t)mesh->vertex_count * sizeof(AfVec2));
    for (uint32_t influence = 0U; influence < influence_count; ++influence) {
        const AfShapeKey *key = af_shape_key(shape_keys, influences[influence].key_id);
        const float weight = influences[influence].weight;
        if (key == NULL || !isfinite(weight)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t vertex = 0U; vertex < key->vertex_count; ++vertex) {
            const AfVec2 delta = shape_keys->deltas[key->delta_start + vertex];
            out_vertices[key->vertex_start + vertex].x += delta.x * weight;
            out_vertices[key->vertex_start + vertex].y += delta.y * weight;
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_shape_key_interpolate(const AfShapeKeyData *shape_keys, uint64_t first_key_id,
                                  uint64_t second_key_id, float amount, AfVec2 *out_deltas,
                                  uint32_t capacity, uint32_t *out_count) {
    const AfShapeKey *first = af_shape_key(shape_keys, first_key_id);
    const AfShapeKey *second = af_shape_key(shape_keys, second_key_id);
    if (first == NULL || second == NULL || first->vertex_start != second->vertex_start ||
        first->vertex_count != second->vertex_count || !isfinite(amount) || amount < 0.0F ||
        amount > 1.0F || out_deltas == NULL || out_count == NULL ||
        capacity < first->vertex_count) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < first->vertex_count; ++index) {
        out_deltas[index] = af_deform_lerp(shape_keys->deltas[first->delta_start + index],
                                           shape_keys->deltas[second->delta_start + index], amount);
    }
    *out_count = first->vertex_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_path_follow(const AfPathData *paths, uint64_t path_id, float normalized_distance,
                        float tolerance, AfPathTessellationScratch *scratch,
                        AfTransform2D *out_transform) {
    AfVec2 points[AF_MAX_PATH_FLATTENED_POINTS];
    uint32_t count = 0U;
    if (out_transform == NULL || !isfinite(normalized_distance)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    AfResult result =
        af_trim_path(paths, path_id, 0.0F, fminf(fmaxf(normalized_distance, 0.000001F), 1.0F),
                     tolerance, scratch, points, AF_MAX_PATH_FLATTENED_POINTS, &count);
    if (result != AF_RESULT_SUCCESS || count < 2U) {
        return result;
    }
    const AfVec2 previous = points[count - 2U];
    const AfVec2 last = points[count - 1U];
    *out_transform = (AfTransform2D){.position = last,
                                     .rotation = atan2f(last.y - previous.y, last.x - previous.x),
                                     .scale = {1.0F, 1.0F}};
    return AF_RESULT_SUCCESS;
}
