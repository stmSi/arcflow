#ifndef AF_PROJECT_H
#define AF_PROJECT_H

#include "arcflow/arcflow_result.h"
#include "assets/package.h"
#include "binding/binding.h"
#include "core/allocator.h"
#include "layout/layout.h"
#include "project/project_collaboration.h"
#include "scene/scene.h"

#include <stddef.h>
#include <stdint.h>

#define AF_PROJECT_FORMAT_VERSION 13U
#define AF_PROJECT_MAX_FILE_SIZE (32U * 1024U * 1024U)
#define AF_PROJECT_NAME_CAPACITY 64U
#define AF_PROJECT_MAX_CLIPS 64U
#define AF_PROJECT_MAX_TRACKS 1024U
#define AF_PROJECT_MAX_KEYFRAMES 16384U
#define AF_PROJECT_VALIDATION_ID_CAPACITY 32768U

typedef struct AfProjectClip {
    uint64_t id;
    char name[AF_PROJECT_NAME_CAPACITY];
    float duration;
    float loop_start;
    float loop_end;
    bool loop;
} AfProjectClip;

typedef struct AfProjectTrack {
    uint64_t id;
    uint64_t clip_id;
    uint64_t node_id;
    AfPropertyType property;
    uint32_t element_index;
} AfProjectTrack;

typedef struct AfProjectKeyframe {
    uint64_t id;
    uint64_t track_id;
    AfKeyframe keyframe;
} AfProjectKeyframe;

typedef struct AfProject {
    AfScene scene;
    AfProjectMetadata metadata;
    char (*names)[AF_PROJECT_NAME_CAPACITY];
    AfVec2 canvas_size;
    uint64_t next_id;
    AfProjectClip *clips;
    uint32_t clip_count;
    uint32_t clip_capacity;
    AfProjectTrack *tracks;
    uint32_t track_count;
    uint32_t track_capacity;
    AfProjectKeyframe *keyframes;
    uint32_t keyframe_count;
    uint32_t keyframe_capacity;
    uint64_t next_animation_id;
    uint64_t next_asset_id;
    AfPathData path_data;
    AfStyleData style_data;
    AfLayoutData layout_data;
    AfParticleData particle_data;
    AfImageData image_data;
    AfBindingData binding_data;
    uint64_t *validation_ids;
    AfAllocator *allocator;
} AfProject;

typedef struct AfProjectNodeSnapshot {
    AfSceneNode node;
    AfStyle style;
    AfLayoutNode layout;
    AfParticleEmitter emitter;
    AfVfxLodSettings vfx_lod;
    bool has_layout;
    bool has_emitter;
    AfImageBinding image_binding;
    bool has_image_binding;
    char name[AF_PROJECT_NAME_CAPACITY];
    uint32_t index;
} AfProjectNodeSnapshot;

typedef struct AfProjectClipSnapshot {
    AfProjectClip clip;
    uint32_t index;
} AfProjectClipSnapshot;

typedef struct AfProjectTrackSnapshot {
    AfProjectTrack track;
    uint32_t index;
} AfProjectTrackSnapshot;

typedef struct AfProjectKeyframeSnapshot {
    AfProjectKeyframe keyframe;
    uint32_t index;
} AfProjectKeyframeSnapshot;

typedef struct AfProjectPathSnapshot {
    AfPath path;
    uint32_t index;
    uint32_t command_offset;
} AfProjectPathSnapshot;

AfResult af_project_create(AfProject *project, AfAllocator *allocator, uint32_t capacity);
void af_project_destroy(AfProject *project);
AfResult af_project_validate(const AfProject *project);
AfResult af_project_add_node(AfProject *project, const AfSceneNode *node, const char *name);
AfResult af_project_insert_snapshot(AfProject *project, const AfProjectNodeSnapshot *snapshot);
AfResult af_project_remove_node_exact(AfProject *project, uint64_t id);
AfResult af_project_delete_subtree(AfProject *project, uint64_t id);
AfResult af_project_snapshot(const AfProject *project, uint64_t id,
                             AfProjectNodeSnapshot *out_snapshot);
AfResult af_project_collect_subtree(const AfProject *project, uint64_t id,
                                    AfProjectNodeSnapshot *snapshots, uint32_t capacity,
                                    uint32_t *out_count);
uint32_t af_project_find_index(const AfProject *project, uint64_t id);
const char *af_project_name(const AfProject *project, uint64_t id);
AfResult af_project_set_name(AfProject *project, uint64_t id, const char *name);
AfResult af_project_allocate_id(AfProject *project, uint64_t *out_id);
AfStyle *af_project_find_node_style(AfProject *project, uint64_t node_id);
AfResult af_project_add_style_preset(AfProject *project, const AfStyle *style);
AfResult af_project_set_layout(AfProject *project, const AfLayoutNode *layout);
AfResult af_project_clear_layout(AfProject *project, uint64_t node_id);
AfParticleEmitter *af_project_find_emitter(AfProject *project, uint64_t id);
AfParticleEmitter *af_project_find_node_emitter(AfProject *project, uint64_t node_id);
const AfParticleEmitter *af_project_find_node_emitter_const(const AfProject *project,
                                                            uint64_t node_id);
AfResult af_project_add_emitter(AfProject *project, const AfParticleEmitter *emitter);
AfResult af_project_remove_emitter(AfProject *project, uint64_t id);

AfProjectClip *af_project_find_clip(AfProject *project, uint64_t id);
const AfProjectClip *af_project_find_clip_const(const AfProject *project, uint64_t id);
AfProjectTrack *af_project_find_track(AfProject *project, uint64_t id);
const AfProjectTrack *af_project_find_track_const(const AfProject *project, uint64_t id);
AfProjectKeyframe *af_project_find_keyframe(AfProject *project, uint64_t id);
const AfProjectKeyframe *af_project_find_keyframe_const(const AfProject *project, uint64_t id);
AfResult af_project_add_clip(AfProject *project, const AfProjectClip *clip);
AfResult af_project_add_track(AfProject *project, const AfProjectTrack *track);
AfResult af_project_add_keyframe(AfProject *project, const AfProjectKeyframe *keyframe);
AfResult af_project_insert_clip_snapshot(AfProject *project, const AfProjectClipSnapshot *snapshot);
AfResult af_project_insert_track_snapshot(AfProject *project,
                                          const AfProjectTrackSnapshot *snapshot);
AfResult af_project_insert_keyframe_snapshot(AfProject *project,
                                             const AfProjectKeyframeSnapshot *snapshot);
AfResult af_project_remove_clip_exact(AfProject *project, uint64_t id);
AfResult af_project_remove_track_exact(AfProject *project, uint64_t id);
AfResult af_project_remove_keyframe_exact(AfProject *project, uint64_t id);
AfResult af_project_clip_snapshot(const AfProject *project, uint64_t id,
                                  AfProjectClipSnapshot *out_snapshot);
AfResult af_project_track_snapshot(const AfProject *project, uint64_t id,
                                   AfProjectTrackSnapshot *out_snapshot);
AfResult af_project_keyframe_snapshot(const AfProject *project, uint64_t id,
                                      AfProjectKeyframeSnapshot *out_snapshot);
AfResult af_project_allocate_animation_id(AfProject *project, uint64_t *out_id);
AfResult af_project_animation_apply(const AfProject *project, uint64_t clip_id, float time,
                                    AfScene *scene);
AfPath *af_project_find_path(AfProject *project, uint64_t path_id);
AfPath *af_project_find_node_path(AfProject *project, uint64_t node_id);
AfResult af_project_add_path(AfProject *project, const AfPath *path, const AfPathCommand *commands);
AfResult af_project_insert_path_snapshot(AfProject *project, const AfProjectPathSnapshot *snapshot,
                                         const AfPathCommand *commands);
AfResult af_project_remove_path_exact(AfProject *project, uint64_t path_id);
AfResult af_project_path_snapshot(const AfProject *project, uint64_t path_id,
                                  AfProjectPathSnapshot *out_snapshot, AfPathCommand *out_commands,
                                  uint32_t command_capacity);

AfResult af_project_serialize(const AfProject *project, AfAllocator *allocator,
                              AfPackageBlob *out_text);
AfResult af_project_deserialize_versioned(const void *data, size_t size, AfAllocator *allocator,
                                          uint32_t *out_source_format_version,
                                          AfProject *out_project);
AfResult af_project_deserialize(const void *data, size_t size, AfAllocator *allocator,
                                AfProject *out_project);
AfResult af_project_save_file(const AfProject *project, const char *path, AfAllocator *allocator);
AfResult af_project_load_file_versioned(const char *path, AfAllocator *allocator,
                                        uint32_t *out_source_format_version,
                                        AfProject *out_project);
AfResult af_project_load_file(const char *path, AfAllocator *allocator, AfProject *out_project);
AfResult af_project_compile(const AfProject *project, AfAllocator *allocator,
                            AfPackageBlob *out_package);
AfResult af_project_compile_release(const AfProject *project, AfAllocator *allocator,
                                    AfPackageBlob *out_package);
AfResult af_project_add_exposed_property(AfProject *project, const AfExposedProperty *property,
                                         const AfBindingTarget *targets);
AfResult af_project_remove_exposed_property(AfProject *project, uint64_t property_id);

#endif
