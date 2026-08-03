#ifndef AF_ALLOCATOR_H
#define AF_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

typedef void *(*AfAllocateFn)(void *user, size_t size, size_t alignment);
typedef void (*AfDeallocateFn)(void *user, void *memory);

typedef struct AfAllocator {
    AfAllocateFn allocate;
    AfDeallocateFn deallocate;
    void *user;
    uint64_t total_allocations;
    uint64_t frame_allocations;
    int frame_active;
} AfAllocator;

void af_allocator_init_default(AfAllocator *allocator);
void *af_allocate(AfAllocator *allocator, size_t size, size_t alignment);
void *af_allocate_array(AfAllocator *allocator, size_t count, size_t element_size,
                        size_t alignment);
void af_deallocate(AfAllocator *allocator, void *memory);
void af_allocator_begin_frame(AfAllocator *allocator);
uint64_t af_allocator_end_frame(AfAllocator *allocator);

#endif
