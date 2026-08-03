#ifndef AF_RIGGING_H
#define AF_RIGGING_H

#include "arcflow/arcflow_result.h"
#include "deformation/deformation.h"

#include <stdbool.h>
#include <stdint.h>

#define AF_MAX_BONES 256U
#define AF_MAX_SKIN_WEIGHTS (AF_MAX_MESH_VERTICES * 4U)
#define AF_MAX_WEIGHTS_PER_VERTEX 4U
#define AF_MAX_RIG_CONSTRAINTS 512U
#define AF_MAX_IK_CHAIN_LENGTH 32U
#define AF_MAX_SOLVER_ITERATIONS 32U
#define AF_MAX_BONE_KEYFRAMES 65536U
#define AF_MAX_PROCEDURAL_TRACKS 256U
#define AF_MAX_FIXED_STEPS_PER_UPDATE 8U
#define AF_PROCEDURAL_FIXED_STEP (1.0F / 120.0F)

typedef struct AfBone {
    uint64_t id;
    uint64_t parent_id;
    AfTransform2D rest_transform;
    AfTransform2D local_transform;
    AfAffine2 world_transform;
    float length;
} AfBone;

typedef struct AfSkinWeight {
    uint32_t vertex_index;
    uint64_t bone_id;
    float weight;
} AfSkinWeight;

typedef enum AfConstraintType {
    AF_CONSTRAINT_IK = 0,
    AF_CONSTRAINT_AIM,
    AF_CONSTRAINT_POSITION,
    AF_CONSTRAINT_ROTATION,
    AF_CONSTRAINT_SCALE,
    AF_CONSTRAINT_DISTANCE,
    AF_CONSTRAINT_PATH_FOLLOW,
    AF_CONSTRAINT_TRANSFORM_COPY
} AfConstraintType;

typedef struct AfRigConstraint {
    uint64_t id;
    AfConstraintType type;
    uint64_t bone_id;
    uint64_t target_bone_id;
    uint64_t path_id;
    AfVec2 target_position;
    float target_rotation;
    AfVec2 target_scale;
    float distance;
    float path_position;
    float weight;
    uint32_t chain_length;
    uint32_t iterations;
} AfRigConstraint;

typedef struct AfRigData {
    AfBone *bones;
    uint32_t bone_count;
    uint32_t bone_capacity;
    AfSkinWeight *weights;
    uint32_t weight_count;
    uint32_t weight_capacity;
    AfRigConstraint *constraints;
    uint32_t constraint_count;
    uint32_t constraint_capacity;
} AfRigData;

typedef struct AfBoneKeyframe {
    uint64_t bone_id;
    float time;
    AfTransform2D transform;
} AfBoneKeyframe;

typedef struct AfBoneAnimation {
    AfBoneKeyframe *keyframes;
    uint32_t keyframe_count;
    uint32_t keyframe_capacity;
    float duration;
} AfBoneAnimation;

typedef enum AfProceduralType {
    AF_PROCEDURAL_SPRING = 0,
    AF_PROCEDURAL_DAMPED,
    AF_PROCEDURAL_OSCILLATION,
    AF_PROCEDURAL_NOISE,
    AF_PROCEDURAL_FOLLOW_THROUGH,
    AF_PROCEDURAL_SECONDARY
} AfProceduralType;

typedef enum AfProceduralProperty {
    AF_PROCEDURAL_POSITION_X = 0,
    AF_PROCEDURAL_POSITION_Y,
    AF_PROCEDURAL_ROTATION,
    AF_PROCEDURAL_SCALE_X,
    AF_PROCEDURAL_SCALE_Y
} AfProceduralProperty;

typedef struct AfProceduralTrack {
    uint64_t bone_id;
    uint64_t source_bone_id;
    AfProceduralType type;
    AfProceduralProperty property;
    float target;
    float amplitude;
    float frequency;
    float stiffness;
    float damping;
    uint32_t seed;
} AfProceduralTrack;

typedef struct AfProceduralData {
    AfProceduralTrack *tracks;
    uint32_t track_count;
    uint32_t track_capacity;
} AfProceduralData;

typedef struct AfProceduralState {
    float value;
    float velocity;
    bool initialized;
} AfProceduralState;

typedef struct AfProceduralRuntime {
    const AfProceduralData *data;
    AfProceduralState *states;
    uint32_t state_count;
    uint32_t state_capacity;
    float accumulator;
    float time;
    uint64_t tick;
} AfProceduralRuntime;

typedef struct AfRigMetrics {
    uint64_t solver_iterations;
    uint64_t fixed_steps;
    uint64_t deformed_vertices;
} AfRigMetrics;

AfResult af_rig_validate(const AfRigData *rig, uint32_t vertex_count, const AfPathData *paths);
AfResult af_rig_evaluate(AfRigData *rig, const AfPathData *paths,
                         AfPathTessellationScratch *scratch, AfRigMetrics *metrics);
AfResult af_rig_skin(const AfRigData *rig, const AfVec2 *bind_vertices, uint32_t vertex_count,
                     AfVec2 *out_vertices, uint32_t capacity, AfRigMetrics *metrics);
AfResult af_rig_add_bone(AfRigData *rig, const AfBone *bone);
AfResult af_rig_delete_bone(AfRigData *rig, uint64_t bone_id);
AfResult af_rig_reparent_bone(AfRigData *rig, uint64_t bone_id, uint64_t parent_id);
AfResult af_bone_animation_validate(const AfBoneAnimation *animation, const AfRigData *rig);
AfResult af_bone_animation_apply(const AfBoneAnimation *animation, float time, AfRigData *rig);
AfResult af_procedural_validate(const AfProceduralData *data, const AfRigData *rig);
AfResult af_procedural_runtime_init(AfProceduralRuntime *runtime, const AfProceduralData *data,
                                    AfProceduralState *states, uint32_t capacity);
AfResult af_procedural_update(AfProceduralRuntime *runtime, float delta_time, AfRigData *rig,
                              AfRigMetrics *metrics);
AfResult af_procedural_bake(AfProceduralRuntime *runtime, AfRigData *rig, float duration,
                            float sample_step, AfBoneKeyframe *out_keyframes, uint32_t capacity,
                            uint32_t *out_count, AfRigMetrics *metrics);

#endif
