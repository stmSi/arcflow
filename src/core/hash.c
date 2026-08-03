#include "core/hash.h"

#include <string.h>

uint64_t af_hash_bytes(const void *data, size_t size) {
    const unsigned char *bytes = data;
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t index = 0U;
    if (data == NULL && size != 0U) {
        return 0U;
    }
    for (index = 0U; index < size; ++index) {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

uint64_t af_hash_string(const char *string) {
    return string != NULL ? af_hash_bytes(string, strlen(string)) : 0U;
}
