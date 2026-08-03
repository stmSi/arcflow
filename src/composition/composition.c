#include "composition/composition.h"

#include <math.h>
#include <stddef.h>

static const AfCompositionLayer *af_composition_layer(const AfCompositionData *data, uint64_t id) {
    if (data == NULL || id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < data->layer_count; ++index) {
        if (data->layers[index].id == id) {
            return &data->layers[index];
        }
    }
    return NULL;
}

static const AfCompositionClip *af_composition_clip(const AfCompositionData *data, uint64_t id) {
    if (id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < data->clip_count; ++index) {
        if (data->clips[index].id == id) {
            return &data->clips[index];
        }
    }
    return NULL;
}

static const AfCompositionMask *af_composition_mask(const AfCompositionData *data, uint64_t id) {
    if (id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < data->mask_count; ++index) {
        if (data->masks[index].id == id) {
            return &data->masks[index];
        }
    }
    return NULL;
}

static bool af_composition_mask_chain_valid(const AfCompositionData *data, uint64_t id) {
    uint64_t current = id;
    for (uint32_t depth = 0U; current != 0U; ++depth) {
        const AfCompositionMask *mask = af_composition_mask(data, current);
        if (mask == NULL || depth >= AF_MAX_COMPOSITION_DEPTH) {
            return false;
        }
        current = mask->parent_mask_id;
        if (current == id) {
            return false;
        }
    }
    return true;
}

static bool af_composition_path_exists(const AfScene *scene, uint64_t id) {
    if (scene->paths == NULL || id == 0U) {
        return false;
    }
    for (uint32_t index = 0U; index < scene->paths->path_count; ++index) {
        if (scene->paths->paths[index].id == id) {
            return true;
        }
    }
    return false;
}

static bool af_composition_layer_chain_valid(const AfCompositionData *data, uint64_t id) {
    uint64_t current = id;
    for (uint32_t depth = 0U; current != 0U; ++depth) {
        const AfCompositionLayer *layer = af_composition_layer(data, current);
        if (layer == NULL || depth >= AF_MAX_COMPOSITION_DEPTH) {
            return false;
        }
        current = layer->parent_layer_id;
        if (current == id) {
            return false;
        }
    }
    return true;
}

AfResult af_composition_validate(const AfCompositionData *data, const AfScene *scene) {
    if (data == NULL || scene == NULL || data->layer_count > AF_MAX_COMPOSITION_LAYERS ||
        data->clip_count > AF_MAX_COMPOSITION_CLIPS ||
        data->mask_count > AF_MAX_COMPOSITION_MASKS || data->filter_count > AF_MAX_LAYER_FILTERS ||
        data->layer_count > data->layer_capacity || data->clip_count > data->clip_capacity ||
        data->mask_count > data->mask_capacity || data->filter_count > data->filter_capacity ||
        (data->layer_count != 0U && data->layers == NULL) ||
        (data->clip_count != 0U && data->clips == NULL) ||
        (data->mask_count != 0U && data->masks == NULL) ||
        (data->filter_count != 0U && data->filters == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < data->clip_count; ++index) {
        const AfCompositionClip *clip = &data->clips[index];
        if (clip->id == 0U || af_scene_find_const(scene, clip->node_id) == NULL ||
            clip->type > AF_CLIP_VECTOR_PATH || !isfinite(clip->corner_radius) ||
            clip->corner_radius < 0.0F ||
            (clip->type == AF_CLIP_VECTOR_PATH &&
             !af_composition_path_exists(scene, clip->path_id))) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t other = index + 1U; other < data->clip_count; ++other) {
            if (clip->id == data->clips[other].id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    for (uint32_t index = 0U; index < data->mask_count; ++index) {
        const AfCompositionMask *mask = &data->masks[index];
        if (mask->id == 0U || mask->owner_node_id == mask->mask_node_id ||
            af_scene_find_const(scene, mask->owner_node_id) == NULL ||
            af_scene_find_const(scene, mask->mask_node_id) == NULL || !isfinite(mask->alpha) ||
            mask->alpha < 0.0F || mask->alpha > 1.0F ||
            !af_composition_mask_chain_valid(data, mask->id)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t other = index + 1U; other < data->mask_count; ++other) {
            if (mask->id == data->masks[other].id ||
                (mask->owner_node_id == data->masks[other].mask_node_id &&
                 mask->mask_node_id == data->masks[other].owner_node_id)) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    for (uint32_t index = 0U; index < data->filter_count; ++index) {
        const AfLayerFilter *filter = &data->filters[index];
        if (filter->type > AF_FILTER_COLOR_ADJUSTMENT || !isfinite(filter->radius) ||
            filter->radius < 0.0F || filter->radius > AF_MAX_FILTER_RADIUS ||
            !isfinite(filter->offset.x) || !isfinite(filter->offset.y)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t component = 0U; component < 20U; ++component) {
            if (!isfinite(filter->color_matrix[component])) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    for (uint32_t index = 0U; index < data->layer_count; ++index) {
        const AfCompositionLayer *layer = &data->layers[index];
        if (layer->id == 0U || af_scene_find_const(scene, layer->node_id) == NULL ||
            (layer->parent_layer_id != 0U &&
             af_composition_layer(data, layer->parent_layer_id) == NULL) ||
            (layer->clip_id != 0U && af_composition_clip(data, layer->clip_id) == NULL) ||
            (layer->mask_id != 0U && af_composition_mask(data, layer->mask_id) == NULL) ||
            !isfinite(layer->group_opacity) || layer->group_opacity < 0.0F ||
            layer->group_opacity > 1.0F || layer->filter_start > data->filter_count ||
            layer->filter_count > data->filter_count - layer->filter_start ||
            !af_composition_layer_chain_valid(data, layer->id)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        if (layer->blend_mode >= AF_BLEND_UNSUPPORTED) {
            return AF_RESULT_UNSUPPORTED_FEATURE;
        }
        for (uint32_t other = index + 1U; other < data->layer_count; ++other) {
            if (layer->id == data->layers[other].id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    return AF_RESULT_SUCCESS;
}

void af_render_target_pool_begin_frame(AfRenderTargetPool *pool) {
    if (pool != NULL) {
        for (uint32_t index = 0U; index < pool->count; ++index) {
            pool->targets[index].in_use = false;
        }
    }
}

AfResult af_render_target_acquire(AfRenderTargetPool *pool, uint32_t width, uint32_t height,
                                  uint64_t cache_key, uint64_t generation, bool cached,
                                  uint32_t *out_index, bool *out_reused) {
    const uint64_t pixels = (uint64_t)width * height;
    if (pool == NULL || out_index == NULL || out_reused == NULL || width == 0U || height == 0U ||
        width > AF_MAX_RENDER_TARGET_DIMENSION || height > AF_MAX_RENDER_TARGET_DIMENSION ||
        pixels > AF_MAX_RENDER_TARGET_PIXELS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < pool->count; ++index) {
        AfRenderTarget *target = &pool->targets[index];
        if (!target->in_use && target->width == width && target->height == height &&
            ((cached && target->cached && target->cache_key == cache_key &&
              target->generation == generation) ||
             !cached)) {
            target->in_use = true;
            target->cached = cached;
            target->cache_key = cache_key;
            target->generation = generation;
            ++target->reuse_count;
            *out_index = index;
            *out_reused = true;
            return AF_RESULT_SUCCESS;
        }
    }
    if (pool->count >= AF_MAX_RENDER_TARGETS ||
        pixels > AF_MAX_RENDER_TARGET_PIXELS - pool->total_pixels) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    const uint32_t index = pool->count++;
    pool->targets[index] = (AfRenderTarget){.width = width,
                                            .height = height,
                                            .cache_key = cache_key,
                                            .generation = generation,
                                            .in_use = true,
                                            .cached = cached};
    pool->total_pixels += pixels;
    *out_index = index;
    *out_reused = false;
    return AF_RESULT_SUCCESS;
}

void af_render_target_release(AfRenderTargetPool *pool, uint32_t index) {
    if (pool != NULL && index < pool->count) {
        pool->targets[index].in_use = false;
    }
}

AfResult af_composition_build_plan(const AfCompositionData *data, const AfScene *scene,
                                   uint32_t width, uint32_t height, AfRenderTargetPool *pool,
                                   AfCompositionPlan *plan) {
    AfResult result = af_composition_validate(data, scene);
    if (result != AF_RESULT_SUCCESS || pool == NULL || plan == NULL ||
        plan->capacity < data->layer_count || (data->layer_count != 0U && plan->commands == NULL)) {
        return result != AF_RESULT_SUCCESS ? result : AF_RESULT_LIMIT_EXCEEDED;
    }
    plan->count = 0U;
    af_render_target_pool_begin_frame(pool);
    for (uint32_t index = 0U; index < data->layer_count; ++index) {
        const AfCompositionLayer *layer = &data->layers[index];
        const AfCompositionMask *mask = af_composition_mask(data, layer->mask_id);
        AfCompositionCommand command = {.layer_id = layer->id,
                                        .node_id = layer->node_id,
                                        .clip_id = layer->clip_id,
                                        .mask_id = layer->mask_id,
                                        .blend_mode = layer->blend_mode,
                                        .opacity = layer->group_opacity,
                                        .filter_start = layer->filter_start,
                                        .filter_count = layer->filter_count,
                                        .target_index = -1};
        const bool needs_target = layer->offscreen || layer->cache_enabled ||
                                  layer->group_opacity < 1.0F || layer->mask_id != 0U ||
                                  layer->filter_count != 0U || layer->blend_mode != AF_BLEND_NORMAL;
        if (mask != NULL) {
            const AfSceneNode *mask_node = af_scene_find_const(scene, mask->mask_node_id);
            command.mask_transform = mask_node->world_transform;
        }
        if (needs_target) {
            uint32_t target = 0U;
            result =
                af_render_target_acquire(pool, width, height, layer->id, layer->generation,
                                         layer->cache_enabled, &target, &command.target_reused);
            if (result != AF_RESULT_SUCCESS) {
                return result;
            }
            command.target_index = (int32_t)target;
        }
        plan->commands[plan->count++] = command;
    }
    return AF_RESULT_SUCCESS;
}

static float af_blend_channel(AfBlendMode mode, float source, float destination) {
    if (mode == AF_BLEND_ADD) {
        return fminf(source + destination, 1.0F);
    }
    if (mode == AF_BLEND_MULTIPLY) {
        return source * destination;
    }
    if (mode == AF_BLEND_SCREEN) {
        return source + destination - source * destination;
    }
    return source;
}

AfColor af_composition_blend(AfBlendMode mode, AfColor source, AfColor destination) {
    AfColor result = {0};
    if (mode >= AF_BLEND_UNSUPPORTED) {
        return destination;
    }
    const float inverse_alpha = 1.0F - source.a;
    result.r = af_blend_channel(mode, source.r, destination.r) + destination.r * inverse_alpha;
    result.g = af_blend_channel(mode, source.g, destination.g) + destination.g * inverse_alpha;
    result.b = af_blend_channel(mode, source.b, destination.b) + destination.b * inverse_alpha;
    result.a = source.a + destination.a * inverse_alpha;
    result.r = fminf(result.r, result.a);
    result.g = fminf(result.g, result.a);
    result.b = fminf(result.b, result.a);
    return result;
}

AfVec2 af_composition_filter_expansion(const AfLayerFilter *filter) {
    if (filter == NULL || !isfinite(filter->radius)) {
        return (AfVec2){0};
    }
    const float radius = fminf(fmaxf(filter->radius, 0.0F), AF_MAX_FILTER_RADIUS);
    return (AfVec2){radius * 2.0F + fabsf(filter->offset.x),
                    radius * 2.0F + fabsf(filter->offset.y)};
}
