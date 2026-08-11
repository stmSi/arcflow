#ifndef AF_TEXT_H
#define AF_TEXT_H

#include "arcflow/arcflow_result.h"
#include "core/allocator.h"
#include "core/math.h"
#include "path/path.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AF_MAX_TEXT_FONTS 16U
#define AF_MAX_TEXT_FALLBACKS 8U
#define AF_MAX_TEXT_BYTES 4096U
#define AF_MAX_TEXT_GLYPHS 4096U
#define AF_MAX_TEXT_LINES 256U
#define AF_MAX_GLYPH_CACHE_ENTRIES 4096U
#define AF_TEXT_ATLAS_WIDTH 1024U
#define AF_TEXT_ATLAS_HEIGHT 1024U

typedef enum AfTextDirection {
    AF_TEXT_DIRECTION_AUTO = 0,
    AF_TEXT_DIRECTION_LTR,
    AF_TEXT_DIRECTION_RTL
} AfTextDirection;

typedef enum AfTextAlignment {
    AF_TEXT_ALIGN_START = 0,
    AF_TEXT_ALIGN_CENTER,
    AF_TEXT_ALIGN_END
} AfTextAlignment;

typedef enum AfTextWrapMode {
    AF_TEXT_WRAP_NONE = 0,
    AF_TEXT_WRAP_WORD,
    AF_TEXT_WRAP_CHARACTER
} AfTextWrapMode;

typedef struct AfTextRequest {
    const char *utf8;
    uint32_t byte_count;
    uint64_t font_ids[AF_MAX_TEXT_FALLBACKS];
    uint32_t font_count;
    float font_size;
    float maximum_width;
    float line_spacing;
    uint32_t maximum_lines;
    AfTextDirection direction;
    AfTextAlignment alignment;
    AfTextWrapMode wrap;
    bool kerning;
    bool ligatures;
    bool truncate;
} AfTextRequest;

typedef struct AfGlyphPlacement {
    uint64_t font_id;
    uint32_t glyph_id;
    uint32_t cluster;
    AfVec2 position;
    AfVec2 advance;
    AfVec2 bearing;
    AfVec2 bitmap_size;
    AfVec2 atlas_min;
    AfVec2 atlas_max;
} AfGlyphPlacement;

typedef struct AfTextLine {
    uint32_t glyph_start;
    uint32_t glyph_count;
    float width;
    float baseline;
} AfTextLine;

typedef struct AfTextLayout {
    AfGlyphPlacement *glyphs;
    uint32_t glyph_count;
    uint32_t glyph_capacity;
    AfTextLine *lines;
    uint32_t line_count;
    uint32_t line_capacity;
    AfVec2 bounds;
    bool truncated;
} AfTextLayout;

typedef struct AfTextRunCache {
    uint64_t request_hash;
    uint64_t generation;
    uint64_t rebuild_count;
    AfTextLayout layout;
} AfTextRunCache;

typedef struct AfTextEngine AfTextEngine;

AfResult af_text_engine_create(AfAllocator *allocator, AfTextEngine **out_engine);
void af_text_engine_destroy(AfTextEngine *engine);
AfResult af_text_font_load_memory(AfTextEngine *engine, uint64_t font_id, const void *data,
                                  size_t size, uint32_t face_index);
AfResult af_text_shape(AfTextEngine *engine, const AfTextRequest *request, AfTextLayout *layout);
AfResult af_text_glyph_outline(AfTextEngine *engine, uint64_t font_id, uint32_t glyph_id,
                               float font_size, AfVec2 baseline, AfPathCommand *commands,
                               uint32_t command_capacity, uint32_t *out_command_count);
AfResult af_text_run_cache_update(AfTextEngine *engine, const AfTextRequest *request,
                                  uint64_t generation, AfTextRunCache *cache);
const uint8_t *af_text_atlas_pixels(const AfTextEngine *engine, uint32_t *out_width,
                                    uint32_t *out_height, uint64_t *out_generation);

#endif
