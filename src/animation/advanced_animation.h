#ifndef AF_ADVANCED_ANIMATION_H
#define AF_ADVANCED_ANIMATION_H

#include "animation/animation.h"

#include <stdbool.h>
#include <stdint.h>

#define AF_MAX_CLIP_MARKERS 4096U
#define AF_MAX_TIMELINE_REGIONS 1024U
#define AF_MAX_NESTED_TIMELINES 1024U
#define AF_MAX_ANIMATION_LAYERS 64U
#define AF_MAX_SPEED_POINTS 2048U
#define AF_MAX_MOTION_PATH_POINTS 4096U
#define AF_MAX_GHOST_FRAMES 32U
#define AF_MAX_NESTED_TIMELINE_DEPTH 16U

typedef enum AfEasingPreset {
    AF_EASING_EASE_IN = 0,
    AF_EASING_EASE_OUT,
    AF_EASING_EASE_IN_OUT,
    AF_EASING_BACK,
    AF_EASING_BOUNCE,
    AF_EASING_ELASTIC,
    AF_EASING_OVERSHOOT,
    AF_EASING_CUSTOM_CUBIC
} AfEasingPreset;

typedef struct AfClipMarker {
    uint64_t clip_id;
    uint64_t event_id;
    float time;
} AfClipMarker;

typedef struct AfTimelineRegion {
    uint64_t clip_id;
    uint64_t id;
    float start_time;
    float end_time;
} AfTimelineRegion;

typedef struct AfNestedTimeline {
    uint64_t owner_clip_id;
    uint64_t child_clip_id;
    float start_time;
    float duration;
    float child_start_time;
    float child_end_time;
    float speed;
    bool hold;
} AfNestedTimeline;

typedef struct AfAnimationLayer {
    uint64_t clip_id;
    float time;
    float weight;
    float transition_duration;
    int32_t order;
    bool additive;
    bool enabled;
} AfAnimationLayer;

typedef struct AfSpeedPoint {
    uint64_t clip_id;
    float time;
    float speed;
} AfSpeedPoint;

typedef struct AfAdvancedAnimationData {
    AfClipMarker *markers;
    uint32_t marker_count;
    uint32_t marker_capacity;
    AfTimelineRegion *regions;
    uint32_t region_count;
    uint32_t region_capacity;
    AfNestedTimeline *nested;
    uint32_t nested_count;
    uint32_t nested_capacity;
    AfAnimationLayer *layers;
    uint32_t layer_count;
    uint32_t layer_capacity;
    AfSpeedPoint *speed_points;
    uint32_t speed_point_count;
    uint32_t speed_point_capacity;
} AfAdvancedAnimationData;

typedef struct AfMotionPathPoint {
    float time;
    AfVec2 position;
} AfMotionPathPoint;

typedef struct AfAnimationEditSnapshot {
    AfAnimationClip *clips;
    uint32_t clip_count;
    uint32_t clip_capacity;
    AfAnimationTrack *tracks;
    uint32_t track_count;
    uint32_t track_capacity;
    AfKeyframe *keyframes;
    uint32_t keyframe_count;
    uint32_t keyframe_capacity;
} AfAnimationEditSnapshot;

AfResult af_advanced_animation_validate(const AfAdvancedAnimationData *advanced,
                                        const AfAnimationData *animation);
AfResult af_advanced_clip_duplicate(AfAnimationData *animation, uint32_t clip_capacity,
                                    uint32_t track_capacity, uint32_t keyframe_capacity,
                                    uint64_t source_clip_id, uint64_t new_clip_id,
                                    uint64_t first_track_id);
AfResult af_advanced_clip_trim(AfAnimationData *animation, uint64_t clip_id, float start_time,
                               float end_time);
AfResult af_advanced_clip_split(AfAnimationData *animation, uint32_t clip_capacity,
                                uint32_t track_capacity, uint32_t keyframe_capacity,
                                uint64_t clip_id, uint64_t new_clip_id, uint64_t first_track_id,
                                float split_time);
AfResult af_advanced_retime_keys(AfAnimationData *animation, const uint32_t *key_indices,
                                 uint32_t key_count, float pivot, float scale, bool reverse);
AfResult af_advanced_snapshot_capture(const AfAnimationData *animation,
                                      AfAnimationEditSnapshot *snapshot);
AfResult af_advanced_snapshot_restore(AfAnimationData *animation,
                                      const AfAnimationEditSnapshot *snapshot);
float af_advanced_easing_evaluate(AfEasingPreset preset, const float custom[4], float progress);
AfResult af_advanced_map_nested_time(const AfAdvancedAnimationData *advanced,
                                     const AfAnimationData *animation, uint64_t owner_clip_id,
                                     uint64_t child_clip_id, float owner_time, bool reverse,
                                     float *out_child_time);
AfResult af_advanced_apply_layers(const AfAdvancedAnimationData *advanced,
                                  const AfAnimationData *animation, AfScene *scene);
AfResult af_advanced_motion_path(const AfAnimationData *animation, uint64_t clip_id,
                                 uint64_t node_id, uint32_t sample_count,
                                 AfMotionPathPoint *out_points, uint32_t capacity,
                                 uint32_t *out_count);
AfResult af_advanced_motion_path_edit(AfAnimationData *animation, uint64_t track_id,
                                      uint32_t key_offset, AfVec2 position);
AfResult af_advanced_ghost_frames(float current_time, float frame_step, uint32_t before,
                                  uint32_t after, float *out_times, uint32_t capacity,
                                  uint32_t *out_count);

#endif
