#include "editor/editor_workflow.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

void af_editor_workspace_default(AfEditorWorkspace *workspace) {
    if (workspace != NULL) {
        *workspace = (AfEditorWorkspace){.left_width = 230.0F,
                                         .right_width = 286.0F,
                                         .timeline_height = 270.0F,
                                         .hierarchy_visible = true,
                                         .inspector_visible = true,
                                         .timeline_visible = true,
                                         .assets_visible = true};
    }
}

AfResult af_editor_workspace_validate(AfEditorWorkspace *workspace, float window_width,
                                      float window_height) {
    if (workspace == NULL || !isfinite(window_width) || !isfinite(window_height) ||
        window_width < 480.0F || window_height < 320.0F ||
        workspace->guide_count > AF_EDITOR_WORKFLOW_MAX_GUIDES) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    workspace->left_width = fminf(fmaxf(workspace->left_width, 150.0F), window_width * 0.4F);
    workspace->right_width = fminf(fmaxf(workspace->right_width, 150.0F), window_width * 0.4F);
    workspace->timeline_height =
        fminf(fmaxf(workspace->timeline_height, 120.0F), window_height * 0.65F);
    for (uint32_t index = 0U; index < workspace->guide_count; ++index) {
        if (!isfinite(workspace->guides[index].position)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_workspace_serialize(const AfEditorWorkspace *workspace, char *output,
                                       size_t output_capacity, size_t *out_size) {
    size_t written = 0U;
    int count = 0;
    if (workspace == NULL || output == NULL || output_capacity == 0U || out_size == NULL ||
        workspace->guide_count > AF_EDITOR_WORKFLOW_MAX_GUIDES) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    count = snprintf(output, output_capacity,
                     "arcflow_workspace 1\n%.9g %.9g %.9g %u %u %u %u %u %u %u\n",
                     workspace->left_width, workspace->right_width, workspace->timeline_height,
                     workspace->hierarchy_visible ? 1U : 0U, workspace->inspector_visible ? 1U : 0U,
                     workspace->timeline_visible ? 1U : 0U, workspace->assets_visible ? 1U : 0U,
                     workspace->profiling_visible ? 1U : 0U,
                     workspace->validation_visible ? 1U : 0U, workspace->guide_count);
    if (count < 0 || (size_t)count >= output_capacity) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    written = (size_t)count;
    for (uint32_t index = 0U; index < workspace->guide_count; ++index) {
        count = snprintf(output + written, output_capacity - written, "guide %u %.9g\n",
                         workspace->guides[index].vertical ? 1U : 0U,
                         workspace->guides[index].position);
        if (count < 0 || (size_t)count >= output_capacity - written) {
            output[0] = '\0';
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        written += (size_t)count;
    }
    *out_size = written;
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_workspace_parse(const char *text, size_t text_size,
                                   AfEditorWorkspace *out_workspace) {
    char bounded_text[16385];
    AfEditorWorkspace parsed = {0};
    unsigned int values[7] = {0};
    int offset = 0;
    if (text == NULL || out_workspace == NULL || text_size == 0U || text_size > 16384U ||
        memchr(text, '\0', text_size) != NULL) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    memcpy(bounded_text, text, text_size);
    bounded_text[text_size] = '\0';
    if (sscanf(bounded_text, "arcflow_workspace 1\n%f %f %f %u %u %u %u %u %u %u%n",
               &parsed.left_width, &parsed.right_width, &parsed.timeline_height, &values[0],
               &values[1], &values[2], &values[3], &values[4], &values[5], &values[6],
               &offset) != 10 ||
        values[0] > 1U || values[1] > 1U || values[2] > 1U || values[3] > 1U || values[4] > 1U ||
        values[5] > 1U || values[6] > AF_EDITOR_WORKFLOW_MAX_GUIDES) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    parsed.hierarchy_visible = values[0] != 0U;
    parsed.inspector_visible = values[1] != 0U;
    parsed.timeline_visible = values[2] != 0U;
    parsed.assets_visible = values[3] != 0U;
    parsed.profiling_visible = values[4] != 0U;
    parsed.validation_visible = values[5] != 0U;
    parsed.guide_count = values[6];
    for (uint32_t index = 0U; index < parsed.guide_count; ++index) {
        unsigned int vertical = 0U;
        int consumed = 0;
        while ((size_t)offset < text_size && isspace((unsigned char)text[offset]) != 0) {
            ++offset;
        }
        if ((size_t)offset >= text_size ||
            sscanf(bounded_text + offset, "guide %u %f%n", &vertical,
                   &parsed.guides[index].position, &consumed) != 2 ||
            vertical > 1U || consumed <= 0) {
            return AF_RESULT_INVALID_PACKAGE;
        }
        parsed.guides[index].vertical = vertical != 0U;
        offset += consumed;
    }
    while ((size_t)offset < text_size && isspace((unsigned char)text[offset]) != 0) {
        ++offset;
    }
    if ((size_t)offset != text_size) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    *out_workspace = parsed;
    return AF_RESULT_SUCCESS;
}

static AfResult af_editor_nodes_bounds(AfScene *scene, const uint64_t *ids, uint32_t count,
                                       float *left, float *top, float *right, float *bottom) {
    if (scene == NULL || ids == NULL || count == 0U || left == NULL || top == NULL ||
        right == NULL || bottom == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *left = INFINITY;
    *top = INFINITY;
    *right = -INFINITY;
    *bottom = -INFINITY;
    for (uint32_t index = 0U; index < count; ++index) {
        AfSceneNode *node = af_scene_find(scene, ids[index]);
        if (node == NULL) {
            return AF_RESULT_NOT_FOUND;
        }
        *left = fminf(*left, node->local_transform.position.x);
        *top = fminf(*top, node->local_transform.position.y);
        *right = fmaxf(*right, node->local_transform.position.x + node->bounds.x);
        *bottom = fmaxf(*bottom, node->local_transform.position.y + node->bounds.y);
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_align_nodes(AfScene *scene, const uint64_t *node_ids, uint32_t node_count,
                               AfEditorAlignMode mode) {
    float left = 0.0F;
    float top = 0.0F;
    float right = 0.0F;
    float bottom = 0.0F;
    AfResult result =
        af_editor_nodes_bounds(scene, node_ids, node_count, &left, &top, &right, &bottom);
    if (result != AF_RESULT_SUCCESS || mode > AF_EDITOR_ALIGN_BOTTOM) {
        return result != AF_RESULT_SUCCESS ? result : AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < node_count; ++index) {
        AfSceneNode *node = af_scene_find(scene, node_ids[index]);
        if (mode == AF_EDITOR_ALIGN_LEFT) {
            node->local_transform.position.x = left;
        } else if (mode == AF_EDITOR_ALIGN_CENTER_X) {
            node->local_transform.position.x = (left + right - node->bounds.x) * 0.5F;
        } else if (mode == AF_EDITOR_ALIGN_RIGHT) {
            node->local_transform.position.x = right - node->bounds.x;
        } else if (mode == AF_EDITOR_ALIGN_TOP) {
            node->local_transform.position.y = top;
        } else if (mode == AF_EDITOR_ALIGN_CENTER_Y) {
            node->local_transform.position.y = (top + bottom - node->bounds.y) * 0.5F;
        } else {
            node->local_transform.position.y = bottom - node->bounds.y;
        }
    }
    return af_scene_evaluate_world(scene);
}

AfResult af_editor_distribute_nodes(AfScene *scene, const uint64_t *node_ids, uint32_t node_count,
                                    AfEditorDistributeMode mode) {
    AfSceneNode *ordered[AF_EDITOR_WORKFLOW_MAX_CLIPBOARD_NODES];
    float total_size = 0.0F;
    float start = 0.0F;
    float end = 0.0F;
    float gap = 0.0F;
    if (scene == NULL || node_ids == NULL || node_count < 3U ||
        node_count > AF_EDITOR_WORKFLOW_MAX_CLIPBOARD_NODES ||
        mode > AF_EDITOR_DISTRIBUTE_VERTICAL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < node_count; ++index) {
        ordered[index] = af_scene_find(scene, node_ids[index]);
        if (ordered[index] == NULL) {
            return AF_RESULT_NOT_FOUND;
        }
        total_size += mode == AF_EDITOR_DISTRIBUTE_HORIZONTAL ? ordered[index]->bounds.x
                                                              : ordered[index]->bounds.y;
    }
    for (uint32_t index = 1U; index < node_count; ++index) {
        AfSceneNode *value = ordered[index];
        uint32_t write = index;
        const float coordinate = mode == AF_EDITOR_DISTRIBUTE_HORIZONTAL
                                     ? value->local_transform.position.x
                                     : value->local_transform.position.y;
        while (write > 0U) {
            const float previous = mode == AF_EDITOR_DISTRIBUTE_HORIZONTAL
                                       ? ordered[write - 1U]->local_transform.position.x
                                       : ordered[write - 1U]->local_transform.position.y;
            if (previous <= coordinate) {
                break;
            }
            ordered[write] = ordered[write - 1U];
            --write;
        }
        ordered[write] = value;
    }
    start = mode == AF_EDITOR_DISTRIBUTE_HORIZONTAL ? ordered[0]->local_transform.position.x
                                                    : ordered[0]->local_transform.position.y;
    end = mode == AF_EDITOR_DISTRIBUTE_HORIZONTAL
              ? ordered[node_count - 1U]->local_transform.position.x +
                    ordered[node_count - 1U]->bounds.x
              : ordered[node_count - 1U]->local_transform.position.y +
                    ordered[node_count - 1U]->bounds.y;
    gap = (end - start - total_size) / (float)(node_count - 1U);
    for (uint32_t index = 1U; index + 1U < node_count; ++index) {
        const float previous_end =
            mode == AF_EDITOR_DISTRIBUTE_HORIZONTAL
                ? ordered[index - 1U]->local_transform.position.x + ordered[index - 1U]->bounds.x
                : ordered[index - 1U]->local_transform.position.y + ordered[index - 1U]->bounds.y;
        if (mode == AF_EDITOR_DISTRIBUTE_HORIZONTAL) {
            ordered[index]->local_transform.position.x = previous_end + gap;
        } else {
            ordered[index]->local_transform.position.y = previous_end + gap;
        }
    }
    return af_scene_evaluate_world(scene);
}

static bool af_editor_name_matches(const char *name, const char *query) {
    if (query[0] == '\0') {
        return true;
    }
    for (size_t start = 0U; name[start] != '\0'; ++start) {
        size_t offset = 0U;
        while (query[offset] != '\0' && name[start + offset] != '\0' &&
               tolower((unsigned char)query[offset]) ==
                   tolower((unsigned char)name[start + offset])) {
            ++offset;
        }
        if (query[offset] == '\0') {
            return true;
        }
    }
    return false;
}

AfResult af_editor_filter_nodes(const AfScene *scene, const char (*names)[64], const char *query,
                                uint32_t *out_indices, uint32_t capacity, uint32_t *out_count) {
    if (scene == NULL || names == NULL || query == NULL || out_count == NULL ||
        memchr(query, '\0', 128U) == NULL || (capacity != 0U && out_indices == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_count = 0U;
    for (uint32_t index = 0U; index < scene->count; ++index) {
        if (af_editor_name_matches(names[index], query)) {
            if (*out_count >= capacity) {
                return AF_RESULT_LIMIT_EXCEEDED;
            }
            out_indices[(*out_count)++] = index;
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_clipboard_copy(const AfScene *scene, const char (*names)[64],
                                  const uint64_t *node_ids, uint32_t node_count,
                                  AfEditorClipboard *clipboard) {
    if (scene == NULL || names == NULL || node_ids == NULL || clipboard == NULL ||
        node_count == 0U || node_count > AF_EDITOR_WORKFLOW_MAX_CLIPBOARD_NODES) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *clipboard = (AfEditorClipboard){0};
    for (uint32_t index = 0U; index < node_count; ++index) {
        const AfSceneNode *node = af_scene_find_const(scene, node_ids[index]);
        if (node == NULL) {
            *clipboard = (AfEditorClipboard){0};
            return AF_RESULT_NOT_FOUND;
        }
        clipboard->nodes[index] = *node;
        memcpy(clipboard->names[index], names[node - scene->nodes], 64U);
    }
    clipboard->count = node_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_clipboard_paste(const AfEditorClipboard *clipboard, AfScene *scene,
                                   char (*names)[64], uint64_t *next_id, AfVec2 offset,
                                   uint64_t *out_ids, uint32_t out_capacity, uint32_t *out_count) {
    uint64_t mapped[AF_EDITOR_WORKFLOW_MAX_CLIPBOARD_NODES] = {0};
    const uint32_t original_count = scene == NULL ? 0U : scene->count;
    const uint64_t original_next_id = next_id == NULL ? 0U : *next_id;
    if (clipboard == NULL || scene == NULL || names == NULL || next_id == NULL ||
        out_count == NULL || clipboard->count == 0U || clipboard->count > out_capacity ||
        clipboard->count > scene->capacity - scene->count || out_ids == NULL ||
        !isfinite(offset.x) || !isfinite(offset.y)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < clipboard->count; ++index) {
        if (*next_id == 0U || *next_id == AF_INVALID_NODE_ID) {
            scene->count = original_count;
            *next_id = original_next_id;
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        mapped[index] = (*next_id)++;
    }
    for (uint32_t index = 0U; index < clipboard->count; ++index) {
        AfSceneNode node = clipboard->nodes[index];
        const uint64_t old_parent = node.parent_id;
        node.id = mapped[index];
        node.local_transform.position.x += offset.x;
        node.local_transform.position.y += offset.y;
        node.parent_id = AF_INVALID_NODE_ID;
        for (uint32_t parent = 0U; parent < clipboard->count; ++parent) {
            if (clipboard->nodes[parent].id == old_parent) {
                node.parent_id = mapped[parent];
                break;
            }
        }
        scene->nodes[scene->count] = node;
        memcpy(names[scene->count], clipboard->names[index], 64U);
        out_ids[index] = mapped[index];
        ++scene->count;
    }
    if (af_scene_evaluate_world(scene) != AF_RESULT_SUCCESS) {
        scene->count = original_count;
        *next_id = original_next_id;
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_count = clipboard->count;
    return AF_RESULT_SUCCESS;
}
