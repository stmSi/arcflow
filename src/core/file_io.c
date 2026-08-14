#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "core/file_io.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#define AF_FILE_TEMP_ATTEMPTS 64U
#define AF_FILE_COPY_BUFFER_SIZE 65536U

static atomic_uint af_file_temp_sequence = 1U;

static unsigned long af_file_process_id(void) {
#if defined(_WIN32)
    return (unsigned long)GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}

static AfResult af_file_backup_path(const char *path, uint32_t index, char **out_path) {
    int length = 0;
    char *backup_path = NULL;
    if (path == NULL || index == 0U || out_path == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_path = NULL;
    length = snprintf(NULL, 0, "%s.bak%u", path, index);
    if (length < 0 || (size_t)length == SIZE_MAX) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    backup_path = malloc((size_t)length + 1U);
    if (backup_path == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    if (snprintf(backup_path, (size_t)length + 1U, "%s.bak%u", path, index) != length) {
        free(backup_path);
        return AF_RESULT_IO_ERROR;
    }
    *out_path = backup_path;
    return AF_RESULT_SUCCESS;
}

static AfResult af_file_open_temporary(const char *target_path, char **out_path, FILE **out_file) {
    const unsigned long process_id = af_file_process_id();
    if (target_path == NULL || target_path[0] == '\0' || out_path == NULL || out_file == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_path = NULL;
    *out_file = NULL;
    for (uint32_t attempt = 0U; attempt < AF_FILE_TEMP_ATTEMPTS; ++attempt) {
        const unsigned int sequence =
            atomic_fetch_add_explicit(&af_file_temp_sequence, 1U, memory_order_relaxed);
        int length = snprintf(NULL, 0, "%s.tmp.%lu.%u", target_path, process_id, sequence);
        char *temporary_path = NULL;
        FILE *temporary_file = NULL;
        int open_error = 0;
        if (length < 0 || (size_t)length == SIZE_MAX) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        temporary_path = malloc((size_t)length + 1U);
        if (temporary_path == NULL) {
            return AF_RESULT_OUT_OF_MEMORY;
        }
        if (snprintf(temporary_path, (size_t)length + 1U, "%s.tmp.%lu.%u", target_path, process_id,
                     sequence) != length) {
            free(temporary_path);
            return AF_RESULT_IO_ERROR;
        }
        errno = 0;
        temporary_file = fopen(temporary_path, "wbx");
        open_error = errno;
        if (temporary_file != NULL) {
            *out_path = temporary_path;
            *out_file = temporary_file;
            return AF_RESULT_SUCCESS;
        }
        free(temporary_path);
        if (open_error != EEXIST) {
            return AF_RESULT_IO_ERROR;
        }
    }
    return AF_RESULT_IO_ERROR;
}

#if !defined(_WIN32)
static bool af_file_sync_unavailable(int error_code) {
    if (error_code == EINVAL) {
        return true;
    }
#if defined(ENOTSUP)
    if (error_code == ENOTSUP) {
        return true;
    }
#endif
#if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || EOPNOTSUPP != ENOTSUP)
    if (error_code == EOPNOTSUPP) {
        return true;
    }
#endif
    return false;
}
#endif

static AfResult af_file_flush_and_sync(FILE *file) {
    if (file == NULL || fflush(file) != 0) {
        return AF_RESULT_IO_ERROR;
    }
#if defined(_WIN32)
    if (_commit(_fileno(file)) != 0) {
        return AF_RESULT_IO_ERROR;
    }
#else
    {
        const int descriptor = fileno(file);
        int sync_result = 0;
        if (descriptor < 0) {
            return AF_RESULT_IO_ERROR;
        }
        do {
            sync_result = fsync(descriptor);
        } while (sync_result != 0 && errno == EINTR);
        if (sync_result != 0 && !af_file_sync_unavailable(errno)) {
            return AF_RESULT_IO_ERROR;
        }
    }
#endif
    return AF_RESULT_SUCCESS;
}

static AfResult af_file_close_synced(FILE **file) {
    AfResult result = AF_RESULT_SUCCESS;
    FILE *stream = NULL;
    if (file == NULL || *file == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    stream = *file;
    *file = NULL;
    result = af_file_flush_and_sync(stream);
    if (fclose(stream) != 0 && result == AF_RESULT_SUCCESS) {
        result = AF_RESULT_IO_ERROR;
    }
    return result;
}

static AfResult af_file_remove_optional(const char *path) {
    if (path == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
#if defined(_WIN32)
    if (DeleteFileA(path) != 0) {
        return AF_RESULT_SUCCESS;
    }
    {
        const DWORD error_code = GetLastError();
        return error_code == ERROR_FILE_NOT_FOUND || error_code == ERROR_PATH_NOT_FOUND
                   ? AF_RESULT_SUCCESS
                   : AF_RESULT_IO_ERROR;
    }
#else
    if (unlink(path) == 0 || errno == ENOENT) {
        return AF_RESULT_SUCCESS;
    }
    return AF_RESULT_IO_ERROR;
#endif
}

static AfResult af_file_replace(const char *source_path, const char *destination_path) {
    if (source_path == NULL || destination_path == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
#if defined(_WIN32)
    return MoveFileExA(source_path, destination_path,
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0
               ? AF_RESULT_SUCCESS
               : AF_RESULT_IO_ERROR;
#else
    return rename(source_path, destination_path) == 0 ? AF_RESULT_SUCCESS : AF_RESULT_IO_ERROR;
#endif
}

static AfResult af_file_move_optional(const char *source_path, const char *destination_path) {
    if (source_path == NULL || destination_path == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
#if defined(_WIN32)
    if (MoveFileExA(source_path, destination_path,
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0) {
        return AF_RESULT_SUCCESS;
    }
    {
        const DWORD error_code = GetLastError();
        return error_code == ERROR_FILE_NOT_FOUND || error_code == ERROR_PATH_NOT_FOUND
                   ? AF_RESULT_SUCCESS
                   : AF_RESULT_IO_ERROR;
    }
#else
    if (rename(source_path, destination_path) == 0 || errno == ENOENT) {
        return AF_RESULT_SUCCESS;
    }
    return AF_RESULT_IO_ERROR;
#endif
}

static AfResult af_file_sync_parent_directory(const char *path) {
#if defined(_WIN32)
    (void)path;
    return AF_RESULT_SUCCESS;
#else
    const char *separator = NULL;
    const char *directory = ".";
    char *owned_directory = NULL;
    int descriptor = -1;
    int sync_result = 0;
    AfResult result = AF_RESULT_SUCCESS;
    if (path == NULL || path[0] == '\0') {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    separator = strrchr(path, '/');
    if (separator != NULL) {
        const size_t length = separator == path ? 1U : (size_t)(separator - path);
        owned_directory = malloc(length + 1U);
        if (owned_directory == NULL) {
            return AF_RESULT_OUT_OF_MEMORY;
        }
        memcpy(owned_directory, path, length);
        owned_directory[length] = '\0';
        directory = owned_directory;
    }
    descriptor = open(directory,
#if defined(O_DIRECTORY)
                      O_RDONLY | O_DIRECTORY
#else
                      O_RDONLY
#endif
    );
    free(owned_directory);
    if (descriptor < 0) {
        return AF_RESULT_IO_ERROR;
    }
    do {
        sync_result = fsync(descriptor);
    } while (sync_result != 0 && errno == EINTR);
    if (sync_result != 0 && !af_file_sync_unavailable(errno)) {
        result = AF_RESULT_IO_ERROR;
    }
    if (close(descriptor) != 0 && result == AF_RESULT_SUCCESS) {
        result = AF_RESULT_IO_ERROR;
    }
    return result;
#endif
}

static AfResult af_file_write_temporary(const char *target_path, const void *data, size_t size,
                                        char **out_temporary_path) {
    char *temporary_path = NULL;
    FILE *temporary_file = NULL;
    AfResult result = AF_RESULT_SUCCESS;
    if (out_temporary_path == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_temporary_path = NULL;
    result = af_file_open_temporary(target_path, &temporary_path, &temporary_file);
    if (result == AF_RESULT_SUCCESS && size != 0U &&
        fwrite(data, 1U, size, temporary_file) != size) {
        result = AF_RESULT_IO_ERROR;
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_file_close_synced(&temporary_file);
    }
    if (temporary_file != NULL) {
        (void)fclose(temporary_file);
    }
    if (result != AF_RESULT_SUCCESS) {
        (void)af_file_remove_optional(temporary_path);
        free(temporary_path);
        return result;
    }
    *out_temporary_path = temporary_path;
    return AF_RESULT_SUCCESS;
}

static AfResult af_file_copy_to_temporary(const char *source_path, const char *target_path,
                                          char **out_temporary_path, bool *out_source_exists) {
    uint8_t buffer[AF_FILE_COPY_BUFFER_SIZE];
    FILE *source_file = NULL;
    FILE *temporary_file = NULL;
    char *temporary_path = NULL;
    AfResult result = AF_RESULT_SUCCESS;
    if (source_path == NULL || target_path == NULL || out_temporary_path == NULL ||
        out_source_exists == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_temporary_path = NULL;
    *out_source_exists = false;
    errno = 0;
    source_file = fopen(source_path, "rb");
    if (source_file == NULL) {
        return errno == ENOENT ? AF_RESULT_SUCCESS : AF_RESULT_IO_ERROR;
    }
    *out_source_exists = true;
    result = af_file_open_temporary(target_path, &temporary_path, &temporary_file);
    while (result == AF_RESULT_SUCCESS) {
        const size_t read_size = fread(buffer, 1U, sizeof(buffer), source_file);
        if (read_size != 0U && fwrite(buffer, 1U, read_size, temporary_file) != read_size) {
            result = AF_RESULT_IO_ERROR;
            break;
        }
        if (read_size != sizeof(buffer)) {
            if (ferror(source_file) != 0) {
                result = AF_RESULT_IO_ERROR;
            }
            break;
        }
    }
    if (fclose(source_file) != 0 && result == AF_RESULT_SUCCESS) {
        result = AF_RESULT_IO_ERROR;
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_file_close_synced(&temporary_file);
    }
    if (temporary_file != NULL) {
        (void)fclose(temporary_file);
    }
    if (result != AF_RESULT_SUCCESS) {
        (void)af_file_remove_optional(temporary_path);
        free(temporary_path);
        return result;
    }
    *out_temporary_path = temporary_path;
    return AF_RESULT_SUCCESS;
}

static AfResult af_file_rotate_backups(const char *path, uint32_t backup_count,
                                       const char *first_backup_temporary_path) {
    AfResult result = AF_RESULT_SUCCESS;
    for (uint32_t index = backup_count; result == AF_RESULT_SUCCESS && index > 1U; --index) {
        char *source_path = NULL;
        char *destination_path = NULL;
        result = af_file_backup_path(path, index - 1U, &source_path);
        if (result == AF_RESULT_SUCCESS) {
            result = af_file_backup_path(path, index, &destination_path);
        }
        if (result == AF_RESULT_SUCCESS) {
            result = af_file_remove_optional(destination_path);
        }
        if (result == AF_RESULT_SUCCESS) {
            result = af_file_move_optional(source_path, destination_path);
        }
        free(destination_path);
        free(source_path);
    }
    if (result == AF_RESULT_SUCCESS) {
        char *first_backup_path = NULL;
        result = af_file_backup_path(path, 1U, &first_backup_path);
        if (result == AF_RESULT_SUCCESS) {
            result = af_file_replace(first_backup_temporary_path, first_backup_path);
        }
        free(first_backup_path);
    }
    return result;
}

AfResult af_file_read_limited(const char *path, size_t maximum_size, AfAllocator *allocator,
                              AfFileData *out_file) {
    FILE *file = NULL;
    long length = 0L;
    uint8_t *data = NULL;
    if (path == NULL || allocator == NULL || out_file == NULL || maximum_size == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_file = (AfFileData){0};
    file = fopen(path, "rb");
    if (file == NULL) {
        return AF_RESULT_IO_ERROR;
    }
    if (fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) < 0L ||
        fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return AF_RESULT_IO_ERROR;
    }
    if ((uintmax_t)length > (uintmax_t)maximum_size) {
        (void)fclose(file);
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    if (length == 0L) {
        (void)fclose(file);
        return AF_RESULT_SUCCESS;
    }
    data = af_allocate(allocator, (size_t)length, _Alignof(max_align_t));
    if (data == NULL) {
        (void)fclose(file);
        return AF_RESULT_OUT_OF_MEMORY;
    }
    if (fread(data, 1U, (size_t)length, file) != (size_t)length) {
        (void)fclose(file);
        af_deallocate(allocator, data);
        return AF_RESULT_IO_ERROR;
    }
    if (fclose(file) != 0) {
        af_deallocate(allocator, data);
        return AF_RESULT_IO_ERROR;
    }
    out_file->data = data;
    out_file->size = (size_t)length;
    return AF_RESULT_SUCCESS;
}

AfResult af_file_write(const char *path, const void *data, size_t size) {
    FILE *file = NULL;
    if (path == NULL || (data == NULL && size != 0U)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    file = fopen(path, "wb");
    if (file == NULL) {
        return AF_RESULT_IO_ERROR;
    }
    if (size != 0U && fwrite(data, 1U, size, file) != size) {
        (void)fclose(file);
        return AF_RESULT_IO_ERROR;
    }
    if (fclose(file) != 0) {
        return AF_RESULT_IO_ERROR;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_file_write_atomic(const char *path, const void *data, size_t size,
                              uint32_t backup_count) {
    char *temporary_path = NULL;
    char *first_backup_path = NULL;
    char *backup_temporary_path = NULL;
    bool source_exists = false;
    AfResult result = AF_RESULT_SUCCESS;
    if (path == NULL || path[0] == '\0' || (data == NULL && size != 0U) ||
        backup_count > AF_FILE_MAX_BACKUPS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_file_write_temporary(path, data, size, &temporary_path);
    if (result == AF_RESULT_SUCCESS && backup_count != 0U) {
        result = af_file_backup_path(path, 1U, &first_backup_path);
    }
    if (result == AF_RESULT_SUCCESS && backup_count != 0U) {
        result = af_file_copy_to_temporary(path, first_backup_path, &backup_temporary_path,
                                           &source_exists);
    }
    if (result == AF_RESULT_SUCCESS && source_exists) {
        result = af_file_rotate_backups(path, backup_count, backup_temporary_path);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_file_replace(temporary_path, path);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_file_sync_parent_directory(path);
    }
    if (backup_temporary_path != NULL) {
        (void)af_file_remove_optional(backup_temporary_path);
    }
    if (temporary_path != NULL) {
        (void)af_file_remove_optional(temporary_path);
    }
    free(backup_temporary_path);
    free(first_backup_path);
    free(temporary_path);
    return result;
}

void af_file_data_destroy(AfAllocator *allocator, AfFileData *file) {
    if (file != NULL) {
        af_deallocate(allocator, file->data);
        *file = (AfFileData){0};
    }
}
