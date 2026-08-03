#include "layout/layout.h"

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

AfResult af_layout_validate(const AfLayoutData *data, const AfScene *scene) {
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
    for (uint32_t index = 0U; index < data->count; ++index) {
        const AfLayoutNode *layout = &data->nodes[index];
        const float values[] = {
            layout->anchor_min.x,   layout->anchor_min.y,   layout->anchor_max.x,
            layout->anchor_max.y,   layout->pivot.x,        layout->pivot.y,
            layout->offset.x,       layout->offset.y,       layout->width.value,
            layout->height.value,   layout->minimum_size.x, layout->minimum_size.y,
            layout->maximum_size.x, layout->maximum_size.y, layout->aspect_ratio,
            layout->spacing};
        if (af_layout_node_validate(layout) != AF_RESULT_SUCCESS ||
            af_scene_find_const(scene, layout->node_id) == NULL ||
            layout->width.mode > AF_LAYOUT_VALUE_PERCENT ||
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
        for (uint32_t value_index = 0U; value_index < sizeof(values) / sizeof(values[0]);
             ++value_index) {
            if (!isfinite(values[value_index]) || fabsf(values[value_index]) > AF_LAYOUT_LIMIT) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        for (uint32_t other = index + 1U; other < data->count; ++other) {
            if (layout->node_id == data->nodes[other].node_id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    return AF_RESULT_SUCCESS;
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
    const AfLayoutNode *layout = af_layout_find_const(context->data, node->id);
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
        float cursor = 0.0F;
        uint32_t ordinal = 0U;
        for (uint32_t sibling_index = 0U; sibling_index < context->scene->count; ++sibling_index) {
            const AfSceneNode *sibling = &context->scene->nodes[sibling_index];
            AfVec2 sibling_size;
            if (sibling->parent_id != node->parent_id) {
                continue;
            }
            sibling_size = af_layout_resolve_size(
                sibling, af_layout_find_const(context->data, sibling->id), parent_content.size);
            if (sibling_index == node_index) {
                break;
            }
            cursor += parent_layout->direction == AF_LAYOUT_DIRECTION_HORIZONTAL ? sibling_size.x
                                                                                 : sibling_size.y;
            ++ordinal;
        }
        cursor += (float)ordinal * parent_layout->spacing;
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
        bool found = false;
        for (uint32_t parent_index = 0U; parent_index < context->scene->count; ++parent_index) {
            if (context->scene->nodes[parent_index].id == node->parent_id) {
                AfResult result = af_layout_evaluate_node(context, parent_index, depth + 1U);
                if (result != AF_RESULT_SUCCESS) {
                    return result;
                }
                parent_world = context->scene->nodes[parent_index].world_transform;
                parent_layout = af_layout_find_const(context->data, node->parent_id);
                parent_content =
                    af_layout_parent_content(&context->scene->nodes[parent_index], parent_layout);
                node->world_opacity =
                    context->scene->nodes[parent_index].world_opacity * node->opacity;
                node->world_visible =
                    context->scene->nodes[parent_index].world_visible && node->visible;
                found = true;
                break;
            }
        }
        if (!found) {
            return AF_RESULT_NOT_FOUND;
        }
    } else {
        node->world_opacity = node->opacity;
        node->world_visible = node->visible;
    }
    {
        const AfLayoutRect rect =
            af_layout_node_rect(context, index, parent_content, parent_layout);
        const AfAffine2 layout_transform = {
            .m00 = 1.0F, .m11 = 1.0F, .tx = rect.position.x, .ty = rect.position.y};
        const AfAffine2 base_animated = af_affine_from_transform(node->local_transform);
        node->resolved_bounds = rect.size;
        node->world_transform =
            af_affine_multiply(parent_world, af_affine_multiply(layout_transform, base_animated));
    }
    context->states[index] = 2U;
    return AF_RESULT_SUCCESS;
}

AfResult af_layout_evaluate_scene(AfScene *scene) {
    AfLayoutContext context = {.scene = scene, .data = scene == NULL ? NULL : scene->layouts};
    float scale_x = 1.0F;
    float scale_y = 1.0F;
    if (scene == NULL || scene->layouts == NULL ||
        af_layout_validate(scene->layouts, scene) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
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
