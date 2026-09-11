#ifndef AF_ID_INDEX_H
#define AF_ID_INDEX_H

#include "arcflow/arcflow_result.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Temporary lookup for records whose first field is a uint64_t ID. Rebuild after structural
 * edits; storage belongs to the caller and no persistent cache or heap allocation is needed. */
typedef struct AfIdIndex {
    const unsigned char *records;
    size_t stride;
    uint32_t *slots;
    uint32_t mask;
} AfIdIndex;

static inline uint32_t af_id_index_hash(uint64_t id) {
    id ^= id >> 30U;
    id *= UINT64_C(0xbf58476d1ce4e5b9);
    id ^= id >> 27U;
    id *= UINT64_C(0x94d049bb133111eb);
    return (uint32_t)(id ^ (id >> 31U));
}

static inline uint64_t af_id_index_key(const AfIdIndex *index, uint32_t record) {
    uint64_t id;
    memcpy(&id, index->records + (size_t)record * index->stride, sizeof(id));
    return id;
}

static inline AfResult af_id_index_build(AfIdIndex *index, const void *records, uint32_t count,
                                         size_t stride, uint32_t *slots, uint32_t slot_capacity) {
    uint32_t capacity = 2U;
    if (index == NULL || slots == NULL || (count != 0U && records == NULL) ||
        stride < sizeof(uint64_t) || count > slot_capacity / 2U || slot_capacity < 2U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    while (capacity < count * 2U) {
        if (capacity > slot_capacity / 2U) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        capacity *= 2U;
    }
    *index =
        (AfIdIndex){.records = records, .stride = stride, .slots = slots, .mask = capacity - 1U};
    memset(slots, 0, (size_t)capacity * sizeof(*slots));
    for (uint32_t record = 0U; record < count; ++record) {
        const uint64_t id = af_id_index_key(index, record);
        uint32_t slot = af_id_index_hash(id) & index->mask;
        while (slots[slot] != 0U) {
            if (af_id_index_key(index, slots[slot] - 1U) == id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            slot = (slot + 1U) & index->mask;
        }
        slots[slot] = record + 1U;
    }
    return AF_RESULT_SUCCESS;
}

static inline uint32_t af_id_index_find(const AfIdIndex *index, uint64_t id) {
    uint32_t slot = af_id_index_hash(id) & index->mask;
    while (index->slots[slot] != 0U) {
        const uint32_t record = index->slots[slot] - 1U;
        if (af_id_index_key(index, record) == id) {
            return record;
        }
        slot = (slot + 1U) & index->mask;
    }
    return UINT32_MAX;
}

#endif
