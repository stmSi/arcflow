#include "text/text.h"

#include "core/hash.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>

#include <math.h>
#include <stdalign.h>
#include <stddef.h>
#include <string.h>

typedef struct AfTextFont {
    uint64_t id;
    const uint8_t *data;
    size_t size;
    FT_Face face;
    hb_font_t *font;
} AfTextFont;

typedef struct AfGlyphCacheEntry {
    uint64_t font_id;
    uint32_t glyph_id;
    uint32_t pixel_size;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    int16_t bearing_x;
    int16_t bearing_y;
} AfGlyphCacheEntry;

struct AfTextEngine {
    AfAllocator *allocator;
    FT_Library library;
    hb_buffer_t *buffer;
    AfTextFont fonts[AF_MAX_TEXT_FONTS];
    uint32_t font_count;
    AfGlyphCacheEntry glyphs[AF_MAX_GLYPH_CACHE_ENTRIES];
    uint32_t glyph_count;
    uint32_t atlas_x;
    uint32_t atlas_y;
    uint32_t atlas_row_height;
    uint8_t *atlas_pixels;
    uint64_t atlas_generation;
};

static AfTextFont *af_text_find_font(AfTextEngine *engine, uint64_t id) {
    if (engine == NULL || id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < engine->font_count; ++index) {
        if (engine->fonts[index].id == id) {
            return &engine->fonts[index];
        }
    }
    return NULL;
}

AfResult af_text_engine_create(AfAllocator *allocator, AfTextEngine **out_engine) {
    AfTextEngine *engine = NULL;
    if (allocator == NULL || out_engine == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_engine = NULL;
    engine = af_allocate(allocator, sizeof(*engine), alignof(AfTextEngine));
    if (engine == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memset(engine, 0, sizeof(*engine));
    engine->allocator = allocator;
    engine->atlas_pixels =
        af_allocate_array(allocator, AF_TEXT_ATLAS_WIDTH, AF_TEXT_ATLAS_HEIGHT, alignof(uint8_t));
    if (engine->atlas_pixels == NULL || FT_Init_FreeType(&engine->library) != 0) {
        af_text_engine_destroy(engine);
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memset(engine->atlas_pixels, 0, AF_TEXT_ATLAS_WIDTH * AF_TEXT_ATLAS_HEIGHT);
    engine->buffer = hb_buffer_create();
    if (engine->buffer == NULL || !hb_buffer_allocation_successful(engine->buffer) ||
        !hb_buffer_pre_allocate(engine->buffer, AF_MAX_TEXT_BYTES)) {
        af_text_engine_destroy(engine);
        return AF_RESULT_OUT_OF_MEMORY;
    }
    engine->atlas_generation = 1U;
    *out_engine = engine;
    return AF_RESULT_SUCCESS;
}

void af_text_engine_destroy(AfTextEngine *engine) {
    if (engine == NULL) {
        return;
    }
    for (uint32_t index = 0U; index < engine->font_count; ++index) {
        if (engine->fonts[index].font != NULL) {
            hb_font_destroy(engine->fonts[index].font);
        }
        if (engine->fonts[index].face != NULL) {
            (void)FT_Done_Face(engine->fonts[index].face);
        }
    }
    if (engine->buffer != NULL) {
        hb_buffer_destroy(engine->buffer);
    }
    if (engine->library != NULL) {
        (void)FT_Done_FreeType(engine->library);
    }
    af_deallocate(engine->allocator, engine->atlas_pixels);
    {
        AfAllocator *allocator = engine->allocator;
        af_deallocate(allocator, engine);
    }
}

AfResult af_text_font_load_memory(AfTextEngine *engine, uint64_t font_id, const void *data,
                                  size_t size, uint32_t face_index) {
    AfTextFont *font = NULL;
    if (engine == NULL || font_id == 0U || data == NULL || size == 0U || size > INT32_MAX ||
        face_index > INT32_MAX || engine->font_count >= AF_MAX_TEXT_FONTS ||
        af_text_find_font(engine, font_id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    font = &engine->fonts[engine->font_count];
    if (FT_New_Memory_Face(engine->library, data, (FT_Long)size, (FT_Long)face_index,
                           &font->face) != 0) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    font->font = hb_ft_font_create_referenced(font->face);
    if (font->font == NULL) {
        (void)FT_Done_Face(font->face);
        *font = (AfTextFont){0};
        return AF_RESULT_OUT_OF_MEMORY;
    }
    font->id = font_id;
    font->data = data;
    font->size = size;
    ++engine->font_count;
    return AF_RESULT_SUCCESS;
}

static bool af_text_request_valid(const AfTextEngine *engine, const AfTextRequest *request,
                                  const AfTextLayout *layout) {
    if (engine == NULL || request == NULL || layout == NULL || request->utf8 == NULL ||
        request->byte_count == 0U || request->byte_count > AF_MAX_TEXT_BYTES ||
        memchr(request->utf8, '\0', request->byte_count) != NULL || request->font_count == 0U ||
        request->font_count > AF_MAX_TEXT_FALLBACKS || !isfinite(request->font_size) ||
        !isfinite(request->maximum_width) || !isfinite(request->line_spacing) ||
        request->font_size <= 0.0F || request->font_size > 1024.0F ||
        request->maximum_width < 0.0F || request->maximum_width > 1000000.0F ||
        request->line_spacing <= 0.0F || request->line_spacing > 100.0F ||
        request->maximum_lines > AF_MAX_TEXT_LINES || request->direction > AF_TEXT_DIRECTION_RTL ||
        request->alignment > AF_TEXT_ALIGN_END || request->wrap > AF_TEXT_WRAP_CHARACTER ||
        layout->glyphs == NULL || layout->lines == NULL || layout->glyph_capacity == 0U ||
        layout->glyph_capacity > AF_MAX_TEXT_GLYPHS || layout->line_capacity == 0U ||
        layout->line_capacity > AF_MAX_TEXT_LINES) {
        return false;
    }
    for (uint32_t index = 0U; index < request->font_count; ++index) {
        if (request->font_ids[index] == 0U) {
            return false;
        }
    }
    return true;
}

static uint32_t af_text_utf8_codepoint(const char *text, uint32_t size, uint32_t offset) {
    if (offset >= size) {
        return UINT32_MAX;
    }
    const uint8_t first = (uint8_t)text[offset];
    if (first < 0x80U) {
        return first;
    }
    if ((first & 0xE0U) == 0xC0U && offset + 1U < size) {
        return ((uint32_t)(first & 0x1FU) << 6U) | ((uint32_t)(uint8_t)text[offset + 1U] & 0x3FU);
    }
    if ((first & 0xF0U) == 0xE0U && offset + 2U < size) {
        return ((uint32_t)(first & 0x0FU) << 12U) |
               (((uint32_t)(uint8_t)text[offset + 1U] & 0x3FU) << 6U) |
               ((uint32_t)(uint8_t)text[offset + 2U] & 0x3FU);
    }
    if ((first & 0xF8U) == 0xF0U && offset + 3U < size) {
        return ((uint32_t)(first & 0x07U) << 18U) |
               (((uint32_t)(uint8_t)text[offset + 1U] & 0x3FU) << 12U) |
               (((uint32_t)(uint8_t)text[offset + 2U] & 0x3FU) << 6U) |
               ((uint32_t)(uint8_t)text[offset + 3U] & 0x3FU);
    }
    return UINT32_MAX;
}

static bool af_text_utf8_valid(const char *text, uint32_t size) {
    uint32_t offset = 0U;
    while (offset < size) {
        const uint8_t first = (uint8_t)text[offset];
        uint32_t length = 0U;
        uint32_t codepoint = 0U;
        if (first < 0x80U) {
            ++offset;
            continue;
        }
        if (first >= 0xC2U && first <= 0xDFU) {
            length = 2U;
        } else if (first >= 0xE0U && first <= 0xEFU) {
            length = 3U;
        } else if (first >= 0xF0U && first <= 0xF4U) {
            length = 4U;
        } else {
            return false;
        }
        if (length > size - offset) {
            return false;
        }
        for (uint32_t index = 1U; index < length; ++index) {
            if (((uint8_t)text[offset + index] & 0xC0U) != 0x80U) {
                return false;
            }
        }
        codepoint = af_text_utf8_codepoint(text, size, offset);
        if (codepoint == UINT32_MAX || (length == 3U && codepoint < 0x800U) ||
            (length == 4U && codepoint < 0x10000U) ||
            (codepoint >= 0xD800U && codepoint <= 0xDFFFU) || codepoint > 0x10FFFFU) {
            return false;
        }
        offset += length;
    }
    return true;
}

static AfGlyphCacheEntry *af_text_cache_glyph(AfTextEngine *engine, AfTextFont *font,
                                              uint32_t glyph_id, uint32_t pixel_size) {
    FT_GlyphSlot slot = NULL;
    AfGlyphCacheEntry *entry = NULL;
    for (uint32_t index = 0U; index < engine->glyph_count; ++index) {
        entry = &engine->glyphs[index];
        if (entry->font_id == font->id && entry->glyph_id == glyph_id &&
            entry->pixel_size == pixel_size) {
            return entry;
        }
    }
    if (engine->glyph_count >= AF_MAX_GLYPH_CACHE_ENTRIES ||
        FT_Load_Glyph(font->face, glyph_id, FT_LOAD_DEFAULT) != 0 ||
        FT_Render_Glyph(font->face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
        return NULL;
    }
    slot = font->face->glyph;
    if (slot->bitmap.width > UINT16_MAX || slot->bitmap.rows > UINT16_MAX ||
        slot->bitmap.width > AF_TEXT_ATLAS_WIDTH || slot->bitmap.rows > AF_TEXT_ATLAS_HEIGHT) {
        return NULL;
    }
    if (engine->atlas_x + slot->bitmap.width + 1U > AF_TEXT_ATLAS_WIDTH) {
        engine->atlas_x = 0U;
        engine->atlas_y += engine->atlas_row_height + 1U;
        engine->atlas_row_height = 0U;
    }
    if (engine->atlas_y + slot->bitmap.rows + 1U > AF_TEXT_ATLAS_HEIGHT) {
        return NULL;
    }
    entry = &engine->glyphs[engine->glyph_count++];
    *entry = (AfGlyphCacheEntry){.font_id = font->id,
                                 .glyph_id = glyph_id,
                                 .pixel_size = pixel_size,
                                 .x = (uint16_t)engine->atlas_x,
                                 .y = (uint16_t)engine->atlas_y,
                                 .width = (uint16_t)slot->bitmap.width,
                                 .height = (uint16_t)slot->bitmap.rows,
                                 .bearing_x = (int16_t)slot->bitmap_left,
                                 .bearing_y = (int16_t)slot->bitmap_top};
    for (uint32_t row = 0U; row < slot->bitmap.rows; ++row) {
        const uint32_t source_row = slot->bitmap.pitch < 0 ? slot->bitmap.rows - row - 1U : row;
        const size_t source_pitch =
            (size_t)(slot->bitmap.pitch < 0 ? -(int64_t)slot->bitmap.pitch : slot->bitmap.pitch);
        memcpy(engine->atlas_pixels + (engine->atlas_y + row) * AF_TEXT_ATLAS_WIDTH +
                   engine->atlas_x,
               slot->bitmap.buffer + (size_t)source_row * source_pitch, slot->bitmap.width);
    }
    engine->atlas_x += slot->bitmap.width + 1U;
    engine->atlas_row_height =
        slot->bitmap.rows > engine->atlas_row_height ? slot->bitmap.rows : engine->atlas_row_height;
    ++engine->atlas_generation;
    return entry;
}

static AfTextFont *af_text_fallback_font(AfTextEngine *engine, const AfTextRequest *request,
                                         uint32_t codepoint, uint32_t *out_glyph) {
    for (uint32_t index = 1U; index < request->font_count; ++index) {
        AfTextFont *font = af_text_find_font(engine, request->font_ids[index]);
        const uint32_t glyph = font == NULL ? 0U : FT_Get_Char_Index(font->face, codepoint);
        if (glyph != 0U) {
            *out_glyph = glyph;
            return font;
        }
    }
    return NULL;
}

static void af_text_align_lines(const AfTextRequest *request, AfTextLayout *layout) {
    const float width = request->maximum_width > 0.0F ? request->maximum_width : layout->bounds.x;
    for (uint32_t line_index = 0U; line_index < layout->line_count; ++line_index) {
        AfTextLine *line = &layout->lines[line_index];
        float offset = 0.0F;
        if (request->alignment == AF_TEXT_ALIGN_CENTER) {
            offset = (width - line->width) * 0.5F;
        } else if (request->alignment == AF_TEXT_ALIGN_END) {
            offset = width - line->width;
        }
        for (uint32_t glyph_index = 0U; glyph_index < line->glyph_count; ++glyph_index) {
            layout->glyphs[line->glyph_start + glyph_index].position.x += offset;
        }
    }
}

AfResult af_text_shape(AfTextEngine *engine, const AfTextRequest *request, AfTextLayout *layout) {
    AfTextFont *primary = NULL;
    hb_glyph_info_t *infos = NULL;
    hb_glyph_position_t *positions = NULL;
    unsigned int count = 0U;
    hb_feature_t features[2];
    uint32_t feature_count = 0U;
    uint32_t pixel_size = 0U;
    float pen_x = 0.0F;
    float baseline = 0.0F;
    if (!af_text_request_valid(engine, request, layout) ||
        !af_text_utf8_valid(request->utf8, request->byte_count) ||
        (primary = af_text_find_font(engine, request->font_ids[0])) == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *layout = (AfTextLayout){.glyphs = layout->glyphs,
                             .glyph_capacity = layout->glyph_capacity,
                             .lines = layout->lines,
                             .line_capacity = layout->line_capacity};
    pixel_size = (uint32_t)ceilf(request->font_size);
    if (FT_Set_Pixel_Sizes(primary->face, 0U, pixel_size) != 0) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    hb_ft_font_changed(primary->font);
    hb_buffer_reset(engine->buffer);
    hb_buffer_add_utf8(engine->buffer, request->utf8, (int)request->byte_count, 0,
                       (int)request->byte_count);
    if (request->direction == AF_TEXT_DIRECTION_LTR) {
        hb_buffer_set_direction(engine->buffer, HB_DIRECTION_LTR);
    } else if (request->direction == AF_TEXT_DIRECTION_RTL) {
        hb_buffer_set_direction(engine->buffer, HB_DIRECTION_RTL);
    }
    hb_buffer_guess_segment_properties(engine->buffer);
    (void)hb_feature_from_string(request->kerning ? "kern=1" : "kern=0", -1,
                                 &features[feature_count++]);
    (void)hb_feature_from_string(request->ligatures ? "liga=1" : "liga=0", -1,
                                 &features[feature_count++]);
    hb_shape(primary->font, engine->buffer, features, feature_count);
    infos = hb_buffer_get_glyph_infos(engine->buffer, &count);
    positions = hb_buffer_get_glyph_positions(engine->buffer, &count);
    if (count > layout->glyph_capacity || count > AF_MAX_TEXT_GLYPHS ||
        !hb_buffer_allocation_successful(engine->buffer)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    layout->lines[0] = (AfTextLine){.baseline = request->font_size};
    layout->line_count = 1U;
    baseline = request->font_size;
    for (uint32_t index = 0U; index < count; ++index) {
        AfTextFont *font = primary;
        uint32_t glyph_id = infos[index].codepoint;
        const uint32_t codepoint =
            af_text_utf8_codepoint(request->utf8, request->byte_count, infos[index].cluster);
        float advance_x = (float)positions[index].x_advance / 64.0F;
        float advance_y = (float)positions[index].y_advance / 64.0F;
        AfGlyphCacheEntry *cache_entry = NULL;
        if (glyph_id == 0U && codepoint != UINT32_MAX) {
            AfTextFont *fallback = af_text_fallback_font(engine, request, codepoint, &glyph_id);
            if (fallback != NULL) {
                font = fallback;
                (void)FT_Set_Pixel_Sizes(font->face, 0U, pixel_size);
                if (FT_Load_Glyph(font->face, glyph_id, FT_LOAD_DEFAULT) == 0) {
                    advance_x = (float)font->face->glyph->advance.x / 64.0F;
                    advance_y = (float)font->face->glyph->advance.y / 64.0F;
                }
            }
        }
        if (request->wrap != AF_TEXT_WRAP_NONE && request->maximum_width > 0.0F &&
            pen_x + fabsf(advance_x) > request->maximum_width &&
            layout->lines[layout->line_count - 1U].glyph_count != 0U) {
            if ((request->maximum_lines != 0U && layout->line_count >= request->maximum_lines) ||
                layout->line_count >= layout->line_capacity) {
                layout->truncated = request->truncate;
                break;
            }
            layout->lines[layout->line_count - 1U].width = pen_x;
            pen_x = 0.0F;
            baseline += request->font_size * request->line_spacing;
            layout->lines[layout->line_count] =
                (AfTextLine){.glyph_start = layout->glyph_count, .baseline = baseline};
            ++layout->line_count;
        }
        cache_entry = af_text_cache_glyph(engine, font, glyph_id, pixel_size);
        if (cache_entry == NULL) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        layout->glyphs[layout->glyph_count] = (AfGlyphPlacement){
            .font_id = font->id,
            .glyph_id = glyph_id,
            .cluster = infos[index].cluster,
            .position = {pen_x + (float)positions[index].x_offset / 64.0F,
                         baseline - (float)positions[index].y_offset / 64.0F},
            .advance = {advance_x, advance_y},
            .bearing = {(float)cache_entry->bearing_x, (float)cache_entry->bearing_y},
            .bitmap_size = {(float)cache_entry->width, (float)cache_entry->height},
            .atlas_min = {(float)cache_entry->x / (float)AF_TEXT_ATLAS_WIDTH,
                          (float)cache_entry->y / (float)AF_TEXT_ATLAS_HEIGHT},
            .atlas_max = {(float)(cache_entry->x + cache_entry->width) / (float)AF_TEXT_ATLAS_WIDTH,
                          (float)(cache_entry->y + cache_entry->height) /
                              (float)AF_TEXT_ATLAS_HEIGHT}};
        ++layout->glyph_count;
        ++layout->lines[layout->line_count - 1U].glyph_count;
        pen_x += fabsf(advance_x);
    }
    layout->lines[layout->line_count - 1U].width = pen_x;
    for (uint32_t line = 0U; line < layout->line_count; ++line) {
        layout->bounds.x = fmaxf(layout->bounds.x, layout->lines[line].width);
    }
    layout->bounds.y = baseline + request->font_size * 0.25F;
    af_text_align_lines(request, layout);
    return AF_RESULT_SUCCESS;
}

static uint64_t af_text_request_hash(const AfTextRequest *request) {
    uint64_t hash = af_hash_bytes(request->utf8, request->byte_count);
    hash ^= af_hash_bytes(request->font_ids, request->font_count * sizeof(request->font_ids[0]));
    hash ^= af_hash_bytes(&request->font_size, sizeof(request->font_size));
    hash ^= af_hash_bytes(&request->maximum_width, sizeof(request->maximum_width));
    hash ^= af_hash_bytes(&request->line_spacing, sizeof(request->line_spacing));
    hash ^= af_hash_bytes(&request->maximum_lines, sizeof(request->maximum_lines));
    hash ^= ((uint64_t)(uint32_t)request->direction << 1U) ^
            ((uint64_t)(uint32_t)request->alignment << 5U) ^
            ((uint64_t)(uint32_t)request->wrap << 9U) ^ ((uint64_t)request->kerning << 13U) ^
            ((uint64_t)request->ligatures << 14U) ^ ((uint64_t)request->truncate << 15U);
    return hash;
}

AfResult af_text_run_cache_update(AfTextEngine *engine, const AfTextRequest *request,
                                  uint64_t generation, AfTextRunCache *cache) {
    uint64_t hash = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (engine == NULL || request == NULL || cache == NULL || generation == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    hash = af_text_request_hash(request);
    if (cache->generation == generation && cache->request_hash == hash) {
        return AF_RESULT_SUCCESS;
    }
    result = af_text_shape(engine, request, &cache->layout);
    if (result == AF_RESULT_SUCCESS) {
        cache->generation = generation;
        cache->request_hash = hash;
        ++cache->rebuild_count;
    }
    return result;
}

const uint8_t *af_text_atlas_pixels(const AfTextEngine *engine, uint32_t *out_width,
                                    uint32_t *out_height, uint64_t *out_generation) {
    if (engine == NULL) {
        return NULL;
    }
    if (out_width != NULL) {
        *out_width = AF_TEXT_ATLAS_WIDTH;
    }
    if (out_height != NULL) {
        *out_height = AF_TEXT_ATLAS_HEIGHT;
    }
    if (out_generation != NULL) {
        *out_generation = engine->atlas_generation;
    }
    return engine->atlas_pixels;
}
