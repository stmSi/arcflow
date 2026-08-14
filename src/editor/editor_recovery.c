#include "editor/editor_recovery.h"

#include "core/file_io.h"

#include <errno.h>
#include <stdalign.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define AF_EDITOR_RECOVERY_HEADER_SIZE 88U
#define AF_EDITOR_RECOVERY_SOURCE_EXISTS (UINT32_C(1) << 0U)
#define AF_EDITOR_RECOVERY_SOURCE_FINGERPRINT (UINT32_C(1) << 1U)
#define AF_EDITOR_RECOVERY_SOURCE_FLAGS                                                            \
    (AF_EDITOR_RECOVERY_SOURCE_EXISTS | AF_EDITOR_RECOVERY_SOURCE_FINGERPRINT)
#define AF_EDITOR_RECOVERY_FNV_OFFSET UINT64_C(14695981039346656037)
#define AF_EDITOR_RECOVERY_FNV_PRIME UINT64_C(1099511628211)

typedef struct AfEditorRecoveryFileInfo {
    bool exists;
    bool fingerprint_valid;
    uint64_t modified_utc_seconds;
    uint64_t size;
    uint64_t checksum;
} AfEditorRecoveryFileInfo;

typedef struct AfEditorRecoveryRecord {
    uint32_t project_format_version;
    uint32_t flags;
    uint64_t revision;
    uint64_t created_utc_seconds;
    uint64_t source_modified_utc_seconds;
    uint64_t source_size;
    uint64_t source_checksum;
    uint64_t payload_size;
    uint64_t payload_checksum;
    const uint8_t *payload;
} AfEditorRecoveryRecord;

typedef struct AfEditorRecoverySelection {
    AfEditorRecoverySource source;
    uint32_t backup_index;
    AfEditorRecoveryFileInfo file;
    AfEditorRecoveryRecord record;
    bool source_matches;
    char path[AF_EDITOR_RECOVERY_PATH_CAPACITY];
} AfEditorRecoverySelection;

static const uint8_t af_editor_recovery_magic[8] = {'A', 'F', 'R', 'E', 'C', 'V', '1', '\0'};

static bool af_editor_recovery_state_valid(const AfEditorRecoveryState *state) {
    return state != NULL && state->struct_size >= (uint32_t)sizeof(*state) &&
           state->api_version == AF_EDITOR_RECOVERY_API_VERSION &&
           state->paths.project[0] != '\0' && state->paths.autosave[0] != '\0' &&
           state->paths.recovery[0] != '\0';
}

static bool af_editor_recovery_output_valid(uint32_t struct_size, uint32_t expected_size,
                                            uint32_t api_version) {
    return struct_size >= expected_size && api_version == AF_EDITOR_RECOVERY_API_VERSION;
}

static size_t af_editor_recovery_string_length(const char *text, size_t capacity) {
    size_t length = 0U;
    if (text == NULL) {
        return capacity;
    }
    while (length < capacity && text[length] != '\0') {
        ++length;
    }
    return length;
}

static AfResult af_editor_recovery_append_path(const char *base, const char *suffix,
                                               char *out_path) {
    const size_t base_length =
        af_editor_recovery_string_length(base, AF_EDITOR_RECOVERY_PATH_CAPACITY);
    const size_t suffix_length = strlen(suffix);
    if (base_length == 0U || base_length >= AF_EDITOR_RECOVERY_PATH_CAPACITY ||
        suffix_length >= AF_EDITOR_RECOVERY_PATH_CAPACITY ||
        base_length > AF_EDITOR_RECOVERY_PATH_CAPACITY - suffix_length - 1U) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memcpy(out_path, base, base_length);
    memcpy(out_path + base_length, suffix, suffix_length + 1U);
    return AF_RESULT_SUCCESS;
}

static AfResult af_editor_recovery_backup_path(const char *path, uint32_t backup_index,
                                               char *out_path) {
    char suffix[24];
    const int length = snprintf(suffix, sizeof(suffix), ".bak%u", backup_index);
    if (backup_index == 0U || length < 0 || (size_t)length >= sizeof(suffix)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return af_editor_recovery_append_path(path, suffix, out_path);
}

static AfResult af_editor_recovery_candidate_path(const AfEditorRecoveryState *state,
                                                  AfEditorRecoverySource source,
                                                  uint32_t backup_index, char *out_path) {
    if (!af_editor_recovery_state_valid(state) || out_path == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (source == AF_EDITOR_RECOVERY_SOURCE_RESTORE_STAGING && backup_index == 0U) {
        memcpy(out_path, state->paths.recovery, sizeof(state->paths.recovery));
        return AF_RESULT_SUCCESS;
    }
    if (source != AF_EDITOR_RECOVERY_SOURCE_AUTOSAVE ||
        backup_index > AF_EDITOR_RECOVERY_AUTOSAVE_BACKUPS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (backup_index == 0U) {
        memcpy(out_path, state->paths.autosave, sizeof(state->paths.autosave));
        return AF_RESULT_SUCCESS;
    }
    return af_editor_recovery_backup_path(state->paths.autosave, backup_index, out_path);
}

static uint64_t af_editor_recovery_hash(const void *data, size_t size) {
    const uint8_t *bytes = data;
    uint64_t hash = AF_EDITOR_RECOVERY_FNV_OFFSET;
    for (size_t index = 0U; index < size; ++index) {
        hash ^= (uint64_t)bytes[index];
        hash *= AF_EDITOR_RECOVERY_FNV_PRIME;
    }
    return hash;
}

static uint64_t af_editor_recovery_hash_extend(uint64_t hash, const void *data, size_t size) {
    const uint8_t *bytes = data;
    for (size_t index = 0U; index < size; ++index) {
        hash ^= (uint64_t)bytes[index];
        hash *= AF_EDITOR_RECOVERY_FNV_PRIME;
    }
    return hash;
}

static void af_editor_recovery_write_u32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)(value & UINT32_C(0xff));
    bytes[1] = (uint8_t)((value >> 8U) & UINT32_C(0xff));
    bytes[2] = (uint8_t)((value >> 16U) & UINT32_C(0xff));
    bytes[3] = (uint8_t)((value >> 24U) & UINT32_C(0xff));
}

static void af_editor_recovery_write_u64(uint8_t *bytes, uint64_t value) {
    for (uint32_t index = 0U; index < 8U; ++index) {
        bytes[index] = (uint8_t)((value >> (index * 8U)) & UINT64_C(0xff));
    }
}

static uint32_t af_editor_recovery_read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) | ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static uint64_t af_editor_recovery_read_u64(const uint8_t *bytes) {
    uint64_t value = 0U;
    for (uint32_t index = 0U; index < 8U; ++index) {
        value |= (uint64_t)bytes[index] << (index * 8U);
    }
    return value;
}

static AfResult af_editor_recovery_file_info(const char *path, bool fingerprint,
                                             AfEditorRecoveryFileInfo *out_info) {
    struct stat status;
    FILE *file = NULL;
    uint8_t buffer[65536];
    uint64_t checksum = AF_EDITOR_RECOVERY_FNV_OFFSET;
    uint64_t total_size = 0U;
    if (path == NULL || out_info == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_info = (AfEditorRecoveryFileInfo){0};
    errno = 0;
    if (stat(path, &status) != 0) {
        return errno == ENOENT ? AF_RESULT_SUCCESS : AF_RESULT_IO_ERROR;
    }
    if (status.st_size < 0) {
        return AF_RESULT_IO_ERROR;
    }
    out_info->exists = true;
    out_info->size = (uint64_t)status.st_size;
    if (status.st_mtime > 0) {
        out_info->modified_utc_seconds = (uint64_t)status.st_mtime;
    }
    if (!fingerprint) {
        return AF_RESULT_SUCCESS;
    }
    if (out_info->size > AF_EDITOR_RECOVERY_MAX_SNAPSHOT_SIZE) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        return AF_RESULT_IO_ERROR;
    }
    for (;;) {
        const size_t read_size = fread(buffer, 1U, sizeof(buffer), file);
        if (read_size != 0U) {
            checksum = af_editor_recovery_hash_extend(checksum, buffer, read_size);
            total_size += (uint64_t)read_size;
        }
        if (read_size != sizeof(buffer)) {
            break;
        }
    }
    if (ferror(file) != 0 || fclose(file) != 0 || total_size != out_info->size) {
        return AF_RESULT_IO_ERROR;
    }
    out_info->fingerprint_valid = true;
    out_info->checksum = checksum;
    return AF_RESULT_SUCCESS;
}

static AfResult af_editor_recovery_parse(const void *data, size_t size,
                                         AfEditorRecoveryRecord *out_record) {
    const uint8_t *bytes = data;
    AfEditorRecoveryRecord record = {0};
    uint64_t expected_size = 0U;
    if (data == NULL || out_record == NULL || size < AF_EDITOR_RECOVERY_HEADER_SIZE) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    if (memcmp(bytes, af_editor_recovery_magic, sizeof(af_editor_recovery_magic)) != 0) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    if (af_editor_recovery_read_u32(bytes + 8U) != AF_EDITOR_RECOVERY_FORMAT_VERSION) {
        return AF_RESULT_UNSUPPORTED_VERSION;
    }
    if (af_editor_recovery_read_u32(bytes + 12U) != AF_EDITOR_RECOVERY_HEADER_SIZE) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    record.project_format_version = af_editor_recovery_read_u32(bytes + 16U);
    record.flags = af_editor_recovery_read_u32(bytes + 20U);
    record.revision = af_editor_recovery_read_u64(bytes + 24U);
    record.created_utc_seconds = af_editor_recovery_read_u64(bytes + 32U);
    record.source_modified_utc_seconds = af_editor_recovery_read_u64(bytes + 40U);
    record.source_size = af_editor_recovery_read_u64(bytes + 48U);
    record.source_checksum = af_editor_recovery_read_u64(bytes + 56U);
    record.payload_size = af_editor_recovery_read_u64(bytes + 64U);
    record.payload_checksum = af_editor_recovery_read_u64(bytes + 72U);
    if (af_editor_recovery_read_u64(bytes + 80U) != af_editor_recovery_hash(bytes, 80U)) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    if (record.project_format_version == 0U ||
        (record.flags & ~AF_EDITOR_RECOVERY_SOURCE_FLAGS) != 0U ||
        ((record.flags & AF_EDITOR_RECOVERY_SOURCE_FINGERPRINT) != 0U &&
         (record.flags & AF_EDITOR_RECOVERY_SOURCE_EXISTS) == 0U) ||
        ((record.flags & AF_EDITOR_RECOVERY_SOURCE_EXISTS) == 0U &&
         (record.source_modified_utc_seconds != 0U || record.source_size != 0U ||
          record.source_checksum != 0U)) ||
        record.payload_size == 0U || record.payload_size > AF_EDITOR_RECOVERY_MAX_SNAPSHOT_SIZE) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    expected_size = (uint64_t)AF_EDITOR_RECOVERY_HEADER_SIZE + record.payload_size;
    if (expected_size != (uint64_t)size) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    record.payload = bytes + AF_EDITOR_RECOVERY_HEADER_SIZE;
    if (af_editor_recovery_hash(record.payload, (size_t)record.payload_size) !=
        record.payload_checksum) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    *out_record = record;
    return AF_RESULT_SUCCESS;
}

static bool af_editor_recovery_source_matches(const AfEditorRecoveryRecord *record,
                                              const AfEditorRecoveryFileInfo *project) {
    const bool source_exists = (record->flags & AF_EDITOR_RECOVERY_SOURCE_EXISTS) != 0U;
    if (source_exists != project->exists) {
        return false;
    }
    if (!source_exists) {
        return true;
    }
    if ((record->flags & AF_EDITOR_RECOVERY_SOURCE_FINGERPRINT) != 0U &&
        project->fingerprint_valid) {
        return record->source_size == project->size && record->source_checksum == project->checksum;
    }
    return record->source_modified_utc_seconds == project->modified_utc_seconds;
}

static bool af_editor_recovery_record_available(const AfEditorRecoverySelection *selection,
                                                const AfEditorRecoveryFileInfo *project) {
    if (selection->source_matches || !project->exists) {
        return true;
    }
    return selection->file.modified_utc_seconds > project->modified_utc_seconds ||
           selection->record.created_utc_seconds > project->modified_utc_seconds;
}

static bool af_editor_recovery_selection_newer(const AfEditorRecoverySelection *candidate,
                                               const AfEditorRecoverySelection *selected) {
    if (selected->source == AF_EDITOR_RECOVERY_SOURCE_NONE) {
        return true;
    }
    if (candidate->record.created_utc_seconds != selected->record.created_utc_seconds) {
        return candidate->record.created_utc_seconds > selected->record.created_utc_seconds;
    }
    if (candidate->file.modified_utc_seconds != selected->file.modified_utc_seconds) {
        return candidate->file.modified_utc_seconds > selected->file.modified_utc_seconds;
    }
    if (candidate->record.revision != selected->record.revision) {
        return candidate->record.revision > selected->record.revision;
    }
    if (candidate->source != selected->source) {
        return candidate->source == AF_EDITOR_RECOVERY_SOURCE_RESTORE_STAGING;
    }
    return candidate->backup_index < selected->backup_index;
}

static AfResult af_editor_recovery_remove_optional(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    errno = 0;
    if (remove(path) == 0 || errno == ENOENT) {
        return AF_RESULT_SUCCESS;
    }
    return AF_RESULT_IO_ERROR;
}

AfResult af_editor_recovery_query_settings(AfEditorRecoverySettings *out_settings) {
    if (out_settings == NULL ||
        !af_editor_recovery_output_valid(out_settings->struct_size, (uint32_t)sizeof(*out_settings),
                                         out_settings->api_version)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_settings =
        (AfEditorRecoverySettings){.struct_size = (uint32_t)sizeof(*out_settings),
                                   .api_version = AF_EDITOR_RECOVERY_API_VERSION,
                                   .autosave_interval_ms = AF_EDITOR_RECOVERY_AUTOSAVE_INTERVAL_MS,
                                   .idle_delay_ms = AF_EDITOR_RECOVERY_IDLE_DELAY_MS,
                                   .maximum_snapshot_size = AF_EDITOR_RECOVERY_MAX_SNAPSHOT_SIZE,
                                   .autosave_backup_count = AF_EDITOR_RECOVERY_AUTOSAVE_BACKUPS,
                                   .project_backup_count = AF_EDITOR_RECOVERY_PROJECT_BACKUPS};
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_recovery_derive_paths(const char *project_path,
                                         AfEditorRecoveryPaths *out_paths) {
    const size_t length =
        af_editor_recovery_string_length(project_path, AF_EDITOR_RECOVERY_PATH_CAPACITY);
    AfEditorRecoveryPaths paths = {0};
    AfResult result = AF_RESULT_SUCCESS;
    if (out_paths == NULL || length == 0U || length >= AF_EDITOR_RECOVERY_PATH_CAPACITY ||
        project_path[length - 1U] == '/' || project_path[length - 1U] == '\\') {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    memcpy(paths.project, project_path, length + 1U);
    result = af_editor_recovery_append_path(project_path, ".autosave", paths.autosave);
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_recovery_append_path(project_path, ".recovery", paths.recovery);
    }
    if (result == AF_RESULT_SUCCESS) {
        *out_paths = paths;
    }
    return result;
}

AfResult af_editor_recovery_init(AfEditorRecoveryState *state, const char *project_path,
                                 uint64_t now_ms) {
    AfEditorRecoveryPaths paths = {0};
    AfResult result = AF_RESULT_SUCCESS;
    if (state == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_editor_recovery_derive_paths(project_path, &paths);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    *state = (AfEditorRecoveryState){.struct_size = (uint32_t)sizeof(*state),
                                     .api_version = AF_EDITOR_RECOVERY_API_VERSION,
                                     .paths = paths,
                                     .current_revision = 1U,
                                     .saved_revision = 1U,
                                     .autosaved_revision = 1U,
                                     .last_change_ms = now_ms,
                                     .last_autosave_ms = now_ms,
                                     .dirty = false};
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_recovery_note_change(AfEditorRecoveryState *state, uint64_t now_ms) {
    if (!af_editor_recovery_state_valid(state)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    ++state->current_revision;
    if (state->current_revision == 0U) {
        state->current_revision = 1U;
        state->saved_revision = 0U;
        state->autosaved_revision = 0U;
    }
    state->last_change_ms = now_ms;
    state->dirty = state->current_revision != state->saved_revision;
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_recovery_mark_project_saved(AfEditorRecoveryState *state, uint64_t now_ms) {
    AfResult cleanup_result = AF_RESULT_SUCCESS;
    if (!af_editor_recovery_state_valid(state)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    state->saved_revision = state->current_revision;
    state->autosaved_revision = state->current_revision;
    state->last_change_ms = now_ms;
    state->last_autosave_ms = now_ms;
    state->dirty = false;
    cleanup_result = af_editor_recovery_discard(state);
    return cleanup_result;
}

bool af_editor_recovery_autosave_due(const AfEditorRecoveryState *state, uint64_t now_ms) {
    return af_editor_recovery_state_valid(state) && state->dirty &&
           state->current_revision != state->autosaved_revision &&
           now_ms >= state->last_autosave_ms && now_ms >= state->last_change_ms &&
           now_ms - state->last_autosave_ms >= AF_EDITOR_RECOVERY_AUTOSAVE_INTERVAL_MS &&
           now_ms - state->last_change_ms >= AF_EDITOR_RECOVERY_IDLE_DELAY_MS;
}

uint64_t af_editor_recovery_next_due_ms(const AfEditorRecoveryState *state) {
    uint64_t interval_due = UINT64_MAX;
    uint64_t idle_due = UINT64_MAX;
    if (!af_editor_recovery_state_valid(state) || !state->dirty ||
        state->current_revision == state->autosaved_revision) {
        return UINT64_MAX;
    }
    if (state->last_autosave_ms <= UINT64_MAX - AF_EDITOR_RECOVERY_AUTOSAVE_INTERVAL_MS) {
        interval_due = state->last_autosave_ms + AF_EDITOR_RECOVERY_AUTOSAVE_INTERVAL_MS;
    }
    if (state->last_change_ms <= UINT64_MAX - AF_EDITOR_RECOVERY_IDLE_DELAY_MS) {
        idle_due = state->last_change_ms + AF_EDITOR_RECOVERY_IDLE_DELAY_MS;
    }
    return interval_due > idle_due ? interval_due : idle_due;
}

AfResult af_editor_recovery_write_autosave(AfEditorRecoveryState *state,
                                           uint32_t project_format_version,
                                           const void *serialized_project, size_t size,
                                           uint64_t now_ms, AfAllocator *allocator,
                                           bool *out_written) {
    AfEditorRecoveryFileInfo source = {0};
    uint8_t *envelope = NULL;
    size_t envelope_size = 0U;
    uint32_t flags = 0U;
    time_t wall_time = 0;
    AfResult result = AF_RESULT_SUCCESS;
    if (out_written == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_written = false;
    if (!af_editor_recovery_state_valid(state) || project_format_version == 0U ||
        serialized_project == NULL || size == 0U || size > AF_EDITOR_RECOVERY_MAX_SNAPSHOT_SIZE ||
        allocator == NULL || allocator->allocate == NULL || allocator->deallocate == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (!state->dirty || state->current_revision == state->autosaved_revision) {
        return AF_RESULT_SUCCESS;
    }
    result = af_editor_recovery_file_info(state->paths.project, true, &source);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    if (size > SIZE_MAX - AF_EDITOR_RECOVERY_HEADER_SIZE) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    envelope_size = size + AF_EDITOR_RECOVERY_HEADER_SIZE;
    envelope = af_allocate(allocator, envelope_size, alignof(max_align_t));
    if (envelope == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memset(envelope, 0, AF_EDITOR_RECOVERY_HEADER_SIZE);
    memcpy(envelope, af_editor_recovery_magic, sizeof(af_editor_recovery_magic));
    if (source.exists) {
        flags |= AF_EDITOR_RECOVERY_SOURCE_EXISTS;
    }
    if (source.fingerprint_valid) {
        flags |= AF_EDITOR_RECOVERY_SOURCE_FINGERPRINT;
    }
    wall_time = time(NULL);
    af_editor_recovery_write_u32(envelope + 8U, AF_EDITOR_RECOVERY_FORMAT_VERSION);
    af_editor_recovery_write_u32(envelope + 12U, AF_EDITOR_RECOVERY_HEADER_SIZE);
    af_editor_recovery_write_u32(envelope + 16U, project_format_version);
    af_editor_recovery_write_u32(envelope + 20U, flags);
    af_editor_recovery_write_u64(envelope + 24U, state->current_revision);
    af_editor_recovery_write_u64(envelope + 32U, wall_time > 0 ? (uint64_t)wall_time : UINT64_C(0));
    af_editor_recovery_write_u64(envelope + 40U, source.modified_utc_seconds);
    af_editor_recovery_write_u64(envelope + 48U, source.size);
    af_editor_recovery_write_u64(envelope + 56U, source.checksum);
    af_editor_recovery_write_u64(envelope + 64U, (uint64_t)size);
    af_editor_recovery_write_u64(envelope + 72U, af_editor_recovery_hash(serialized_project, size));
    af_editor_recovery_write_u64(envelope + 80U, af_editor_recovery_hash(envelope, 80U));
    memcpy(envelope + AF_EDITOR_RECOVERY_HEADER_SIZE, serialized_project, size);
    result = af_file_write_atomic(state->paths.autosave, envelope, envelope_size,
                                  AF_EDITOR_RECOVERY_AUTOSAVE_BACKUPS);
    af_deallocate(allocator, envelope);
    if (result == AF_RESULT_SUCCESS) {
        state->autosaved_revision = state->current_revision;
        state->last_autosave_ms = now_ms;
        *out_written = true;
    }
    return result;
}

AfResult af_editor_recovery_find_candidate(const AfEditorRecoveryState *state,
                                           uint32_t maximum_project_format_version,
                                           AfAllocator *allocator,
                                           AfEditorRecoveryCandidate *out_candidate) {
    AfEditorRecoveryFileInfo project = {0};
    AfEditorRecoverySelection selected = {0};
    AfEditorRecoveryIssueFlags issues = 0U;
    bool found_any = false;
    AfResult result = AF_RESULT_SUCCESS;
    if (!af_editor_recovery_state_valid(state) || maximum_project_format_version == 0U ||
        allocator == NULL || out_candidate == NULL ||
        !af_editor_recovery_output_valid(out_candidate->struct_size,
                                         (uint32_t)sizeof(*out_candidate),
                                         out_candidate->api_version)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_candidate = (AfEditorRecoveryCandidate){.struct_size = (uint32_t)sizeof(*out_candidate),
                                                 .api_version = AF_EDITOR_RECOVERY_API_VERSION};
    result = af_editor_recovery_file_info(state->paths.project, true, &project);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    for (uint32_t slot = 0U; slot < AF_EDITOR_RECOVERY_AUTOSAVE_BACKUPS + 2U; ++slot) {
        AfEditorRecoverySelection item = {0};
        AfFileData file = {0};
        bool available = false;
        if (slot == 0U) {
            item.source = AF_EDITOR_RECOVERY_SOURCE_RESTORE_STAGING;
            item.backup_index = 0U;
        } else {
            item.source = AF_EDITOR_RECOVERY_SOURCE_AUTOSAVE;
            item.backup_index = slot - 1U;
        }
        result =
            af_editor_recovery_candidate_path(state, item.source, item.backup_index, item.path);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
        result = af_editor_recovery_file_info(item.path, false, &item.file);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
        if (!item.file.exists) {
            continue;
        }
        found_any = true;
        result = af_file_read_limited(
            item.path, AF_EDITOR_RECOVERY_MAX_SNAPSHOT_SIZE + AF_EDITOR_RECOVERY_HEADER_SIZE,
            allocator, &file);
        if (result == AF_RESULT_SUCCESS) {
            result = af_editor_recovery_parse(file.data, file.size, &item.record);
        }
        if (result == AF_RESULT_UNSUPPORTED_VERSION) {
            issues |= AF_EDITOR_RECOVERY_ISSUE_INCOMPATIBLE;
        } else if (result != AF_RESULT_SUCCESS) {
            issues |= AF_EDITOR_RECOVERY_ISSUE_CORRUPT;
        } else if (item.record.project_format_version > maximum_project_format_version) {
            issues |= AF_EDITOR_RECOVERY_ISSUE_INCOMPATIBLE;
        } else {
            item.source_matches = af_editor_recovery_source_matches(&item.record, &project);
            available = af_editor_recovery_record_available(&item, &project);
            if (!available) {
                issues |= AF_EDITOR_RECOVERY_ISSUE_STALE;
            } else if (af_editor_recovery_selection_newer(&item, &selected)) {
                selected = item;
                selected.record.payload = NULL;
            }
        }
        af_file_data_destroy(allocator, &file);
    }
    out_candidate->issues = issues;
    out_candidate->project_exists = project.exists;
    if (selected.source != AF_EDITOR_RECOVERY_SOURCE_NONE) {
        out_candidate->status = AF_EDITOR_RECOVERY_STATUS_AVAILABLE;
        out_candidate->source = selected.source;
        out_candidate->backup_index = selected.backup_index;
        out_candidate->project_format_version = selected.record.project_format_version;
        out_candidate->revision = selected.record.revision;
        out_candidate->created_utc_seconds = selected.record.created_utc_seconds;
        out_candidate->payload_size = selected.record.payload_size;
        out_candidate->payload_checksum = selected.record.payload_checksum;
        out_candidate->source_project_matches = selected.source_matches;
        memcpy(out_candidate->path, selected.path, sizeof(out_candidate->path));
    } else if ((issues & AF_EDITOR_RECOVERY_ISSUE_CORRUPT) != 0U) {
        out_candidate->status = AF_EDITOR_RECOVERY_STATUS_CORRUPT;
    } else if ((issues & AF_EDITOR_RECOVERY_ISSUE_INCOMPATIBLE) != 0U) {
        out_candidate->status = AF_EDITOR_RECOVERY_STATUS_INCOMPATIBLE;
    } else if ((issues & AF_EDITOR_RECOVERY_ISSUE_STALE) != 0U) {
        out_candidate->status = AF_EDITOR_RECOVERY_STATUS_STALE;
    } else if (found_any) {
        out_candidate->status = AF_EDITOR_RECOVERY_STATUS_CORRUPT;
        out_candidate->issues |= AF_EDITOR_RECOVERY_ISSUE_CORRUPT;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_recovery_restore(AfEditorRecoveryState *state,
                                    uint32_t maximum_project_format_version,
                                    const AfEditorRecoveryCandidate *candidate,
                                    AfAllocator *allocator) {
    AfEditorRecoveryCandidate current = {.struct_size = sizeof(current),
                                         .api_version = AF_EDITOR_RECOVERY_API_VERSION};
    AfEditorRecoveryRecord record = {0};
    AfFileData file = {0};
    char path[AF_EDITOR_RECOVERY_PATH_CAPACITY];
    AfResult result = AF_RESULT_SUCCESS;
    if (!af_editor_recovery_state_valid(state) || maximum_project_format_version == 0U ||
        candidate == NULL || allocator == NULL ||
        !af_editor_recovery_output_valid(candidate->struct_size, (uint32_t)sizeof(*candidate),
                                         candidate->api_version) ||
        candidate->status != AF_EDITOR_RECOVERY_STATUS_AVAILABLE) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_editor_recovery_find_candidate(state, maximum_project_format_version, allocator,
                                               &current);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    if (current.status != AF_EDITOR_RECOVERY_STATUS_AVAILABLE ||
        current.source != candidate->source || current.backup_index != candidate->backup_index ||
        current.revision != candidate->revision ||
        current.payload_checksum != candidate->payload_checksum) {
        return AF_RESULT_STALE_HANDLE;
    }
    result = af_editor_recovery_candidate_path(state, current.source, current.backup_index, path);
    if (result == AF_RESULT_SUCCESS) {
        result = af_file_read_limited(
            path, AF_EDITOR_RECOVERY_MAX_SNAPSHOT_SIZE + AF_EDITOR_RECOVERY_HEADER_SIZE, allocator,
            &file);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_editor_recovery_parse(file.data, file.size, &record);
    }
    if (result == AF_RESULT_SUCCESS &&
        record.project_format_version > maximum_project_format_version) {
        result = AF_RESULT_UNSUPPORTED_VERSION;
    }
    if (result == AF_RESULT_SUCCESS &&
        current.source != AF_EDITOR_RECOVERY_SOURCE_RESTORE_STAGING) {
        result = af_file_write_atomic(state->paths.recovery, file.data, file.size, 0U);
    }
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_file_write_atomic(state->paths.project, record.payload, (size_t)record.payload_size,
                                 AF_EDITOR_RECOVERY_PROJECT_BACKUPS);
    }
    af_file_data_destroy(allocator, &file);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    state->current_revision = record.revision;
    state->saved_revision = record.revision;
    state->autosaved_revision = record.revision;
    state->dirty = false;
    return af_editor_recovery_discard(state);
}

AfResult af_editor_recovery_discard(const AfEditorRecoveryState *state) {
    char backup_path[AF_EDITOR_RECOVERY_PATH_CAPACITY];
    AfResult result = AF_RESULT_SUCCESS;
    AfResult remove_result = AF_RESULT_SUCCESS;
    if (!af_editor_recovery_state_valid(state)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    remove_result = af_editor_recovery_remove_optional(state->paths.recovery);
    if (remove_result != AF_RESULT_SUCCESS) {
        result = remove_result;
    }
    remove_result = af_editor_recovery_remove_optional(state->paths.autosave);
    if (remove_result != AF_RESULT_SUCCESS && result == AF_RESULT_SUCCESS) {
        result = remove_result;
    }
    for (uint32_t index = 1U; index <= AF_EDITOR_RECOVERY_AUTOSAVE_BACKUPS; ++index) {
        remove_result = af_editor_recovery_backup_path(state->paths.autosave, index, backup_path);
        if (remove_result == AF_RESULT_SUCCESS) {
            remove_result = af_editor_recovery_remove_optional(backup_path);
        }
        if (remove_result != AF_RESULT_SUCCESS && result == AF_RESULT_SUCCESS) {
            result = remove_result;
        }
    }
    return result;
}
