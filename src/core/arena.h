#ifndef AF_ARENA_H
#define AF_ARENA_H

#include "arcflow/arcflow_result.h"

#include <stddef.h>
#include <stdint.h>

typedef struct AfArena {
    uint8_t *memory;
    size_t capacity;
    size_t offset;
} AfArena;

typedef struct AfScratch {
    AfArena *arena;
    size_t saved_offset;
} AfScratch;

void af_arena_init(AfArena *arena, void *memory, size_t capacity);
AfResult af_arena_allocate(AfArena *arena, size_t size, size_t alignment, void **out_memory);
void af_arena_reset(AfArena *arena);
size_t af_arena_remaining(const AfArena *arena);
AfScratch af_scratch_begin(AfArena *arena);
void af_scratch_end(AfScratch scratch);

#endif
