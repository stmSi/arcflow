#ifndef AF_PROJECT_COLLABORATION_H
#define AF_PROJECT_COLLABORATION_H

#include "arcflow/arcflow_result.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AF_PROJECT_ID_CAPACITY 64U
#define AF_PROJECT_CHANGE_TOKEN_CAPACITY 64U
#define AF_PROJECT_MAX_CHANGES 32U
#define AF_PROJECT_LOCK_PATH_CAPACITY 768U
#define AF_PROJECT_LOCK_OWNER_CAPACITY 96U
#define AF_PROJECT_MAX_CONFLICTS 32U

typedef struct AfProjectChange {
    uint64_t revision;
    char author[AF_PROJECT_CHANGE_TOKEN_CAPACITY];
    char summary[AF_PROJECT_CHANGE_TOKEN_CAPACITY];
} AfProjectChange;

typedef struct AfProjectMetadata {
    char project_id[AF_PROJECT_ID_CAPACITY];
    uint64_t revision;
    AfProjectChange changes[AF_PROJECT_MAX_CHANGES];
    uint32_t change_count;
} AfProjectMetadata;

typedef enum AfProjectConflictMarker {
    AF_PROJECT_CONFLICT_OURS = 0,
    AF_PROJECT_CONFLICT_BASE,
    AF_PROJECT_CONFLICT_SEPARATOR,
    AF_PROJECT_CONFLICT_THEIRS
} AfProjectConflictMarker;

typedef struct AfProjectConflict {
    uint32_t line;
    AfProjectConflictMarker marker;
} AfProjectConflict;

typedef struct AfProjectConflictReport {
    AfProjectConflict entries[AF_PROJECT_MAX_CONFLICTS];
    uint32_t count;
    bool truncated;
} AfProjectConflictReport;

typedef struct AfProjectFingerprint {
    uint64_t content_hash;
    uint64_t size;
    bool exists;
} AfProjectFingerprint;

typedef struct AfProjectLock {
    char path[AF_PROJECT_LOCK_PATH_CAPACITY];
    char owner[AF_PROJECT_LOCK_OWNER_CAPACITY];
    uint64_t token;
    bool acquired;
} AfProjectLock;

void af_project_metadata_default(AfProjectMetadata *metadata);
AfResult af_project_metadata_validate(const AfProjectMetadata *metadata);
AfResult af_project_metadata_record(AfProjectMetadata *metadata, const char *author,
                                    const char *summary);
AfResult af_project_conflicts_scan(const void *data, size_t size,
                                   AfProjectConflictReport *out_report);
const char *af_project_conflict_marker_name(AfProjectConflictMarker marker);
AfResult af_project_fingerprint_file(const char *path, size_t maximum_size,
                                     AfProjectFingerprint *out_fingerprint);
bool af_project_fingerprint_changed(const AfProjectFingerprint *baseline,
                                    const AfProjectFingerprint *current);
AfResult af_project_lock_acquire(const char *project_path, const char *owner,
                                 AfProjectLock *out_lock, char *existing_owner,
                                 size_t existing_owner_capacity);
AfResult af_project_lock_query(const char *project_path, char *owner, size_t owner_capacity);
AfResult af_project_lock_release(AfProjectLock *lock);

#endif
