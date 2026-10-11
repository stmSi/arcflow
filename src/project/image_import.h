#ifndef AF_IMAGE_IMPORT_H
#define AF_IMAGE_IMPORT_H

#include "af/assets/image.h"
#include "project/project.h"

typedef struct AfDecodedImage {
    uint8_t *pixels;
    uint32_t width;
    uint32_t height;
    AfImageSourceFormat format;
    AfImageColorSpace color_space;
    AfAllocator *allocator;
} AfDecodedImage;

AfResult af_image_decode_memory(const void *data, size_t size, AfImageSourceFormat format,
                                AfAllocator *allocator, AfDecodedImage *out_image);
AfResult af_image_decode_file(const char *path, AfAllocator *allocator, AfDecodedImage *out_image);
void af_decoded_image_destroy(AfDecodedImage *image);
AfResult af_project_import_image(AfProject *project, const char *project_path,
                                 const char *source_path, uint64_t *out_asset_id,
                                 uint64_t *out_node_id);
AfResult af_project_reimport_images(AfProject *project, const char *project_path,
                                    uint32_t *out_missing_count);
AfResult af_project_repair_image_source(AfProject *project, const char *project_path,
                                        uint64_t asset_id, const char *replacement_path);

AfResult af_image_decode_svg(const void *data, size_t size, AfAllocator *allocator,
                             AfDecodedImage *out_image);

#endif
