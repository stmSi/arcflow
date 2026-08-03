#ifndef AF_ARRAY_H
#define AF_ARRAY_H

#include "arcflow/arcflow_result.h"
#include "core/allocator.h"

#include <stddef.h>

typedef struct AfArray {
    void *data;
    size_t count;
    size_t capacity;
    size_t element_size;
    size_t alignment;
    AfAllocator *allocator;
} AfArray;

void af_array_init(AfArray *array, size_t element_size, size_t alignment, AfAllocator *allocator);
void af_array_destroy(AfArray *array);
AfResult af_array_reserve(AfArray *array, size_t capacity);
AfResult af_array_push(AfArray *array, const void *element);
void *af_array_at(AfArray *array, size_t index);

#endif
