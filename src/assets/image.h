#ifndef AF_IMAGE_H
#define AF_IMAGE_H

#include "arcflow/arcflow_result.h"
#include "core/allocator.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AF_MAX_IMAGE_ASSETS 256U
#define AF_MAX_IMAGE_BINDINGS 1024U
#define AF_MAX_IMAGE_SEQUENCES 64U
#define AF_MAX_IMAGE_SEQUENCE_FRAMES 2048U
#define AF_MAX_IMAGE_ATLASES 2U
#define AF_MAX_IMAGE_DIMENSION 4096U
#define AF_MAX_IMAGE_PIXELS (16U * 1024U * 1024U)
#define AF_MAX_IMAGE_SOURCE_SIZE (32U * 1024U * 1024U)
#define AF_IMAGE_PATH_CAPACITY 384U
#define AF_IMAGE_NAME_CAPACITY 64U
#define AF_IMAGE_MAX_MIPS 13U

typedef enum AfImageSourceFormat {
    AF_IMAGE_SOURCE_PNG = 0,
    AF_IMAGE_SOURCE_JPEG = 1,
    AF_IMAGE_SOURCE_KTX2 = 2,
    AF_IMAGE_SOURCE_SVG = 3
} AfImageSourceFormat;

typedef enum AfImageColorSpace {
    AF_IMAGE_COLOR_SRGB = 0,
    AF_IMAGE_COLOR_LINEAR = 1
} AfImageColorSpace;

typedef enum AfImageCompression {
    AF_IMAGE_COMPRESSION_NONE = 0,
    AF_IMAGE_COMPRESSION_LOSSLESS = 1
} AfImageCompression;

typedef enum AfImageDisplayMode {
    AF_IMAGE_DISPLAY_STRETCH = 0,
    AF_IMAGE_DISPLAY_NINE_SLICE = 1
} AfImageDisplayMode;

typedef struct AfImageBorders {
    uint16_t left;
    uint16_t top;
    uint16_t right;
    uint16_t bottom;
} AfImageBorders;

typedef struct AfImageAsset {
    uint64_t id;
    uint64_t content_hash;
    uint32_t width;
    uint32_t height;
    uint32_t trim_x;
    uint32_t trim_y;
    uint32_t trim_width;
    uint32_t trim_height;
    uint32_t atlas_index;
    uint32_t atlas_x;
    uint32_t atlas_y;
    uint32_t pixel_size;
    uint8_t *pixels;
    AfImageSourceFormat source_format;
    AfImageColorSpace color_space;
    AfImageCompression compression;
    AfImageBorders sprite_borders;
    bool trim_enabled;
    bool mipmaps_enabled;
    bool missing;
    char name[AF_IMAGE_NAME_CAPACITY];
    char source_path[AF_IMAGE_PATH_CAPACITY];
} AfImageAsset;

typedef struct AfImageBinding {
    uint64_t node_id;
    uint64_t asset_id;
    uint64_t sequence_id;
    AfImageDisplayMode display_mode;
    AfImageBorders borders;
    float frames_per_second;
    bool loop;
} AfImageBinding;

typedef struct AfImageSequence {
    uint64_t id;
    uint32_t frame_start;
    uint32_t frame_count;
    float frames_per_second;
    bool loop;
    char name[AF_IMAGE_NAME_CAPACITY];
} AfImageSequence;

typedef struct AfImageAtlas {
    uint32_t width;
    uint32_t height;
    uint32_t mip_count;
    uint32_t pixel_size;
    uint32_t mip_offsets[AF_IMAGE_MAX_MIPS];
    AfImageColorSpace color_space;
    AfImageCompression compression;
    uint8_t *pixels;
} AfImageAtlas;

typedef struct AfImageData {
    AfImageAsset *assets;
    uint32_t asset_count;
    uint32_t asset_capacity;
    AfImageBinding *bindings;
    uint32_t binding_count;
    uint32_t binding_capacity;
    AfImageSequence *sequences;
    uint32_t sequence_count;
    uint32_t sequence_capacity;
    uint64_t *sequence_frames;
    uint32_t sequence_frame_count;
    uint32_t sequence_frame_capacity;
    AfImageAtlas atlases[AF_MAX_IMAGE_ATLASES];
    uint32_t atlas_count;
    uint64_t generation;
    AfAllocator *allocator;
} AfImageData;

AfResult af_image_data_create(AfImageData *data, AfAllocator *allocator);
AfResult af_image_data_clone(const AfImageData *source, AfAllocator *allocator,
                             AfImageData *out_data);
void af_image_data_destroy(AfImageData *data);
AfResult af_image_data_validate(const AfImageData *data, bool require_sources);
bool af_image_path_is_project_relative(const char *path);
AfImageAsset *af_image_find(AfImageData *data, uint64_t id);
const AfImageAsset *af_image_find_const(const AfImageData *data, uint64_t id);
AfImageBinding *af_image_find_binding(AfImageData *data, uint64_t node_id);
const AfImageBinding *af_image_find_binding_const(const AfImageData *data, uint64_t node_id);
const AfImageSequence *af_image_find_sequence_const(const AfImageData *data, uint64_t id);
AfResult af_image_add(AfImageData *data, const AfImageAsset *asset, const uint8_t *rgba_pixels,
                      uint64_t *out_asset_id, bool *out_deduplicated);
AfResult af_image_remove(AfImageData *data, uint64_t id);
AfResult af_image_set_binding(AfImageData *data, const AfImageBinding *binding);
AfResult af_image_remove_binding(AfImageData *data, uint64_t node_id);
AfResult af_image_add_sequence(AfImageData *data, const AfImageSequence *sequence,
                               const uint64_t *frame_ids);
AfResult af_image_remove_sequence(AfImageData *data, uint64_t id);
AfResult af_image_trim(AfImageAsset *asset);
AfResult af_image_build_atlases(AfImageData *data);

#endif
