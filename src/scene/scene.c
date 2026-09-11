#include "scene/scene.h"

#include "core/id_index.h"
#include "layout/layout.h"

#include <math.h>
#include <stddef.h>

void af_scene_init(AfScene *scene, AfSceneNode *nodes, uint32_t capacity) {
    if (scene != NULL) {
        *scene = (AfScene){.nodes = nodes, .capacity = capacity};
    }
}

AfSceneNode *af_scene_find(AfScene *scene, uint64_t id) {
    uint32_t index = 0U;
    if (scene == NULL) {
        return NULL;
    }
    for (index = 0U; index < scene->count; ++index) {
        if (scene->nodes[index].id == id) {
            return &scene->nodes[index];
        }
    }
    return NULL;
}

const AfSceneNode *af_scene_find_const(const AfScene *scene, uint64_t id) {
    uint32_t index = 0U;
    if (scene == NULL) {
        return NULL;
    }
    for (index = 0U; index < scene->count; ++index) {
        if (scene->nodes[index].id == id) {
            return &scene->nodes[index];
        }
    }
    return NULL;
}

AfResult af_scene_add_node(AfScene *scene, const AfSceneNode *node) {
    if (scene == NULL || node == NULL || scene->nodes == NULL || node->id == AF_INVALID_NODE_ID ||
        node->id == 0U || scene->count >= scene->capacity || scene->count >= AF_MAX_NODES) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (af_scene_find(scene, node->id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    scene->nodes[scene->count++] = *node;
    return AF_RESULT_SUCCESS;
}

static bool af_scene_would_cycle(const AfScene *scene, uint64_t id, uint64_t parent_id) {
    uint32_t depth = 0U;
    uint64_t current = parent_id;
    while (current != AF_INVALID_NODE_ID && depth++ <= AF_MAX_HIERARCHY_DEPTH) {
        const AfSceneNode *parent = af_scene_find_const(scene, current);
        if (current == id) {
            return true;
        }
        if (parent == NULL) {
            return false;
        }
        current = parent->parent_id;
    }
    return current != AF_INVALID_NODE_ID;
}

AfResult af_scene_reparent(AfScene *scene, uint64_t id, uint64_t parent_id) {
    AfSceneNode *node = af_scene_find(scene, id);
    if (node == NULL ||
        (parent_id != AF_INVALID_NODE_ID && af_scene_find(scene, parent_id) == NULL)) {
        return AF_RESULT_NOT_FOUND;
    }
    if (af_scene_would_cycle(scene, id, parent_id)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    node->parent_id = parent_id;
    return AF_RESULT_SUCCESS;
}

AfResult af_scene_delete_node(AfScene *scene, uint64_t id) {
    uint32_t index = 0U;
    uint32_t write_index = 0U;
    bool deleted[AF_MAX_NODES] = {false};
    bool changed = true;
    bool found = false;
    if (scene == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < scene->count; ++index) {
        if (scene->nodes[index].id == id) {
            deleted[index] = true;
            found = true;
            break;
        }
    }
    if (!found) {
        return AF_RESULT_NOT_FOUND;
    }
    while (changed) {
        changed = false;
        for (index = 0U; index < scene->count; ++index) {
            uint32_t parent_index = 0U;
            if (deleted[index]) {
                continue;
            }
            for (parent_index = 0U; parent_index < scene->count; ++parent_index) {
                if (deleted[parent_index] &&
                    scene->nodes[index].parent_id == scene->nodes[parent_index].id) {
                    deleted[index] = true;
                    changed = true;
                    break;
                }
            }
        }
    }
    for (index = 0U; index < scene->count; ++index) {
        if (deleted[index]) {
            continue;
        }
        scene->nodes[write_index++] = scene->nodes[index];
    }
    scene->count = write_index;
    return AF_RESULT_SUCCESS;
}

AfResult af_scene_validate(const AfScene *scene) {
    uint32_t index = 0U;
    uint32_t other = 0U;
    if (scene == NULL || (scene->nodes == NULL && scene->count != 0U) ||
        scene->count > AF_MAX_NODES || scene->count > scene->capacity) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < scene->count; ++index) {
        const AfSceneNode *node = &scene->nodes[index];
        if (node->id == 0U || node->id == AF_INVALID_NODE_ID || node->type > AF_NODE_IMAGE ||
            !isfinite(node->local_transform.position.x) ||
            !isfinite(node->local_transform.position.y) ||
            !isfinite(node->local_transform.rotation) || !isfinite(node->local_transform.scale.x) ||
            !isfinite(node->local_transform.scale.y) || !isfinite(node->local_transform.skew.x) ||
            !isfinite(node->local_transform.skew.y) ||
            fabsf(node->local_transform.skew.x) > AF_TRANSFORM_MAX_SKEW_RADIANS ||
            fabsf(node->local_transform.skew.y) > AF_TRANSFORM_MAX_SKEW_RADIANS ||
            !isfinite(node->bounds.x) || !isfinite(node->bounds.y) || !isfinite(node->fill.r) ||
            !isfinite(node->fill.g) || !isfinite(node->fill.b) || !isfinite(node->fill.a) ||
            !isfinite(node->opacity) || !isfinite(node->corner_radius) || node->opacity < 0.0F ||
            node->opacity > 1.0F || node->fill.r < 0.0F || node->fill.r > 1.0F ||
            node->fill.g < 0.0F || node->fill.g > 1.0F || node->fill.b < 0.0F ||
            node->fill.b > 1.0F || node->fill.a < 0.0F || node->fill.a > 1.0F ||
            node->bounds.x < 0.0F || node->bounds.y < 0.0F || node->corner_radius < 0.0F) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (other = index + 1U; other < scene->count; ++other) {
            if (node->id == scene->nodes[other].id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        if (node->parent_id != AF_INVALID_NODE_ID &&
            af_scene_find_const(scene, node->parent_id) == NULL) {
            return AF_RESULT_NOT_FOUND;
        }
        if (af_scene_would_cycle(scene, node->id, node->parent_id)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    return AF_RESULT_SUCCESS;
}

static AfResult af_scene_evaluate_node(AfScene *scene, uint32_t index, uint8_t *states,
                                       const AfIdIndex *lookup, uint32_t depth) {
    AfSceneNode *node = &scene->nodes[index];

    if (states[index] == 2U) {
        return AF_RESULT_SUCCESS;
    }
    if (states[index] == 1U || depth > AF_MAX_HIERARCHY_DEPTH) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    const AfAffine2 local = af_affine_from_transform(node->local_transform);
    states[index] = 1U;
    node->resolved_bounds = node->bounds;
    if (node->parent_id == AF_INVALID_NODE_ID) {
        node->world_transform = local;
        node->world_opacity = node->opacity;
        node->world_visible = node->visible;
    } else {
        const uint32_t parent_index = af_id_index_find(lookup, node->parent_id);
        if (parent_index == UINT32_MAX) {
            return AF_RESULT_NOT_FOUND;
        }
        AfResult result = af_scene_evaluate_node(scene, parent_index, states, lookup, depth + 1U);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
        node->world_transform =
            af_affine_multiply(scene->nodes[parent_index].world_transform, local);
        node->world_opacity = scene->nodes[parent_index].world_opacity * node->opacity;
        node->world_visible = scene->nodes[parent_index].world_visible && node->visible;
    }
    states[index] = 2U;
    return AF_RESULT_SUCCESS;
}

AfResult af_scene_evaluate_world(AfScene *scene) {
    uint8_t states[AF_MAX_NODES];
    uint32_t slots[AF_MAX_NODES * 2U];
    AfIdIndex lookup;
    uint32_t index = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (scene == NULL || scene->count > AF_MAX_NODES || scene->count > scene->capacity ||
        (scene->count != 0U && scene->nodes == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (scene->layouts != NULL) {
        return af_layout_evaluate_scene(scene);
    }
    result = af_id_index_build(&lookup, scene->nodes, scene->count, sizeof(*scene->nodes), slots,
                               AF_MAX_NODES * 2U);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    memset(states, 0, (size_t)scene->count * sizeof(*states));
    for (index = 0U; index < scene->count; ++index) {
        result = af_scene_evaluate_node(scene, index, states, &lookup, 0U);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
    }
    return AF_RESULT_SUCCESS;
}
