#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "project/project_collaboration.h"

#include "af/core/hash.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#define AF_PROJECT_LOCK_MAX_SIZE 512U
#define AF_PROJECT_FINGERPRINT_BUFFER_SIZE 65536U

static atomic_uint_fast64_t af_project_lock_sequence = 1U;

static bool af_project_token_valid(const char *token, size_t capacity) {
    size_t length = 0U;
    if (token == NULL || token[0] == '\0') {
        return false;
    }
    while (length < capacity && token[length] != '\0') {
        ++length;
    }
    if (length == capacity) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        const unsigned char byte = (unsigned char)token[index];
        const bool alphabetic = (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z');
        const bool numeric = byte >= '0' && byte <= '9';
        if (!(alphabetic || numeric || byte == '-' || byte == '_' || byte == '.')) {
            return false;
        }
    }
    return true;
}

static unsigned long af_project_process_id(void) {
#if defined(_WIN32)
    return (unsigned long)GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}

static AfResult af_project_lock_path(const char *project_path, char *output, size_t capacity) {
    int written = 0;
    if (project_path == NULL || project_path[0] == '\0' || output == NULL || capacity == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    written = snprintf(output, capacity, "%s.aflock", project_path);
    return written >= 0 && (size_t)written < capacity ? AF_RESULT_SUCCESS
                                                      : AF_RESULT_LIMIT_EXCEEDED;
}

void af_project_metadata_default(AfProjectMetadata *metadata) {
    if (metadata != NULL) {
        *metadata = (AfProjectMetadata){0};
        (void)snprintf(metadata->project_id, sizeof(metadata->project_id), "arcflow-project");
    }
}

AfResult af_project_metadata_validate(const AfProjectMetadata *metadata) {
    uint64_t previous_revision = 0U;
    if (metadata == NULL ||
        !af_project_token_valid(metadata->project_id, sizeof(metadata->project_id)) ||
        metadata->revision == UINT64_MAX || metadata->change_count > AF_PROJECT_MAX_CHANGES) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < metadata->change_count; ++index) {
        const AfProjectChange *change = &metadata->changes[index];
        if (change->revision == 0U || change->revision > metadata->revision ||
            change->revision <= previous_revision ||
            !af_project_token_valid(change->author, sizeof(change->author)) ||
            !af_project_token_valid(change->summary, sizeof(change->summary))) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        previous_revision = change->revision;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_metadata_record(AfProjectMetadata *metadata, const char *author,
                                    const char *summary) {
    AfProjectChange *change = NULL;
    if (metadata == NULL || metadata->revision == UINT64_MAX - 1U ||
        !af_project_token_valid(author, AF_PROJECT_CHANGE_TOKEN_CAPACITY) ||
        !af_project_token_valid(summary, AF_PROJECT_CHANGE_TOKEN_CAPACITY) ||
        af_project_metadata_validate(metadata) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (metadata->change_count == AF_PROJECT_MAX_CHANGES) {
        memmove(&metadata->changes[0], &metadata->changes[1],
                (AF_PROJECT_MAX_CHANGES - 1U) * sizeof(metadata->changes[0]));
        --metadata->change_count;
    }
    ++metadata->revision;
    change = &metadata->changes[metadata->change_count++];
    *change = (AfProjectChange){.revision = metadata->revision};
    (void)snprintf(change->author, sizeof(change->author), "%s", author);
    (void)snprintf(change->summary, sizeof(change->summary), "%s", summary);
    return AF_RESULT_SUCCESS;
}

AfResult af_project_conflicts_scan(const void *data, size_t size,
                                   AfProjectConflictReport *out_report) {
    const unsigned char *bytes = data;
    size_t line_start = 0U;
    uint32_t line = 1U;
    if ((data == NULL && size != 0U) || out_report == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_report = (AfProjectConflictReport){0};
    while (line_start < size) {
        size_t line_end = line_start;
        AfProjectConflictMarker marker = AF_PROJECT_CONFLICT_OURS;
        bool conflict = false;
        while (line_end < size && bytes[line_end] != '\n' && bytes[line_end] != '\r') {
            ++line_end;
        }
        if (line_end - line_start >= 7U && memcmp(bytes + line_start, "<<<<<<<", 7U) == 0) {
            marker = AF_PROJECT_CONFLICT_OURS;
            conflict = true;
        } else if (line_end - line_start >= 7U && memcmp(bytes + line_start, "|||||||", 7U) == 0) {
            marker = AF_PROJECT_CONFLICT_BASE;
            conflict = true;
        } else if (line_end - line_start >= 7U && memcmp(bytes + line_start, "=======", 7U) == 0) {
            marker = AF_PROJECT_CONFLICT_SEPARATOR;
            conflict = true;
        } else if (line_end - line_start >= 7U && memcmp(bytes + line_start, ">>>>>>>", 7U) == 0) {
            marker = AF_PROJECT_CONFLICT_THEIRS;
            conflict = true;
        }
        if (conflict) {
            if (out_report->count < AF_PROJECT_MAX_CONFLICTS) {
                out_report->entries[out_report->count++] =
                    (AfProjectConflict){.line = line, .marker = marker};
            } else {
                out_report->truncated = true;
            }
        }
        if (line_end < size) {
            const unsigned char newline = bytes[line_end++];
            if (newline == '\r' && line_end < size && bytes[line_end] == '\n') {
                ++line_end;
            }
            ++line;
        }
        line_start = line_end;
    }
    return out_report->count == 0U ? AF_RESULT_SUCCESS : AF_RESULT_INVALID_PACKAGE;
}

const char *af_project_conflict_marker_name(AfProjectConflictMarker marker) {
    static const char *const names[] = {"ours", "merge-base", "separator", "theirs"};
    return (uint32_t)marker <= (uint32_t)AF_PROJECT_CONFLICT_THEIRS ? names[marker] : "unknown";
}

AfResult af_project_fingerprint_file(const char *path, size_t maximum_size,
                                     AfProjectFingerprint *out_fingerprint) {
    unsigned char buffer[AF_PROJECT_FINGERPRINT_BUFFER_SIZE];
    FILE *file = NULL;
    uint64_t hash = UINT64_C(14695981039346656037);
    uint64_t total = 0U;
    if (path == NULL || path[0] == '\0' || maximum_size == 0U || out_fingerprint == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_fingerprint = (AfProjectFingerprint){0};
    errno = 0;
    file = fopen(path, "rb");
    if (file == NULL) {
        return errno == ENOENT ? AF_RESULT_SUCCESS : AF_RESULT_IO_ERROR;
    }
    out_fingerprint->exists = true;
    for (;;) {
        const size_t count = fread(buffer, 1U, sizeof(buffer), file);
        if (count != 0U) {
            if ((uint64_t)count > (uint64_t)maximum_size ||
                total > (uint64_t)maximum_size - (uint64_t)count) {
                (void)fclose(file);
                return AF_RESULT_LIMIT_EXCEEDED;
            }
            for (size_t index = 0U; index < count; ++index) {
                hash ^= buffer[index];
                hash *= UINT64_C(1099511628211);
            }
            total += (uint64_t)count;
        }
        if (count != sizeof(buffer)) {
            if (ferror(file) != 0) {
                (void)fclose(file);
                return AF_RESULT_IO_ERROR;
            }
            break;
        }
    }
    if (fclose(file) != 0) {
        return AF_RESULT_IO_ERROR;
    }
    out_fingerprint->content_hash = hash;
    out_fingerprint->size = total;
    return AF_RESULT_SUCCESS;
}

bool af_project_fingerprint_changed(const AfProjectFingerprint *baseline,
                                    const AfProjectFingerprint *current) {
    return baseline == NULL || current == NULL || baseline->exists != current->exists ||
           baseline->size != current->size || baseline->content_hash != current->content_hash;
}

AfResult af_project_lock_query(const char *project_path, char *owner, size_t owner_capacity) {
    char lock_path[AF_PROJECT_LOCK_PATH_CAPACITY];
    char line[AF_PROJECT_LOCK_MAX_SIZE];
    char parsed_owner[AF_PROJECT_LOCK_OWNER_CAPACITY];
    FILE *file = NULL;
    unsigned long process_id = 0UL;
    unsigned long long token = 0ULL;
    if (owner == NULL || owner_capacity == 0U ||
        af_project_lock_path(project_path, lock_path, sizeof(lock_path)) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    owner[0] = '\0';
    errno = 0;
    file = fopen(lock_path, "rb");
    if (file == NULL) {
        return errno == ENOENT ? AF_RESULT_NOT_FOUND : AF_RESULT_IO_ERROR;
    }
    if (fgets(line, (int)sizeof(line), file) == NULL) {
        (void)fclose(file);
        return AF_RESULT_INVALID_PACKAGE;
    }
    if (fclose(file) != 0 ||
        sscanf(line, "arcflow_lock 1 %lu %llu %95s", &process_id, &token, parsed_owner) != 3 ||
        !af_project_token_valid(parsed_owner, sizeof(parsed_owner)) ||
        strlen(parsed_owner) >= owner_capacity) {
        owner[0] = '\0';
        return AF_RESULT_INVALID_PACKAGE;
    }
    (void)snprintf(owner, owner_capacity, "%s", parsed_owner);
    (void)process_id;
    (void)token;
    return AF_RESULT_SUCCESS;
}

AfResult af_project_lock_acquire(const char *project_path, const char *owner,
                                 AfProjectLock *out_lock, char *existing_owner,
                                 size_t existing_owner_capacity) {
    char lock_path[AF_PROJECT_LOCK_PATH_CAPACITY];
    FILE *file = NULL;
    unsigned long process_id = af_project_process_id();
    uint64_t token = 0U;
    int written = 0;
    bool write_failed = false;
    if (out_lock == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_lock = (AfProjectLock){0};
    if (!af_project_token_valid(owner, AF_PROJECT_LOCK_OWNER_CAPACITY) ||
        (existing_owner == NULL && existing_owner_capacity != 0U) ||
        af_project_lock_path(project_path, lock_path, sizeof(lock_path)) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (existing_owner != NULL && existing_owner_capacity != 0U) {
        existing_owner[0] = '\0';
    }
    token = atomic_fetch_add_explicit(&af_project_lock_sequence, 1U, memory_order_relaxed) ^
            ((uint64_t)process_id << 32U);
    errno = 0;
    file = fopen(lock_path, "wbx");
    if (file == NULL) {
        if (errno == EEXIST && existing_owner != NULL && existing_owner_capacity != 0U) {
            (void)af_project_lock_query(project_path, existing_owner, existing_owner_capacity);
            return AF_RESULT_STALE_HANDLE;
        }
        return AF_RESULT_IO_ERROR;
    }
    written =
        fprintf(file, "arcflow_lock 1 %lu %llu %s\n", process_id, (unsigned long long)token, owner);
    write_failed = written <= 0;
    if (fflush(file) != 0) {
        write_failed = true;
    }
    if (fclose(file) != 0) {
        write_failed = true;
    }
    if (write_failed) {
        (void)remove(lock_path);
        return AF_RESULT_IO_ERROR;
    }
    (void)snprintf(out_lock->path, sizeof(out_lock->path), "%s", lock_path);
    out_lock->token = token;
    out_lock->acquired = true;
    (void)snprintf(out_lock->owner, sizeof(out_lock->owner), "%s", owner);
    return AF_RESULT_SUCCESS;
}

AfResult af_project_lock_release(AfProjectLock *lock) {
    char line[AF_PROJECT_LOCK_MAX_SIZE];
    FILE *file = NULL;
    unsigned long process_id = 0UL;
    unsigned long long token = 0ULL;
    char owner[AF_PROJECT_LOCK_OWNER_CAPACITY];
    if (lock == NULL || !lock->acquired || lock->path[0] == '\0') {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    file = fopen(lock->path, "rb");
    if (file == NULL) {
        return AF_RESULT_IO_ERROR;
    }
    if (fgets(line, (int)sizeof(line), file) == NULL) {
        (void)fclose(file);
        return AF_RESULT_IO_ERROR;
    }
    if (fclose(file) != 0 ||
        sscanf(line, "arcflow_lock 1 %lu %llu %95s", &process_id, &token, owner) != 3) {
        return AF_RESULT_IO_ERROR;
    }
    (void)process_id;
    if ((uint64_t)token != lock->token || strcmp(owner, lock->owner) != 0) {
        return AF_RESULT_STALE_HANDLE;
    }
    if (remove(lock->path) != 0) {
        return AF_RESULT_IO_ERROR;
    }
    *lock = (AfProjectLock){0};
    return AF_RESULT_SUCCESS;
}
