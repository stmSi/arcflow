#ifndef AF_HASH_H
#define AF_HASH_H

#include <stddef.h>
#include <stdint.h>

uint64_t af_hash_bytes(const void *data, size_t size);
uint64_t af_hash_string(const char *string);

#endif
