#ifndef AF_HANDLES_H
#define AF_HANDLES_H

#include "arcflow/arcflow_result.h"

#include <stdint.h>

typedef struct AfHandleSlot {
    uint32_t generation;
    uint8_t occupied;
} AfHandleSlot;

typedef struct AfHandlePool {
    AfHandleSlot *slots;
    uint32_t capacity;
} AfHandlePool;

void af_handle_pool_init(AfHandlePool *pool, AfHandleSlot *slots, uint32_t capacity);
AfResult af_handle_allocate(AfHandlePool *pool, uint64_t *out_handle);
AfResult af_handle_release(AfHandlePool *pool, uint64_t handle);
AfResult af_handle_validate(const AfHandlePool *pool, uint64_t handle, uint32_t *out_index);

#endif
