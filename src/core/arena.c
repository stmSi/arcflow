#include "core/arena.h"

#include "core/safe_math.h"

#include <stdint.h>

void af_arena_init(AfArena *arena, void *memory, size_t capacity) {
    if (arena != NULL) {
        *arena = (AfArena){.memory = memory, .capacity = capacity, .offset = 0U};
    }
}

AfResult af_arena_allocate(AfArena *arena, size_t size, size_t alignment, void **out_memory) {
    uintptr_t base = 0U;
    uintptr_t current = 0U;
    uintptr_t aligned = 0U;
    size_t padding = 0U;
    size_t end = 0U;
    if (arena == NULL || out_memory == NULL || arena->memory == NULL || size == 0U ||
        !af_is_power_of_two(alignment)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_memory = NULL;
    base = (uintptr_t)arena->memory;
    current = base + arena->offset;
    aligned = (current + alignment - 1U) & ~(uintptr_t)(alignment - 1U);
    padding = (size_t)(aligned - current);
    if (af_size_add_overflow(arena->offset, padding, &end) ||
        af_size_add_overflow(end, size, &end) || end > arena->capacity) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    arena->offset = end;
    *out_memory = (void *)aligned;
    return AF_RESULT_SUCCESS;
}

void af_arena_reset(AfArena *arena) {
    if (arena != NULL) {
        arena->offset = 0U;
    }
}

size_t af_arena_remaining(const AfArena *arena) {
    return arena != NULL && arena->offset <= arena->capacity ? arena->capacity - arena->offset : 0U;
}

AfScratch af_scratch_begin(AfArena *arena) {
    return (AfScratch){.arena = arena, .saved_offset = arena != NULL ? arena->offset : 0U};
}

void af_scratch_end(AfScratch scratch) {
    if (scratch.arena != NULL && scratch.saved_offset <= scratch.arena->capacity) {
        scratch.arena->offset = scratch.saved_offset;
    }
}
