#include "core/handles.h"

#include <stddef.h>

void af_handle_pool_init(AfHandlePool *pool, AfHandleSlot *slots, uint32_t capacity) {
    uint32_t index = 0U;
    if (pool == NULL) {
        return;
    }
    pool->slots = slots;
    pool->capacity = capacity;
    for (index = 0U; index < capacity; ++index) {
        slots[index] = (AfHandleSlot){.generation = 1U};
    }
}

AfResult af_handle_allocate(AfHandlePool *pool, uint64_t *out_handle) {
    uint32_t index = 0U;
    if (pool == NULL || pool->slots == NULL || out_handle == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < pool->capacity; ++index) {
        if (pool->slots[index].occupied == 0U) {
            pool->slots[index].occupied = 1U;
            *out_handle = ((uint64_t)pool->slots[index].generation << 32U) | (uint64_t)(index + 1U);
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_LIMIT_EXCEEDED;
}

AfResult af_handle_validate(const AfHandlePool *pool, uint64_t handle, uint32_t *out_index) {
    const uint32_t encoded_index = (uint32_t)handle;
    const uint32_t generation = (uint32_t)(handle >> 32U);
    uint32_t index = 0U;
    if (pool == NULL || pool->slots == NULL || encoded_index == 0U) {
        return AF_RESULT_STALE_HANDLE;
    }
    index = encoded_index - 1U;
    if (index >= pool->capacity || pool->slots[index].occupied == 0U ||
        pool->slots[index].generation != generation) {
        return AF_RESULT_STALE_HANDLE;
    }
    if (out_index != NULL) {
        *out_index = index;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_handle_release(AfHandlePool *pool, uint64_t handle) {
    uint32_t index = 0U;
    AfResult result = af_handle_validate(pool, handle, &index);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    pool->slots[index].occupied = 0U;
    pool->slots[index].generation++;
    if (pool->slots[index].generation == 0U) {
        pool->slots[index].generation = 1U;
    }
    return AF_RESULT_SUCCESS;
}
