#ifndef AF_RENDER_ITEMS_H
#define AF_RENDER_ITEMS_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef enum AfVkRenderItemType {
    AF_VK_RENDER_ANALYTIC = 0,
    AF_VK_RENDER_PATH,
    AF_VK_RENDER_IMAGE
} AfVkRenderItemType;

typedef struct AfVkRenderItem {
    AfVkRenderItemType type;
    int32_t layer_order;
    uint32_t first;
    uint32_t count;
    uint32_t atlas_index;
} AfVkRenderItem;

/* Both arrays have count entries and must not alias. Stable sorting preserves painter order for
 * equal layers. Scratch is retained by the renderer, so frame preparation never allocates. */
static inline uint32_t af_vk_prepare_render_items(AfVkRenderItem *items, AfVkRenderItem *scratch,
                                                  uint32_t count) {
    bool sorted = true;
    for (uint32_t index = 1U; index < count; ++index) {
        if (items[index - 1U].layer_order > items[index].layer_order) {
            sorted = false;
            break;
        }
    }
    if (!sorted) {
        AfVkRenderItem *source = items;
        AfVkRenderItem *target = scratch;
        for (uint32_t width = 1U; width < count;) {
            for (uint32_t first = 0U; first < count;) {
                const uint32_t middle = count - first < width ? count : first + width;
                const uint32_t end = count - middle < width ? count : middle + width;
                uint32_t left = first;
                uint32_t right = middle;
                for (uint32_t write = first; write < end; ++write) {
                    if (left < middle &&
                        (right == end || source[left].layer_order <= source[right].layer_order)) {
                        target[write] = source[left++];
                    } else {
                        target[write] = source[right++];
                    }
                }
                first = end;
            }
            AfVkRenderItem *swap = source;
            source = target;
            target = swap;
            if (width > count / 2U) {
                break;
            }
            width *= 2U;
        }
        if (source != items) {
            memcpy(items, source, (size_t)count * sizeof(*items));
        }
    }
    uint32_t batches = 0U;
    for (uint32_t index = 0U; index < count; ++index) {
        const AfVkRenderItem next = items[index];
        AfVkRenderItem *previous = batches == 0U ? NULL : &items[batches - 1U];
        /* Keep layer boundaries, including the particle insertion boundary, intact. */
        if (previous != NULL && previous->type == next.type &&
            previous->layer_order == next.layer_order &&
            (next.type != AF_VK_RENDER_IMAGE || previous->atlas_index == next.atlas_index) &&
            next.first >= previous->first && next.first - previous->first == previous->count &&
            next.count <= UINT32_MAX - previous->count) {
            previous->count += next.count;
        } else {
            items[batches++] = next;
        }
    }
    return batches;
}

#endif
