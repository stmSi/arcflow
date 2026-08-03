#ifndef AF_COMPOSITION_H
#define AF_COMPOSITION_H

#include "arcflow/arcflow_result.h"
#include "scene/scene.h"

#include <stdbool.h>
#include <stdint.h>

#define AF_MAX_COMPOSITION_LAYERS 1024U
#define AF_MAX_COMPOSITION_CLIPS 1024U
#define AF_MAX_COMPOSITION_MASKS 1024U
#define AF_MAX_LAYER_FILTERS 2048U
#define AF_MAX_RENDER_TARGETS 64U
#define AF_MAX_COMPOSITION_DEPTH 16U
#define AF_MAX_FILTER_RADIUS 128.0F
#define AF_MAX_RENDER_TARGET_DIMENSION 4096U
#define AF_MAX_RENDER_TARGET_PIXELS (64U * 1024U * 1024U)

typedef enum AfClipType {
    AF_CLIP_RECTANGLE = 0,
    AF_CLIP_ROUNDED_RECTANGLE,
    AF_CLIP_VECTOR_PATH
} AfClipType;

typedef enum AfBlendMode {
    AF_BLEND_NORMAL = 0,
    AF_BLEND_ADD,
    AF_BLEND_MULTIPLY,
    AF_BLEND_SCREEN,
    AF_BLEND_UNSUPPORTED
} AfBlendMode;

typedef enum AfFilterType {
    AF_FILTER_BLUR = 0,
    AF_FILTER_DROP_SHADOW,
    AF_FILTER_INNER_SHADOW,
    AF_FILTER_GLOW,
    AF_FILTER_COLOR_ADJUSTMENT
} AfFilterType;

typedef struct AfCompositionClip {
    uint64_t id;
    uint64_t node_id;
    uint64_t path_id;
    AfClipType type;
    float corner_radius;
} AfCompositionClip;

typedef struct AfCompositionMask {
    uint64_t id;
    uint64_t owner_node_id;
    uint64_t mask_node_id;
    uint64_t parent_mask_id;
    float alpha;
    bool inverted;
} AfCompositionMask;

typedef struct AfLayerFilter {
    AfFilterType type;
    float radius;
    AfVec2 offset;
    AfColor color;
    float color_matrix[20];
} AfLayerFilter;

typedef struct AfCompositionLayer {
    uint64_t id;
    uint64_t node_id;
    uint64_t parent_layer_id;
    uint64_t clip_id;
    uint64_t mask_id;
    uint64_t generation;
    uint32_t filter_start;
    uint32_t filter_count;
    float group_opacity;
    AfBlendMode blend_mode;
    bool offscreen;
    bool cache_enabled;
} AfCompositionLayer;

typedef struct AfCompositionData {
    AfCompositionLayer *layers;
    uint32_t layer_count;
    uint32_t layer_capacity;
    AfCompositionClip *clips;
    uint32_t clip_count;
    uint32_t clip_capacity;
    AfCompositionMask *masks;
    uint32_t mask_count;
    uint32_t mask_capacity;
    AfLayerFilter *filters;
    uint32_t filter_count;
    uint32_t filter_capacity;
} AfCompositionData;

typedef struct AfRenderTarget {
    uint32_t width;
    uint32_t height;
    uint64_t cache_key;
    uint64_t generation;
    uint64_t reuse_count;
    bool in_use;
    bool cached;
} AfRenderTarget;

typedef struct AfRenderTargetPool {
    AfRenderTarget targets[AF_MAX_RENDER_TARGETS];
    uint32_t count;
    uint64_t total_pixels;
} AfRenderTargetPool;

typedef struct AfCompositionCommand {
    uint64_t layer_id;
    uint64_t node_id;
    uint64_t clip_id;
    uint64_t mask_id;
    AfAffine2 mask_transform;
    AfBlendMode blend_mode;
    float opacity;
    uint32_t filter_start;
    uint32_t filter_count;
    int32_t target_index;
    bool target_reused;
} AfCompositionCommand;

typedef struct AfCompositionPlan {
    AfCompositionCommand *commands;
    uint32_t count;
    uint32_t capacity;
} AfCompositionPlan;

AfResult af_composition_validate(const AfCompositionData *data, const AfScene *scene);
void af_render_target_pool_begin_frame(AfRenderTargetPool *pool);
AfResult af_render_target_acquire(AfRenderTargetPool *pool, uint32_t width, uint32_t height,
                                  uint64_t cache_key, uint64_t generation, bool cached,
                                  uint32_t *out_index, bool *out_reused);
void af_render_target_release(AfRenderTargetPool *pool, uint32_t index);
AfResult af_composition_build_plan(const AfCompositionData *data, const AfScene *scene,
                                   uint32_t width, uint32_t height, AfRenderTargetPool *pool,
                                   AfCompositionPlan *plan);
AfColor af_composition_blend(AfBlendMode mode, AfColor source, AfColor destination);
AfVec2 af_composition_filter_expansion(const AfLayerFilter *filter);

#endif
