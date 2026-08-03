#include "core/array.h"

#include "core/safe_math.h"

#include <string.h>

void af_array_init(AfArray *array, size_t element_size, size_t alignment, AfAllocator *allocator) {
    if (array != NULL) {
        *array =
            (AfArray){.element_size = element_size, .alignment = alignment, .allocator = allocator};
    }
}

void af_array_destroy(AfArray *array) {
    if (array != NULL) {
        af_deallocate(array->allocator, array->data);
        *array = (AfArray){0};
    }
}

AfResult af_array_reserve(AfArray *array, size_t capacity) {
    void *new_data = NULL;
    size_t bytes = 0U;
    if (array == NULL || array->allocator == NULL || array->element_size == 0U ||
        !af_is_power_of_two(array->alignment)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (capacity <= array->capacity) {
        return AF_RESULT_SUCCESS;
    }
    if (af_size_mul_overflow(capacity, array->element_size, &bytes)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    new_data = af_allocate(array->allocator, bytes, array->alignment);
    if (new_data == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    if (array->data != NULL) {
        memcpy(new_data, array->data, array->count * array->element_size);
        af_deallocate(array->allocator, array->data);
    }
    array->data = new_data;
    array->capacity = capacity;
    return AF_RESULT_SUCCESS;
}

AfResult af_array_push(AfArray *array, const void *element) {
    size_t new_capacity = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (array == NULL || element == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (array->count == array->capacity) {
        new_capacity = array->capacity == 0U ? 8U : array->capacity * 2U;
        if (new_capacity < array->capacity) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        result = af_array_reserve(array, new_capacity);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
    }
    memcpy((unsigned char *)array->data + array->count * array->element_size, element,
           array->element_size);
    array->count++;
    return AF_RESULT_SUCCESS;
}

void *af_array_at(AfArray *array, size_t index) {
    if (array == NULL || index >= array->count) {
        return NULL;
    }
    return (unsigned char *)array->data + index * array->element_size;
}
