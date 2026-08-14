#ifndef AF_EDITOR_RECOVERY_H
#define AF_EDITOR_RECOVERY_H

#include "arcflow/arcflow_result.h"
#include "core/allocator.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AF_EDITOR_RECOVERY_API_VERSION 1U
#define AF_EDITOR_RECOVERY_FORMAT_VERSION 1U
#define AF_EDITOR_RECOVERY_PATH_CAPACITY 4096U
#define AF_EDITOR_RECOVERY_MAX_SNAPSHOT_SIZE (32U * 1024U * 1024U)
#define AF_EDITOR_RECOVERY_AUTOSAVE_INTERVAL_MS UINT64_C(30000)
#define AF_EDITOR_RECOVERY_IDLE_DELAY_MS UINT64_C(2000)
#define AF_EDITOR_RECOVERY_AUTOSAVE_BACKUPS 2U
#define AF_EDITOR_RECOVERY_PROJECT_BACKUPS 3U

typedef struct AfEditorRecoverySettings {
    uint32_t struct_size;
    uint32_t api_version;
    uint64_t autosave_interval_ms;
    uint64_t idle_delay_ms;
    size_t maximum_snapshot_size;
    uint32_t autosave_backup_count;
    uint32_t project_backup_count;
} AfEditorRecoverySettings;

typedef struct AfEditorRecoveryPaths {
    char project[AF_EDITOR_RECOVERY_PATH_CAPACITY];
    char autosave[AF_EDITOR_RECOVERY_PATH_CAPACITY];
    char recovery[AF_EDITOR_RECOVERY_PATH_CAPACITY];
} AfEditorRecoveryPaths;

typedef struct AfEditorRecoveryState {
    uint32_t struct_size;
    uint32_t api_version;
    AfEditorRecoveryPaths paths;
    uint64_t current_revision;
    uint64_t saved_revision;
    uint64_t autosaved_revision;
    uint64_t last_change_ms;
    uint64_t last_autosave_ms;
    bool dirty;
} AfEditorRecoveryState;

typedef enum AfEditorRecoverySource {
    AF_EDITOR_RECOVERY_SOURCE_NONE = 0,
    AF_EDITOR_RECOVERY_SOURCE_AUTOSAVE,
    AF_EDITOR_RECOVERY_SOURCE_RESTORE_STAGING
} AfEditorRecoverySource;

typedef enum AfEditorRecoveryStatus {
    AF_EDITOR_RECOVERY_STATUS_NONE = 0,
    AF_EDITOR_RECOVERY_STATUS_AVAILABLE,
    AF_EDITOR_RECOVERY_STATUS_STALE,
    AF_EDITOR_RECOVERY_STATUS_INCOMPATIBLE,
    AF_EDITOR_RECOVERY_STATUS_CORRUPT
} AfEditorRecoveryStatus;

typedef uint32_t AfEditorRecoveryIssueFlags;

#define AF_EDITOR_RECOVERY_ISSUE_STALE (UINT32_C(1) << 0U)
#define AF_EDITOR_RECOVERY_ISSUE_INCOMPATIBLE (UINT32_C(1) << 1U)
#define AF_EDITOR_RECOVERY_ISSUE_CORRUPT (UINT32_C(1) << 2U)

typedef struct AfEditorRecoveryCandidate {
    uint32_t struct_size;
    uint32_t api_version;
    AfEditorRecoveryStatus status;
    AfEditorRecoverySource source;
    AfEditorRecoveryIssueFlags issues;
    uint32_t backup_index;
    uint32_t project_format_version;
    uint64_t revision;
    uint64_t created_utc_seconds;
    uint64_t payload_size;
    uint64_t payload_checksum;
    bool project_exists;
    bool source_project_matches;
    char path[AF_EDITOR_RECOVERY_PATH_CAPACITY];
} AfEditorRecoveryCandidate;

AfResult af_editor_recovery_query_settings(AfEditorRecoverySettings *out_settings);
AfResult af_editor_recovery_derive_paths(const char *project_path,
                                         AfEditorRecoveryPaths *out_paths);
AfResult af_editor_recovery_init(AfEditorRecoveryState *state, const char *project_path,
                                 uint64_t now_ms);
AfResult af_editor_recovery_note_change(AfEditorRecoveryState *state, uint64_t now_ms);
AfResult af_editor_recovery_mark_project_saved(AfEditorRecoveryState *state, uint64_t now_ms);
bool af_editor_recovery_autosave_due(const AfEditorRecoveryState *state, uint64_t now_ms);
uint64_t af_editor_recovery_next_due_ms(const AfEditorRecoveryState *state);
AfResult af_editor_recovery_write_autosave(AfEditorRecoveryState *state,
                                           uint32_t project_format_version,
                                           const void *serialized_project, size_t size,
                                           uint64_t now_ms, AfAllocator *allocator,
                                           bool *out_written);
AfResult af_editor_recovery_find_candidate(const AfEditorRecoveryState *state,
                                           uint32_t maximum_project_format_version,
                                           AfAllocator *allocator,
                                           AfEditorRecoveryCandidate *out_candidate);
AfResult af_editor_recovery_restore(AfEditorRecoveryState *state,
                                    uint32_t maximum_project_format_version,
                                    const AfEditorRecoveryCandidate *candidate,
                                    AfAllocator *allocator);
AfResult af_editor_recovery_discard(const AfEditorRecoveryState *state);

#endif
