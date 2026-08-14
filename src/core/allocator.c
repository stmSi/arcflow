#include "core/allocator.h"

#include "core/safe_math.h"

#include <stdlib.h>
#if defined(_WIN32)
#include <malloc.h>
#endif

static void *af_default_allocate(void *user, size_t size, size_t alignment) {
    size_t rounded_size = 0U;
    (void)user;
    if (!af_is_power_of_two(alignment) || size == 0U) {
        return NULL;
    }
    if (alignment < sizeof(void *)) {
        alignment = sizeof(void *);
    }
    if (af_size_add_overflow(size, alignment - 1U, &rounded_size)) {
        return NULL;
    }
    rounded_size &= ~(alignment - 1U);
#if defined(_WIN32)
    return _aligned_malloc(rounded_size, alignment);
#else
    return aligned_alloc(alignment, rounded_size);
#endif
}

static void af_default_deallocate(void *user, void *memory) {
    (void)user;
#if defined(_WIN32)
    _aligned_free(memory);
#else
    free(memory);
#endif
}

void af_allocator_init_default(AfAllocator *allocator) {
    if (allocator == NULL) {
        return;
    }
    *allocator =
        (AfAllocator){.allocate = af_default_allocate, .deallocate = af_default_deallocate};
}

void *af_allocate(AfAllocator *allocator, size_t size, size_t alignment) {
    void *memory = NULL;
    if (allocator == NULL || allocator->allocate == NULL) {
        return NULL;
    }
    memory = allocator->allocate(allocator->user, size, alignment);
    if (memory != NULL) {
        allocator->total_allocations++;
        if (allocator->frame_active != 0) {
            allocator->frame_allocations++;
        }
    }
    return memory;
}

void *af_allocate_array(AfAllocator *allocator, size_t count, size_t element_size,
                        size_t alignment) {
    size_t size = 0U;
    if (af_size_mul_overflow(count, element_size, &size)) {
        return NULL;
    }
    return af_allocate(allocator, size, alignment);
}

void af_deallocate(AfAllocator *allocator, void *memory) {
    if (allocator != NULL && allocator->deallocate != NULL && memory != NULL) {
        allocator->deallocate(allocator->user, memory);
    }
}

void af_allocator_begin_frame(AfAllocator *allocator) {
    if (allocator != NULL) {
        allocator->frame_allocations = 0U;
        allocator->frame_active = 1;
    }
}

uint64_t af_allocator_end_frame(AfAllocator *allocator) {
    if (allocator == NULL) {
        return 0U;
    }
    allocator->frame_active = 0;
    return allocator->frame_allocations;
}
