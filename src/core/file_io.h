#ifndef AF_FILE_IO_H
#define AF_FILE_IO_H

#include "arcflow/arcflow_result.h"
#include "core/allocator.h"

#include <stddef.h>
#include <stdint.h>

typedef struct AfFileData {
    uint8_t *data;
    size_t size;
} AfFileData;

AfResult af_file_read_limited(const char *path, size_t maximum_size, AfAllocator *allocator,
                              AfFileData *out_file);
AfResult af_file_write(const char *path, const void *data, size_t size);
void af_file_data_destroy(AfAllocator *allocator, AfFileData *file);

#endif
