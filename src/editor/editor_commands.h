#ifndef AF_EDITOR_COMMANDS_H
#define AF_EDITOR_COMMANDS_H

#include "af/core/allocator.h"
#include "af/path/path_boolean.h"
#include "arcflow/arcflow_result.h"
#include "project/project.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AF_EDITOR_COMMAND_MAX_SNAPSHOTS 64U
#define AF_EDITOR_COMMAND_MAX_CLIPS 8U
#define AF_EDITOR_COMMAND_MAX_TRACKS 64U
#define AF_EDITOR_COMMAND_MAX_KEYFRAMES 256U
#define AF_EDITOR_COMMAND_MAX_PATHS AF_EDITOR_COMMAND_MAX_SNAPSHOTS
#define AF_EDITOR_COMMAND_MAX_PATH_COMMANDS 4096U
#define AF_EDITOR_HISTORY_MAX_COMMANDS 128U
#define AF_EDITOR_HISTORY_DEFAULT_BYTES (16U * 1024U * 1024U)

typedef enum AfEditorCommandType {
    AF_EDITOR_COMMAND_CREATE = 0,
    AF_EDITOR_COMMAND_DELETE,
    AF_EDITOR_COMMAND_DUPLICATE,
    AF_EDITOR_COMMAND_MODIFY,
    AF_EDITOR_COMMAND_ANIMATION,
    AF_EDITOR_COMMAND_PATH,
    AF_EDITOR_COMMAND_CONVERT_PATH,
    AF_EDITOR_COMMAND_GROUP,
    AF_EDITOR_COMMAND_REORDER,
    AF_EDITOR_COMMAND_COMPOUND
} AfEditorCommandType;

typedef enum AfEditorMergeKey {
    AF_EDITOR_MERGE_NONE = 0,
    AF_EDITOR_MERGE_TRANSFORM,
    AF_EDITOR_MERGE_FILL,
    AF_EDITOR_MERGE_OPACITY,
    AF_EDITOR_MERGE_RADIUS,
    AF_EDITOR_MERGE_RENAME,
    AF_EDITOR_MERGE_LAYER,
    AF_EDITOR_MERGE_PARENT,
    AF_EDITOR_MERGE_KEYFRAME_TIME,
    AF_EDITOR_MERGE_KEYFRAME_VALUE,
    AF_EDITOR_MERGE_TANGENT,
    AF_EDITOR_MERGE_LOOP_REGION,
    AF_EDITOR_MERGE_PATH_POINT,
    AF_EDITOR_MERGE_STYLE,
    AF_EDITOR_MERGE_PARTICLE
} AfEditorMergeKey;

typedef struct AfEditorCommand {
    AfEditorCommandType type;
    AfEditorMergeKey merge_key;
    uint64_t target_id;
    uint32_t before_count;
    uint32_t after_count;
    AfProjectNodeSnapshot before[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    AfProjectNodeSnapshot after[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint32_t clip_before_count;
    uint32_t clip_after_count;
    uint32_t track_before_count;
    uint32_t track_after_count;
    uint32_t keyframe_before_count;
    uint32_t keyframe_after_count;
    AfProjectClipSnapshot clip_before[AF_EDITOR_COMMAND_MAX_CLIPS];
    AfProjectClipSnapshot clip_after[AF_EDITOR_COMMAND_MAX_CLIPS];
    AfProjectTrackSnapshot track_before[AF_EDITOR_COMMAND_MAX_TRACKS];
    AfProjectTrackSnapshot track_after[AF_EDITOR_COMMAND_MAX_TRACKS];
    AfProjectKeyframeSnapshot keyframe_before[AF_EDITOR_COMMAND_MAX_KEYFRAMES];
    AfProjectKeyframeSnapshot keyframe_after[AF_EDITOR_COMMAND_MAX_KEYFRAMES];
    uint32_t path_before_count;
    uint32_t path_after_count;
    uint32_t path_command_before_count;
    uint32_t path_command_after_count;
    AfProjectPathSnapshot path_before[AF_EDITOR_COMMAND_MAX_PATHS];
    AfProjectPathSnapshot path_after[AF_EDITOR_COMMAND_MAX_PATHS];
    AfPathCommand path_command_before[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
    AfPathCommand path_command_after[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS];
} AfEditorCommand;

typedef struct AfEditorHistory {
    AfEditorCommand *commands;
    uint32_t count;
    uint32_t cursor;
    uint32_t capacity;
    size_t byte_limit;
    AfAllocator *allocator;
} AfEditorHistory;

AfResult af_editor_history_create(AfEditorHistory *history, AfAllocator *allocator,
                                  size_t byte_limit);
void af_editor_history_destroy(AfEditorHistory *history);
void af_editor_history_clear(AfEditorHistory *history);
size_t af_editor_history_memory_used(const AfEditorHistory *history);
bool af_editor_history_can_undo(const AfEditorHistory *history);
bool af_editor_history_can_redo(const AfEditorHistory *history);

AfResult af_editor_command_create(const AfProjectNodeSnapshot *snapshot,
                                  AfEditorCommand *out_command);
AfResult af_editor_command_delete(const AfProject *project, uint64_t id,
                                  AfEditorCommand *out_command);
AfResult af_editor_command_delete_many(const AfProject *project, const uint64_t *ids,
                                       uint32_t id_count, AfEditorCommand *out_command);
AfResult af_editor_command_duplicate(const AfProjectNodeSnapshot *snapshots, uint32_t count,
                                     AfEditorCommand *out_command);
AfResult af_editor_command_modify(const AfProjectNodeSnapshot *before,
                                  const AfProjectNodeSnapshot *after, AfEditorMergeKey merge_key,
                                  AfEditorCommand *out_command);
AfResult af_editor_command_animation(AfEditorMergeKey merge_key, uint64_t target_id,
                                     AfEditorCommand *out_command);
AfResult af_editor_command_path(AfEditorMergeKey merge_key, uint64_t target_id,
                                AfEditorCommand *out_command);
AfResult af_editor_command_convert_path(const AfProject *project, uint64_t node_id,
                                        AfEditorCommand *out_command);
AfResult af_editor_command_group(const AfProject *project, const uint64_t *ids, uint32_t count,
                                 uint64_t group_id, AfEditorCommand *out_command);
/* Static leaf siblings with solid appearance. The primary object's appearance is retained;
 * Combine uses even-odd filling. Builders never allocate IDs or mutate authored project data. */
AfResult af_editor_command_combine(AfProject *project, const uint64_t *ids, uint32_t count,
                                   uint64_t primary_id, AfEditorCommand *out_command);
AfResult af_editor_command_break_apart(AfProject *project, uint64_t node_id,
                                       AfEditorCommand *out_command);
/* Two closed static shapes. Subtract cuts other_id out of primary_id. Curves become
 * line contours; empty results return NOT_FOUND and leave the sources intact. */
AfResult af_editor_command_boolean(AfProject *project, uint64_t primary_id, uint64_t other_id,
                                   AfPathBooleanOperation operation, AfEditorCommand *out_command);
/* Outline expansion retains a visible source fill as a separate object. Offset replaces
 * the selected closed fill; distance is measured in the object's local units. */
AfResult af_editor_command_outline(AfProject *project, uint64_t id, bool offset, float distance,
                                   AfEditorCommand *out_command);
bool af_editor_can_reorder(const AfProject *project, uint64_t node_id, uint64_t target_id);
AfResult af_editor_command_reorder(AfProject *project, uint64_t node_id, uint64_t target_id,
                                   bool after, AfEditorCommand *out_command);
bool af_editor_can_reparent(const AfProject *project, uint64_t node_id, uint64_t parent_id);
AfResult af_editor_command_reparent(AfProject *project, uint64_t node_id, uint64_t parent_id,
                                    AfEditorCommand *out_command);
AfResult af_editor_command_polygon(const AfProject *project, uint64_t node_id, AfVec2 size,
                                   uint32_t sides, float inset, AfEditorCommand *out_command);
AfResult af_editor_history_execute(AfEditorHistory *history, AfProject *project,
                                   const AfEditorCommand *command, bool already_applied,
                                   bool allow_merge);
AfResult af_editor_history_undo(AfEditorHistory *history, AfProject *project);
AfResult af_editor_history_redo(AfEditorHistory *history, AfProject *project);

#endif
