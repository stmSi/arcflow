#include "editor/editor_text.h"
#include <math.h>
#include <stdalign.h>
#include <stdio.h>
#include <string.h>

static bool af_text_guide_frame(const AfFlattenedPath *flat, float distance, AfAffine2 *frame) {
    const AfPathContour contour = flat->contours[0];
    const uint32_t segments = contour.point_count - (contour.closed ? 0U : 1U);
    if (distance < 0)
        return false;
    for (uint32_t i = 0; i < segments; ++i) {
        const AfVec2 a = flat->points[i], b = flat->points[(i + 1U) % contour.point_count];
        const float length = hypotf(b.x - a.x, b.y - a.y);
        if (length < 1e-6F)
            continue;
        if (distance <= length) {
            const float dx = (b.x - a.x) / length, dy = (b.y - a.y) / length;
            *frame = (AfAffine2){.m00 = dx,
                                 .m01 = -dy,
                                 .m10 = dy,
                                 .m11 = dx,
                                 .tx = a.x + dx * distance,
                                 .ty = a.y + dy * distance};
            return true;
        }
        distance -= length;
    }
    return false;
}

AfResult af_editor_command_text(AfProject *project, AfTextEngine *engine, uint64_t font_id,
                                uint64_t id, AfVec2 position, const AfProjectText *text,
                                AfEditorCommand *command) {
    if (!project || !engine || !command || !af_project_text_valid(text) ||
        text->mode == AF_PROJECT_TEXT_NONE)
        return AF_RESULT_INVALID_ARGUMENT;
    *command = (AfEditorCommand){
        .type = AF_EDITOR_COMMAND_COMPOUND, .after_count = 1U, .path_after_count = 1U};
    AfProjectNodeSnapshot *snapshot = &command->after[0];
    const AfPath *original = NULL;
    if (id != 0) {
        AfResult result = af_project_snapshot(project, id, snapshot);
        if (result != AF_RESULT_SUCCESS)
            return result;
        if (snapshot->text.mode == AF_PROJECT_TEXT_NONE)
            return AF_RESULT_INVALID_ARGUMENT;
        original = af_path_find_node_const(&project->path_data, id);
        if (!original || original->geometry_generation == UINT64_MAX)
            return AF_RESULT_INVALID_ARGUMENT;
        for (uint32_t i = 0; i < project->track_count; ++i)
            if (project->tracks[i].node_id == id &&
                af_property_is_path(project->tracks[i].property))
                return AF_RESULT_UNSUPPORTED_FEATURE;
        command->before[0] = *snapshot;
        command->before_count = command->path_before_count = 1U;
        result = af_project_path_snapshot(project, original->id, command->path_before,
                                          command->path_command_before,
                                          AF_EDITOR_COMMAND_MAX_PATH_COMMANDS);
        if (result != AF_RESULT_SUCCESS)
            return result;
        command->path_command_before_count = original->command_count;
    } else {
        id = project->next_id;
        while (id < AF_INVALID_NODE_ID - 1U && (af_scene_find_const(&project->scene, id) ||
                                                af_path_find_const(&project->path_data, id) ||
                                                af_style_find_const(&project->style_data, id)))
            ++id;
        if (!id || id >= AF_INVALID_NODE_ID - 1U)
            return AF_RESULT_LIMIT_EXCEEDED;
        snapshot->node = (AfSceneNode){.id = id,
                                       .parent_id = AF_INVALID_NODE_ID,
                                       .type = AF_NODE_PATH,
                                       .local_transform = {.position = position, .scale = {1, 1}},
                                       .bounds = {1, 1},
                                       .fill = {0.9F, 0.94F, 1.0F, 1},
                                       .opacity = 1,
                                       .visible = true};
        snapshot->style = af_style_default(id, id, snapshot->node.fill);
        snapshot->style.stroke.enabled = true;
        snapshot->layout = af_layout_node_default(id);
        snapshot->has_layout = true;
        snapshot->index = project->scene.count;
        (void)snprintf(snapshot->name, sizeof(snapshot->name), "Text %llu", (unsigned long long)id);
    }
    command->target_id = id;
    snapshot->text = *text;
    AfGlyphPlacement glyphs[AF_PROJECT_TEXT_BYTES];
    AfTextLine lines[AF_MAX_TEXT_LINES];
    AfTextLayout layout = {.glyphs = glyphs,
                           .glyph_capacity = AF_PROJECT_TEXT_BYTES,
                           .lines = lines,
                           .line_capacity = AF_MAX_TEXT_LINES};
    const AfTextRequest request = {
        .utf8 = text->utf8,
        .byte_count = (uint32_t)strlen(text->utf8),
        .font_ids = {font_id},
        .font_count = 1U,
        .font_size = text->font_size,
        .maximum_width = text->mode == AF_PROJECT_TEXT_PARAGRAPH ? text->width : 0,
        .maximum_lines = AF_MAX_TEXT_LINES,
        .line_spacing = text->line_spacing > 0 ? text->line_spacing : 1.2F,
        .letter_spacing = text->letter_spacing,
        .alignment = text->mode == AF_PROJECT_TEXT_ON_PATH ? AF_TEXT_ALIGN_START
                                                           : (AfTextAlignment)text->alignment,
        .wrap = text->mode == AF_PROJECT_TEXT_PARAGRAPH ? AF_TEXT_WRAP_WORD : AF_TEXT_WRAP_NONE,
        .kerning = true,
        .ligatures = text->letter_spacing == 0};
    AfResult result = af_text_shape(engine, &request, &layout);
    if (result != AF_RESULT_SUCCESS)
        return result;
    if (layout.truncated)
        return AF_RESULT_LIMIT_EXCEEDED;
    AfVec2 points[AF_MAX_PATH_FLATTENED_POINTS];
    AfPathContour contours[AF_MAX_PATH_CONTOURS];
    AfFlattenedPath guide = {.points = points,
                             .point_capacity = AF_MAX_PATH_FLATTENED_POINTS,
                             .contours = contours,
                             .contour_capacity = AF_MAX_PATH_CONTOURS};
    float path_start = text->offset;
    if (text->mode == AF_PROJECT_TEXT_ON_PATH) {
        if (layout.line_count != 1U)
            return AF_RESULT_UNSUPPORTED_FEATURE;
        AfPath path = {
            .id = 1, .node_id = 1, .geometry_generation = 1, .command_count = text->guide_count};
        AfPathData data = {.paths = &path,
                           .path_count = 1,
                           .path_capacity = 1,
                           .commands = (AfPathCommand *)text->guide,
                           .command_count = text->guide_count,
                           .command_capacity = AF_PROJECT_TEXT_GUIDE_COMMANDS};
        result = af_path_flatten(&data, &path, .1F, &guide);
        if (result != AF_RESULT_SUCCESS)
            return result;
        float length = 0;
        const AfPathContour contour = guide.contours[0];
        for (uint32_t i = 0; i < contour.point_count - (contour.closed ? 0U : 1U); ++i) {
            const AfVec2 a = points[i], b = points[(i + 1U) % contour.point_count];
            length += hypotf(b.x - a.x, b.y - a.y);
        }
        path_start += (length - layout.bounds.x) * (text->alignment == 1   ? .5F
                                                    : text->alignment == 2 ? 1
                                                                           : 0);
        AfAffine2 endpoint;
        if (!af_text_guide_frame(&guide, path_start, &endpoint) ||
            !af_text_guide_frame(&guide, path_start + layout.bounds.x, &endpoint))
            return AF_RESULT_LIMIT_EXCEEDED;
    }
    uint32_t count = 0U;
    for (uint32_t i = 0; i < layout.glyph_count; ++i) {
        const AfGlyphPlacement glyph = glyphs[i];
        if (glyph.glyph_id == 0)
            return AF_RESULT_UNSUPPORTED_FEATURE;
        AfVec2 baseline = glyph.position;
        AfAffine2 frame = af_affine_identity();
        if (text->mode == AF_PROJECT_TEXT_ON_PATH) {
            const float center = glyph.position.x + glyph.advance.x * .5F;
            if (!af_text_guide_frame(&guide, path_start + center, &frame))
                return AF_RESULT_LIMIT_EXCEEDED;
            baseline = (AfVec2){-glyph.advance.x * .5F, glyph.position.y - lines[0].baseline};
        }
        uint32_t added = 0;
        result = af_text_glyph_outline(engine, glyph.font_id, glyph.glyph_id, text->font_size,
                                       baseline, command->path_command_after + count,
                                       AF_EDITOR_COMMAND_MAX_PATH_COMMANDS - count, &added);
        if (result != AF_RESULT_SUCCESS)
            return result;
        if (text->mode == AF_PROJECT_TEXT_ON_PATH)
            for (uint32_t j = count; j < count + added; ++j) {
                AfPathCommand *c = &command->path_command_after[j];
                if (c->type != AF_PATH_CLOSE)
                    c->point = af_affine_transform_point(frame, c->point);
                if (c->type == AF_PATH_CUBIC) {
                    c->control1 = af_affine_transform_point(frame, c->control1);
                    c->control2 = af_affine_transform_point(frame, c->control2);
                }
            }
        count += added;
    }
    if (!count)
        return AF_RESULT_INVALID_ARGUMENT;
    AfVec2 bounds = {fmaxf(layout.bounds.x, 1), fmaxf(layout.bounds.y, 1)};
    if (text->mode == AF_PROJECT_TEXT_PARAGRAPH)
        bounds.x = text->width;
    if (text->mode == AF_PROJECT_TEXT_ON_PATH) {
        bounds = (AfVec2){1, 1};
        for (uint32_t i = 0; i < count; ++i) {
            bounds.x = fmaxf(bounds.x, command->path_command_after[i].point.x);
            bounds.y = fmaxf(bounds.y, command->path_command_after[i].point.y);
        }
    }
    snapshot->node.bounds = bounds;
    snapshot->layout.width = (AfLayoutValue){AF_LAYOUT_VALUE_FIXED, bounds.x};
    snapshot->layout.height = (AfLayoutValue){AF_LAYOUT_VALUE_FIXED, bounds.y};
    command->path_after[0] = (AfProjectPathSnapshot){
        .path = {.id = original ? original->id : id,
                 .node_id = id,
                 .geometry_generation = original ? original->geometry_generation + 1U : 1U,
                 .command_count = count,
                 .fill_rule = AF_PATH_FILL_NON_ZERO},
        .index = original ? command->path_before[0].index : project->path_data.path_count};
    command->path_command_after_count = count;
    /* Reject text that cannot fit the runtime tessellator before committing metadata. */
    AfPath render_path = command->path_after[0].path;
    AfPathData render_data = {.paths = &render_path,
                              .path_count = 1,
                              .path_capacity = 1,
                              .commands = command->path_command_after,
                              .command_count = count,
                              .command_capacity = count};
    AfPathTessellationScratch scratch;
    AfVec2 *vertices = af_allocate_array(project->allocator, AF_MAX_PATH_TESSELLATED_VERTICES,
                                         sizeof(*vertices), alignof(AfVec2));
    if (!vertices)
        return AF_RESULT_OUT_OF_MEMORY;
    uint32_t vertex_count = 0;
    result = af_path_tessellate(&render_data, &render_path, .25F, &scratch, vertices,
                                AF_MAX_PATH_TESSELLATED_VERTICES, &vertex_count);
    af_deallocate(project->allocator, vertices);
    return result;
}
