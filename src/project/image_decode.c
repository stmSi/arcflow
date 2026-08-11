#include "project/image_import.h"

#include "core/file_io.h"
#include "core/safe_math.h"

#include <SDL3/SDL.h>

#include <limits.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_STDIO
#define STBI_NO_THREAD_LOCALS
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "../../third_party/SDL/src/video/stb_image.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

static uint32_t af_image_read_u32_le(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8U | (uint32_t)bytes[2] << 16U |
           (uint32_t)bytes[3] << 24U;
}

static uint16_t af_image_read_u16_be(const uint8_t *bytes) {
    return (uint16_t)((uint16_t)bytes[0] << 8U | (uint16_t)bytes[1]);
}

static uint32_t af_image_read_u32_be(const uint8_t *bytes) {
    return (uint32_t)bytes[0] << 24U | (uint32_t)bytes[1] << 16U | (uint32_t)bytes[2] << 8U |
           (uint32_t)bytes[3];
}

static uint64_t af_image_read_u64_le(const uint8_t *bytes) {
    return (uint64_t)af_image_read_u32_le(bytes) | (uint64_t)af_image_read_u32_le(bytes + 4U)
                                                       << 32U;
}

static bool af_decoded_dimensions_valid(uint32_t width, uint32_t height, size_t *out_size) {
    size_t pixels = 0U;
    size_t bytes = 0U;
    if (width == 0U || height == 0U || width > AF_MAX_IMAGE_DIMENSION ||
        height > AF_MAX_IMAGE_DIMENSION || af_size_mul_overflow(width, height, &pixels) ||
        pixels > AF_MAX_IMAGE_PIXELS || af_size_mul_overflow(pixels, 4U, &bytes)) {
        return false;
    }
    *out_size = bytes;
    return true;
}

static uint32_t af_image_max_source_mips(uint32_t width, uint32_t height) {
    uint32_t edge = width > height ? width : height;
    uint32_t count = edge == 0U ? 0U : 1U;
    while (edge > 1U) {
        edge /= 2U;
        ++count;
    }
    return count;
}

static bool af_image_ktx2_dfd_valid(const uint8_t *dfd, uint32_t size, uint32_t format) {
    const bool srgb = format == 43U || format == 50U;
    const bool bgra = format == 44U || format == 50U;
    uint32_t descriptor_version = 0U;
    if (size != 92U) {
        return false;
    }
    descriptor_version = af_image_read_u32_le(dfd + 8U) & 0xFFFFU;
    if (af_image_read_u32_le(dfd) != 92U || af_image_read_u32_le(dfd + 4U) != 0U ||
        descriptor_version > 2U || (af_image_read_u32_le(dfd + 8U) >> 16U) != 88U ||
        af_image_read_u32_le(dfd + 12U) != (1U | (1U << 8U) | ((srgb ? 2U : 1U) << 16U)) ||
        af_image_read_u32_le(dfd + 16U) != 0U || af_image_read_u32_le(dfd + 20U) != 4U ||
        af_image_read_u32_le(dfd + 24U) != 0U) {
        return false;
    }
    for (uint32_t sample = 0U; sample < 4U; ++sample) {
        const uint8_t *record = dfd + 28U + sample * 16U;
        uint32_t channel = sample;
        if (bgra && (sample == 0U || sample == 2U)) {
            channel ^= 2U;
        }
        if (sample == 3U) {
            channel = srgb ? 31U : 15U;
        }
        if (af_image_read_u32_le(record) != (sample * 8U | (7U << 16U) | (channel << 24U)) ||
            af_image_read_u32_le(record + 4U) != 0U || af_image_read_u32_le(record + 8U) != 0U ||
            af_image_read_u32_le(record + 12U) != 255U) {
            return false;
        }
    }
    return true;
}

static bool af_image_preflight_png(const uint8_t *bytes, size_t size, uint32_t *out_width,
                                   uint32_t *out_height) {
    static const uint8_t signature[8] = {0x89U, 0x50U, 0x4EU, 0x47U, 0x0DU, 0x0AU, 0x1AU, 0x0AU};
    if (size < 33U || memcmp(bytes, signature, sizeof(signature)) != 0 ||
        af_image_read_u32_be(bytes + 8U) != 13U || memcmp(bytes + 12U, "IHDR", 4U) != 0) {
        return false;
    }
    *out_width = af_image_read_u32_be(bytes + 16U);
    *out_height = af_image_read_u32_be(bytes + 20U);
    return true;
}

static bool af_image_is_jpeg_sof(uint8_t marker) {
    return marker >= 0xC0U && marker <= 0xCFU && marker != 0xC4U && marker != 0xC8U &&
           marker != 0xCCU;
}

static bool af_image_preflight_jpeg(const uint8_t *bytes, size_t size, uint32_t *out_width,
                                    uint32_t *out_height) {
    size_t cursor = 2U;
    if (size < 4U || bytes[0] != 0xFFU || bytes[1] != 0xD8U) {
        return false;
    }
    while (cursor < size) {
        uint8_t marker = 0U;
        uint16_t segment_size = 0U;
        if (bytes[cursor] != 0xFFU) {
            return false;
        }
        while (cursor < size && bytes[cursor] == 0xFFU) {
            cursor += 1U;
        }
        if (cursor >= size) {
            return false;
        }
        marker = bytes[cursor++];
        if (marker == 0x00U) {
            return false;
        }
        if (marker == 0xD8U || marker == 0x01U || (marker >= 0xD0U && marker <= 0xD7U)) {
            continue;
        }
        if (marker == 0xD9U || marker == 0xDAU || size - cursor < 2U) {
            return false;
        }
        segment_size = af_image_read_u16_be(bytes + cursor);
        if (segment_size < 2U || (size_t)segment_size > size - cursor) {
            return false;
        }
        if (af_image_is_jpeg_sof(marker)) {
            if (segment_size < 8U) {
                return false;
            }
            *out_height = af_image_read_u16_be(bytes + cursor + 3U);
            *out_width = af_image_read_u16_be(bytes + cursor + 5U);
            return true;
        }
        cursor += segment_size;
    }
    return false;
}

static AfResult af_image_decode_stbi(const void *data, size_t size, AfImageSourceFormat format,
                                     AfAllocator *allocator, AfDecodedImage *out_image) {
    stbi_uc *decoded = NULL;
    uint8_t *pixels = NULL;
    int width = 0;
    int height = 0;
    int channels = 0;
    uint32_t info_width = 0U;
    uint32_t info_height = 0U;
    size_t pixel_size = 0U;
    if (size > INT_MAX) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    if (!((format == AF_IMAGE_SOURCE_PNG &&
           af_image_preflight_png(data, size, &info_width, &info_height)) ||
          (format == AF_IMAGE_SOURCE_JPEG &&
           af_image_preflight_jpeg(data, size, &info_width, &info_height))) ||
        !af_decoded_dimensions_valid(info_width, info_height, &pixel_size)) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    decoded = stbi_load_from_memory(data, (int)size, &width, &height, &channels, 4);
    (void)channels;
    if (decoded == NULL || width <= 0 || height <= 0 || (uint32_t)width != info_width ||
        (uint32_t)height != info_height) {
        stbi_image_free(decoded);
        return AF_RESULT_INVALID_PACKAGE;
    }
    pixels = af_allocate(allocator, pixel_size, alignof(uint8_t));
    if (pixels == NULL) {
        stbi_image_free(decoded);
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memcpy(pixels, decoded, pixel_size);
    stbi_image_free(decoded);
    *out_image = (AfDecodedImage){.pixels = pixels,
                                  .width = (uint32_t)width,
                                  .height = (uint32_t)height,
                                  .format = format,
                                  .color_space = AF_IMAGE_COLOR_SRGB,
                                  .allocator = allocator};
    return AF_RESULT_SUCCESS;
}

static AfResult af_image_decode_ktx2(const void *data, size_t size, AfAllocator *allocator,
                                     AfDecodedImage *out_image) {
    static const uint8_t identifier[12] = {0xABU, 0x4BU, 0x54U, 0x58U, 0x20U, 0x32U,
                                           0x30U, 0xBBU, 0x0DU, 0x0AU, 0x1AU, 0x0AU};
    const uint8_t *bytes = data;
    uint32_t format = 0U;
    uint32_t width = 0U;
    uint32_t height = 0U;
    uint32_t level_count = 0U;
    uint32_t dfd_offset = 0U;
    uint32_t dfd_size = 0U;
    uint32_t kvd_offset = 0U;
    uint32_t kvd_size = 0U;
    uint64_t level_index_end = 0U;
    uint64_t level_offset = 0U;
    size_t pixel_size = 0U;
    uint8_t *pixels = NULL;
    bool bgra = false;
    if (size < 104U || memcmp(bytes, identifier, sizeof(identifier)) != 0) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    format = af_image_read_u32_le(bytes + 12U);
    width = af_image_read_u32_le(bytes + 20U);
    height = af_image_read_u32_le(bytes + 24U);
    level_count = af_image_read_u32_le(bytes + 40U);
    dfd_offset = af_image_read_u32_le(bytes + 48U);
    dfd_size = af_image_read_u32_le(bytes + 52U);
    kvd_offset = af_image_read_u32_le(bytes + 56U);
    kvd_size = af_image_read_u32_le(bytes + 60U);
    level_index_end = 80U + (uint64_t)level_count * 24U;
    if ((format != 37U && format != 43U && format != 44U && format != 50U) ||
        af_image_read_u32_le(bytes + 16U) != 1U || af_image_read_u32_le(bytes + 28U) != 0U ||
        af_image_read_u32_le(bytes + 32U) != 0U || af_image_read_u32_le(bytes + 36U) != 1U ||
        level_count == 0U || level_count > AF_IMAGE_MAX_MIPS ||
        level_count > af_image_max_source_mips(width, height) ||
        af_image_read_u32_le(bytes + 44U) != 0U || level_index_end > size ||
        dfd_offset % 4U != 0U || dfd_offset < level_index_end || dfd_offset > size ||
        dfd_size > size - dfd_offset ||
        !af_image_ktx2_dfd_valid(bytes + dfd_offset, dfd_size, format) ||
        (kvd_size != 0U && (kvd_offset % 4U != 0U || kvd_offset < level_index_end ||
                            kvd_offset > size || kvd_size > size - kvd_offset)) ||
        (kvd_size != 0U && dfd_offset < (uint64_t)kvd_offset + kvd_size &&
         kvd_offset < (uint64_t)dfd_offset + dfd_size) ||
        af_image_read_u64_le(bytes + 64U) != 0U || af_image_read_u64_le(bytes + 72U) != 0U ||
        !af_decoded_dimensions_valid(width, height, &pixel_size)) {
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    for (uint32_t level = 0U; level < level_count; ++level) {
        const uint8_t *record = bytes + 80U + (size_t)level * 24U;
        const uint64_t offset = af_image_read_u64_le(record);
        const uint64_t length = af_image_read_u64_le(record + 8U);
        const uint64_t uncompressed = af_image_read_u64_le(record + 16U);
        const uint32_t mip_width = width >> level != 0U ? width >> level : 1U;
        const uint32_t mip_height = height >> level != 0U ? height >> level : 1U;
        const uint64_t expected = (uint64_t)mip_width * mip_height * 4U;
        if (offset % 4U != 0U || offset < level_index_end || offset > size || length != expected ||
            uncompressed != length || length > size - offset ||
            (offset < (uint64_t)dfd_offset + dfd_size && dfd_offset < offset + length) ||
            (kvd_size != 0U && offset < (uint64_t)kvd_offset + kvd_size &&
             kvd_offset < offset + length)) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        for (uint32_t other = 0U; other < level; ++other) {
            const uint8_t *candidate = bytes + 80U + (size_t)other * 24U;
            const uint64_t candidate_offset = af_image_read_u64_le(candidate);
            const uint64_t candidate_length = af_image_read_u64_le(candidate + 8U);
            if (offset < candidate_offset + candidate_length &&
                candidate_offset < offset + length) {
                return AF_RESULT_INVALID_PACKAGE;
            }
        }
    }
    level_offset = af_image_read_u64_le(bytes + 80U);
    pixels = af_allocate(allocator, pixel_size, alignof(uint8_t));
    if (pixels == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memcpy(pixels, bytes + level_offset, pixel_size);
    bgra = format == 44U || format == 50U;
    if (bgra) {
        for (size_t index = 0U; index < pixel_size; index += 4U) {
            const uint8_t red = pixels[index];
            pixels[index] = pixels[index + 2U];
            pixels[index + 2U] = red;
        }
    }
    *out_image = (AfDecodedImage){
        .pixels = pixels,
        .width = width,
        .height = height,
        .format = AF_IMAGE_SOURCE_KTX2,
        .color_space = format == 43U || format == 50U ? AF_IMAGE_COLOR_SRGB : AF_IMAGE_COLOR_LINEAR,
        .allocator = allocator};
    return AF_RESULT_SUCCESS;
}

AfResult af_image_decode_memory(const void *data, size_t size, AfImageSourceFormat format,
                                AfAllocator *allocator, AfDecodedImage *out_image) {
    if (data == NULL || size == 0U || size > AF_MAX_IMAGE_SOURCE_SIZE || allocator == NULL ||
        out_image == NULL || format > AF_IMAGE_SOURCE_SVG) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_image = (AfDecodedImage){0};
    if (format == AF_IMAGE_SOURCE_PNG || format == AF_IMAGE_SOURCE_JPEG) {
        return af_image_decode_stbi(data, size, format, allocator, out_image);
    }
    if (format == AF_IMAGE_SOURCE_KTX2) {
        return af_image_decode_ktx2(data, size, allocator, out_image);
    }
    return af_image_decode_svg(data, size, allocator, out_image);
}

static bool af_image_extension_format(const char *path, AfImageSourceFormat *out_format) {
    const char *extension = path == NULL ? NULL : strrchr(path, '.');
    if (extension == NULL) {
        return false;
    }
    if (SDL_strcasecmp(extension, ".png") == 0) {
        *out_format = AF_IMAGE_SOURCE_PNG;
    } else if (SDL_strcasecmp(extension, ".jpg") == 0 || SDL_strcasecmp(extension, ".jpeg") == 0) {
        *out_format = AF_IMAGE_SOURCE_JPEG;
    } else if (SDL_strcasecmp(extension, ".ktx2") == 0) {
        *out_format = AF_IMAGE_SOURCE_KTX2;
    } else if (SDL_strcasecmp(extension, ".svg") == 0) {
        *out_format = AF_IMAGE_SOURCE_SVG;
    } else {
        return false;
    }
    return true;
}

AfResult af_image_decode_file(const char *path, AfAllocator *allocator, AfDecodedImage *out_image) {
    AfFileData file = {0};
    AfImageSourceFormat format = AF_IMAGE_SOURCE_PNG;
    AfResult result = AF_RESULT_SUCCESS;
    if (!af_image_extension_format(path, &format)) {
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    result = af_file_read_limited(path, AF_MAX_IMAGE_SOURCE_SIZE, allocator, &file);
    if (result == AF_RESULT_SUCCESS) {
        result = af_image_decode_memory(file.data, file.size, format, allocator, out_image);
    }
    af_file_data_destroy(allocator, &file);
    return result;
}

void af_decoded_image_destroy(AfDecodedImage *image) {
    if (image != NULL) {
        af_deallocate(image->allocator, image->pixels);
        *image = (AfDecodedImage){0};
    }
}
