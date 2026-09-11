#include "layout/layout.h"

#include "core/id_index.h"
#include "scene/scene.h"

#include <math.h>
#include <stddef.h>

#define AF_LAYOUT_LIMIT 1000000.0F

typedef struct AfLayoutContext {
    AfScene *scene;
    const AfLayoutData *data;
    AfAffine2 canvas_transform;
    AfVec2 root_origin;
    AfVec2 root_size;
    uint8_t states[AF_MAX_LAYOUT_NODES];
    AfIdIndex node_lookup;
    uint32_t node_slots[AF_MAX_NODES * 2U];
    const AfLayoutNode *node_layouts[AF_MAX_NODES];
    uint32_t first_child[AF_MAX_NODES];
    uint32_t next_sibling[AF_MAX_NODES];
    float child_offsets[AF_MAX_NODES];
    bool children_prepared[AF_MAX_NODES];
} AfLayoutContext;

typedef struct AfLayoutRect {
    AfVec2 position;
    AfVec2 size;
} AfLayoutRect;

AfLayoutNode af_layout_node_default(uint64_t node_id) {
    return (AfLayoutNode){.node_id = node_id,
                          .anchor_min = {0.0F, 0.0F},
                          .anchor_max = {0.0F, 0.0F},
                          .pivot = {0.0F, 0.0F},
                          .width = {.mode = AF_LAYOUT_VALUE_INTRINSIC},
                          .height = {.mode = AF_LAYOUT_VALUE_INTRINSIC},
                          .maximum_size = {AF_LAYOUT_LIMIT, AF_LAYOUT_LIMIT},
                          .child_alignment = AF_LAYOUT_ALIGN_START};
}

const AfLayoutNode *af_layout_find_const(const AfLayoutData *data, uint64_t node_id) {
    if (data == NULL || data->nodes == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < data->count; ++index) {
        if (data->nodes[index].node_id == node_id) {
            return &data->nodes[index];
        }
    }
    return NULL;
}

static bool af_layout_finite_insets(AfInsets insets) {
    return isfinite(insets.left) && isfinite(insets.top) && isfinite(insets.right) &&
           isfinite(insets.bottom) && insets.left >= 0.0F && insets.top >= 0.0F &&
           insets.right >= 0.0F && insets.bottom >= 0.0F && insets.left <= AF_LAYOUT_LIMIT &&
           insets.top <= AF_LAYOUT_LIMIT && insets.right <= AF_LAYOUT_LIMIT &&
           insets.bottom <= AF_LAYOUT_LIMIT;
}

AfResult af_layout_node_validate(const AfLayoutNode *layout) {
    if (layout == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    {
        const float values[] = {
            layout->anchor_min.x,   layout->anchor_min.y,   layout->anchor_max.x,
            layout->anchor_max.y,   layout->pivot.x,        layout->pivot.y,
            layout->offset.x,       layout->offset.y,       layout->width.value,
            layout->height.value,   layout->minimum_size.x, layout->minimum_size.y,
            layout->maximum_size.x, layout->maximum_size.y, layout->aspect_ratio,
            layout->spacing};
        if (layout->node_id == 0U || layout->width.mode > AF_LAYOUT_VALUE_PERCENT ||
            layout->height.mode > AF_LAYOUT_VALUE_PERCENT ||
            layout->aspect > AF_ASPECT_HEIGHT_CONTROLS_WIDTH ||
            layout->direction > AF_LAYOUT_DIRECTION_VERTICAL ||
            layout->child_alignment > AF_LAYOUT_ALIGN_STRETCH ||
            !af_layout_finite_insets(layout->padding) || layout->anchor_min.x < 0.0F ||
            layout->anchor_min.y < 0.0F || layout->anchor_max.x < layout->anchor_min.x ||
            layout->anchor_max.y < layout->anchor_min.y || layout->anchor_max.x > 1.0F ||
            layout->anchor_max.y > 1.0F || layout->pivot.x < 0.0F || layout->pivot.x > 1.0F ||
            layout->pivot.y < 0.0F || layout->pivot.y > 1.0F || layout->width.value < 0.0F ||
            layout->height.value < 0.0F || layout->minimum_size.x < 0.0F ||
            layout->minimum_size.y < 0.0F || layout->maximum_size.x < layout->minimum_size.x ||
            layout->maximum_size.y < layout->minimum_size.y || layout->spacing < 0.0F ||
            (layout->aspect != AF_ASPECT_NONE && layout->aspect_ratio <= 0.0F)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t index = 0U; index < sizeof(values) / sizeof(values[0]); ++index) {
            if (!isfinite(values[index]) || fabsf(values[index]) > AF_LAYOUT_LIMIT) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    return AF_RESULT_SUCCESS;
}

static AfResult af_layout_validate_indexed(const AfLayoutData *data, const AfScene *scene,
                                           const AfIdIndex *lookup,
                                           const AfLayoutNode **node_layouts) {
    if (data == NULL || scene == NULL || data->count > data->capacity ||
        data->capacity > AF_MAX_LAYOUT_NODES || (data->capacity != 0U && data->nodes == NULL) ||
        !isfinite(data->reference_size.x) || !isfinite(data->reference_size.y) ||
        !isfinite(data->target_size.x) || !isfinite(data->target_size.y) ||
        data->reference_size.x <= 0.0F || data->reference_size.y <= 0.0F ||
        data->target_size.x <= 0.0F || data->target_size.y <= 0.0F ||
        data->reference_size.x > AF_LAYOUT_LIMIT || data->reference_size.y > AF_LAYOUT_LIMIT ||
        data->target_size.x > AF_LAYOUT_LIMIT || data->target_size.y > AF_LAYOUT_LIMIT ||
        data->scale_mode > AF_CANVAS_SCALE_STRETCH || !af_layout_finite_insets(data->safe_area) ||
        data->safe_area.left + data->safe_area.right >= data->target_size.x ||
        data->safe_area.top + data->safe_area.bottom >= data->target_size.y) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    memset(node_layouts, 0, (size_t)scene->count * sizeof(*node_layouts));
    for (uint32_t index = 0U; index < data->count; ++index) {
        const AfLayoutNode *layout = &data->nodes[index];
        const uint32_t node = af_id_index_find(lookup, layout->node_id);
        if (af_layout_node_validate(layout) != AF_RESULT_SUCCESS || node == UINT32_MAX ||
            node_layouts[node] != NULL) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        node_layouts[node] = layout;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_layout_validate(const AfLayoutData *data, const AfScene *scene) {
    AfIdIndex lookup;
    uint32_t slots[AF_MAX_NODES * 2U];
    const AfLayoutNode *node_layouts[AF_MAX_NODES];
    if (scene == NULL || scene->count > AF_MAX_NODES || scene->count > scene->capacity ||
        af_id_index_build(&lookup, scene->nodes, scene->count, sizeof(*scene->nodes), slots,
                          AF_MAX_NODES * 2U) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return af_layout_validate_indexed(data, scene, &lookup, node_layouts);
}

static float af_layout_clamp(float value, float minimum, float maximum) {
    return fmaxf(minimum, fminf(value, maximum));
}

static float af_layout_resolve_value(AfLayoutValue value, float intrinsic, float parent_size) {
    if (value.mode == AF_LAYOUT_VALUE_FIXED) {
        return value.value;
    }
    if (value.mode == AF_LAYOUT_VALUE_PERCENT) {
        return parent_size * value.value;
    }
    return intrinsic;
}

static AfVec2 af_layout_resolve_size(const AfSceneNode *node, const AfLayoutNode *layout,
                                     AfVec2 available) {
    AfVec2 size = node->bounds;
    if (layout == NULL) {
        return size;
    }
    size.x = af_layout_resolve_value(layout->width, node->bounds.x, available.x);
    size.y = af_layout_resolve_value(layout->height, node->bounds.y, available.y);
    if (layout->anchor_max.x > layout->anchor_min.x &&
        layout->width.mode == AF_LAYOUT_VALUE_INTRINSIC) {
        size.x = available.x * (layout->anchor_max.x - layout->anchor_min.x);
    }
    if (layout->anchor_max.y > layout->anchor_min.y &&
        layout->height.mode == AF_LAYOUT_VALUE_INTRINSIC) {
        size.y = available.y * (layout->anchor_max.y - layout->anchor_min.y);
    }
    size.x = af_layout_clamp(size.x, layout->minimum_size.x, layout->maximum_size.x);
    size.y = af_layout_clamp(size.y, layout->minimum_size.y, layout->maximum_size.y);
    if (layout->aspect == AF_ASPECT_WIDTH_CONTROLS_HEIGHT) {
        size.y = size.x / layout->aspect_ratio;
    } else if (layout->aspect == AF_ASPECT_HEIGHT_CONTROLS_WIDTH) {
        size.x = size.y * layout->aspect_ratio;
    } else if (layout->aspect == AF_ASPECT_FIT && size.x > 0.0F && size.y > 0.0F) {
        if (size.x / size.y > layout->aspect_ratio) {
            size.x = size.y * layout->aspect_ratio;
        } else {
            size.y = size.x / layout->aspect_ratio;
        }
    }
    return size;
}

static AfLayoutRect af_layout_parent_content(const AfSceneNode *parent,
                                             const AfLayoutNode *parent_layout) {
    AfLayoutRect content = {.size = parent->resolved_bounds};
    if (parent_layout != NULL) {
        content.position = (AfVec2){parent_layout->padding.left, parent_layout->padding.top};
        content.size.x = fmaxf(0.0F, content.size.x - parent_layout->padding.left -
                                         parent_layout->padding.right);
        content.size.y = fmaxf(0.0F, content.size.y - parent_layout->padding.top -
                                         parent_layout->padding.bottom);
    }
    return content;
}

static AfLayoutRect af_layout_node_rect(const AfLayoutContext *context, uint32_t node_index,
                                        AfLayoutRect parent_content,
                                        const AfLayoutNode *parent_layout) {
    const AfSceneNode *node = &context->scene->nodes[node_index];
    const AfLayoutNode *layout = context->node_layouts[node_index];
    AfLayoutRect rect = {.size = af_layout_resolve_size(node, layout, parent_content.size)};
    AfVec2 anchor = {0.0F, 0.0F};
    AfVec2 pivot = {0.0F, 0.0F};
    if (layout != NULL) {
        anchor = layout->anchor_min;
        pivot = layout->pivot;
        rect.position = layout->offset;
    }
    rect.position.x +=
        parent_content.position.x + parent_content.size.x * anchor.x - rect.size.x * pivot.x;
    rect.position.y +=
        parent_content.position.y + parent_content.size.y * anchor.y - rect.size.y * pivot.y;
    if (parent_layout != NULL && parent_layout->direction != AF_LAYOUT_DIRECTION_NONE) {
        const float cursor = context->child_offsets[node_index];
        if (parent_layout->direction == AF_LAYOUT_DIRECTION_HORIZONTAL) {
            rect.position.x = parent_content.position.x + cursor;
            if (parent_layout->child_alignment == AF_LAYOUT_ALIGN_CENTER) {
                rect.position.y =
                    parent_content.position.y + (parent_content.size.y - rect.size.y) * 0.5F;
            } else if (parent_layout->child_alignment == AF_LAYOUT_ALIGN_END) {
                rect.position.y = parent_content.position.y + parent_content.size.y - rect.size.y;
            } else if (parent_layout->child_alignment == AF_LAYOUT_ALIGN_STRETCH) {
                rect.position.y = parent_content.position.y;
                rect.size.y = parent_content.size.y;
            }
        } else {
            rect.position.y = parent_content.position.y + cursor;
            if (parent_layout->child_alignment == AF_LAYOUT_ALIGN_CENTER) {
                rect.position.x =
                    parent_content.position.x + (parent_content.size.x - rect.size.x) * 0.5F;
            } else if (parent_layout->child_alignment == AF_LAYOUT_ALIGN_END) {
                rect.position.x = parent_content.position.x + parent_content.size.x - rect.size.x;
            } else if (parent_layout->child_alignment == AF_LAYOUT_ALIGN_STRETCH) {
                rect.position.x = parent_content.position.x;
                rect.size.x = parent_content.size.x;
            }
        }
    }
    return rect;
}

static void af_layout_prepare_children(AfLayoutContext *context, uint32_t parent,
                                       AfLayoutRect content, const AfLayoutNode *layout) {
    if (layout == NULL || layout->direction == AF_LAYOUT_DIRECTION_NONE ||
        context->children_prepared[parent]) {
        return;
    }
    float cursor = 0.0F;
    uint32_t ordinal = 0U;
    for (uint32_t child = context->first_child[parent]; child != UINT32_MAX;
         child = context->next_sibling[child]) {
        const AfVec2 size = af_layout_resolve_size(&context->scene->nodes[child],
                                                   context->node_layouts[child], content.size);
        context->child_offsets[child] = cursor + (float)ordinal * layout->spacing;
        cursor += layout->direction == AF_LAYOUT_DIRECTION_HORIZONTAL ? size.x : size.y;
        ++ordinal;
    }
    context->children_prepared[parent] = true;
}

static AfResult af_layout_evaluate_node(AfLayoutContext *context, uint32_t index, uint32_t depth) {
    AfSceneNode *node = &context->scene->nodes[index];
    AfAffine2 parent_world = context->canvas_transform;
    AfLayoutRect parent_content = {.position = context->root_origin, .size = context->root_size};
    const AfLayoutNode *parent_layout = NULL;
    if (context->states[index] == 2U) {
        return AF_RESULT_SUCCESS;
    }
    if (context->states[index] == 1U || depth > AF_MAX_HIERARCHY_DEPTH) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    context->states[index] = 1U;
    if (node->parent_id != AF_INVALID_NODE_ID) {
        const uint32_t parent_index = af_id_index_find(&context->node_lookup, node->parent_id);
        if (parent_index == UINT32_MAX) {
            return AF_RESULT_NOT_FOUND;
        }
        AfResult result = af_layout_evaluate_node(context, parent_index, depth + 1U);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
        const AfSceneNode *parent = &context->scene->nodes[parent_index];
        parent_world = parent->world_transform;
        parent_layout = context->node_layouts[parent_index];
        parent_content = af_layout_parent_content(parent, parent_layout);
        node->world_opacity = parent->world_opacity * node->opacity;
        node->world_visible = parent->world_visible && node->visible;
        af_layout_prepare_children(context, parent_index, parent_content, parent_layout);
    } else {
        node->world_opacity = node->opacity;
        node->world_visible = node->visible;
    }
    {
        const AfLayoutRect rect =
            af_layout_node_rect(context, index, parent_content, parent_layout);
        const AfLayoutNode *layout = context->node_layouts[index];
        const AfVec2 pivot =
            layout == NULL ? (AfVec2){0}
                           : (AfVec2){rect.size.x * layout->pivot.x, rect.size.y * layout->pivot.y};
        const AfAffine2 layout_transform = {
            .m00 = 1.0F, .m11 = 1.0F, .tx = rect.position.x, .ty = rect.position.y};
        const AfAffine2 base_animated =
            af_affine_from_transform_pivot(node->local_transform, pivot);
        node->resolved_bounds = rect.size;
        node->world_transform =
            af_affine_multiply(parent_world, af_affine_multiply(layout_transform, base_animated));
    }
    context->states[index] = 2U;
    return AF_RESULT_SUCCESS;
}

AfResult af_layout_evaluate_scene(AfScene *scene) {
    AfLayoutContext context;
    context.scene = scene;
    context.data = scene == NULL ? NULL : scene->layouts;
    float scale_x = 1.0F;
    float scale_y = 1.0F;
    if (scene == NULL || scene->layouts == NULL || scene->count > AF_MAX_NODES ||
        scene->count > scene->capacity ||
        af_id_index_build(&context.node_lookup, scene->nodes, scene->count, sizeof(*scene->nodes),
                          context.node_slots, AF_MAX_NODES * 2U) != AF_RESULT_SUCCESS ||
        af_layout_validate_indexed(scene->layouts, scene, &context.node_lookup,
                                   context.node_layouts) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    memset(context.states, 0, (size_t)scene->count * sizeof(*context.states));
    memset(context.children_prepared, 0, (size_t)scene->count * sizeof(*context.children_prepared));
    for (uint32_t index = 0U; index < scene->count; ++index) {
        context.first_child[index] = UINT32_MAX;
        context.next_sibling[index] = UINT32_MAX;
    }
    /* Link siblings in scene order, even when children precede their parents in storage. */
    for (uint32_t cursor = scene->count; cursor > 0U; --cursor) {
        const uint32_t index = cursor - 1U;
        if (scene->nodes[index].parent_id == AF_INVALID_NODE_ID) {
            continue;
        }
        const uint32_t parent =
            af_id_index_find(&context.node_lookup, scene->nodes[index].parent_id);
        if (parent == UINT32_MAX) {
            return AF_RESULT_NOT_FOUND;
        }
        context.next_sibling[index] = context.first_child[parent];
        context.first_child[parent] = index;
    }
    scale_x = context.data->target_size.x / context.data->reference_size.x;
    scale_y = context.data->target_size.y / context.data->reference_size.y;
    if (context.data->scale_mode == AF_CANVAS_SCALE_NONE) {
        scale_x = 1.0F;
        scale_y = 1.0F;
    } else if (context.data->scale_mode == AF_CANVAS_SCALE_FIT) {
        scale_x = scale_y = fminf(scale_x, scale_y);
    } else if (context.data->scale_mode == AF_CANVAS_SCALE_FILL) {
        scale_x = scale_y = fmaxf(scale_x, scale_y);
    }
    context.canvas_transform = (AfAffine2){
        .m00 = scale_x,
        .m11 = scale_y,
        .tx = (context.data->target_size.x - context.data->reference_size.x * scale_x) * 0.5F,
        .ty = (context.data->target_size.y - context.data->reference_size.y * scale_y) * 0.5F};
    context.root_origin =
        (AfVec2){context.data->safe_area.left / scale_x, context.data->safe_area.top / scale_y};
    context.root_size = (AfVec2){(context.data->target_size.x - context.data->safe_area.left -
                                  context.data->safe_area.right) /
                                     scale_x,
                                 (context.data->target_size.y - context.data->safe_area.top -
                                  context.data->safe_area.bottom) /
                                     scale_y};
    for (uint32_t index = 0U; index < scene->count; ++index) {
        AfResult result = af_layout_evaluate_node(&context, index, 0U);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
    }
    return AF_RESULT_SUCCESS;
}
