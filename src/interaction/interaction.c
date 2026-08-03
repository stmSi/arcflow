#include "interaction/interaction.h"

#include <math.h>
#include <stddef.h>

static const AfInteractionNode *af_interaction_node(const AfInteractionSystem *system,
                                                    uint64_t node_id) {
    if (system == NULL || node_id == 0U) {
        return NULL;
    }
    for (uint32_t index = 0U; index < system->data.count; ++index) {
        if (system->data.nodes[index].node_id == node_id) {
            return &system->data.nodes[index];
        }
    }
    return NULL;
}

static bool af_point_in_node(const AfSceneNode *node, float x, float y) {
    const float determinant = node->world_transform.m00 * node->world_transform.m11 -
                              node->world_transform.m01 * node->world_transform.m10;
    float local_x = 0.0F;
    float local_y = 0.0F;
    if (!isfinite(determinant) || fabsf(determinant) <= 0.000001F) {
        return false;
    }
    const float translated_x = x - node->world_transform.tx;
    const float translated_y = y - node->world_transform.ty;
    local_x =
        (node->world_transform.m11 * translated_x - node->world_transform.m01 * translated_y) /
        determinant;
    local_y =
        (-node->world_transform.m10 * translated_x + node->world_transform.m00 * translated_y) /
        determinant;
    return local_x >= 0.0F && local_y >= 0.0F && local_x <= node->resolved_bounds.x &&
           local_y <= node->resolved_bounds.y;
}

static bool af_interaction_clipped(const AfInteractionSystem *system, const AfScene *scene,
                                   const AfSceneNode *node, float x, float y) {
    uint64_t parent_id = node->parent_id;
    for (uint32_t depth = 0U; parent_id != AF_INVALID_NODE_ID && depth < AF_MAX_HIERARCHY_DEPTH;
         ++depth) {
        const AfSceneNode *parent = af_scene_find_const(scene, parent_id);
        const AfInteractionNode *interaction = af_interaction_node(system, parent_id);
        if (parent == NULL) {
            return true;
        }
        if (!parent->world_visible || parent->world_opacity <= 0.0F) {
            return true;
        }
        if (interaction != NULL && interaction->clip_children && !af_point_in_node(parent, x, y)) {
            return true;
        }
        parent_id = parent->parent_id;
    }
    return parent_id != AF_INVALID_NODE_ID;
}

static AfResult af_interaction_push(AfInteractionSystem *system, AfRuntimeEvent event) {
    uint32_t tail = 0U;
    if (system == NULL || system->queue.count >= AF_MAX_RUNTIME_EVENTS) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    tail = (system->queue.head + system->queue.count) % AF_MAX_RUNTIME_EVENTS;
    system->queue.events[tail] = event;
    ++system->queue.count;
    return AF_RESULT_SUCCESS;
}

static AfResult af_interaction_focus(AfInteractionSystem *system, uint64_t node_id, double time) {
    AfResult result = AF_RESULT_SUCCESS;
    if (system->focused_node_id == node_id) {
        return AF_RESULT_SUCCESS;
    }
    if (system->focused_node_id != 0U) {
        result = af_interaction_push(system, (AfRuntimeEvent){.type = AF_EVENT_BLUR,
                                                              .node_id = system->focused_node_id,
                                                              .timestamp = time});
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
    }
    system->focused_node_id = node_id;
    if (node_id != 0U) {
        result = af_interaction_push(
            system,
            (AfRuntimeEvent){.type = AF_EVENT_FOCUS, .node_id = node_id, .timestamp = time});
    }
    return result;
}

AfResult af_interaction_validate(const AfInteractionData *data, const AfScene *scene) {
    if (data == NULL || scene == NULL || data->count > AF_MAX_INTERACTION_NODES ||
        (data->count != 0U && data->nodes == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < data->count; ++index) {
        const AfInteractionNode *node = &data->nodes[index];
        if (af_scene_find_const(scene, node->node_id) == NULL ||
            !isfinite(node->opacity_threshold) || node->opacity_threshold < 0.0F ||
            node->opacity_threshold > 1.0F) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t other = index + 1U; other < data->count; ++other) {
            if (node->node_id == data->nodes[other].node_id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    return AF_RESULT_SUCCESS;
}

void af_interaction_init(AfInteractionSystem *system, const AfInteractionNode *nodes,
                         uint32_t count) {
    if (system != NULL) {
        *system = (AfInteractionSystem){.data = {.nodes = nodes, .count = count},
                                        .last_click_time = -INFINITY};
    }
}

uint64_t af_interaction_hit_test(const AfInteractionSystem *system, const AfScene *scene, float x,
                                 float y) {
    uint64_t hit = 0U;
    int32_t best_layer = INT32_MIN;
    uint32_t best_index = 0U;
    if (system == NULL || scene == NULL || !isfinite(x) || !isfinite(y)) {
        return 0U;
    }
    for (uint32_t index = 0U; index < scene->count; ++index) {
        const AfSceneNode *node = &scene->nodes[index];
        const AfInteractionNode *interaction = af_interaction_node(system, node->id);
        if (interaction == NULL || !interaction->enabled || interaction->pointer_passthrough ||
            !node->world_visible || node->world_opacity <= interaction->opacity_threshold ||
            !af_point_in_node(node, x, y) || af_interaction_clipped(system, scene, node, x, y)) {
            continue;
        }
        if (hit == 0U || node->layer_order > best_layer ||
            (node->layer_order == best_layer && index >= best_index)) {
            hit = node->id;
            best_layer = node->layer_order;
            best_index = index;
        }
    }
    return hit;
}

AfResult af_interaction_pointer_move(AfInteractionSystem *system, const AfScene *scene, float x,
                                     float y, double timestamp) {
    const uint64_t hit = af_interaction_hit_test(system, scene, x, y);
    AfResult result = AF_RESULT_SUCCESS;
    if (system == NULL || !isfinite(timestamp)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (hit != system->hovered_node_id) {
        if (system->hovered_node_id != 0U &&
            (result = af_interaction_push(
                 system, (AfRuntimeEvent){.type = AF_EVENT_POINTER_LEAVE,
                                          .node_id = system->hovered_node_id,
                                          .x = x,
                                          .y = y,
                                          .timestamp = timestamp})) != AF_RESULT_SUCCESS) {
            return result;
        }
        system->hovered_node_id = hit;
        if (hit != 0U &&
            (result = af_interaction_push(system, (AfRuntimeEvent){.type = AF_EVENT_POINTER_ENTER,
                                                                   .node_id = hit,
                                                                   .x = x,
                                                                   .y = y,
                                                                   .timestamp = timestamp})) !=
                AF_RESULT_SUCCESS) {
            return result;
        }
    }
    if (hit != 0U && (result = af_interaction_push(
                          system, (AfRuntimeEvent){.type = AF_EVENT_POINTER_MOVE,
                                                   .node_id = hit,
                                                   .x = x,
                                                   .y = y,
                                                   .timestamp = timestamp})) != AF_RESULT_SUCCESS) {
        return result;
    }
    if (system->captured_node_id != 0U) {
        const float dx = x - system->press_x;
        const float dy = y - system->press_y;
        if (!system->dragging && dx * dx + dy * dy >= AF_DRAG_THRESHOLD * AF_DRAG_THRESHOLD) {
            system->dragging = true;
            result =
                af_interaction_push(system, (AfRuntimeEvent){.type = AF_EVENT_DRAG_START,
                                                             .node_id = system->captured_node_id,
                                                             .x = x,
                                                             .y = y,
                                                             .timestamp = timestamp});
        }
        if (result == AF_RESULT_SUCCESS && system->dragging) {
            result =
                af_interaction_push(system, (AfRuntimeEvent){.type = AF_EVENT_DRAG,
                                                             .node_id = system->captured_node_id,
                                                             .x = x,
                                                             .y = y,
                                                             .timestamp = timestamp});
        }
    }
    return result;
}

AfResult af_interaction_pointer_down(AfInteractionSystem *system, const AfScene *scene, float x,
                                     float y, double timestamp) {
    const uint64_t hit = af_interaction_hit_test(system, scene, x, y);
    const AfInteractionNode *interaction = af_interaction_node(system, hit);
    AfResult result = AF_RESULT_SUCCESS;
    if (system == NULL || !isfinite(timestamp)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    system->pressed_node_id = hit;
    system->captured_node_id = hit;
    system->press_x = x;
    system->press_y = y;
    system->dragging = false;
    if (hit != 0U) {
        result = af_interaction_push(system, (AfRuntimeEvent){.type = AF_EVENT_POINTER_DOWN,
                                                              .node_id = hit,
                                                              .x = x,
                                                              .y = y,
                                                              .timestamp = timestamp});
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_interaction_focus(
            system, interaction != NULL && interaction->focusable ? hit : 0U, timestamp);
    }
    return result;
}

AfResult af_interaction_pointer_up(AfInteractionSystem *system, const AfScene *scene, float x,
                                   float y, double timestamp) {
    const uint64_t hit = af_interaction_hit_test(system, scene, x, y);
    uint64_t target = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (system == NULL || !isfinite(timestamp)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    target = system->captured_node_id != 0U ? system->captured_node_id : hit;
    if (target != 0U) {
        result = af_interaction_push(system, (AfRuntimeEvent){.type = AF_EVENT_POINTER_UP,
                                                              .node_id = target,
                                                              .x = x,
                                                              .y = y,
                                                              .timestamp = timestamp});
    }
    if (result == AF_RESULT_SUCCESS && system->dragging && target != 0U) {
        result = af_interaction_push(system, (AfRuntimeEvent){.type = AF_EVENT_DRAG_END,
                                                              .node_id = target,
                                                              .x = x,
                                                              .y = y,
                                                              .timestamp = timestamp});
    } else if (result == AF_RESULT_SUCCESS && target != 0U && target == system->pressed_node_id &&
               target == hit) {
        const bool double_click = target == system->last_click_node_id &&
                                  timestamp >= system->last_click_time &&
                                  timestamp - system->last_click_time <= AF_DOUBLE_CLICK_SECONDS;
        result = af_interaction_push(
            system, (AfRuntimeEvent){.type = double_click ? AF_EVENT_DOUBLE_CLICK : AF_EVENT_CLICK,
                                     .node_id = target,
                                     .x = x,
                                     .y = y,
                                     .timestamp = timestamp});
        system->last_click_node_id = target;
        system->last_click_time = timestamp;
    }
    system->pressed_node_id = 0U;
    system->captured_node_id = 0U;
    system->dragging = false;
    return result;
}

AfResult af_interaction_keyboard_activate(AfInteractionSystem *system, double timestamp) {
    if (system == NULL || !isfinite(timestamp)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (system->focused_node_id == 0U) {
        return AF_RESULT_NOT_FOUND;
    }
    return af_interaction_push(system, (AfRuntimeEvent){.type = AF_EVENT_KEYBOARD_ACTIVATE,
                                                        .node_id = system->focused_node_id,
                                                        .timestamp = timestamp});
}

AfResult af_interaction_controller_navigate(AfInteractionSystem *system, const AfScene *scene,
                                            int32_t direction, double timestamp) {
    uint32_t current = UINT32_MAX;
    uint32_t next = UINT32_MAX;
    if (system == NULL || scene == NULL || (direction != -1 && direction != 1) ||
        !isfinite(timestamp)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < scene->count; ++index) {
        if (scene->nodes[index].id == system->focused_node_id) {
            current = index;
            break;
        }
    }
    for (uint32_t index = 0U; index < scene->count; ++index) {
        const AfInteractionNode *interaction = af_interaction_node(system, scene->nodes[index].id);
        if (interaction != NULL && interaction->enabled && interaction->focusable &&
            scene->nodes[index].world_visible &&
            scene->nodes[index].world_opacity > interaction->opacity_threshold) {
            if (direction > 0 && (current == UINT32_MAX || index > current) && next == UINT32_MAX) {
                next = index;
            } else if (direction < 0 && (current == UINT32_MAX || index < current)) {
                next = index;
            }
        }
    }
    if (next == UINT32_MAX) {
        for (uint32_t index = 0U; index < scene->count; ++index) {
            const uint32_t candidate = direction > 0 ? index : scene->count - index - 1U;
            const AfInteractionNode *interaction =
                af_interaction_node(system, scene->nodes[candidate].id);
            if (interaction != NULL && interaction->enabled && interaction->focusable &&
                scene->nodes[candidate].world_visible) {
                next = candidate;
                break;
            }
        }
    }
    if (next == UINT32_MAX) {
        return AF_RESULT_NOT_FOUND;
    }
    AfResult result = af_interaction_focus(system, scene->nodes[next].id, timestamp);
    if (result == AF_RESULT_SUCCESS) {
        result = af_interaction_push(system, (AfRuntimeEvent){.type = AF_EVENT_CONTROLLER_NAVIGATE,
                                                              .node_id = scene->nodes[next].id,
                                                              .timestamp = timestamp,
                                                              .controller_direction = direction});
    }
    return result;
}

AfResult af_interaction_queue_animation_event(AfInteractionSystem *system, uint64_t name_id,
                                              uint64_t node_id, double timestamp) {
    if (system == NULL || name_id == 0U || !isfinite(timestamp)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return af_interaction_push(system, (AfRuntimeEvent){.type = AF_EVENT_ANIMATION,
                                                        .node_id = node_id,
                                                        .name_id = name_id,
                                                        .timestamp = timestamp});
}

AfResult af_interaction_poll(AfInteractionSystem *system, AfRuntimeEvent *out_event) {
    if (system == NULL || out_event == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (system->queue.count == 0U) {
        return AF_RESULT_NOT_FOUND;
    }
    *out_event = system->queue.events[system->queue.head];
    system->queue.head = (system->queue.head + 1U) % AF_MAX_RUNTIME_EVENTS;
    --system->queue.count;
    return AF_RESULT_SUCCESS;
}
