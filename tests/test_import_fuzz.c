#include "af/core/allocator.h"
#include "af/core/file_io.h"
#include "af/core/hash.h"
#include "af/text/text.h"
#include "project/image_import.h"
#include "project/interchange.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AF_FUZZ_IMAGE_CASES_PER_FORMAT 256U
#define AF_FUZZ_EASING_CASES 256U
#define AF_FUZZ_FONT_CASES 128U
#define AF_FUZZ_MUTATION_SLACK 32U
#define AF_FUZZ_SEED UINT64_C(0x494d504f5254465a)

typedef struct AfFuzzStats {
    uint64_t case_count;
    uint64_t accepted_count;
    uint64_t rejected_count;
    uint64_t decoded_count;
} AfFuzzStats;

typedef struct AfFuzzImageSeed {
    const uint8_t *data;
    size_t size;
    AfImageSourceFormat format;
    const char *name;
} AfFuzzImageSeed;

static uint64_t af_fuzz_next(uint64_t *state) {
    uint64_t value = *state;
    value ^= value >> 12U;
    value ^= value << 25U;
    value ^= value >> 27U;
    *state = value;
    return value * UINT64_C(2685821657736338717);
}

static size_t af_fuzz_min_size(size_t left, size_t right) { return left < right ? left : right; }

static void af_fuzz_mutate(const uint8_t *input, size_t size, uint8_t *output, size_t capacity,
                           uint32_t case_index, uint64_t *random_state, size_t *out_size) {
    const uint32_t mode = case_index % 8U;
    size_t position = 0U;
    size_t length = 0U;
    memcpy(output, input, size);
    *out_size = size;
    if (size == 0U) {
        return;
    }
    position = (size_t)(af_fuzz_next(random_state) % (uint64_t)size);
    switch (mode) {
    case 0U:
        output[position] ^= (uint8_t)(1U << (af_fuzz_next(random_state) & 7U));
        break;
    case 1U:
        output[position] = (uint8_t)af_fuzz_next(random_state);
        break;
    case 2U:
        *out_size = (size_t)(af_fuzz_next(random_state) % ((uint64_t)size + 1U));
        break;
    case 3U:
        length = 1U + (size_t)(af_fuzz_next(random_state) % 16U);
        length = af_fuzz_min_size(length, size - position);
        length = af_fuzz_min_size(length, capacity - size);
        memcpy(output + size, output + position, length);
        *out_size = size + length;
        break;
    case 4U:
        length = 1U + (size_t)(af_fuzz_next(random_state) % 16U);
        length = af_fuzz_min_size(length, size - position);
        memset(output + position, 0, length);
        break;
    case 5U:
        length = 1U + (size_t)(af_fuzz_next(random_state) % 16U);
        length = af_fuzz_min_size(length, size - position);
        memmove(output + position, output + position + length, size - position - length);
        *out_size = size - length;
        break;
    case 6U:
        length = af_fuzz_min_size(8U, size - position);
        memset(output + position, (case_index & 8U) == 0U ? 0xff : 0x80, length);
        break;
    case 7U:
        output[position] = (uint8_t)((case_index & 8U) == 0U ? '\0' : '\n');
        break;
    default:
        break;
    }
}

static void af_fuzz_write_u32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    bytes[2] = (uint8_t)(value >> 16U);
    bytes[3] = (uint8_t)(value >> 24U);
}

static void af_fuzz_write_u64(uint8_t *bytes, uint64_t value) {
    af_fuzz_write_u32(bytes, (uint32_t)value);
    af_fuzz_write_u32(bytes + 4U, (uint32_t)(value >> 32U));
}

static void af_fuzz_write_ktx2_dfd(uint8_t *bytes) {
    af_fuzz_write_u32(bytes, 92U);
    af_fuzz_write_u32(bytes + 8U, 2U | (88U << 16U));
    af_fuzz_write_u32(bytes + 12U, 1U | (1U << 8U) | (1U << 16U));
    af_fuzz_write_u32(bytes + 20U, 4U);
    for (uint32_t channel = 0U; channel < 4U; ++channel) {
        const uint32_t channel_id = channel == 3U ? 15U : channel;
        af_fuzz_write_u32(bytes + 28U + channel * 16U,
                          channel * 8U | (7U << 16U) | (channel_id << 24U));
        af_fuzz_write_u32(bytes + 40U + channel * 16U, 255U);
    }
}

static void af_fuzz_make_ktx2(uint8_t bytes[260]) {
    static const uint8_t identifier[12] = {0xABU, 0x4BU, 0x54U, 0x58U, 0x20U, 0x32U,
                                           0x30U, 0xBBU, 0x0DU, 0x0AU, 0x1AU, 0x0AU};
    memset(bytes, 0, 260U);
    memcpy(bytes, identifier, sizeof(identifier));
    af_fuzz_write_u32(bytes + 12U, 37U);
    af_fuzz_write_u32(bytes + 16U, 1U);
    af_fuzz_write_u32(bytes + 20U, 4U);
    af_fuzz_write_u32(bytes + 24U, 4U);
    af_fuzz_write_u32(bytes + 36U, 1U);
    af_fuzz_write_u32(bytes + 40U, 1U);
    af_fuzz_write_u32(bytes + 48U, 104U);
    af_fuzz_write_u32(bytes + 52U, 92U);
    af_fuzz_write_u64(bytes + 80U, 196U);
    af_fuzz_write_u64(bytes + 88U, 64U);
    af_fuzz_write_u64(bytes + 96U, 64U);
    af_fuzz_write_ktx2_dfd(bytes + 104U);
    memset(bytes + 196U, 255, 64U);
}

static bool af_fuzz_image_result_expected(AfResult result) {
    return result == AF_RESULT_SUCCESS || result == AF_RESULT_INVALID_ARGUMENT ||
           result == AF_RESULT_LIMIT_EXCEEDED || result == AF_RESULT_INVALID_PACKAGE ||
           result == AF_RESULT_UNSUPPORTED_FEATURE;
}

static bool af_fuzz_image_valid(const AfDecodedImage *image, AfImageSourceFormat format) {
    return image->pixels != NULL && image->width > 0U && image->height > 0U &&
           image->width <= AF_MAX_IMAGE_DIMENSION && image->height <= AF_MAX_IMAGE_DIMENSION &&
           (uint64_t)image->width * image->height <= AF_MAX_IMAGE_PIXELS &&
           image->format == format && image->color_space <= AF_IMAGE_COLOR_LINEAR &&
           image->allocator != NULL;
}

static bool af_fuzz_images(const AfFuzzImageSeed *seed, AfAllocator *allocator,
                           uint64_t *random_state, AfFuzzStats *stats) {
    uint8_t *mutation = malloc(seed->size + AF_FUZZ_MUTATION_SLACK);
    if (mutation == NULL) {
        return false;
    }
    for (uint32_t case_index = 0U; case_index < AF_FUZZ_IMAGE_CASES_PER_FORMAT; ++case_index) {
        AfDecodedImage decoded = {0};
        size_t mutation_size = 0U;
        AfResult result = AF_RESULT_SUCCESS;
        af_fuzz_mutate(seed->data, seed->size, mutation, seed->size + AF_FUZZ_MUTATION_SLACK,
                       case_index, random_state, &mutation_size);
        result = af_image_decode_memory(mutation, mutation_size, seed->format, allocator, &decoded);
        ++stats->case_count;
        if (!af_fuzz_image_result_expected(result)) {
            af_decoded_image_destroy(&decoded);
            free(mutation);
            return false;
        }
        if (result == AF_RESULT_SUCCESS) {
            ++stats->accepted_count;
            if (!af_fuzz_image_valid(&decoded, seed->format)) {
                af_decoded_image_destroy(&decoded);
                free(mutation);
                return false;
            }
            ++stats->decoded_count;
        } else {
            ++stats->rejected_count;
        }
        af_decoded_image_destroy(&decoded);
    }
    free(mutation);
    return true;
}

static bool af_fuzz_easing_valid(const AfInterchangeEasing *easing) {
    if (easing->kind > AF_INTERCHANGE_EASING_CUBIC_BEZIER) {
        return false;
    }
    for (uint32_t index = 0U; index < 4U; ++index) {
        if (!isfinite(easing->control_points[index])) {
            return false;
        }
    }
    return easing->kind != AF_INTERCHANGE_EASING_CUBIC_BEZIER ||
           (easing->control_points[0] >= 0.0F && easing->control_points[0] <= 1.0F &&
            easing->control_points[2] >= 0.0F && easing->control_points[2] <= 1.0F);
}

static bool af_fuzz_easing(uint64_t *random_state, AfFuzzStats *stats) {
    static const char *const seeds[] = {"linear", "ease-in-out",
                                        "cubic-bezier(0.25, 0.1, 0.75, 1.0)"};
    uint8_t mutation[AF_INTERCHANGE_MAX_EASING_TEXT + AF_FUZZ_MUTATION_SLACK];
    for (uint32_t case_index = 0U; case_index < AF_FUZZ_EASING_CASES; ++case_index) {
        const char *seed = seeds[case_index % (sizeof(seeds) / sizeof(seeds[0]))];
        const size_t seed_size = strlen(seed);
        AfInterchangeDiagnostics diagnostics = {0};
        AfInterchangeEasing easing = {0};
        size_t mutation_size = 0U;
        AfResult result = AF_RESULT_SUCCESS;
        af_fuzz_mutate((const uint8_t *)seed, seed_size, mutation, sizeof(mutation), case_index,
                       random_state, &mutation_size);
        result = af_interchange_parse_easing((const char *)mutation, mutation_size, &easing,
                                             &diagnostics);
        ++stats->case_count;
        if (result != AF_RESULT_SUCCESS && result != AF_RESULT_INVALID_ARGUMENT) {
            return false;
        }
        if (result == AF_RESULT_SUCCESS) {
            ++stats->accepted_count;
            if (!af_fuzz_easing_valid(&easing)) {
                return false;
            }
            ++stats->decoded_count;
        } else {
            ++stats->rejected_count;
            if (diagnostics.error_count == 0U) {
                return false;
            }
        }
    }
    return true;
}

static bool af_fuzz_font(const AfFileData *seed, AfAllocator *allocator, uint64_t *random_state,
                         AfFuzzStats *stats) {
    uint8_t *mutation = malloc(seed->size + AF_FUZZ_MUTATION_SLACK);
    if (mutation == NULL) {
        return false;
    }
    for (uint32_t case_index = 0U; case_index < AF_FUZZ_FONT_CASES; ++case_index) {
        AfTextEngine *engine = NULL;
        AfGlyphPlacement glyphs[8];
        AfTextLine lines[2];
        AfTextLayout layout = {
            .glyphs = glyphs, .glyph_capacity = 8U, .lines = lines, .line_capacity = 2U};
        const AfTextRequest request = {.utf8 = "A",
                                       .byte_count = 1U,
                                       .font_ids = {1U},
                                       .font_count = 1U,
                                       .font_size = 16.0F,
                                       .line_spacing = 1.0F,
                                       .direction = AF_TEXT_DIRECTION_LTR,
                                       .alignment = AF_TEXT_ALIGN_START,
                                       .wrap = AF_TEXT_WRAP_NONE,
                                       .kerning = true,
                                       .ligatures = true};
        size_t mutation_size = 0U;
        AfResult result = AF_RESULT_SUCCESS;
        af_fuzz_mutate(seed->data, seed->size, mutation, seed->size + AF_FUZZ_MUTATION_SLACK,
                       case_index, random_state, &mutation_size);
        if (af_text_engine_create(allocator, &engine) != AF_RESULT_SUCCESS) {
            free(mutation);
            return false;
        }
        result = af_text_font_load_memory(engine, 1U, mutation, mutation_size, 0U);
        ++stats->case_count;
        if (result != AF_RESULT_SUCCESS && result != AF_RESULT_INVALID_ARGUMENT &&
            result != AF_RESULT_INVALID_PACKAGE) {
            af_text_engine_destroy(engine);
            free(mutation);
            return false;
        }
        if (result == AF_RESULT_SUCCESS) {
            ++stats->accepted_count;
            result = af_text_shape(engine, &request, &layout);
            if (result != AF_RESULT_SUCCESS && result != AF_RESULT_INVALID_ARGUMENT &&
                result != AF_RESULT_LIMIT_EXCEEDED) {
                af_text_engine_destroy(engine);
                free(mutation);
                return false;
            }
            if (result == AF_RESULT_SUCCESS) {
                ++stats->decoded_count;
            }
        } else {
            ++stats->rejected_count;
        }
        af_text_engine_destroy(engine);
    }
    free(mutation);
    return true;
}

static uint64_t af_fuzz_hash_combine(uint64_t hash, const void *data, size_t size) {
    return hash ^
           (af_hash_bytes(data, size) + UINT64_C(0x9e3779b97f4a7c15) + (hash << 6U) + (hash >> 2U));
}

int main(void) {
    static const char svg[] =
        "<svg width=\"16\" height=\"12\" viewBox=\"0 0 16 12\">"
        "<path d=\"M1 10 L4 2 C6 1 8 1 10 4 Q12 7 15 2 Z\" fill=\"#31a8ffcc\"/>"
        "<polyline points=\"1,11 5,7 9,11 15,6\" fill=\"#ff8040\"/>"
        "</svg>";
    static const uint8_t jpeg[] = {
        0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00,
        0x01, 0x00, 0x01, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43, 0x00, 0x06, 0x04, 0x05, 0x06, 0x05,
        0x04, 0x06, 0x06, 0x05, 0x06, 0x07, 0x07, 0x06, 0x08, 0x0a, 0x10, 0x0a, 0x0a, 0x09, 0x09,
        0x0a, 0x14, 0x0e, 0x0f, 0x0c, 0x10, 0x17, 0x14, 0x18, 0x18, 0x17, 0x14, 0x16, 0x16, 0x1a,
        0x1d, 0x25, 0x1f, 0x1a, 0x1b, 0x23, 0x1c, 0x16, 0x16, 0x20, 0x2c, 0x20, 0x23, 0x26, 0x27,
        0x29, 0x2a, 0x29, 0x19, 0x1f, 0x2d, 0x30, 0x2d, 0x28, 0x30, 0x25, 0x28, 0x29, 0x28, 0xff,
        0xdb, 0x00, 0x43, 0x01, 0x07, 0x07, 0x07, 0x0a, 0x08, 0x0a, 0x13, 0x0a, 0x0a, 0x13, 0x28,
        0x1a, 0x16, 0x1a, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
        0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
        0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
        0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0xff, 0xc0, 0x00, 0x11, 0x08, 0x00, 0x02,
        0x00, 0x02, 0x03, 0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xc4, 0x00,
        0x15, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x05, 0xff, 0xc4, 0x00, 0x14, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc4, 0x00,
        0x14, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x07, 0xff, 0xc4, 0x00, 0x14, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xda, 0x00, 0x0c,
        0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f, 0x00, 0xa6, 0x01, 0x24, 0x64, 0xff,
        0xd9};
    static const char *const easing_seeds[] = {"linear", "ease-in-out",
                                               "cubic-bezier(0.25, 0.1, 0.75, 1.0)"};
    AfAllocator allocator = {0};
    AfFileData png = {0};
    AfFileData font = {0};
    uint8_t ktx2[260];
    AfFuzzStats image_stats[4] = {0};
    AfFuzzStats easing_stats = {0};
    AfFuzzStats font_stats = {0};
    uint64_t random_state = AF_FUZZ_SEED;
    uint64_t workload_hash = 0U;
    bool passed = true;
    af_allocator_init_default(&allocator);
    af_fuzz_make_ktx2(ktx2);
    if (af_file_read_limited(ARCFLOW_FUZZ_PNG, 1024U * 1024U, &allocator, &png) !=
            AF_RESULT_SUCCESS ||
        af_file_read_limited(ARCFLOW_FUZZ_FONT, 1024U * 1024U, &allocator, &font) !=
            AF_RESULT_SUCCESS) {
        passed = false;
    }
    if (passed) {
        const AfFuzzImageSeed image_seeds[] = {
            {.data = png.data, .size = png.size, .format = AF_IMAGE_SOURCE_PNG, .name = "png"},
            {.data = jpeg, .size = sizeof(jpeg), .format = AF_IMAGE_SOURCE_JPEG, .name = "jpeg"},
            {.data = ktx2, .size = sizeof(ktx2), .format = AF_IMAGE_SOURCE_KTX2, .name = "ktx2"},
            {.data = (const uint8_t *)svg,
             .size = sizeof(svg) - 1U,
             .format = AF_IMAGE_SOURCE_SVG,
             .name = "svg"}};
        for (uint32_t index = 0U; passed && index < 4U; ++index) {
            workload_hash = af_fuzz_hash_combine(workload_hash, image_seeds[index].data,
                                                 image_seeds[index].size);
            passed =
                af_fuzz_images(&image_seeds[index], &allocator, &random_state, &image_stats[index]);
        }
        workload_hash = af_fuzz_hash_combine(workload_hash, font.data, font.size);
        for (uint32_t index = 0U; index < 3U; ++index) {
            workload_hash = af_fuzz_hash_combine(workload_hash, easing_seeds[index],
                                                 strlen(easing_seeds[index]));
        }
        passed = passed && af_fuzz_easing(&random_state, &easing_stats) &&
                 af_fuzz_font(&font, &allocator, &random_state, &font_stats);
    }
    {
        AfInterchangeDiagnostics diagnostics = {0};
        passed =
            passed &&
            af_interchange_check_import_support(AF_INTERCHANGE_FORMAT_LOTTIE_JSON, &diagnostics) ==
                AF_RESULT_UNSUPPORTED_FEATURE &&
            diagnostics.warning_count == 1U;
    }
    for (uint32_t index = 0U; index < 4U; ++index) {
        passed = passed && image_stats[index].case_count == AF_FUZZ_IMAGE_CASES_PER_FORMAT &&
                 image_stats[index].accepted_count != 0U && image_stats[index].rejected_count != 0U;
    }
    passed = passed && easing_stats.case_count == AF_FUZZ_EASING_CASES &&
             easing_stats.accepted_count != 0U && easing_stats.rejected_count != 0U &&
             font_stats.case_count == AF_FUZZ_FONT_CASES && font_stats.accepted_count != 0U &&
             font_stats.rejected_count != 0U;
    (void)fprintf(stdout,
                  "{\"seed\":\"%016llx\",\"workload_hash\":\"%016llx\","
                  "\"image_cases_per_format\":%u,"
                  "\"png\":{\"accepted\":%llu,\"rejected\":%llu},"
                  "\"jpeg\":{\"accepted\":%llu,\"rejected\":%llu},"
                  "\"ktx2\":{\"accepted\":%llu,\"rejected\":%llu},"
                  "\"svg\":{\"accepted\":%llu,\"rejected\":%llu},"
                  "\"easing\":{\"cases\":%llu,\"accepted\":%llu,\"rejected\":%llu},"
                  "\"font\":{\"cases\":%llu,\"accepted\":%llu,\"rejected\":%llu,"
                  "\"shaped\":%llu},\"lottie_supported\":false,\"passed\":%s}\n",
                  (unsigned long long)AF_FUZZ_SEED, (unsigned long long)workload_hash,
                  AF_FUZZ_IMAGE_CASES_PER_FORMAT, (unsigned long long)image_stats[0].accepted_count,
                  (unsigned long long)image_stats[0].rejected_count,
                  (unsigned long long)image_stats[1].accepted_count,
                  (unsigned long long)image_stats[1].rejected_count,
                  (unsigned long long)image_stats[2].accepted_count,
                  (unsigned long long)image_stats[2].rejected_count,
                  (unsigned long long)image_stats[3].accepted_count,
                  (unsigned long long)image_stats[3].rejected_count,
                  (unsigned long long)easing_stats.case_count,
                  (unsigned long long)easing_stats.accepted_count,
                  (unsigned long long)easing_stats.rejected_count,
                  (unsigned long long)font_stats.case_count,
                  (unsigned long long)font_stats.accepted_count,
                  (unsigned long long)font_stats.rejected_count,
                  (unsigned long long)font_stats.decoded_count, passed ? "true" : "false");
    af_file_data_destroy(&allocator, &font);
    af_file_data_destroy(&allocator, &png);
    return passed ? 0 : 1;
}
