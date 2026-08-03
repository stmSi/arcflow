#ifndef AF_SAFE_MATH_H
#define AF_SAFE_MATH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static inline bool af_size_add_overflow(size_t a, size_t b, size_t *out) {
    if (a > SIZE_MAX - b) {
        return true;
    }
    *out = a + b;
    return false;
}

static inline bool af_size_mul_overflow(size_t a, size_t b, size_t *out) {
    if (a != 0U && b > SIZE_MAX / a) {
        return true;
    }
    *out = a * b;
    return false;
}

static inline bool af_u64_add_overflow(uint64_t a, uint64_t b, uint64_t *out) {
    if (a > UINT64_MAX - b) {
        return true;
    }
    *out = a + b;
    return false;
}

static inline bool af_u64_mul_overflow(uint64_t a, uint64_t b, uint64_t *out) {
    if (a != 0U && b > UINT64_MAX / a) {
        return true;
    }
    *out = a * b;
    return false;
}

static inline bool af_is_power_of_two(size_t value) {
    return value != 0U && (value & (value - 1U)) == 0U;
}

#endif
