#include "assets/image.h"

#include "core/hash.h"
#include "core/safe_math.h"

#include <math.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

typedef struct AfImagePackEntry {
    uint32_t asset_index;
    uint32_t width;
    uint32_t height;
    uint64_t hash;
    uint64_t id;
} AfImagePackEntry;

static bool af_image_name_valid(const char *name) {
    const size_t length = name == NULL ? 0U : strlen(name);
    if (length == 0U || length >= AF_IMAGE_NAME_CAPACITY) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        if ((unsigned char)name[index] < 32U || (unsigned char)name[index] == 127U) {
            return false;
        }
    }
    return true;
}

bool af_image_path_is_project_relative(const char *path) {
    const size_t length = path == NULL ? 0U : strlen(path);
    if (length == 0U || length >= AF_IMAGE_PATH_CAPACITY || path[0] == '/' || path[0] == '\\' ||
        (length >= 2U && path[1] == ':')) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        if ((unsigned char)path[index] < 32U || path[index] == '\\' ||
            (path[index] == '.' && index + 1U < length && path[index + 1U] == '.' &&
             (index == 0U || path[index - 1U] == '/') &&
             (index + 2U == length || path[index + 2U] == '/'))) {
            return false;
        }
    }
    return true;
}

AfResult af_image_data_create(AfImageData *data, AfAllocator *allocator) {
    if (data == NULL || allocator == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *data = (AfImageData){0};
    data->assets = af_allocate_array(allocator, AF_MAX_IMAGE_ASSETS, sizeof(*data->assets),
                                     alignof(AfImageAsset));
    data->bindings = af_allocate_array(allocator, AF_MAX_IMAGE_BINDINGS, sizeof(*data->bindings),
                                       alignof(AfImageBinding));
    data->sequences = af_allocate_array(allocator, AF_MAX_IMAGE_SEQUENCES, sizeof(*data->sequences),
                                        alignof(AfImageSequence));
    data->sequence_frames = af_allocate_array(allocator, AF_MAX_IMAGE_SEQUENCE_FRAMES,
                                              sizeof(*data->sequence_frames), alignof(uint64_t));
    if (data->assets == NULL || data->bindings == NULL || data->sequences == NULL ||
        data->sequence_frames == NULL) {
        af_image_data_destroy(data);
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memset(data->assets, 0, AF_MAX_IMAGE_ASSETS * sizeof(*data->assets));
    memset(data->bindings, 0, AF_MAX_IMAGE_BINDINGS * sizeof(*data->bindings));
    memset(data->sequences, 0, AF_MAX_IMAGE_SEQUENCES * sizeof(*data->sequences));
    memset(data->sequence_frames, 0, AF_MAX_IMAGE_SEQUENCE_FRAMES * sizeof(*data->sequence_frames));
    data->asset_capacity = AF_MAX_IMAGE_ASSETS;
    data->binding_capacity = AF_MAX_IMAGE_BINDINGS;
    data->sequence_capacity = AF_MAX_IMAGE_SEQUENCES;
    data->sequence_frame_capacity = AF_MAX_IMAGE_SEQUENCE_FRAMES;
    data->allocator = allocator;
    return AF_RESULT_SUCCESS;
}

AfResult af_image_data_clone(const AfImageData *source, AfAllocator *allocator,
                             AfImageData *out_data) {
    uint64_t source_ids[AF_MAX_IMAGE_ASSETS];
    uint64_t target_ids[AF_MAX_IMAGE_ASSETS];
    AfResult result = AF_RESULT_SUCCESS;
    if (source == NULL || allocator == NULL || out_data == NULL ||
        af_image_data_validate(source, false) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_image_data_create(out_data, allocator);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    for (uint32_t index = 0U; index < source->asset_count; ++index) {
        const AfImageAsset *asset = &source->assets[index];
        uint64_t target_id = 0U;
        bool deduplicated = false;
        if (asset->missing || asset->pixels == NULL) {
            af_image_data_destroy(out_data);
            return AF_RESULT_NOT_FOUND;
        }
        result = af_image_add(out_data, asset, asset->pixels, &target_id, &deduplicated);
        if (result != AF_RESULT_SUCCESS) {
            af_image_data_destroy(out_data);
            return result;
        }
        source_ids[index] = asset->id;
        target_ids[index] = target_id;
        if (!deduplicated) {
            AfImageAsset *target = &out_data->assets[out_data->asset_count - 1U];
            target->trim_x = asset->trim_x;
            target->trim_y = asset->trim_y;
            target->trim_width = asset->trim_width;
            target->trim_height = asset->trim_height;
            target->content_hash = asset->content_hash;
        }
    }
    memcpy(out_data->sequences, source->sequences,
           (size_t)source->sequence_count * sizeof(AfImageSequence));
    memcpy(out_data->sequence_frames, source->sequence_frames,
           (size_t)source->sequence_frame_count * sizeof(uint64_t));
    memcpy(out_data->bindings, source->bindings,
           (size_t)source->binding_count * sizeof(AfImageBinding));
    out_data->sequence_count = source->sequence_count;
    out_data->sequence_frame_count = source->sequence_frame_count;
    out_data->binding_count = source->binding_count;
    for (uint32_t binding = 0U; binding < out_data->binding_count; ++binding) {
        if (out_data->bindings[binding].asset_id == 0U) {
            continue;
        }
        for (uint32_t asset = 0U; asset < source->asset_count; ++asset) {
            if (out_data->bindings[binding].asset_id == source_ids[asset]) {
                out_data->bindings[binding].asset_id = target_ids[asset];
                break;
            }
        }
    }
    for (uint32_t frame = 0U; frame < out_data->sequence_frame_count; ++frame) {
        for (uint32_t asset = 0U; asset < source->asset_count; ++asset) {
            if (out_data->sequence_frames[frame] == source_ids[asset]) {
                out_data->sequence_frames[frame] = target_ids[asset];
                break;
            }
        }
    }
    out_data->generation = source->generation;
    return AF_RESULT_SUCCESS;
}

void af_image_data_destroy(AfImageData *data) {
    if (data == NULL) {
        return;
    }
    if (data->allocator != NULL) {
        for (uint32_t index = 0U; index < data->asset_count; ++index) {
            af_deallocate(data->allocator, data->assets[index].pixels);
        }
        for (uint32_t index = 0U; index < data->atlas_count; ++index) {
            af_deallocate(data->allocator, data->atlases[index].pixels);
        }
        af_deallocate(data->allocator, data->sequence_frames);
        af_deallocate(data->allocator, data->sequences);
        af_deallocate(data->allocator, data->bindings);
        af_deallocate(data->allocator, data->assets);
    }
    *data = (AfImageData){0};
}

AfImageAsset *af_image_find(AfImageData *data, uint64_t id) {
    if (data != NULL) {
        for (uint32_t index = 0U; index < data->asset_count; ++index) {
            if (data->assets[index].id == id) {
                return &data->assets[index];
            }
        }
    }
    return NULL;
}

const AfImageAsset *af_image_find_const(const AfImageData *data, uint64_t id) {
    return af_image_find((AfImageData *)data, id);
}

AfImageBinding *af_image_find_binding(AfImageData *data, uint64_t node_id) {
    if (data != NULL) {
        for (uint32_t index = 0U; index < data->binding_count; ++index) {
            if (data->bindings[index].node_id == node_id) {
                return &data->bindings[index];
            }
        }
    }
    return NULL;
}

const AfImageBinding *af_image_find_binding_const(const AfImageData *data, uint64_t node_id) {
    return af_image_find_binding((AfImageData *)data, node_id);
}

const AfImageSequence *af_image_find_sequence_const(const AfImageData *data, uint64_t id) {
    if (data != NULL) {
        for (uint32_t index = 0U; index < data->sequence_count; ++index) {
            if (data->sequences[index].id == id) {
                return &data->sequences[index];
            }
        }
    }
    return NULL;
}

static bool af_image_dimensions_valid(uint32_t width, uint32_t height, size_t *out_size) {
    size_t pixels = 0U;
    size_t bytes = 0U;
    if (width == 0U || height == 0U || width > AF_MAX_IMAGE_DIMENSION ||
        height > AF_MAX_IMAGE_DIMENSION || af_size_mul_overflow(width, height, &pixels) ||
        pixels > AF_MAX_IMAGE_PIXELS || af_size_mul_overflow(pixels, 4U, &bytes) ||
        bytes > UINT32_MAX) {
        return false;
    }
    if (out_size != NULL) {
        *out_size = bytes;
    }
    return true;
}

AfResult af_image_add(AfImageData *data, const AfImageAsset *asset, const uint8_t *rgba_pixels,
                      uint64_t *out_asset_id, bool *out_deduplicated) {
    size_t pixel_size = 0U;
    uint64_t content_hash = 0U;
    uint8_t *copy = NULL;
    if (data == NULL || asset == NULL || rgba_pixels == NULL || data->allocator == NULL ||
        asset->id == 0U || asset->id == UINT64_MAX ||
        af_image_find_const(data, asset->id) != NULL ||
        af_image_find_sequence_const(data, asset->id) != NULL ||
        !af_image_name_valid(asset->name) ||
        !af_image_path_is_project_relative(asset->source_path) ||
        asset->source_format > AF_IMAGE_SOURCE_SVG || asset->color_space > AF_IMAGE_COLOR_LINEAR ||
        asset->compression > AF_IMAGE_COMPRESSION_LOSSLESS ||
        !af_image_dimensions_valid(asset->width, asset->height, &pixel_size)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    content_hash = af_hash_bytes(rgba_pixels, pixel_size);
    for (uint32_t index = 0U; index < data->asset_count; ++index) {
        const AfImageAsset *candidate = &data->assets[index];
        if (candidate->content_hash == content_hash && candidate->width == asset->width &&
            candidate->height == asset->height && candidate->color_space == asset->color_space &&
            candidate->pixel_size == pixel_size &&
            memcmp(candidate->pixels, rgba_pixels, pixel_size) == 0) {
            if (out_asset_id != NULL) {
                *out_asset_id = candidate->id;
            }
            if (out_deduplicated != NULL) {
                *out_deduplicated = true;
            }
            return AF_RESULT_SUCCESS;
        }
    }
    if (data->asset_count >= data->asset_capacity) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    copy = af_allocate(data->allocator, pixel_size, alignof(uint8_t));
    if (copy == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memcpy(copy, rgba_pixels, pixel_size);
    data->assets[data->asset_count] = *asset;
    data->assets[data->asset_count].content_hash = content_hash;
    data->assets[data->asset_count].pixel_size = (uint32_t)pixel_size;
    data->assets[data->asset_count].pixels = copy;
    data->assets[data->asset_count].trim_x = 0U;
    data->assets[data->asset_count].trim_y = 0U;
    data->assets[data->asset_count].trim_width = asset->width;
    data->assets[data->asset_count].trim_height = asset->height;
    ++data->asset_count;
    ++data->generation;
    if (out_asset_id != NULL) {
        *out_asset_id = asset->id;
    }
    if (out_deduplicated != NULL) {
        *out_deduplicated = false;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_image_remove(AfImageData *data, uint64_t id) {
    uint32_t index = 0U;
    if (data == NULL || data->allocator == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    while (index < data->asset_count && data->assets[index].id != id) {
        ++index;
    }
    if (index == data->asset_count) {
        return AF_RESULT_NOT_FOUND;
    }
    for (uint32_t binding = 0U; binding < data->binding_count; ++binding) {
        if (data->bindings[binding].asset_id == id) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (uint32_t frame = 0U; frame < data->sequence_frame_count; ++frame) {
        if (data->sequence_frames[frame] == id) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    af_deallocate(data->allocator, data->assets[index].pixels);
    memmove(&data->assets[index], &data->assets[index + 1U],
            (size_t)(data->asset_count - index - 1U) * sizeof(AfImageAsset));
    --data->asset_count;
    ++data->generation;
    return AF_RESULT_SUCCESS;
}

AfResult af_image_set_binding(AfImageData *data, const AfImageBinding *binding) {
    AfImageBinding *existing = NULL;
    if (data == NULL || binding == NULL || binding->node_id == 0U ||
        binding->display_mode > AF_IMAGE_DISPLAY_NINE_SLICE ||
        (binding->asset_id == 0U) == (binding->sequence_id == 0U) ||
        (binding->asset_id != 0U && af_image_find_const(data, binding->asset_id) == NULL) ||
        (binding->sequence_id != 0U &&
         af_image_find_sequence_const(data, binding->sequence_id) == NULL) ||
        !isfinite(binding->frames_per_second) || binding->frames_per_second < 0.0F ||
        binding->frames_per_second > 1000.0F) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    existing = af_image_find_binding(data, binding->node_id);
    if (existing != NULL) {
        *existing = *binding;
    } else {
        if (data->binding_count >= data->binding_capacity) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        data->bindings[data->binding_count++] = *binding;
    }
    ++data->generation;
    return AF_RESULT_SUCCESS;
}

AfResult af_image_remove_binding(AfImageData *data, uint64_t node_id) {
    if (data == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < data->binding_count; ++index) {
        if (data->bindings[index].node_id == node_id) {
            memmove(&data->bindings[index], &data->bindings[index + 1U],
                    (size_t)(data->binding_count - index - 1U) * sizeof(AfImageBinding));
            --data->binding_count;
            ++data->generation;
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_NOT_FOUND;
}

AfResult af_image_add_sequence(AfImageData *data, const AfImageSequence *sequence,
                               const uint64_t *frame_ids) {
    if (data == NULL || sequence == NULL || frame_ids == NULL || sequence->id == 0U ||
        sequence->id == UINT64_MAX || af_image_find_sequence_const(data, sequence->id) != NULL ||
        af_image_find_const(data, sequence->id) != NULL || !af_image_name_valid(sequence->name) ||
        sequence->frame_count == 0U ||
        sequence->frame_count > data->sequence_frame_capacity - data->sequence_frame_count ||
        data->sequence_count >= data->sequence_capacity || !isfinite(sequence->frames_per_second) ||
        sequence->frames_per_second <= 0.0F || sequence->frames_per_second > 1000.0F) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < sequence->frame_count; ++index) {
        if (af_image_find_const(data, frame_ids[index]) == NULL) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    data->sequences[data->sequence_count] = *sequence;
    data->sequences[data->sequence_count].frame_start = data->sequence_frame_count;
    memcpy(&data->sequence_frames[data->sequence_frame_count], frame_ids,
           (size_t)sequence->frame_count * sizeof(uint64_t));
    data->sequence_frame_count += sequence->frame_count;
    ++data->sequence_count;
    ++data->generation;
    return AF_RESULT_SUCCESS;
}

AfResult af_image_remove_sequence(AfImageData *data, uint64_t id) {
    uint32_t index = 0U;
    uint32_t frame_start = 0U;
    uint32_t frame_count = 0U;
    if (data == NULL || id == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    while (index < data->sequence_count && data->sequences[index].id != id) {
        ++index;
    }
    if (index == data->sequence_count) {
        return AF_RESULT_NOT_FOUND;
    }
    for (uint32_t binding = 0U; binding < data->binding_count; ++binding) {
        if (data->bindings[binding].sequence_id == id) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    frame_start = data->sequences[index].frame_start;
    frame_count = data->sequences[index].frame_count;
    memmove(&data->sequence_frames[frame_start], &data->sequence_frames[frame_start + frame_count],
            (size_t)(data->sequence_frame_count - frame_start - frame_count) * sizeof(uint64_t));
    data->sequence_frame_count -= frame_count;
    memmove(&data->sequences[index], &data->sequences[index + 1U],
            (size_t)(data->sequence_count - index - 1U) * sizeof(AfImageSequence));
    --data->sequence_count;
    for (uint32_t sequence = 0U; sequence < data->sequence_count; ++sequence) {
        if (data->sequences[sequence].frame_start > frame_start) {
            data->sequences[sequence].frame_start -= frame_count;
        }
    }
    ++data->generation;
    return AF_RESULT_SUCCESS;
}

AfResult af_image_trim(AfImageAsset *asset) {
    uint32_t minimum_x = 0U;
    uint32_t minimum_y = 0U;
    uint32_t maximum_x = 0U;
    uint32_t maximum_y = 0U;
    bool found = false;
    if (asset == NULL || asset->pixels == NULL ||
        !af_image_dimensions_valid(asset->width, asset->height, NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    minimum_x = asset->width;
    minimum_y = asset->height;
    for (uint32_t y = 0U; y < asset->height; ++y) {
        for (uint32_t x = 0U; x < asset->width; ++x) {
            if (asset->pixels[((size_t)y * asset->width + x) * 4U + 3U] != 0U) {
                minimum_x = x < minimum_x ? x : minimum_x;
                minimum_y = y < minimum_y ? y : minimum_y;
                maximum_x = x > maximum_x ? x : maximum_x;
                maximum_y = y > maximum_y ? y : maximum_y;
                found = true;
            }
        }
    }
    asset->trim_x = found ? minimum_x : 0U;
    asset->trim_y = found ? minimum_y : 0U;
    asset->trim_width = found ? maximum_x - minimum_x + 1U : 1U;
    asset->trim_height = found ? maximum_y - minimum_y + 1U : 1U;
    return AF_RESULT_SUCCESS;
}

static int af_image_pack_compare(const void *left_value, const void *right_value) {
    const AfImagePackEntry *left = left_value;
    const AfImagePackEntry *right = right_value;
    if (left->height != right->height) {
        return left->height > right->height ? -1 : 1;
    }
    if (left->width != right->width) {
        return left->width > right->width ? -1 : 1;
    }
    if (left->hash != right->hash) {
        return left->hash < right->hash ? -1 : 1;
    }
    return left->id == right->id ? 0 : (left->id < right->id ? -1 : 1);
}

static uint32_t af_image_next_power_of_two(uint32_t value) {
    uint32_t result = 1U;
    while (result < value && result < AF_MAX_IMAGE_DIMENSION) {
        result <<= 1U;
    }
    return result;
}

static bool af_image_is_power_of_two(uint32_t value) {
    return value != 0U && (value & (value - 1U)) == 0U;
}

static uint32_t af_image_max_mip_count(uint32_t width, uint32_t height) {
    uint32_t edge = width > height ? width : height;
    uint32_t count = edge == 0U ? 0U : 1U;
    while (edge > 1U) {
        edge /= 2U;
        ++count;
    }
    return count;
}

static uint32_t af_image_mip_size(uint32_t width, uint32_t height, uint32_t mip_count) {
    uint64_t total = 0U;
    for (uint32_t mip = 0U; mip < mip_count; ++mip) {
        total += (uint64_t)width * height * 4U;
        width = width > 1U ? width / 2U : 1U;
        height = height > 1U ? height / 2U : 1U;
    }
    return total <= UINT32_MAX ? (uint32_t)total : 0U;
}

static void af_image_generate_mips(AfImageAtlas *atlas) {
    uint32_t source_width = atlas->width;
    uint32_t source_height = atlas->height;
    for (uint32_t mip = 1U; mip < atlas->mip_count; ++mip) {
        const uint8_t *source = atlas->pixels + atlas->mip_offsets[mip - 1U];
        uint8_t *target = atlas->pixels + atlas->mip_offsets[mip];
        const uint32_t target_width = source_width > 1U ? source_width / 2U : 1U;
        const uint32_t target_height = source_height > 1U ? source_height / 2U : 1U;
        for (uint32_t y = 0U; y < target_height; ++y) {
            for (uint32_t x = 0U; x < target_width; ++x) {
                for (uint32_t channel = 0U; channel < 4U; ++channel) {
                    uint32_t sum = 0U;
                    uint32_t samples = 0U;
                    for (uint32_t dy = 0U; dy < 2U; ++dy) {
                        for (uint32_t dx = 0U; dx < 2U; ++dx) {
                            const uint32_t sx = x * 2U + dx;
                            const uint32_t sy = y * 2U + dy;
                            if (sx < source_width && sy < source_height) {
                                sum += source[((size_t)sy * source_width + sx) * 4U + channel];
                                ++samples;
                            }
                        }
                    }
                    target[((size_t)y * target_width + x) * 4U + channel] =
                        (uint8_t)((sum + samples / 2U) / samples);
                }
            }
        }
        source_width = target_width;
        source_height = target_height;
    }
}

static bool af_image_sequence_contains(const AfImageData *data, uint64_t sequence_id,
                                       uint64_t asset_id) {
    const AfImageSequence *sequence = af_image_find_sequence_const(data, sequence_id);
    if (sequence == NULL) {
        return false;
    }
    for (uint32_t frame = 0U; frame < sequence->frame_count; ++frame) {
        if (data->sequence_frames[sequence->frame_start + frame] == asset_id) {
            return true;
        }
    }
    return false;
}

static bool af_image_requires_full_sprite(const AfImageData *data, uint64_t asset_id) {
    for (uint32_t index = 0U; index < data->binding_count; ++index) {
        const AfImageBinding *binding = &data->bindings[index];
        if (binding->display_mode == AF_IMAGE_DISPLAY_NINE_SLICE &&
            (binding->asset_id == asset_id ||
             (binding->sequence_id != 0U &&
              af_image_sequence_contains(data, binding->sequence_id, asset_id)))) {
            return true;
        }
    }
    return false;
}

static AfResult af_image_pack_color_space(AfImageData *data, AfImageColorSpace color_space,
                                          AfImageAtlas *atlas, uint32_t atlas_index) {
    AfImagePackEntry entries[AF_MAX_IMAGE_ASSETS];
    uint32_t entry_count = 0U;
    uint32_t maximum_width = 1U;
    uint32_t row_width = 0U;
    uint32_t row_height = 0U;
    uint32_t used_width = 0U;
    uint32_t used_height = 0U;
    bool mipmaps = false;
    bool lossless = true;
    for (uint32_t index = 0U; index < data->asset_count; ++index) {
        AfImageAsset *asset = &data->assets[index];
        if (asset->color_space != color_space || asset->missing) {
            continue;
        }
        if (asset->trim_enabled && !af_image_requires_full_sprite(data, asset->id)) {
            if (af_image_trim(asset) != AF_RESULT_SUCCESS) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        } else {
            asset->trim_x = 0U;
            asset->trim_y = 0U;
            asset->trim_width = asset->width;
            asset->trim_height = asset->height;
        }
        entries[entry_count++] = (AfImagePackEntry){.asset_index = index,
                                                    .width = asset->trim_width + 2U,
                                                    .height = asset->trim_height + 2U,
                                                    .hash = asset->content_hash,
                                                    .id = asset->id};
        maximum_width = entries[entry_count - 1U].width > maximum_width
                            ? entries[entry_count - 1U].width
                            : maximum_width;
        mipmaps = mipmaps || asset->mipmaps_enabled;
        lossless = lossless && asset->compression == AF_IMAGE_COMPRESSION_LOSSLESS;
    }
    if (entry_count == 0U) {
        *atlas = (AfImageAtlas){.color_space = color_space};
        return AF_RESULT_SUCCESS;
    }
    qsort(entries, entry_count, sizeof(entries[0]), af_image_pack_compare);
    maximum_width = af_image_next_power_of_two(maximum_width < 256U ? 256U : maximum_width);
    for (;;) {
        row_width = 0U;
        row_height = 0U;
        used_width = 0U;
        used_height = 0U;
        for (uint32_t index = 0U; index < entry_count; ++index) {
            if (row_width != 0U && row_width + entries[index].width > maximum_width) {
                used_height += row_height;
                row_width = 0U;
                row_height = 0U;
            }
            row_width += entries[index].width;
            row_height = entries[index].height > row_height ? entries[index].height : row_height;
            used_width = row_width > used_width ? row_width : used_width;
        }
        used_height += row_height;
        if (used_height <= AF_MAX_IMAGE_DIMENSION || maximum_width >= AF_MAX_IMAGE_DIMENSION) {
            break;
        }
        maximum_width <<= 1U;
    }
    if (used_width > AF_MAX_IMAGE_DIMENSION || used_height > AF_MAX_IMAGE_DIMENSION) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    atlas->width = af_image_next_power_of_two(used_width);
    atlas->height = af_image_next_power_of_two(used_height);
    atlas->mip_count = 1U;
    if (mipmaps) {
        uint32_t edge = atlas->width > atlas->height ? atlas->width : atlas->height;
        while (edge > 1U && atlas->mip_count < AF_IMAGE_MAX_MIPS) {
            edge /= 2U;
            ++atlas->mip_count;
        }
    }
    atlas->pixel_size = af_image_mip_size(atlas->width, atlas->height, atlas->mip_count);
    atlas->color_space = color_space;
    atlas->compression = lossless ? AF_IMAGE_COMPRESSION_LOSSLESS : AF_IMAGE_COMPRESSION_NONE;
    atlas->pixels = af_allocate(data->allocator, atlas->pixel_size, alignof(uint8_t));
    if (atlas->pixels == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memset(atlas->pixels, 0, atlas->pixel_size);
    {
        uint32_t mip_width = atlas->width;
        uint32_t mip_height = atlas->height;
        uint32_t offset = 0U;
        for (uint32_t mip = 0U; mip < atlas->mip_count; ++mip) {
            atlas->mip_offsets[mip] = offset;
            offset += mip_width * mip_height * 4U;
            mip_width = mip_width > 1U ? mip_width / 2U : 1U;
            mip_height = mip_height > 1U ? mip_height / 2U : 1U;
        }
    }
    row_width = 0U;
    row_height = 0U;
    used_height = 0U;
    for (uint32_t index = 0U; index < entry_count; ++index) {
        AfImageAsset *asset = &data->assets[entries[index].asset_index];
        if (row_width != 0U && row_width + entries[index].width > atlas->width) {
            used_height += row_height;
            row_width = 0U;
            row_height = 0U;
        }
        asset->atlas_index = atlas_index;
        asset->atlas_x = row_width + 1U;
        asset->atlas_y = used_height + 1U;
        for (uint32_t y = 0U; y < asset->trim_height; ++y) {
            const uint8_t *source =
                asset->pixels + ((size_t)(asset->trim_y + y) * asset->width + asset->trim_x) * 4U;
            uint8_t *target =
                atlas->pixels + ((size_t)(asset->atlas_y + y) * atlas->width + asset->atlas_x) * 4U;
            memcpy(target, source, (size_t)asset->trim_width * 4U);
        }
        {
            const uint32_t left = asset->atlas_x - 1U;
            const uint32_t right = asset->atlas_x + asset->trim_width;
            const uint32_t top = asset->atlas_y - 1U;
            const uint32_t bottom = asset->atlas_y + asset->trim_height;
            for (uint32_t y = 0U; y < asset->trim_height; ++y) {
                uint8_t *row = atlas->pixels +
                               ((size_t)(asset->atlas_y + y) * atlas->width + asset->atlas_x) * 4U;
                memcpy(atlas->pixels + ((size_t)(asset->atlas_y + y) * atlas->width + left) * 4U,
                       row, 4U);
                memcpy(atlas->pixels + ((size_t)(asset->atlas_y + y) * atlas->width + right) * 4U,
                       row + (size_t)(asset->trim_width - 1U) * 4U, 4U);
            }
            memcpy(atlas->pixels + ((size_t)top * atlas->width + left) * 4U,
                   atlas->pixels + ((size_t)asset->atlas_y * atlas->width + left) * 4U,
                   (size_t)(asset->trim_width + 2U) * 4U);
            memcpy(atlas->pixels + ((size_t)bottom * atlas->width + left) * 4U,
                   atlas->pixels +
                       ((size_t)(asset->atlas_y + asset->trim_height - 1U) * atlas->width + left) *
                           4U,
                   (size_t)(asset->trim_width + 2U) * 4U);
        }
        row_width += entries[index].width;
        row_height = entries[index].height > row_height ? entries[index].height : row_height;
    }
    af_image_generate_mips(atlas);
    return AF_RESULT_SUCCESS;
}

AfResult af_image_build_atlases(AfImageData *data) {
    AfResult result = AF_RESULT_SUCCESS;
    if (data == NULL || data->allocator == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < data->atlas_count; ++index) {
        af_deallocate(data->allocator, data->atlases[index].pixels);
    }
    memset(data->atlases, 0, sizeof(data->atlases));
    data->atlas_count = AF_MAX_IMAGE_ATLASES;
    result = af_image_pack_color_space(data, AF_IMAGE_COLOR_SRGB, &data->atlases[0], 0U);
    if (result == AF_RESULT_SUCCESS) {
        result = af_image_pack_color_space(data, AF_IMAGE_COLOR_LINEAR, &data->atlases[1], 1U);
    }
    if (result != AF_RESULT_SUCCESS) {
        for (uint32_t index = 0U; index < data->atlas_count; ++index) {
            af_deallocate(data->allocator, data->atlases[index].pixels);
        }
        memset(data->atlases, 0, sizeof(data->atlases));
        data->atlas_count = 0U;
        return result;
    }
    ++data->generation;
    return AF_RESULT_SUCCESS;
}

AfResult af_image_data_validate(const AfImageData *data, bool require_sources) {
    uint64_t total_pixels = 0U;
    if (data == NULL || data->assets == NULL || data->bindings == NULL || data->sequences == NULL ||
        data->sequence_frames == NULL || data->asset_count > data->asset_capacity ||
        data->asset_capacity > AF_MAX_IMAGE_ASSETS ||
        data->binding_count > data->binding_capacity ||
        data->binding_capacity > AF_MAX_IMAGE_BINDINGS ||
        data->sequence_count > data->sequence_capacity ||
        data->sequence_capacity > AF_MAX_IMAGE_SEQUENCES ||
        data->sequence_frame_count > data->sequence_frame_capacity ||
        data->sequence_frame_capacity > AF_MAX_IMAGE_SEQUENCE_FRAMES ||
        data->atlas_count > AF_MAX_IMAGE_ATLASES) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < data->atlas_count; ++index) {
        const AfImageAtlas *atlas = &data->atlases[index];
        uint32_t expected_size = 0U;
        if (atlas->color_space > AF_IMAGE_COLOR_LINEAR ||
            atlas->compression > AF_IMAGE_COMPRESSION_LOSSLESS ||
            ((atlas->width == 0U || atlas->height == 0U) != (atlas->pixel_size == 0U)) ||
            (atlas->pixel_size == 0U && (atlas->width != 0U || atlas->height != 0U ||
                                         atlas->mip_count != 0U || atlas->pixels != NULL)) ||
            (atlas->pixel_size != 0U &&
             (!af_image_is_power_of_two(atlas->width) || !af_image_is_power_of_two(atlas->height) ||
              atlas->width > AF_MAX_IMAGE_DIMENSION || atlas->height > AF_MAX_IMAGE_DIMENSION ||
              atlas->mip_count == 0U || atlas->mip_count > AF_IMAGE_MAX_MIPS ||
              atlas->mip_count > af_image_max_mip_count(atlas->width, atlas->height) ||
              atlas->pixels == NULL))) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        expected_size = af_image_mip_size(atlas->width, atlas->height, atlas->mip_count);
        if (expected_size != atlas->pixel_size) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        {
            uint32_t width = atlas->width;
            uint32_t height = atlas->height;
            uint32_t offset = 0U;
            for (uint32_t mip = 0U; mip < atlas->mip_count; ++mip) {
                if (atlas->mip_offsets[mip] != offset) {
                    return AF_RESULT_INVALID_ARGUMENT;
                }
                offset += width * height * 4U;
                width = width > 1U ? width / 2U : 1U;
                height = height > 1U ? height / 2U : 1U;
            }
        }
        for (uint32_t other = index + 1U; other < data->atlas_count; ++other) {
            if (data->atlases[other].color_space == atlas->color_space) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    for (uint32_t index = 0U; index < data->asset_count; ++index) {
        const AfImageAsset *asset = &data->assets[index];
        size_t pixel_size = 0U;
        if (asset->id == 0U || asset->id == UINT64_MAX || !af_image_name_valid(asset->name) ||
            (require_sources && !af_image_path_is_project_relative(asset->source_path)) ||
            asset->source_format > AF_IMAGE_SOURCE_SVG ||
            asset->color_space > AF_IMAGE_COLOR_LINEAR ||
            asset->compression > AF_IMAGE_COMPRESSION_LOSSLESS ||
            !af_image_dimensions_valid(asset->width, asset->height, &pixel_size) ||
            asset->pixel_size != pixel_size || (!asset->missing && asset->pixels == NULL) ||
            asset->trim_width == 0U || asset->trim_height == 0U ||
            asset->trim_width > asset->width || asset->trim_height > asset->height ||
            asset->trim_x > asset->width - asset->trim_width ||
            asset->trim_y > asset->height - asset->trim_height) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        if (!asset->missing && data->atlas_count != 0U) {
            const AfImageAtlas *atlas = NULL;
            if (asset->atlas_index >= data->atlas_count) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            atlas = &data->atlases[asset->atlas_index];
            if (atlas->pixels == NULL || atlas->color_space != asset->color_space ||
                asset->trim_width > atlas->width || asset->trim_height > atlas->height ||
                asset->atlas_x > atlas->width - asset->trim_width ||
                asset->atlas_y > atlas->height - asset->trim_height) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        total_pixels += (uint64_t)asset->width * asset->height;
        for (uint32_t other = index + 1U; other < data->asset_count; ++other) {
            if (data->assets[other].id == asset->id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    if (total_pixels > AF_MAX_IMAGE_PIXELS) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    for (uint32_t index = 0U; index < data->sequence_count; ++index) {
        const AfImageSequence *sequence = &data->sequences[index];
        if (sequence->id == 0U || sequence->id == UINT64_MAX ||
            af_image_find_const(data, sequence->id) != NULL ||
            !af_image_name_valid(sequence->name) || sequence->frame_count == 0U ||
            sequence->frame_start > data->sequence_frame_count ||
            sequence->frame_count > data->sequence_frame_count - sequence->frame_start ||
            !isfinite(sequence->frames_per_second) || sequence->frames_per_second <= 0.0F ||
            sequence->frames_per_second > 1000.0F) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t frame = 0U; frame < sequence->frame_count; ++frame) {
            if (af_image_find_const(data, data->sequence_frames[sequence->frame_start + frame]) ==
                NULL) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        for (uint32_t other = index + 1U; other < data->sequence_count; ++other) {
            if (data->sequences[other].id == sequence->id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    for (uint32_t index = 0U; index < data->binding_count; ++index) {
        const AfImageBinding *binding = &data->bindings[index];
        if (binding->node_id == 0U || (binding->asset_id == 0U) == (binding->sequence_id == 0U) ||
            (binding->asset_id != 0U && af_image_find_const(data, binding->asset_id) == NULL) ||
            (binding->sequence_id != 0U &&
             af_image_find_sequence_const(data, binding->sequence_id) == NULL) ||
            binding->display_mode > AF_IMAGE_DISPLAY_NINE_SLICE ||
            !isfinite(binding->frames_per_second) || binding->frames_per_second < 0.0F ||
            binding->frames_per_second > 1000.0F) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t other = index + 1U; other < data->binding_count; ++other) {
            if (data->bindings[other].node_id == binding->node_id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    return AF_RESULT_SUCCESS;
}
