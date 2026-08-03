#ifndef AF_DEFORMATION_H
#define AF_DEFORMATION_H

#include "composition/composition.h"
#include "path/path.h"
#include "style/style.h"

#include <stdint.h>

#define AF_MAX_SHAPE_ANIMATION_TRACKS 2048U
#define AF_MAX_SHAPE_ANIMATION_SAMPLES 16384U
#define AF_MAX_MESH_VERTICES 8192U
#define AF_MAX_MESH_INDICES 49152U
#define AF_MAX_SHAPE_KEYS 256U
#define AF_MAX_SHAPE_KEY_DELTAS (AF_MAX_MESH_VERTICES * 4U)
#define AF_MAX_SHAPE_KEY_INFLUENCES 64U

typedef enum AfShapeProperty {
    AF_SHAPE_PROPERTY_PATH_POINT = 0,
    AF_SHAPE_PROPERTY_STROKE_WIDTH,
    AF_SHAPE_PROPERTY_GRADIENT_STOP,
    AF_SHAPE_PROPERTY_GRADIENT_TRANSFORM,
    AF_SHAPE_PROPERTY_MASK_ALPHA
} AfShapeProperty;

typedef struct AfShapeAnimationSample {
    float time;
    float value[8];
} AfShapeAnimationSample;

typedef struct AfShapeAnimationTrack {
    uint64_t target_id;
    AfShapeProperty property;
    uint32_t element_index;
    uint32_t sample_start;
    uint32_t sample_count;
} AfShapeAnimationTrack;

typedef struct AfShapeAnimationData {
    AfShapeAnimationTrack *tracks;
    uint32_t track_count;
    uint32_t track_capacity;
    AfShapeAnimationSample *samples;
    uint32_t sample_count;
    uint32_t sample_capacity;
} AfShapeAnimationData;

typedef struct AfMorphCache {
    uint64_t base_generation;
    uint64_t target_generation;
    float amount;
    AfPathCommand *commands;
    uint32_t command_count;
    uint32_t command_capacity;
    uint64_t geometry_generation;
    uint64_t rebuild_count;
} AfMorphCache;

typedef struct AfDeformMesh {
    AfVec2 *vertices;
    uint32_t vertex_count;
    uint32_t vertex_capacity;
    uint32_t *indices;
    uint32_t index_count;
    uint32_t index_capacity;
} AfDeformMesh;

typedef struct AfShapeKey {
    uint64_t id;
    uint32_t vertex_start;
    uint32_t vertex_count;
    uint32_t delta_start;
} AfShapeKey;

typedef struct AfShapeKeyData {
    AfShapeKey *keys;
    uint32_t key_count;
    uint32_t key_capacity;
    AfVec2 *deltas;
    uint32_t delta_count;
    uint32_t delta_capacity;
} AfShapeKeyData;

typedef struct AfShapeKeyInfluence {
    uint64_t key_id;
    float weight;
} AfShapeKeyInfluence;

AfResult af_shape_animation_validate(const AfShapeAnimationData *animation, const AfPathData *paths,
                                     const AfStyleData *styles,
                                     const AfCompositionData *composition);
AfResult af_shape_animation_apply(const AfShapeAnimationData *animation, float time,
                                  AfPathData *paths, AfStyleData *styles,
                                  AfCompositionData *composition);
AfResult af_shape_morph_validate(const AfPathData *paths, uint64_t base_path_id,
                                 uint64_t target_path_id);
AfResult af_shape_morph_update(const AfPathData *paths, uint64_t base_path_id,
                               uint64_t target_path_id, float amount, AfMorphCache *cache);
AfResult af_trim_path(const AfPathData *paths, uint64_t path_id, float start, float end,
                      float tolerance, AfPathTessellationScratch *scratch, AfVec2 *out_points,
                      uint32_t capacity, uint32_t *out_count);
AfResult af_deform_mesh_validate(const AfDeformMesh *mesh, const AfShapeKeyData *shape_keys);
AfResult af_deform_mesh_apply(const AfDeformMesh *mesh, const AfShapeKeyData *shape_keys,
                              const AfShapeKeyInfluence *influences, uint32_t influence_count,
                              AfVec2 *out_vertices, uint32_t capacity);
AfResult af_shape_key_interpolate(const AfShapeKeyData *shape_keys, uint64_t first_key_id,
                                  uint64_t second_key_id, float amount, AfVec2 *out_deltas,
                                  uint32_t capacity, uint32_t *out_count);
AfResult af_path_follow(const AfPathData *paths, uint64_t path_id, float normalized_distance,
                        float tolerance, AfPathTessellationScratch *scratch,
                        AfTransform2D *out_transform);

#endif
