#include "core/file_io.h"

#include <stdio.h>

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

void af_file_data_destroy(AfAllocator *allocator, AfFileData *file) {
    if (file != NULL) {
        af_deallocate(allocator, file->data);
        *file = (AfFileData){0};
    }
}
