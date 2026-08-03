#include "project/project.h"

#include "animation/animation.h"
#include "core/file_io.h"
#include "core/safe_math.h"

#include <errno.h>
#include <math.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    AF_PROJECT_FIELD_ID = 1U << 0U,
    AF_PROJECT_FIELD_PARENT = 1U << 1U,
    AF_PROJECT_FIELD_TYPE = 1U << 2U,
    AF_PROJECT_FIELD_NAME = 1U << 3U,
    AF_PROJECT_FIELD_POSITION = 1U << 4U,
    AF_PROJECT_FIELD_ROTATION = 1U << 5U,
    AF_PROJECT_FIELD_SCALE = 1U << 6U,
    AF_PROJECT_FIELD_BOUNDS = 1U << 7U,
    AF_PROJECT_FIELD_FILL = 1U << 8U,
    AF_PROJECT_FIELD_OPACITY = 1U << 9U,
    AF_PROJECT_FIELD_RADIUS = 1U << 10U,
    AF_PROJECT_FIELD_LAYER = 1U << 11U,
    AF_PROJECT_FIELD_VISIBLE = 1U << 12U,
    AF_PROJECT_REQUIRED_FIELDS = (1U << 13U) - 1U
};

enum {
    AF_PROJECT_CLIP_ID = 1U << 0U,
    AF_PROJECT_CLIP_NAME = 1U << 1U,
    AF_PROJECT_CLIP_DURATION = 1U << 2U,
    AF_PROJECT_CLIP_LOOP = 1U << 3U,
    AF_PROJECT_CLIP_REGION = 1U << 4U,
    AF_PROJECT_CLIP_REQUIRED = (1U << 5U) - 1U,
    AF_PROJECT_TRACK_ID = 1U << 0U,
    AF_PROJECT_TRACK_CLIP = 1U << 1U,
    AF_PROJECT_TRACK_NODE = 1U << 2U,
    AF_PROJECT_TRACK_PROPERTY = 1U << 3U,
    AF_PROJECT_TRACK_REQUIRED = (1U << 4U) - 1U,
    AF_PROJECT_KEY_ID = 1U << 0U,
    AF_PROJECT_KEY_TRACK = 1U << 1U,
    AF_PROJECT_KEY_TIME = 1U << 2U,
    AF_PROJECT_KEY_VALUE = 1U << 3U,
    AF_PROJECT_KEY_INTERPOLATION = 1U << 4U,
    AF_PROJECT_KEY_EASING = 1U << 5U,
    AF_PROJECT_KEY_REQUIRED = (1U << 6U) - 1U,
    AF_PROJECT_PATH_ID = 1U << 0U,
    AF_PROJECT_PATH_NODE = 1U << 1U,
    AF_PROJECT_PATH_FILL_RULE = 1U << 2U,
    AF_PROJECT_PATH_GENERATION = 1U << 3U,
    AF_PROJECT_PATH_REQUIRED = (1U << 4U) - 1U
};

typedef struct AfProjectTextWriter {
    char *data;
    size_t capacity;
    size_t length;
} AfProjectTextWriter;

static bool af_project_name_valid(const char *name) {
    size_t length = 0U;
    if (name == NULL || name[0] == '\0') {
        return false;
    }
    length = strlen(name);
    if (length >= AF_PROJECT_NAME_CAPACITY) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        const unsigned char byte = (unsigned char)name[index];
        if (byte < 32U || byte == 127U) {
            return false;
        }
    }
    return true;
}

AfResult af_project_create(AfProject *project, AfAllocator *allocator, uint32_t capacity) {
    AfSceneNode *nodes = NULL;
    char (*names)[AF_PROJECT_NAME_CAPACITY] = NULL;
    AfProjectClip *clips = NULL;
    AfProjectTrack *tracks = NULL;
    AfProjectKeyframe *keyframes = NULL;
    AfPath *paths = NULL;
    AfPathCommand *path_commands = NULL;
    AfStyle *styles = NULL;
    AfLayoutNode *layouts = NULL;
    uint64_t *validation_ids = NULL;
    if (project == NULL || allocator == NULL || capacity == 0U || capacity > AF_MAX_NODES) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *project = (AfProject){0};
    nodes = af_allocate_array(allocator, capacity, sizeof(*nodes), alignof(AfSceneNode));
    names = af_allocate_array(allocator, capacity, sizeof(*names), alignof(char));
    clips =
        af_allocate_array(allocator, AF_PROJECT_MAX_CLIPS, sizeof(*clips), alignof(AfProjectClip));
    tracks = af_allocate_array(allocator, AF_PROJECT_MAX_TRACKS, sizeof(*tracks),
                               alignof(AfProjectTrack));
    keyframes = af_allocate_array(allocator, AF_PROJECT_MAX_KEYFRAMES, sizeof(*keyframes),
                                  alignof(AfProjectKeyframe));
    paths = af_allocate_array(allocator, AF_MAX_PATHS, sizeof(*paths), alignof(AfPath));
    path_commands = af_allocate_array(allocator, AF_MAX_PATH_COMMANDS, sizeof(*path_commands),
                                      alignof(AfPathCommand));
    styles = af_allocate_array(allocator, AF_MAX_STYLES, sizeof(*styles), alignof(AfStyle));
    layouts =
        af_allocate_array(allocator, AF_MAX_LAYOUT_NODES, sizeof(*layouts), alignof(AfLayoutNode));
    validation_ids = af_allocate_array(allocator, AF_PROJECT_VALIDATION_ID_CAPACITY,
                                       sizeof(*validation_ids), alignof(uint64_t));
    if (nodes == NULL || names == NULL || clips == NULL || tracks == NULL || keyframes == NULL ||
        paths == NULL || path_commands == NULL || styles == NULL || layouts == NULL ||
        validation_ids == NULL) {
        af_deallocate(allocator, validation_ids);
        af_deallocate(allocator, path_commands);
        af_deallocate(allocator, styles);
        af_deallocate(allocator, layouts);
        af_deallocate(allocator, paths);
        af_deallocate(allocator, keyframes);
        af_deallocate(allocator, tracks);
        af_deallocate(allocator, clips);
        af_deallocate(allocator, names);
        af_deallocate(allocator, nodes);
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memset(nodes, 0, (size_t)capacity * sizeof(*nodes));
    memset(names, 0, (size_t)capacity * sizeof(*names));
    memset(clips, 0, AF_PROJECT_MAX_CLIPS * sizeof(*clips));
    memset(tracks, 0, AF_PROJECT_MAX_TRACKS * sizeof(*tracks));
    memset(keyframes, 0, AF_PROJECT_MAX_KEYFRAMES * sizeof(*keyframes));
    memset(paths, 0, AF_MAX_PATHS * sizeof(*paths));
    memset(path_commands, 0, AF_MAX_PATH_COMMANDS * sizeof(*path_commands));
    memset(styles, 0, AF_MAX_STYLES * sizeof(*styles));
    memset(layouts, 0, AF_MAX_LAYOUT_NODES * sizeof(*layouts));
    memset(validation_ids, 0, AF_PROJECT_VALIDATION_ID_CAPACITY * sizeof(*validation_ids));
    af_scene_init(&project->scene, nodes, capacity);
    project->names = names;
    project->canvas_size = (AfVec2){1280.0F, 720.0F};
    project->next_id = 1U;
    project->clips = clips;
    project->clip_capacity = AF_PROJECT_MAX_CLIPS;
    project->tracks = tracks;
    project->track_capacity = AF_PROJECT_MAX_TRACKS;
    project->keyframes = keyframes;
    project->keyframe_capacity = AF_PROJECT_MAX_KEYFRAMES;
    project->next_animation_id = 1U;
    project->path_data = (AfPathData){.paths = paths,
                                      .path_capacity = AF_MAX_PATHS,
                                      .commands = path_commands,
                                      .command_capacity = AF_MAX_PATH_COMMANDS};
    project->scene.paths = &project->path_data;
    project->style_data = (AfStyleData){.styles = styles, .capacity = AF_MAX_STYLES};
    project->scene.styles = &project->style_data;
    project->layout_data = (AfLayoutData){.nodes = layouts,
                                          .capacity = AF_MAX_LAYOUT_NODES,
                                          .reference_size = project->canvas_size,
                                          .target_size = project->canvas_size,
                                          .scale_mode = AF_CANVAS_SCALE_FIT};
    project->scene.layouts = &project->layout_data;
    project->validation_ids = validation_ids;
    project->allocator = allocator;
    return AF_RESULT_SUCCESS;
}

void af_project_destroy(AfProject *project) {
    if (project == NULL) {
        return;
    }
    if (project->allocator != NULL) {
        af_deallocate(project->allocator, project->validation_ids);
        af_deallocate(project->allocator, project->path_data.commands);
        af_deallocate(project->allocator, project->style_data.styles);
        af_deallocate(project->allocator, project->layout_data.nodes);
        af_deallocate(project->allocator, project->path_data.paths);
        af_deallocate(project->allocator, project->keyframes);
        af_deallocate(project->allocator, project->tracks);
        af_deallocate(project->allocator, project->clips);
        af_deallocate(project->allocator, project->names);
        af_deallocate(project->allocator, project->scene.nodes);
    }
    *project = (AfProject){0};
}

static bool af_project_validation_insert(const AfProject *project, uint64_t id) {
    uint32_t slot = (uint32_t)((id * UINT64_C(11400714819323198485)) &
                               (uint64_t)(AF_PROJECT_VALIDATION_ID_CAPACITY - 1U));
    for (uint32_t probe = 0U; probe < AF_PROJECT_VALIDATION_ID_CAPACITY; ++probe) {
        if (project->validation_ids[slot] == 0U) {
            project->validation_ids[slot] = id;
            return true;
        }
        if (project->validation_ids[slot] == id) {
            return false;
        }
        slot = (slot + 1U) & (AF_PROJECT_VALIDATION_ID_CAPACITY - 1U);
    }
    return false;
}

uint32_t af_project_find_index(const AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return UINT32_MAX;
    }
    for (index = 0U; index < project->scene.count; ++index) {
        if (project->scene.nodes[index].id == id) {
            return index;
        }
    }
    return UINT32_MAX;
}

const char *af_project_name(const AfProject *project, uint64_t id) {
    const uint32_t index = af_project_find_index(project, id);
    return index == UINT32_MAX ? NULL : project->names[index];
}

AfResult af_project_validate(const AfProject *project) {
    uint32_t index = 0U;
    uint64_t maximum_id = 0U;
    uint64_t maximum_animation_id = 0U;
    if (project == NULL || project->names == NULL || project->scene.nodes == NULL ||
        project->allocator == NULL || !isfinite(project->canvas_size.x) ||
        !isfinite(project->canvas_size.y) || project->canvas_size.x <= 0.0F ||
        project->canvas_size.y <= 0.0F || project->canvas_size.x > 1000000.0F ||
        project->canvas_size.y > 1000000.0F || project->next_id == 0U ||
        project->next_id == AF_INVALID_NODE_ID || project->clips == NULL ||
        project->tracks == NULL || project->keyframes == NULL || project->validation_ids == NULL ||
        project->clip_count > project->clip_capacity ||
        project->clip_capacity > AF_PROJECT_MAX_CLIPS ||
        project->track_count > project->track_capacity ||
        project->track_capacity > AF_PROJECT_MAX_TRACKS ||
        project->keyframe_count > project->keyframe_capacity ||
        project->keyframe_capacity > AF_PROJECT_MAX_KEYFRAMES || project->next_animation_id == 0U ||
        project->next_animation_id == UINT64_MAX ||
        af_scene_validate(&project->scene) != AF_RESULT_SUCCESS ||
        af_path_data_validate(&project->path_data) != AF_RESULT_SUCCESS ||
        af_style_data_validate(&project->style_data) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (af_layout_validate(&project->layout_data, &project->scene) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    memset(project->validation_ids, 0,
           AF_PROJECT_VALIDATION_ID_CAPACITY * sizeof(*project->validation_ids));
    for (index = 0U; index < project->scene.count; ++index) {
        if (!af_project_name_valid(project->names[index]) ||
            project->scene.nodes[index].id >= AF_INVALID_NODE_ID - 1U) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        if (project->scene.nodes[index].id > maximum_id) {
            maximum_id = project->scene.nodes[index].id;
        }
    }
    for (index = 0U; index < project->style_data.count; ++index) {
        const AfStyle *style = &project->style_data.styles[index];
        if (style->node_id != 0U && af_scene_find_const(&project->scene, style->node_id) == NULL) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (index = 0U; index < project->scene.count; ++index) {
        if (af_style_find_node_const(&project->style_data, project->scene.nodes[index].id) ==
            NULL) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    if (project->next_id <= maximum_id) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < project->path_data.path_count; ++index) {
        const AfPath *path = &project->path_data.paths[index];
        const AfSceneNode *node = af_scene_find_const(&project->scene, path->node_id);
        if (node == NULL || node->type != AF_NODE_PATH) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (index = 0U; index < project->scene.count; ++index) {
        const AfSceneNode *node = &project->scene.nodes[index];
        if (node->type == AF_NODE_PATH &&
            af_path_find_node_const(&project->path_data, node->id) == NULL) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (index = 0U; index < project->clip_count; ++index) {
        const AfProjectClip *clip = &project->clips[index];
        if (clip->id == 0U || clip->id >= UINT64_MAX - 1U || !af_project_name_valid(clip->name) ||
            !af_project_validation_insert(project, clip->id) || !isfinite(clip->duration) ||
            !isfinite(clip->loop_start) || !isfinite(clip->loop_end) || clip->duration <= 0.0F ||
            clip->duration > 86400.0F || clip->loop_start < 0.0F ||
            clip->loop_end < clip->loop_start || clip->loop_end > clip->duration) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        maximum_animation_id = clip->id > maximum_animation_id ? clip->id : maximum_animation_id;
    }
    for (index = 0U; index < project->track_count; ++index) {
        const AfProjectTrack *track = &project->tracks[index];
        uint32_t other = 0U;
        if (track->id == 0U || track->id >= UINT64_MAX - 1U ||
            !af_project_validation_insert(project, track->id) ||
            af_project_find_clip_const(project, track->clip_id) == NULL ||
            af_scene_find_const(&project->scene, track->node_id) == NULL ||
            track->property > AF_PROPERTY_CORNER_RADIUS) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (other = index + 1U; other < project->track_count; ++other) {
            const AfProjectTrack *candidate = &project->tracks[other];
            if (track->clip_id == candidate->clip_id && track->node_id == candidate->node_id &&
                track->property == candidate->property) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        maximum_animation_id = track->id > maximum_animation_id ? track->id : maximum_animation_id;
    }
    for (index = 0U; index < project->keyframe_count; ++index) {
        const AfProjectKeyframe *key = &project->keyframes[index];
        const AfProjectTrack *track = af_project_find_track_const(project, key->track_id);
        const AfProjectClip *clip =
            track == NULL ? NULL : af_project_find_clip_const(project, track->clip_id);
        uint32_t component = 0U;
        if (key->id == 0U || key->id >= UINT64_MAX - 1U || track == NULL || clip == NULL ||
            !af_project_validation_insert(project, key->id) || !isfinite(key->keyframe.time) ||
            key->keyframe.time < 0.0F || key->keyframe.time > clip->duration ||
            key->keyframe.interpolation > AF_INTERPOLATION_CUBIC_BEZIER) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (component = 0U; component < 4U; ++component) {
            if (!isfinite(key->keyframe.value[component]) ||
                !isfinite(key->keyframe.easing[component])) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
        if (key->keyframe.interpolation == AF_INTERPOLATION_CUBIC_BEZIER &&
            (key->keyframe.easing[0] < 0.0F || key->keyframe.easing[0] > 1.0F ||
             key->keyframe.easing[2] < 0.0F || key->keyframe.easing[2] > 1.0F)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        maximum_animation_id = key->id > maximum_animation_id ? key->id : maximum_animation_id;
    }
    return project->next_animation_id > maximum_animation_id ? AF_RESULT_SUCCESS
                                                             : AF_RESULT_INVALID_ARGUMENT;
}

AfResult af_project_set_name(AfProject *project, uint64_t id, const char *name) {
    const uint32_t index = af_project_find_index(project, id);
    if (index == UINT32_MAX) {
        return AF_RESULT_NOT_FOUND;
    }
    if (!af_project_name_valid(name)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    (void)snprintf(project->names[index], AF_PROJECT_NAME_CAPACITY, "%s", name);
    return AF_RESULT_SUCCESS;
}

AfResult af_project_add_node(AfProject *project, const AfSceneNode *node, const char *name) {
    AfResult result = AF_RESULT_SUCCESS;
    if (project == NULL || node == NULL || !af_project_name_valid(name) ||
        node->id >= AF_INVALID_NODE_ID - 1U ||
        project->style_data.count >= project->style_data.capacity ||
        af_style_find_const(&project->style_data, node->id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_scene_add_node(&project->scene, node);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    (void)snprintf(project->names[project->scene.count - 1U], AF_PROJECT_NAME_CAPACITY, "%s", name);
    if (af_scene_validate(&project->scene) != AF_RESULT_SUCCESS) {
        --project->scene.count;
        return AF_RESULT_INVALID_ARGUMENT;
    }
    project->style_data.styles[project->style_data.count++] =
        af_style_default(node->id, node->id, node->fill);
    if (project->next_id <= node->id) {
        project->next_id = node->id + 1U;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_insert_snapshot(AfProject *project, const AfProjectNodeSnapshot *snapshot) {
    uint32_t index = 0U;
    AfStyle style;
    if (project == NULL || snapshot == NULL || !af_project_name_valid(snapshot->name) ||
        snapshot->index > project->scene.count || project->scene.count >= project->scene.capacity ||
        af_project_find_index(project, snapshot->node.id) != UINT32_MAX ||
        snapshot->node.id == 0U || snapshot->node.id >= AF_INVALID_NODE_ID - 1U ||
        project->style_data.count >= project->style_data.capacity ||
        (snapshot->has_layout && project->layout_data.count >= project->layout_data.capacity)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    style = snapshot->style;
    if (af_style_validate(&style) != AF_RESULT_SUCCESS || style.node_id != snapshot->node.id ||
        af_style_find_const(&project->style_data, style.id) != NULL) {
        style = af_style_default(snapshot->node.id, snapshot->node.id, snapshot->node.fill);
    }
    for (index = project->scene.count; index > snapshot->index; --index) {
        project->scene.nodes[index] = project->scene.nodes[index - 1U];
        memcpy(project->names[index], project->names[index - 1U], AF_PROJECT_NAME_CAPACITY);
    }
    project->scene.nodes[snapshot->index] = snapshot->node;
    memcpy(project->names[snapshot->index], snapshot->name, AF_PROJECT_NAME_CAPACITY);
    ++project->scene.count;
    project->style_data.styles[project->style_data.count++] = style;
    if (snapshot->has_layout) {
        AfLayoutNode layout = snapshot->layout;
        layout.node_id = snapshot->node.id;
        project->layout_data.nodes[project->layout_data.count++] = layout;
    }
    if (project->next_id <= snapshot->node.id) {
        project->next_id = snapshot->node.id + 1U;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_remove_node_exact(AfProject *project, uint64_t id) {
    uint32_t index = af_project_find_index(project, id);
    if (index == UINT32_MAX) {
        return AF_RESULT_NOT_FOUND;
    }
    for (; index + 1U < project->scene.count; ++index) {
        project->scene.nodes[index] = project->scene.nodes[index + 1U];
        memcpy(project->names[index], project->names[index + 1U], AF_PROJECT_NAME_CAPACITY);
    }
    --project->scene.count;
    for (uint32_t style_index = 0U; style_index < project->style_data.count; ++style_index) {
        if (project->style_data.styles[style_index].node_id == id) {
            memmove(&project->style_data.styles[style_index],
                    &project->style_data.styles[style_index + 1U],
                    (size_t)(project->style_data.count - style_index - 1U) * sizeof(AfStyle));
            --project->style_data.count;
            break;
        }
    }
    for (uint32_t layout_index = 0U; layout_index < project->layout_data.count; ++layout_index) {
        if (project->layout_data.nodes[layout_index].node_id == id) {
            memmove(&project->layout_data.nodes[layout_index],
                    &project->layout_data.nodes[layout_index + 1U],
                    (size_t)(project->layout_data.count - layout_index - 1U) *
                        sizeof(AfLayoutNode));
            --project->layout_data.count;
            break;
        }
    }
    return AF_RESULT_SUCCESS;
}

static bool af_project_is_descendant(const AfProject *project, uint64_t candidate, uint64_t root) {
    uint32_t depth = 0U;
    uint64_t current = candidate;
    while (current != AF_INVALID_NODE_ID && depth++ <= AF_MAX_HIERARCHY_DEPTH) {
        const AfSceneNode *node = af_scene_find_const(&project->scene, current);
        if (current == root) {
            return true;
        }
        if (node == NULL) {
            return false;
        }
        current = node->parent_id;
    }
    return false;
}

AfResult af_project_collect_subtree(const AfProject *project, uint64_t id,
                                    AfProjectNodeSnapshot *snapshots, uint32_t capacity,
                                    uint32_t *out_count) {
    uint32_t count = 0U;
    uint32_t index = 0U;
    if (project == NULL || snapshots == NULL || out_count == NULL ||
        af_project_find_index(project, id) == UINT32_MAX) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < project->scene.count; ++index) {
        if (af_project_is_descendant(project, project->scene.nodes[index].id, id)) {
            if (count >= capacity) {
                return AF_RESULT_LIMIT_EXCEEDED;
            }
            snapshots[count] =
                (AfProjectNodeSnapshot){.node = project->scene.nodes[index], .index = index};
            snapshots[count].style =
                *af_style_find_node_const(&project->style_data, project->scene.nodes[index].id);
            {
                const AfLayoutNode *layout =
                    af_layout_find_const(&project->layout_data, project->scene.nodes[index].id);
                if (layout != NULL) {
                    snapshots[count].layout = *layout;
                    snapshots[count].has_layout = true;
                }
            }
            memcpy(snapshots[count].name, project->names[index], AF_PROJECT_NAME_CAPACITY);
            ++count;
        }
    }
    *out_count = count;
    return AF_RESULT_SUCCESS;
}

AfResult af_project_delete_subtree(AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    uint32_t write_index = 0U;
    if (project == NULL || af_project_find_index(project, id) == UINT32_MAX) {
        return AF_RESULT_NOT_FOUND;
    }
    for (index = 0U; index < project->scene.count; ++index) {
        if (af_project_is_descendant(project, project->scene.nodes[index].id, id)) {
            continue;
        }
        if (write_index != index) {
            project->scene.nodes[write_index] = project->scene.nodes[index];
            memcpy(project->names[write_index], project->names[index], AF_PROJECT_NAME_CAPACITY);
        }
        ++write_index;
    }
    project->scene.count = write_index;
    for (index = 0U; index < project->style_data.count;) {
        const uint64_t node_id = project->style_data.styles[index].node_id;
        if (node_id != 0U && af_scene_find_const(&project->scene, node_id) == NULL) {
            memmove(&project->style_data.styles[index], &project->style_data.styles[index + 1U],
                    (size_t)(project->style_data.count - index - 1U) * sizeof(AfStyle));
            --project->style_data.count;
        } else {
            ++index;
        }
    }
    for (index = 0U; index < project->layout_data.count;) {
        if (af_scene_find_const(&project->scene, project->layout_data.nodes[index].node_id) ==
            NULL) {
            memmove(&project->layout_data.nodes[index], &project->layout_data.nodes[index + 1U],
                    (size_t)(project->layout_data.count - index - 1U) * sizeof(AfLayoutNode));
            --project->layout_data.count;
        } else {
            ++index;
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_snapshot(const AfProject *project, uint64_t id,
                             AfProjectNodeSnapshot *out_snapshot) {
    const uint32_t index = af_project_find_index(project, id);
    if (out_snapshot == NULL || index == UINT32_MAX) {
        return AF_RESULT_NOT_FOUND;
    }
    *out_snapshot = (AfProjectNodeSnapshot){.node = project->scene.nodes[index], .index = index};
    out_snapshot->style = *af_style_find_node_const(&project->style_data, id);
    {
        const AfLayoutNode *layout = af_layout_find_const(&project->layout_data, id);
        if (layout != NULL) {
            out_snapshot->layout = *layout;
            out_snapshot->has_layout = true;
        }
    }
    memcpy(out_snapshot->name, project->names[index], AF_PROJECT_NAME_CAPACITY);
    return AF_RESULT_SUCCESS;
}

AfStyle *af_project_find_node_style(AfProject *project, uint64_t node_id) {
    if (project == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < project->style_data.count; ++index) {
        if (project->style_data.styles[index].node_id == node_id) {
            return &project->style_data.styles[index];
        }
    }
    return NULL;
}

AfResult af_project_add_style_preset(AfProject *project, const AfStyle *style) {
    uint32_t preset_count = 0U;
    if (project == NULL || style == NULL || style->node_id != 0U ||
        project->style_data.count >= project->style_data.capacity ||
        af_style_validate(style) != AF_RESULT_SUCCESS ||
        af_style_find_const(&project->style_data, style->id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < project->style_data.count; ++index) {
        preset_count += project->style_data.styles[index].node_id == 0U ? 1U : 0U;
    }
    if (preset_count >= AF_MAX_STYLE_PRESETS) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    project->style_data.styles[project->style_data.count++] = *style;
    return AF_RESULT_SUCCESS;
}

AfResult af_project_set_layout(AfProject *project, const AfLayoutNode *layout) {
    if (project == NULL || layout == NULL ||
        af_scene_find_const(&project->scene, layout->node_id) == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < project->layout_data.count; ++index) {
        if (project->layout_data.nodes[index].node_id == layout->node_id) {
            const AfLayoutNode previous = project->layout_data.nodes[index];
            project->layout_data.nodes[index] = *layout;
            if (af_layout_validate(&project->layout_data, &project->scene) != AF_RESULT_SUCCESS) {
                project->layout_data.nodes[index] = previous;
                return AF_RESULT_INVALID_ARGUMENT;
            }
            return AF_RESULT_SUCCESS;
        }
    }
    if (project->layout_data.count >= project->layout_data.capacity) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    project->layout_data.nodes[project->layout_data.count++] = *layout;
    if (af_layout_validate(&project->layout_data, &project->scene) != AF_RESULT_SUCCESS) {
        --project->layout_data.count;
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_clear_layout(AfProject *project, uint64_t node_id) {
    if (project == NULL || af_scene_find_const(&project->scene, node_id) == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < project->layout_data.count; ++index) {
        if (project->layout_data.nodes[index].node_id == node_id) {
            memmove(&project->layout_data.nodes[index], &project->layout_data.nodes[index + 1U],
                    (size_t)(project->layout_data.count - index - 1U) * sizeof(AfLayoutNode));
            --project->layout_data.count;
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_NOT_FOUND;
}

AfResult af_project_allocate_id(AfProject *project, uint64_t *out_id) {
    if (project == NULL || out_id == NULL || project->next_id == 0U ||
        project->next_id >= AF_INVALID_NODE_ID - 1U) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    *out_id = project->next_id++;
    return AF_RESULT_SUCCESS;
}

AfProjectClip *af_project_find_clip(AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return NULL;
    }
    for (index = 0U; index < project->clip_count; ++index) {
        if (project->clips[index].id == id) {
            return &project->clips[index];
        }
    }
    return NULL;
}

const AfProjectClip *af_project_find_clip_const(const AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return NULL;
    }
    for (index = 0U; index < project->clip_count; ++index) {
        if (project->clips[index].id == id) {
            return &project->clips[index];
        }
    }
    return NULL;
}

AfProjectTrack *af_project_find_track(AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return NULL;
    }
    for (index = 0U; index < project->track_count; ++index) {
        if (project->tracks[index].id == id) {
            return &project->tracks[index];
        }
    }
    return NULL;
}

const AfProjectTrack *af_project_find_track_const(const AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return NULL;
    }
    for (index = 0U; index < project->track_count; ++index) {
        if (project->tracks[index].id == id) {
            return &project->tracks[index];
        }
    }
    return NULL;
}

AfProjectKeyframe *af_project_find_keyframe(AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return NULL;
    }
    for (index = 0U; index < project->keyframe_count; ++index) {
        if (project->keyframes[index].id == id) {
            return &project->keyframes[index];
        }
    }
    return NULL;
}

const AfProjectKeyframe *af_project_find_keyframe_const(const AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return NULL;
    }
    for (index = 0U; index < project->keyframe_count; ++index) {
        if (project->keyframes[index].id == id) {
            return &project->keyframes[index];
        }
    }
    return NULL;
}

AfResult af_project_insert_clip_snapshot(AfProject *project,
                                         const AfProjectClipSnapshot *snapshot) {
    uint32_t index = 0U;
    if (project == NULL || snapshot == NULL || snapshot->index > project->clip_count ||
        project->clip_count >= project->clip_capacity || snapshot->clip.id == 0U ||
        af_project_find_clip(project, snapshot->clip.id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (index = project->clip_count; index > snapshot->index; --index) {
        project->clips[index] = project->clips[index - 1U];
    }
    project->clips[snapshot->index] = snapshot->clip;
    ++project->clip_count;
    if (project->next_animation_id <= snapshot->clip.id) {
        project->next_animation_id = snapshot->clip.id + 1U;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_insert_track_snapshot(AfProject *project,
                                          const AfProjectTrackSnapshot *snapshot) {
    uint32_t index = 0U;
    if (project == NULL || snapshot == NULL || snapshot->index > project->track_count ||
        project->track_count >= project->track_capacity || snapshot->track.id == 0U ||
        af_project_find_track(project, snapshot->track.id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (index = project->track_count; index > snapshot->index; --index) {
        project->tracks[index] = project->tracks[index - 1U];
    }
    project->tracks[snapshot->index] = snapshot->track;
    ++project->track_count;
    if (project->next_animation_id <= snapshot->track.id) {
        project->next_animation_id = snapshot->track.id + 1U;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_insert_keyframe_snapshot(AfProject *project,
                                             const AfProjectKeyframeSnapshot *snapshot) {
    uint32_t index = 0U;
    if (project == NULL || snapshot == NULL || snapshot->index > project->keyframe_count ||
        project->keyframe_count >= project->keyframe_capacity || snapshot->keyframe.id == 0U ||
        af_project_find_keyframe(project, snapshot->keyframe.id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (index = project->keyframe_count; index > snapshot->index; --index) {
        project->keyframes[index] = project->keyframes[index - 1U];
    }
    project->keyframes[snapshot->index] = snapshot->keyframe;
    ++project->keyframe_count;
    if (project->next_animation_id <= snapshot->keyframe.id) {
        project->next_animation_id = snapshot->keyframe.id + 1U;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_add_clip(AfProject *project, const AfProjectClip *clip) {
    AfProjectClipSnapshot snapshot = {0};
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    if (project == NULL || clip == NULL) {
        return result;
    }
    snapshot = (AfProjectClipSnapshot){.clip = *clip, .index = project->clip_count};
    result = af_project_insert_clip_snapshot(project, &snapshot);
    if (result == AF_RESULT_SUCCESS && af_project_validate(project) != AF_RESULT_SUCCESS) {
        (void)af_project_remove_clip_exact(project, clip->id);
        result = AF_RESULT_INVALID_ARGUMENT;
    }
    return result;
}

AfResult af_project_add_track(AfProject *project, const AfProjectTrack *track) {
    AfProjectTrackSnapshot snapshot = {0};
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    if (project == NULL || track == NULL) {
        return result;
    }
    snapshot = (AfProjectTrackSnapshot){.track = *track, .index = project->track_count};
    result = af_project_insert_track_snapshot(project, &snapshot);
    if (result == AF_RESULT_SUCCESS && af_project_validate(project) != AF_RESULT_SUCCESS) {
        (void)af_project_remove_track_exact(project, track->id);
        result = AF_RESULT_INVALID_ARGUMENT;
    }
    return result;
}

AfResult af_project_add_keyframe(AfProject *project, const AfProjectKeyframe *keyframe) {
    AfProjectKeyframeSnapshot snapshot = {0};
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    if (project == NULL || keyframe == NULL) {
        return result;
    }
    snapshot = (AfProjectKeyframeSnapshot){.keyframe = *keyframe, .index = project->keyframe_count};
    result = af_project_insert_keyframe_snapshot(project, &snapshot);
    if (result == AF_RESULT_SUCCESS && af_project_validate(project) != AF_RESULT_SUCCESS) {
        (void)af_project_remove_keyframe_exact(project, keyframe->id);
        result = AF_RESULT_INVALID_ARGUMENT;
    }
    return result;
}

AfResult af_project_remove_clip_exact(AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    while (index < project->clip_count && project->clips[index].id != id) {
        ++index;
    }
    if (index == project->clip_count) {
        return AF_RESULT_NOT_FOUND;
    }
    for (; index + 1U < project->clip_count; ++index) {
        project->clips[index] = project->clips[index + 1U];
    }
    --project->clip_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_project_remove_track_exact(AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    while (index < project->track_count && project->tracks[index].id != id) {
        ++index;
    }
    if (index == project->track_count) {
        return AF_RESULT_NOT_FOUND;
    }
    for (; index + 1U < project->track_count; ++index) {
        project->tracks[index] = project->tracks[index + 1U];
    }
    --project->track_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_project_remove_keyframe_exact(AfProject *project, uint64_t id) {
    uint32_t index = 0U;
    if (project == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    while (index < project->keyframe_count && project->keyframes[index].id != id) {
        ++index;
    }
    if (index == project->keyframe_count) {
        return AF_RESULT_NOT_FOUND;
    }
    for (; index + 1U < project->keyframe_count; ++index) {
        project->keyframes[index] = project->keyframes[index + 1U];
    }
    --project->keyframe_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_project_clip_snapshot(const AfProject *project, uint64_t id,
                                  AfProjectClipSnapshot *out_snapshot) {
    uint32_t index = 0U;
    if (project == NULL || out_snapshot == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    while (index < project->clip_count && project->clips[index].id != id) {
        ++index;
    }
    if (index == project->clip_count) {
        return AF_RESULT_NOT_FOUND;
    }
    *out_snapshot = (AfProjectClipSnapshot){.clip = project->clips[index], .index = index};
    return AF_RESULT_SUCCESS;
}

AfResult af_project_track_snapshot(const AfProject *project, uint64_t id,
                                   AfProjectTrackSnapshot *out_snapshot) {
    uint32_t index = 0U;
    if (project == NULL || out_snapshot == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    while (index < project->track_count && project->tracks[index].id != id) {
        ++index;
    }
    if (index == project->track_count) {
        return AF_RESULT_NOT_FOUND;
    }
    *out_snapshot = (AfProjectTrackSnapshot){.track = project->tracks[index], .index = index};
    return AF_RESULT_SUCCESS;
}

AfResult af_project_keyframe_snapshot(const AfProject *project, uint64_t id,
                                      AfProjectKeyframeSnapshot *out_snapshot) {
    uint32_t index = 0U;
    if (project == NULL || out_snapshot == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    while (index < project->keyframe_count && project->keyframes[index].id != id) {
        ++index;
    }
    if (index == project->keyframe_count) {
        return AF_RESULT_NOT_FOUND;
    }
    *out_snapshot =
        (AfProjectKeyframeSnapshot){.keyframe = project->keyframes[index], .index = index};
    return AF_RESULT_SUCCESS;
}

AfResult af_project_allocate_animation_id(AfProject *project, uint64_t *out_id) {
    if (project == NULL || out_id == NULL || project->next_animation_id == 0U ||
        project->next_animation_id >= UINT64_MAX - 1U) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    *out_id = project->next_animation_id++;
    return AF_RESULT_SUCCESS;
}

AfPath *af_project_find_path(AfProject *project, uint64_t path_id) {
    if (project == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < project->path_data.path_count; ++index) {
        if (project->path_data.paths[index].id == path_id) {
            return &project->path_data.paths[index];
        }
    }
    return NULL;
}

AfPath *af_project_find_node_path(AfProject *project, uint64_t node_id) {
    if (project == NULL) {
        return NULL;
    }
    for (uint32_t index = 0U; index < project->path_data.path_count; ++index) {
        if (project->path_data.paths[index].node_id == node_id) {
            return &project->path_data.paths[index];
        }
    }
    return NULL;
}

AfResult af_project_insert_path_snapshot(AfProject *project, const AfProjectPathSnapshot *snapshot,
                                         const AfPathCommand *commands) {
    uint32_t command_count = 0U;
    uint32_t command_start = 0U;
    if (project == NULL || snapshot == NULL || commands == NULL || snapshot->path.id == 0U ||
        snapshot->path.node_id == 0U || snapshot->path.command_count == 0U ||
        snapshot->path.command_count > AF_MAX_PATH_COMMANDS_PER_PATH ||
        snapshot->index > project->path_data.path_count ||
        project->path_data.path_count >= project->path_data.path_capacity ||
        snapshot->path.command_start > project->path_data.command_count ||
        snapshot->path.command_count >
            project->path_data.command_capacity - project->path_data.command_count ||
        af_project_find_path(project, snapshot->path.id) != NULL ||
        af_project_find_node_path(project, snapshot->path.node_id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    command_count = snapshot->path.command_count;
    command_start = snapshot->path.command_start;
    memmove(&project->path_data.commands[command_start + command_count],
            &project->path_data.commands[command_start],
            (size_t)(project->path_data.command_count - command_start) *
                sizeof(*project->path_data.commands));
    memcpy(&project->path_data.commands[command_start], commands,
           (size_t)command_count * sizeof(*commands));
    for (uint32_t index = 0U; index < project->path_data.path_count; ++index) {
        if (project->path_data.paths[index].command_start >= command_start) {
            project->path_data.paths[index].command_start += command_count;
        }
    }
    memmove(&project->path_data.paths[snapshot->index + 1U],
            &project->path_data.paths[snapshot->index],
            (size_t)(project->path_data.path_count - snapshot->index) *
                sizeof(*project->path_data.paths));
    project->path_data.paths[snapshot->index] = snapshot->path;
    ++project->path_data.path_count;
    project->path_data.command_count += command_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_project_add_path(AfProject *project, const AfPath *path,
                             const AfPathCommand *commands) {
    AfProjectPathSnapshot snapshot = {0};
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    if (project == NULL || path == NULL || commands == NULL) {
        return result;
    }
    snapshot = (AfProjectPathSnapshot){.path = *path, .index = project->path_data.path_count};
    snapshot.path.command_start = project->path_data.command_count;
    result = af_project_insert_path_snapshot(project, &snapshot, commands);
    if (result == AF_RESULT_SUCCESS && af_project_validate(project) != AF_RESULT_SUCCESS) {
        (void)af_project_remove_path_exact(project, path->id);
        result = AF_RESULT_INVALID_ARGUMENT;
    }
    return result;
}

AfResult af_project_remove_path_exact(AfProject *project, uint64_t path_id) {
    uint32_t index = 0U;
    uint32_t command_start = 0U;
    uint32_t command_count = 0U;
    if (project == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    while (index < project->path_data.path_count && project->path_data.paths[index].id != path_id) {
        ++index;
    }
    if (index == project->path_data.path_count) {
        return AF_RESULT_NOT_FOUND;
    }
    command_start = project->path_data.paths[index].command_start;
    command_count = project->path_data.paths[index].command_count;
    memmove(&project->path_data.commands[command_start],
            &project->path_data.commands[command_start + command_count],
            (size_t)(project->path_data.command_count - command_start - command_count) *
                sizeof(*project->path_data.commands));
    project->path_data.command_count -= command_count;
    for (uint32_t path_index = 0U; path_index < project->path_data.path_count; ++path_index) {
        if (path_index != index &&
            project->path_data.paths[path_index].command_start > command_start) {
            project->path_data.paths[path_index].command_start -= command_count;
        }
    }
    memmove(&project->path_data.paths[index], &project->path_data.paths[index + 1U],
            (size_t)(project->path_data.path_count - index - 1U) *
                sizeof(*project->path_data.paths));
    --project->path_data.path_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_project_path_snapshot(const AfProject *project, uint64_t path_id,
                                  AfProjectPathSnapshot *out_snapshot, AfPathCommand *out_commands,
                                  uint32_t command_capacity) {
    if (project == NULL || out_snapshot == NULL || out_commands == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < project->path_data.path_count; ++index) {
        const AfPath *path = &project->path_data.paths[index];
        if (path->id != path_id) {
            continue;
        }
        if (path->command_count > command_capacity) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        *out_snapshot = (AfProjectPathSnapshot){.path = *path, .index = index};
        memcpy(out_commands, &project->path_data.commands[path->command_start],
               (size_t)path->command_count * sizeof(*out_commands));
        return AF_RESULT_SUCCESS;
    }
    return AF_RESULT_NOT_FOUND;
}

static float af_project_bezier(float p1, float p2, float t) {
    const float inverse = 1.0F - t;
    return 3.0F * inverse * inverse * t * p1 + 3.0F * inverse * t * t * p2 + t * t * t;
}

static float af_project_cubic_ease(float progress, const float easing[4]) {
    float parameter = 0.0F;
    float low = 0.0F;
    float high = 1.0F;
    uint32_t iteration = 0U;
    if (progress <= 0.0F || progress >= 1.0F) {
        return progress;
    }
    for (iteration = 0U; iteration < 16U; ++iteration) {
        parameter = (low + high) * 0.5F;
        if (af_project_bezier(easing[0], easing[2], parameter) < progress) {
            low = parameter;
        } else {
            high = parameter;
        }
    }
    return af_project_bezier(easing[1], easing[3], (low + high) * 0.5F);
}

static AfResult af_project_sample_track(const AfProject *project, const AfProjectTrack *track,
                                        float time, float value[4]) {
    const AfProjectKeyframe *first = NULL;
    const AfProjectKeyframe *left = NULL;
    const AfProjectKeyframe *right = NULL;
    uint32_t index = 0U;
    float progress = 0.0F;
    for (index = 0U; index < project->keyframe_count; ++index) {
        const AfProjectKeyframe *key = &project->keyframes[index];
        if (key->track_id != track->id) {
            continue;
        }
        if (first == NULL || key->keyframe.time < first->keyframe.time ||
            (key->keyframe.time == first->keyframe.time && key->id < first->id)) {
            first = key;
        }
        if (key->keyframe.time <= time &&
            (left == NULL || key->keyframe.time > left->keyframe.time ||
             (key->keyframe.time == left->keyframe.time && key->id > left->id))) {
            left = key;
        }
        if (key->keyframe.time > time &&
            (right == NULL || key->keyframe.time < right->keyframe.time ||
             (key->keyframe.time == right->keyframe.time && key->id < right->id))) {
            right = key;
        }
    }
    if (first == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    if (left == NULL) {
        left = first;
    }
    if (right == NULL || left->keyframe.interpolation == AF_INTERPOLATION_STEP ||
        right->keyframe.time <= left->keyframe.time) {
        memcpy(value, left->keyframe.value, sizeof(left->keyframe.value));
        return AF_RESULT_SUCCESS;
    }
    progress = (time - left->keyframe.time) / (right->keyframe.time - left->keyframe.time);
    progress = progress < 0.0F ? 0.0F : (progress > 1.0F ? 1.0F : progress);
    if (left->keyframe.interpolation == AF_INTERPOLATION_CUBIC_BEZIER) {
        progress = af_project_cubic_ease(progress, left->keyframe.easing);
    }
    for (index = 0U; index < 4U; ++index) {
        value[index] = left->keyframe.value[index] +
                       (right->keyframe.value[index] - left->keyframe.value[index]) * progress;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_project_animation_apply(const AfProject *project, uint64_t clip_id, float time,
                                    AfScene *scene) {
    const AfProjectClip *clip = af_project_find_clip_const(project, clip_id);
    uint32_t index = 0U;
    if (clip == NULL || scene == NULL || !isfinite(time)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    time = time < 0.0F ? 0.0F : (time > clip->duration ? clip->duration : time);
    for (index = 0U; index < project->track_count; ++index) {
        const AfProjectTrack *track = &project->tracks[index];
        AfSceneNode *node = NULL;
        float value[4] = {0};
        AfResult result = AF_RESULT_SUCCESS;
        if (track->clip_id != clip_id) {
            continue;
        }
        node = af_scene_find(scene, track->node_id);
        if (node == NULL) {
            return AF_RESULT_NOT_FOUND;
        }
        result = af_project_sample_track(project, track, time, value);
        if (result == AF_RESULT_NOT_FOUND) {
            continue;
        }
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
        switch (track->property) {
        case AF_PROPERTY_POSITION:
            node->local_transform.position = (AfVec2){value[0], value[1]};
            break;
        case AF_PROPERTY_ROTATION:
            node->local_transform.rotation = value[0];
            break;
        case AF_PROPERTY_SCALE:
            node->local_transform.scale = (AfVec2){value[0], value[1]};
            break;
        case AF_PROPERTY_OPACITY:
            node->opacity = value[0];
            break;
        case AF_PROPERTY_FILL_COLOR:
            node->fill = (AfColor){value[0], value[1], value[2], value[3]};
            break;
        case AF_PROPERTY_CORNER_RADIUS:
            node->corner_radius = value[0];
            break;
        default:
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    return af_scene_evaluate_world(scene);
}

static AfResult af_project_writer_append(AfProjectTextWriter *writer, const char *format, ...) {
    int written = 0;
    va_list arguments;
    if (writer == NULL || format == NULL || writer->length >= writer->capacity) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    va_start(arguments, format);
    written = vsnprintf(writer->data + writer->length, writer->capacity - writer->length, format,
                        arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= writer->capacity - writer->length) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    writer->length += (size_t)written;
    return AF_RESULT_SUCCESS;
}

static const char *af_project_type_name(AfNodeType type) {
    static const char *const names[] = {"group", "rectangle", "rounded_rectangle", "ellipse",
                                        "path"};
    return type <= AF_NODE_PATH ? names[type] : "invalid";
}

static const char *af_project_property_name(AfPropertyType property) {
    static const char *const names[] = {"position", "rotation",   "scale",
                                        "opacity",  "fill_color", "corner_radius"};
    return property <= AF_PROPERTY_CORNER_RADIUS ? names[property] : "invalid";
}

static const char *af_project_interpolation_name(AfInterpolation interpolation) {
    static const char *const names[] = {"step", "linear", "cubic_bezier"};
    return interpolation <= AF_INTERPOLATION_CUBIC_BEZIER ? names[interpolation] : "invalid";
}

AfResult af_project_serialize(const AfProject *project, AfAllocator *allocator,
                              AfPackageBlob *out_text) {
    size_t capacity = 0U;
    size_t animation_capacity = 0U;
    AfProjectTextWriter writer = {0};
    uint32_t index = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (allocator == NULL || out_text == NULL ||
        af_project_validate(project) != AF_RESULT_SUCCESS ||
        af_size_mul_overflow(project->scene.count, 640U, &capacity) ||
        af_size_mul_overflow(project->clip_count, 256U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->track_count, 256U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->keyframe_count, 384U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->path_data.path_count, 192U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->path_data.command_count, 192U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->style_data.count, 3072U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_mul_overflow(project->layout_data.count, 512U, &animation_capacity) ||
        af_size_add_overflow(capacity, animation_capacity, &capacity) ||
        af_size_add_overflow(capacity, 256U, &capacity)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_text = (AfPackageBlob){0};
    writer.data = af_allocate(allocator, capacity, alignof(char));
    if (writer.data == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    writer.capacity = capacity;
    result = af_project_writer_append(
        &writer,
        "arcflow_project %u\ncanvas %.9g %.9g\nnext_id %llu\nnext_animation_id %llu\n"
        "layout_canvas %.9g %.9g %.9g %.9g %u %.9g %.9g %.9g %.9g\n",
        AF_PROJECT_FORMAT_VERSION, project->canvas_size.x, project->canvas_size.y,
        (unsigned long long)project->next_id, (unsigned long long)project->next_animation_id,
        project->layout_data.reference_size.x, project->layout_data.reference_size.y,
        project->layout_data.target_size.x, project->layout_data.target_size.y,
        (uint32_t)project->layout_data.scale_mode, project->layout_data.safe_area.left,
        project->layout_data.safe_area.top, project->layout_data.safe_area.right,
        project->layout_data.safe_area.bottom);
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->scene.count; ++index) {
        const AfSceneNode *node = &project->scene.nodes[index];
        char parent[32] = {0};
        if (node->parent_id == AF_INVALID_NODE_ID) {
            (void)snprintf(parent, sizeof(parent), "none");
        } else {
            (void)snprintf(parent, sizeof(parent), "%llu", (unsigned long long)node->parent_id);
        }
        result = af_project_writer_append(
            &writer,
            "node\nid %llu\nparent %s\ntype %s\nname %s\nposition %.9g %.9g\nrotation "
            "%.9g\nscale %.9g %.9g\nbounds %.9g %.9g\nfill %.9g %.9g %.9g %.9g\nopacity "
            "%.9g\ncorner_radius %.9g\nlayer %d\nvisible %u\nend\n",
            (unsigned long long)node->id, parent, af_project_type_name(node->type),
            project->names[index], node->local_transform.position.x,
            node->local_transform.position.y, node->local_transform.rotation,
            node->local_transform.scale.x, node->local_transform.scale.y, node->bounds.x,
            node->bounds.y, node->fill.r, node->fill.g, node->fill.b, node->fill.a, node->opacity,
            node->corner_radius, node->layer_order, node->visible ? 1U : 0U);
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->style_data.count; ++index) {
        const AfStyle *style = &project->style_data.styles[index];
        const AfPaint *paints[2] = {&style->fill, &style->stroke_paint};
        result = af_project_writer_append(
            &writer,
            "style %llu %llu %llu %u %.9g %.9g %u %.9g %.9g %u %u %.9g %u %.9g %llu "
            "%llu",
            (unsigned long long)style->id, (unsigned long long)style->node_id,
            (unsigned long long)style->parent_style_id, style->inherit_flags, style->fill_opacity,
            style->stroke_opacity, style->stroke.enabled ? 1U : 0U, style->stroke.width,
            style->stroke.end_width, (uint32_t)style->stroke.cap, (uint32_t)style->stroke.join,
            style->stroke.miter_limit, style->stroke.dash_count, style->stroke.dash_offset,
            (unsigned long long)style->paint_generation,
            (unsigned long long)style->stroke_geometry_generation);
        for (uint32_t dash = 0U; result == AF_RESULT_SUCCESS && dash < style->stroke.dash_count;
             ++dash) {
            result = af_project_writer_append(&writer, " %.9g", style->stroke.dashes[dash]);
        }
        for (uint32_t paint_index = 0U; result == AF_RESULT_SUCCESS && paint_index < 2U;
             ++paint_index) {
            const AfPaint *paint = paints[paint_index];
            result = af_project_writer_append(
                &writer,
                " %u %u %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g "
                "%.9g %.9g %.9g %.9g %.9g",
                (uint32_t)paint->type, paint->stop_count, paint->solid.r, paint->solid.g,
                paint->solid.b, paint->solid.a, paint->start.x, paint->start.y, paint->end.x,
                paint->end.y, paint->start_radius, paint->end_radius, paint->angle,
                paint->transform.m00, paint->transform.m01, paint->transform.m10,
                paint->transform.m11, paint->transform.tx, paint->transform.ty);
            for (uint32_t stop = 0U; result == AF_RESULT_SUCCESS && stop < paint->stop_count;
                 ++stop) {
                const AfGradientStop *gradient_stop = &paint->stops[stop];
                result = af_project_writer_append(&writer, " %.9g %.9g %.9g %.9g %.9g",
                                                  gradient_stop->offset, gradient_stop->color.r,
                                                  gradient_stop->color.g, gradient_stop->color.b,
                                                  gradient_stop->color.a);
            }
        }
        if (result == AF_RESULT_SUCCESS) {
            result = af_project_writer_append(&writer, "\n");
        }
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->layout_data.count; ++index) {
        const AfLayoutNode *layout = &project->layout_data.nodes[index];
        result = af_project_writer_append(
            &writer,
            "layout %llu %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %u %.9g %u %.9g "
            "%.9g %.9g %.9g %.9g %.9g %u %u %u %.9g %.9g %.9g %.9g %.9g\n",
            (unsigned long long)layout->node_id, layout->anchor_min.x, layout->anchor_min.y,
            layout->anchor_max.x, layout->anchor_max.y, layout->pivot.x, layout->pivot.y,
            layout->offset.x, layout->offset.y, (uint32_t)layout->width.mode, layout->width.value,
            (uint32_t)layout->height.mode, layout->height.value, layout->minimum_size.x,
            layout->minimum_size.y, layout->maximum_size.x, layout->maximum_size.y,
            layout->aspect_ratio, (uint32_t)layout->aspect, (uint32_t)layout->direction,
            (uint32_t)layout->child_alignment, layout->spacing, layout->padding.left,
            layout->padding.top, layout->padding.right, layout->padding.bottom);
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->path_data.path_count;
         ++index) {
        const AfPath *path = &project->path_data.paths[index];
        result = af_project_writer_append(
            &writer, "path\nid %llu\nnode %llu\nfill_rule %s\ngeneration %llu\n",
            (unsigned long long)path->id, (unsigned long long)path->node_id,
            path->fill_rule == AF_PATH_FILL_EVEN_ODD ? "even_odd" : "non_zero",
            (unsigned long long)path->geometry_generation);
        for (uint32_t command_index = 0U;
             result == AF_RESULT_SUCCESS && command_index < path->command_count; ++command_index) {
            const AfPathCommand *command =
                &project->path_data.commands[path->command_start + command_index];
            if (command->type == AF_PATH_MOVE) {
                result = af_project_writer_append(&writer, "move %.9g %.9g\n", command->point.x,
                                                  command->point.y);
            } else if (command->type == AF_PATH_LINE) {
                result = af_project_writer_append(&writer, "line %.9g %.9g\n", command->point.x,
                                                  command->point.y);
            } else if (command->type == AF_PATH_CUBIC) {
                result = af_project_writer_append(&writer, "cubic %.9g %.9g %.9g %.9g %.9g %.9g\n",
                                                  command->control1.x, command->control1.y,
                                                  command->control2.x, command->control2.y,
                                                  command->point.x, command->point.y);
            } else {
                result = af_project_writer_append(&writer, "close\n");
            }
        }
        if (result == AF_RESULT_SUCCESS) {
            result = af_project_writer_append(&writer, "end_path\n");
        }
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->clip_count; ++index) {
        const AfProjectClip *clip = &project->clips[index];
        result = af_project_writer_append(
            &writer,
            "clip\nid %llu\nname %s\nduration %.9g\nloop %u\nloop_region %.9g %.9g\nend_clip\n",
            (unsigned long long)clip->id, clip->name, clip->duration, clip->loop ? 1U : 0U,
            clip->loop_start, clip->loop_end);
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->track_count; ++index) {
        const AfProjectTrack *track = &project->tracks[index];
        result = af_project_writer_append(
            &writer, "track\nid %llu\nclip %llu\nnode %llu\nproperty %s\nend_track\n",
            (unsigned long long)track->id, (unsigned long long)track->clip_id,
            (unsigned long long)track->node_id, af_project_property_name(track->property));
    }
    for (index = 0U; result == AF_RESULT_SUCCESS && index < project->keyframe_count; ++index) {
        const AfProjectKeyframe *key = &project->keyframes[index];
        result = af_project_writer_append(
            &writer,
            "keyframe\nid %llu\ntrack %llu\ntime %.9g\nvalue %.9g %.9g %.9g %.9g\n"
            "interpolation %s\neasing %.9g %.9g %.9g %.9g\nend_keyframe\n",
            (unsigned long long)key->id, (unsigned long long)key->track_id, key->keyframe.time,
            key->keyframe.value[0], key->keyframe.value[1], key->keyframe.value[2],
            key->keyframe.value[3], af_project_interpolation_name(key->keyframe.interpolation),
            key->keyframe.easing[0], key->keyframe.easing[1], key->keyframe.easing[2],
            key->keyframe.easing[3]);
    }
    if (result != AF_RESULT_SUCCESS) {
        af_deallocate(allocator, writer.data);
        return result;
    }
    out_text->data = (uint8_t *)writer.data;
    out_text->size = writer.length;
    return AF_RESULT_SUCCESS;
}

static char *af_project_next_line(char **cursor) {
    char *line = NULL;
    char *end = NULL;
    if (cursor == NULL || *cursor == NULL || **cursor == '\0') {
        return NULL;
    }
    line = *cursor;
    end = strchr(line, '\n');
    if (end != NULL) {
        *end = '\0';
        *cursor = end + 1;
    } else {
        *cursor = line + strlen(line);
    }
    end = line + strlen(line);
    if (end > line && end[-1] == '\r') {
        end[-1] = '\0';
    }
    return line;
}

static bool af_project_parse_end(const char *cursor) {
    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }
    return *cursor == '\0';
}

static bool af_project_parse_u64(const char *text, uint64_t *out_value) {
    unsigned long long value = 0ULL;
    char *end = NULL;
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || !af_project_parse_end(end)) {
        return false;
    }
    *out_value = (uint64_t)value;
    return true;
}

static bool af_project_next_u64_token(const char **cursor, uint64_t *out_value) {
    unsigned long long value = 0ULL;
    char *end = NULL;
    while (**cursor == ' ' || **cursor == '\t') {
        ++(*cursor);
    }
    errno = 0;
    value = strtoull(*cursor, &end, 10);
    if (errno != 0 || end == *cursor || (*end != '\0' && *end != ' ' && *end != '\t')) {
        return false;
    }
    *cursor = end;
    *out_value = (uint64_t)value;
    return true;
}

static bool af_project_next_u32_token(const char **cursor, uint32_t *out_value) {
    uint64_t value = 0U;
    if (!af_project_next_u64_token(cursor, &value) || value > UINT32_MAX) {
        return false;
    }
    *out_value = (uint32_t)value;
    return true;
}

static bool af_project_next_float_token(const char **cursor, float *out_value) {
    char *end = NULL;
    while (**cursor == ' ' || **cursor == '\t') {
        ++(*cursor);
    }
    errno = 0;
    *out_value = strtof(*cursor, &end);
    if (errno != 0 || end == *cursor || !isfinite(*out_value) ||
        (*end != '\0' && *end != ' ' && *end != '\t')) {
        return false;
    }
    *cursor = end;
    return true;
}

static bool af_project_parse_paint_tokens(const char **cursor, AfPaint *paint) {
    uint32_t type = 0U;
    float *values[] = {&paint->solid.r,       &paint->solid.g,       &paint->solid.b,
                       &paint->solid.a,       &paint->start.x,       &paint->start.y,
                       &paint->end.x,         &paint->end.y,         &paint->start_radius,
                       &paint->end_radius,    &paint->angle,         &paint->transform.m00,
                       &paint->transform.m01, &paint->transform.m10, &paint->transform.m11,
                       &paint->transform.tx,  &paint->transform.ty};
    if (!af_project_next_u32_token(cursor, &type) ||
        !af_project_next_u32_token(cursor, &paint->stop_count) || type > AF_PAINT_CONIC_GRADIENT ||
        paint->stop_count > AF_MAX_GRADIENT_STOPS) {
        return false;
    }
    paint->type = (AfPaintType)type;
    for (uint32_t index = 0U; index < sizeof(values) / sizeof(values[0]); ++index) {
        if (!af_project_next_float_token(cursor, values[index])) {
            return false;
        }
    }
    for (uint32_t index = 0U; index < paint->stop_count; ++index) {
        AfGradientStop *stop = &paint->stops[index];
        if (!af_project_next_float_token(cursor, &stop->offset) ||
            !af_project_next_float_token(cursor, &stop->color.r) ||
            !af_project_next_float_token(cursor, &stop->color.g) ||
            !af_project_next_float_token(cursor, &stop->color.b) ||
            !af_project_next_float_token(cursor, &stop->color.a)) {
            return false;
        }
    }
    return true;
}

static bool af_project_parse_style_line(const char *line, AfStyle *style) {
    const char *cursor = line;
    uint32_t enabled = 0U;
    uint32_t cap = 0U;
    uint32_t join = 0U;
    if (strncmp(line, "style ", 6U) != 0) {
        return false;
    }
    cursor += 6U;
    if (!af_project_next_u64_token(&cursor, &style->id) ||
        !af_project_next_u64_token(&cursor, &style->node_id) ||
        !af_project_next_u64_token(&cursor, &style->parent_style_id) ||
        !af_project_next_u32_token(&cursor, &style->inherit_flags) ||
        !af_project_next_float_token(&cursor, &style->fill_opacity) ||
        !af_project_next_float_token(&cursor, &style->stroke_opacity) ||
        !af_project_next_u32_token(&cursor, &enabled) || enabled > 1U ||
        !af_project_next_float_token(&cursor, &style->stroke.width) ||
        !af_project_next_float_token(&cursor, &style->stroke.end_width) ||
        !af_project_next_u32_token(&cursor, &cap) || cap > AF_STROKE_CAP_SQUARE ||
        !af_project_next_u32_token(&cursor, &join) || join > AF_STROKE_JOIN_BEVEL ||
        !af_project_next_float_token(&cursor, &style->stroke.miter_limit) ||
        !af_project_next_u32_token(&cursor, &style->stroke.dash_count) ||
        style->stroke.dash_count > AF_MAX_STROKE_DASHES ||
        !af_project_next_float_token(&cursor, &style->stroke.dash_offset) ||
        !af_project_next_u64_token(&cursor, &style->paint_generation) ||
        !af_project_next_u64_token(&cursor, &style->stroke_geometry_generation)) {
        return false;
    }
    style->stroke.enabled = enabled != 0U;
    style->stroke.cap = (AfStrokeCap)cap;
    style->stroke.join = (AfStrokeJoin)join;
    for (uint32_t index = 0U; index < style->stroke.dash_count; ++index) {
        if (!af_project_next_float_token(&cursor, &style->stroke.dashes[index])) {
            return false;
        }
    }
    return af_project_parse_paint_tokens(&cursor, &style->fill) &&
           af_project_parse_paint_tokens(&cursor, &style->stroke_paint) &&
           af_project_parse_end(cursor) && af_style_validate(style) == AF_RESULT_SUCCESS;
}

static bool af_project_parse_layout_line(const char *line, AfLayoutNode *layout) {
    const char *cursor = line;
    uint32_t width_mode = 0U;
    uint32_t height_mode = 0U;
    uint32_t aspect = 0U;
    uint32_t direction = 0U;
    uint32_t alignment = 0U;
    float *values[] = {&layout->anchor_min.x, &layout->anchor_min.y, &layout->anchor_max.x,
                       &layout->anchor_max.y, &layout->pivot.x,      &layout->pivot.y,
                       &layout->offset.x,     &layout->offset.y};
    if (strncmp(line, "layout ", 7U) != 0) {
        return false;
    }
    cursor += 7U;
    if (!af_project_next_u64_token(&cursor, &layout->node_id)) {
        return false;
    }
    for (uint32_t index = 0U; index < sizeof(values) / sizeof(values[0]); ++index) {
        if (!af_project_next_float_token(&cursor, values[index])) {
            return false;
        }
    }
    if (!af_project_next_u32_token(&cursor, &width_mode) ||
        !af_project_next_float_token(&cursor, &layout->width.value) ||
        !af_project_next_u32_token(&cursor, &height_mode) ||
        !af_project_next_float_token(&cursor, &layout->height.value) ||
        !af_project_next_float_token(&cursor, &layout->minimum_size.x) ||
        !af_project_next_float_token(&cursor, &layout->minimum_size.y) ||
        !af_project_next_float_token(&cursor, &layout->maximum_size.x) ||
        !af_project_next_float_token(&cursor, &layout->maximum_size.y) ||
        !af_project_next_float_token(&cursor, &layout->aspect_ratio) ||
        !af_project_next_u32_token(&cursor, &aspect) ||
        !af_project_next_u32_token(&cursor, &direction) ||
        !af_project_next_u32_token(&cursor, &alignment) ||
        !af_project_next_float_token(&cursor, &layout->spacing) ||
        !af_project_next_float_token(&cursor, &layout->padding.left) ||
        !af_project_next_float_token(&cursor, &layout->padding.top) ||
        !af_project_next_float_token(&cursor, &layout->padding.right) ||
        !af_project_next_float_token(&cursor, &layout->padding.bottom) ||
        !af_project_parse_end(cursor) || width_mode > AF_LAYOUT_VALUE_PERCENT ||
        height_mode > AF_LAYOUT_VALUE_PERCENT || aspect > AF_ASPECT_HEIGHT_CONTROLS_WIDTH ||
        direction > AF_LAYOUT_DIRECTION_VERTICAL || alignment > AF_LAYOUT_ALIGN_STRETCH) {
        return false;
    }
    layout->width.mode = (AfLayoutValueMode)width_mode;
    layout->height.mode = (AfLayoutValueMode)height_mode;
    layout->aspect = (AfAspectConstraint)aspect;
    layout->direction = (AfLayoutDirection)direction;
    layout->child_alignment = (AfLayoutAlignment)alignment;
    return true;
}

static bool af_project_parse_layout_canvas(const char *line, AfLayoutData *layout) {
    const char *cursor = line;
    uint32_t mode = 0U;
    if (strncmp(line, "layout_canvas ", 14U) != 0) {
        return false;
    }
    cursor += 14U;
    if (!af_project_next_float_token(&cursor, &layout->reference_size.x) ||
        !af_project_next_float_token(&cursor, &layout->reference_size.y) ||
        !af_project_next_float_token(&cursor, &layout->target_size.x) ||
        !af_project_next_float_token(&cursor, &layout->target_size.y) ||
        !af_project_next_u32_token(&cursor, &mode) || mode > AF_CANVAS_SCALE_STRETCH ||
        !af_project_next_float_token(&cursor, &layout->safe_area.left) ||
        !af_project_next_float_token(&cursor, &layout->safe_area.top) ||
        !af_project_next_float_token(&cursor, &layout->safe_area.right) ||
        !af_project_next_float_token(&cursor, &layout->safe_area.bottom) ||
        !af_project_parse_end(cursor)) {
        return false;
    }
    layout->scale_mode = (AfCanvasScaleMode)mode;
    return true;
}

static bool af_project_parse_i32(const char *text, int32_t *out_value) {
    long value = 0L;
    char *end = NULL;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || value < INT32_MIN || value > INT32_MAX ||
        !af_project_parse_end(end)) {
        return false;
    }
    *out_value = (int32_t)value;
    return true;
}

static bool af_project_parse_floats(const char *text, float *values, uint32_t count) {
    uint32_t index = 0U;
    char *end = NULL;
    for (index = 0U; index < count; ++index) {
        while (*text == ' ' || *text == '\t') {
            ++text;
        }
        errno = 0;
        values[index] = strtof(text, &end);
        if (errno != 0 || end == text || !isfinite(values[index])) {
            return false;
        }
        text = end;
    }
    return af_project_parse_end(text);
}

static const char *af_project_value(const char *line, const char *key) {
    const size_t length = strlen(key);
    if (strncmp(line, key, length) != 0 || line[length] != ' ') {
        return NULL;
    }
    return line + length + 1U;
}

static bool af_project_set_field(uint32_t *fields, uint32_t field) {
    if ((*fields & field) != 0U) {
        return false;
    }
    *fields |= field;
    return true;
}

static bool af_project_parse_node_field(const char *line, AfProjectNodeSnapshot *snapshot,
                                        uint32_t *fields) {
    const char *value = NULL;
    float values[4] = {0};
    uint64_t integer = 0U;
    if ((value = af_project_value(line, "id")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_FIELD_ID) &&
               af_project_parse_u64(value, &snapshot->node.id);
    }
    if ((value = af_project_value(line, "parent")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_PARENT)) {
            return false;
        }
        if (strcmp(value, "none") == 0) {
            snapshot->node.parent_id = AF_INVALID_NODE_ID;
            return true;
        }
        return af_project_parse_u64(value, &snapshot->node.parent_id);
    }
    if ((value = af_project_value(line, "type")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_TYPE)) {
            return false;
        }
        if (strcmp(value, "group") == 0) {
            snapshot->node.type = AF_NODE_GROUP;
        } else if (strcmp(value, "rectangle") == 0) {
            snapshot->node.type = AF_NODE_RECTANGLE;
        } else if (strcmp(value, "rounded_rectangle") == 0) {
            snapshot->node.type = AF_NODE_ROUNDED_RECTANGLE;
        } else if (strcmp(value, "ellipse") == 0) {
            snapshot->node.type = AF_NODE_ELLIPSE;
        } else if (strcmp(value, "path") == 0) {
            snapshot->node.type = AF_NODE_PATH;
        } else {
            return false;
        }
        return true;
    }
    if ((value = af_project_value(line, "name")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_NAME) || !af_project_name_valid(value)) {
            return false;
        }
        (void)snprintf(snapshot->name, sizeof(snapshot->name), "%s", value);
        return true;
    }
    if ((value = af_project_value(line, "position")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_POSITION) ||
            !af_project_parse_floats(value, values, 2U)) {
            return false;
        }
        snapshot->node.local_transform.position = (AfVec2){values[0], values[1]};
        return true;
    }
    if ((value = af_project_value(line, "rotation")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_FIELD_ROTATION) &&
               af_project_parse_floats(value, &snapshot->node.local_transform.rotation, 1U);
    }
    if ((value = af_project_value(line, "scale")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_SCALE) ||
            !af_project_parse_floats(value, values, 2U)) {
            return false;
        }
        snapshot->node.local_transform.scale = (AfVec2){values[0], values[1]};
        return true;
    }
    if ((value = af_project_value(line, "bounds")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_BOUNDS) ||
            !af_project_parse_floats(value, values, 2U)) {
            return false;
        }
        snapshot->node.bounds = (AfVec2){values[0], values[1]};
        return true;
    }
    if ((value = af_project_value(line, "fill")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_FILL) ||
            !af_project_parse_floats(value, values, 4U)) {
            return false;
        }
        snapshot->node.fill = (AfColor){values[0], values[1], values[2], values[3]};
        return true;
    }
    if ((value = af_project_value(line, "opacity")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_FIELD_OPACITY) &&
               af_project_parse_floats(value, &snapshot->node.opacity, 1U);
    }
    if ((value = af_project_value(line, "corner_radius")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_FIELD_RADIUS) &&
               af_project_parse_floats(value, &snapshot->node.corner_radius, 1U);
    }
    if ((value = af_project_value(line, "layer")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_FIELD_LAYER) &&
               af_project_parse_i32(value, &snapshot->node.layer_order);
    }
    if ((value = af_project_value(line, "visible")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_FIELD_VISIBLE) ||
            !af_project_parse_u64(value, &integer) || integer > 1U) {
            return false;
        }
        snapshot->node.visible = integer != 0U;
        return true;
    }
    return true;
}

static bool af_project_parse_clip_field(const char *line, AfProjectClipSnapshot *snapshot,
                                        uint32_t *fields) {
    const char *value = NULL;
    float values[2] = {0};
    uint64_t integer = 0U;
    if ((value = af_project_value(line, "id")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_CLIP_ID) &&
               af_project_parse_u64(value, &snapshot->clip.id);
    }
    if ((value = af_project_value(line, "name")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_CLIP_NAME) || !af_project_name_valid(value)) {
            return false;
        }
        (void)snprintf(snapshot->clip.name, sizeof(snapshot->clip.name), "%s", value);
        return true;
    }
    if ((value = af_project_value(line, "duration")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_CLIP_DURATION) &&
               af_project_parse_floats(value, &snapshot->clip.duration, 1U);
    }
    if ((value = af_project_value(line, "loop")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_CLIP_LOOP) ||
            !af_project_parse_u64(value, &integer) || integer > 1U) {
            return false;
        }
        snapshot->clip.loop = integer != 0U;
        return true;
    }
    if ((value = af_project_value(line, "loop_region")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_CLIP_REGION) ||
            !af_project_parse_floats(value, values, 2U)) {
            return false;
        }
        snapshot->clip.loop_start = values[0];
        snapshot->clip.loop_end = values[1];
        return true;
    }
    return true;
}

static bool af_project_parse_track_field(const char *line, AfProjectTrackSnapshot *snapshot,
                                         uint32_t *fields) {
    const char *value = NULL;
    if ((value = af_project_value(line, "id")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_TRACK_ID) &&
               af_project_parse_u64(value, &snapshot->track.id);
    }
    if ((value = af_project_value(line, "clip")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_TRACK_CLIP) &&
               af_project_parse_u64(value, &snapshot->track.clip_id);
    }
    if ((value = af_project_value(line, "node")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_TRACK_NODE) &&
               af_project_parse_u64(value, &snapshot->track.node_id);
    }
    if ((value = af_project_value(line, "property")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_TRACK_PROPERTY)) {
            return false;
        }
        if (strcmp(value, "position") == 0) {
            snapshot->track.property = AF_PROPERTY_POSITION;
        } else if (strcmp(value, "rotation") == 0) {
            snapshot->track.property = AF_PROPERTY_ROTATION;
        } else if (strcmp(value, "scale") == 0) {
            snapshot->track.property = AF_PROPERTY_SCALE;
        } else if (strcmp(value, "opacity") == 0) {
            snapshot->track.property = AF_PROPERTY_OPACITY;
        } else if (strcmp(value, "fill_color") == 0) {
            snapshot->track.property = AF_PROPERTY_FILL_COLOR;
        } else if (strcmp(value, "corner_radius") == 0) {
            snapshot->track.property = AF_PROPERTY_CORNER_RADIUS;
        } else {
            return false;
        }
        return true;
    }
    return true;
}

static bool af_project_parse_keyframe_field(const char *line, AfProjectKeyframeSnapshot *snapshot,
                                            uint32_t *fields) {
    const char *value = NULL;
    if ((value = af_project_value(line, "id")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_KEY_ID) &&
               af_project_parse_u64(value, &snapshot->keyframe.id);
    }
    if ((value = af_project_value(line, "track")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_KEY_TRACK) &&
               af_project_parse_u64(value, &snapshot->keyframe.track_id);
    }
    if ((value = af_project_value(line, "time")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_KEY_TIME) &&
               af_project_parse_floats(value, &snapshot->keyframe.keyframe.time, 1U);
    }
    if ((value = af_project_value(line, "value")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_KEY_VALUE) &&
               af_project_parse_floats(value, snapshot->keyframe.keyframe.value, 4U);
    }
    if ((value = af_project_value(line, "interpolation")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_KEY_INTERPOLATION)) {
            return false;
        }
        if (strcmp(value, "step") == 0) {
            snapshot->keyframe.keyframe.interpolation = AF_INTERPOLATION_STEP;
        } else if (strcmp(value, "linear") == 0) {
            snapshot->keyframe.keyframe.interpolation = AF_INTERPOLATION_LINEAR;
        } else if (strcmp(value, "cubic_bezier") == 0) {
            snapshot->keyframe.keyframe.interpolation = AF_INTERPOLATION_CUBIC_BEZIER;
        } else {
            return false;
        }
        return true;
    }
    if ((value = af_project_value(line, "easing")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_KEY_EASING) &&
               af_project_parse_floats(value, snapshot->keyframe.keyframe.easing, 4U);
    }
    return true;
}

static bool af_project_parse_path_line(const char *line, AfProjectPathSnapshot *snapshot,
                                       AfPathCommand *commands, uint32_t *command_count,
                                       uint32_t *fields) {
    const char *value = NULL;
    float values[6] = {0};
    if ((value = af_project_value(line, "id")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_PATH_ID) &&
               af_project_parse_u64(value, &snapshot->path.id);
    }
    if ((value = af_project_value(line, "node")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_PATH_NODE) &&
               af_project_parse_u64(value, &snapshot->path.node_id);
    }
    if ((value = af_project_value(line, "fill_rule")) != NULL) {
        if (!af_project_set_field(fields, AF_PROJECT_PATH_FILL_RULE)) {
            return false;
        }
        if (strcmp(value, "non_zero") == 0) {
            snapshot->path.fill_rule = AF_PATH_FILL_NON_ZERO;
        } else if (strcmp(value, "even_odd") == 0) {
            snapshot->path.fill_rule = AF_PATH_FILL_EVEN_ODD;
        } else {
            return false;
        }
        return true;
    }
    if ((value = af_project_value(line, "generation")) != NULL) {
        return af_project_set_field(fields, AF_PROJECT_PATH_GENERATION) &&
               af_project_parse_u64(value, &snapshot->path.geometry_generation);
    }
    if (*command_count >= AF_MAX_PATH_COMMANDS_PER_PATH) {
        return false;
    }
    if ((value = af_project_value(line, "move")) != NULL) {
        if (!af_project_parse_floats(value, values, 2U)) {
            return false;
        }
        commands[(*command_count)++] =
            (AfPathCommand){.type = AF_PATH_MOVE, .point = {values[0], values[1]}};
        return true;
    }
    if ((value = af_project_value(line, "line")) != NULL) {
        if (!af_project_parse_floats(value, values, 2U)) {
            return false;
        }
        commands[(*command_count)++] =
            (AfPathCommand){.type = AF_PATH_LINE, .point = {values[0], values[1]}};
        return true;
    }
    if ((value = af_project_value(line, "cubic")) != NULL) {
        if (!af_project_parse_floats(value, values, 6U)) {
            return false;
        }
        commands[(*command_count)++] = (AfPathCommand){.type = AF_PATH_CUBIC,
                                                       .control1 = {values[0], values[1]},
                                                       .control2 = {values[2], values[3]},
                                                       .point = {values[4], values[5]}};
        return true;
    }
    if (strcmp(line, "close") == 0) {
        commands[(*command_count)++] = (AfPathCommand){.type = AF_PATH_CLOSE};
        return true;
    }
    return true;
}

AfResult af_project_deserialize(const void *data, size_t size, AfAllocator *allocator,
                                AfProject *out_project) {
    char *text = NULL;
    char *cursor = NULL;
    char *line = NULL;
    AfProject project = {0};
    AfProjectNodeSnapshot node = {0};
    AfProjectClipSnapshot clip = {0};
    AfProjectTrackSnapshot track = {0};
    AfProjectKeyframeSnapshot keyframe = {0};
    AfProjectPathSnapshot path = {0};
    AfPathCommand path_commands[AF_MAX_PATH_COMMANDS_PER_PATH];
    uint32_t path_command_count = 0U;
    uint32_t fields = 0U;
    uint32_t node_count = 0U;
    bool in_node = false;
    bool in_clip = false;
    bool in_track = false;
    bool in_keyframe = false;
    bool in_path = false;
    bool header_seen = false;
    bool canvas_seen = false;
    bool layout_canvas_seen = false;
    bool next_id_seen = false;
    bool next_animation_id_seen = false;
    uint64_t parsed_next_id = 0U;
    uint64_t parsed_next_animation_id = 0U;
    uint64_t format_version = 0U;
    AfResult result = AF_RESULT_INVALID_PACKAGE;
    if (data == NULL || size == 0U || size > AF_PROJECT_MAX_FILE_SIZE || allocator == NULL ||
        out_project == NULL || memchr(data, '\0', size) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_project = (AfProject){0};
    text = af_allocate(allocator, size + 1U, alignof(char));
    if (text == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memcpy(text, data, size);
    text[size] = '\0';
    cursor = text;
    for (char *scan = text; *scan != '\0'; ++scan) {
        if (*scan == '\n' && node_count < AF_MAX_NODES) {
            ++node_count;
        }
    }
    node_count = node_count > AF_MAX_NODES ? AF_MAX_NODES : node_count;
    result = af_project_create(&project, allocator, node_count == 0U ? 1U : node_count);
    if (result != AF_RESULT_SUCCESS) {
        af_deallocate(allocator, text);
        return result;
    }
    result = AF_RESULT_INVALID_PACKAGE;
    while ((line = af_project_next_line(&cursor)) != NULL) {
        const char *value = NULL;
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        if (!header_seen) {
            value = af_project_value(line, "arcflow_project");
            if (value == NULL || !af_project_parse_u64(value, &format_version)) {
                goto cleanup;
            }
            if (format_version == 0U || format_version > AF_PROJECT_FORMAT_VERSION) {
                result = AF_RESULT_UNSUPPORTED_VERSION;
                goto cleanup;
            }
            header_seen = true;
            continue;
        }
        if (strncmp(line, "layout_canvas ", 14U) == 0) {
            if (layout_canvas_seen || in_node || in_clip || in_track || in_keyframe || in_path ||
                !af_project_parse_layout_canvas(line, &project.layout_data)) {
                goto cleanup;
            }
            layout_canvas_seen = true;
            continue;
        }
        if (strncmp(line, "layout ", 7U) == 0) {
            AfLayoutNode parsed_layout = {0};
            if (in_node || in_clip || in_track || in_keyframe || in_path ||
                !af_project_parse_layout_line(line, &parsed_layout) ||
                af_layout_find_const(&project.layout_data, parsed_layout.node_id) != NULL ||
                af_project_set_layout(&project, &parsed_layout) != AF_RESULT_SUCCESS) {
                goto cleanup;
            }
            continue;
        }
        if (strncmp(line, "style ", 6U) == 0) {
            AfStyle parsed_style = {0};
            AfStyle *node_style = NULL;
            const AfStyle *id_style = NULL;
            if (in_node || in_clip || in_track || in_keyframe || in_path ||
                !af_project_parse_style_line(line, &parsed_style)) {
                goto cleanup;
            }
            id_style = af_style_find_const(&project.style_data, parsed_style.id);
            if (parsed_style.node_id == 0U) {
                if (id_style != NULL ||
                    af_project_add_style_preset(&project, &parsed_style) != AF_RESULT_SUCCESS) {
                    goto cleanup;
                }
            } else {
                node_style = af_project_find_node_style(&project, parsed_style.node_id);
                if (node_style == NULL || (id_style != NULL && id_style != node_style)) {
                    goto cleanup;
                }
                *node_style = parsed_style;
            }
            continue;
        }
        if (strcmp(line, "node") == 0) {
            if (in_node || in_clip || in_track || in_keyframe || in_path ||
                project.scene.count >= project.scene.capacity) {
                goto cleanup;
            }
            node = (AfProjectNodeSnapshot){.index = project.scene.count};
            fields = 0U;
            in_node = true;
            continue;
        }
        if (strcmp(line, "end") == 0) {
            if (!in_node || fields != AF_PROJECT_REQUIRED_FIELDS ||
                af_project_insert_snapshot(&project, &node) != AF_RESULT_SUCCESS) {
                goto cleanup;
            }
            in_node = false;
            continue;
        }
        if (in_node) {
            if (!af_project_parse_node_field(line, &node, &fields)) {
                goto cleanup;
            }
            continue;
        }
        if (strcmp(line, "path") == 0) {
            if (in_clip || in_track || in_keyframe || in_path ||
                project.path_data.path_count >= project.path_data.path_capacity) {
                goto cleanup;
            }
            path = (AfProjectPathSnapshot){.index = project.path_data.path_count};
            path.path.command_start = project.path_data.command_count;
            path_command_count = 0U;
            fields = 0U;
            in_path = true;
            continue;
        }
        if (strcmp(line, "end_path") == 0) {
            path.path.command_count = path_command_count;
            if (!in_path || fields != AF_PROJECT_PATH_REQUIRED || path_command_count == 0U ||
                af_project_insert_path_snapshot(&project, &path, path_commands) !=
                    AF_RESULT_SUCCESS) {
                goto cleanup;
            }
            in_path = false;
            continue;
        }
        if (in_path) {
            if (!af_project_parse_path_line(line, &path, path_commands, &path_command_count,
                                            &fields)) {
                goto cleanup;
            }
            continue;
        }
        if (strcmp(line, "clip") == 0) {
            if (in_clip || in_track || in_keyframe || in_path ||
                project.clip_count >= project.clip_capacity) {
                goto cleanup;
            }
            clip = (AfProjectClipSnapshot){.index = project.clip_count};
            fields = 0U;
            in_clip = true;
            continue;
        }
        if (strcmp(line, "end_clip") == 0) {
            if (!in_clip || fields != AF_PROJECT_CLIP_REQUIRED ||
                af_project_insert_clip_snapshot(&project, &clip) != AF_RESULT_SUCCESS) {
                goto cleanup;
            }
            in_clip = false;
            continue;
        }
        if (in_clip) {
            if (!af_project_parse_clip_field(line, &clip, &fields)) {
                goto cleanup;
            }
            continue;
        }
        if (strcmp(line, "track") == 0) {
            if (in_track || in_keyframe || in_path ||
                project.track_count >= project.track_capacity) {
                goto cleanup;
            }
            track = (AfProjectTrackSnapshot){.index = project.track_count};
            fields = 0U;
            in_track = true;
            continue;
        }
        if (strcmp(line, "end_track") == 0) {
            if (!in_track || fields != AF_PROJECT_TRACK_REQUIRED ||
                af_project_insert_track_snapshot(&project, &track) != AF_RESULT_SUCCESS) {
                goto cleanup;
            }
            in_track = false;
            continue;
        }
        if (in_track) {
            if (!af_project_parse_track_field(line, &track, &fields)) {
                goto cleanup;
            }
            continue;
        }
        if (strcmp(line, "keyframe") == 0) {
            if (in_keyframe || in_path || project.keyframe_count >= project.keyframe_capacity) {
                goto cleanup;
            }
            keyframe = (AfProjectKeyframeSnapshot){.index = project.keyframe_count};
            fields = 0U;
            in_keyframe = true;
            continue;
        }
        if (strcmp(line, "end_keyframe") == 0) {
            if (!in_keyframe || fields != AF_PROJECT_KEY_REQUIRED ||
                af_project_insert_keyframe_snapshot(&project, &keyframe) != AF_RESULT_SUCCESS) {
                goto cleanup;
            }
            in_keyframe = false;
            continue;
        }
        if (in_keyframe) {
            if (!af_project_parse_keyframe_field(line, &keyframe, &fields)) {
                goto cleanup;
            }
            continue;
        }
        if ((value = af_project_value(line, "canvas")) != NULL) {
            float values[2] = {0};
            if (canvas_seen || !af_project_parse_floats(value, values, 2U)) {
                goto cleanup;
            }
            project.canvas_size = (AfVec2){values[0], values[1]};
            canvas_seen = true;
        } else if ((value = af_project_value(line, "next_id")) != NULL) {
            if (next_id_seen || !af_project_parse_u64(value, &parsed_next_id)) {
                goto cleanup;
            }
            next_id_seen = true;
        } else if ((value = af_project_value(line, "next_animation_id")) != NULL) {
            if (next_animation_id_seen || !af_project_parse_u64(value, &parsed_next_animation_id)) {
                goto cleanup;
            }
            next_animation_id_seen = true;
        }
    }
    if (in_node || in_clip || in_track || in_keyframe || in_path || !header_seen || !canvas_seen) {
        goto cleanup;
    }
    if (next_id_seen && parsed_next_id > project.next_id) {
        project.next_id = parsed_next_id;
    }
    if (next_animation_id_seen && parsed_next_animation_id > project.next_animation_id) {
        project.next_animation_id = parsed_next_animation_id;
    }
    if (!layout_canvas_seen) {
        project.layout_data.reference_size = project.canvas_size;
        project.layout_data.target_size = project.canvas_size;
    }
    if (af_project_validate(&project) != AF_RESULT_SUCCESS ||
        af_scene_evaluate_world(&project.scene) != AF_RESULT_SUCCESS) {
        goto cleanup;
    }
    *out_project = project;
    out_project->scene.paths = &out_project->path_data;
    out_project->scene.styles = &out_project->style_data;
    out_project->scene.layouts = &out_project->layout_data;
    project = (AfProject){0};
    result = AF_RESULT_SUCCESS;

cleanup:
    af_project_destroy(&project);
    af_deallocate(allocator, text);
    return result;
}

AfResult af_project_save_file(const AfProject *project, const char *path, AfAllocator *allocator) {
    AfPackageBlob text = {0};
    AfResult result = AF_RESULT_SUCCESS;
    if (path == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    result = af_project_serialize(project, allocator, &text);
    if (result == AF_RESULT_SUCCESS) {
        result = af_file_write(path, text.data, text.size);
    }
    af_package_blob_destroy(allocator, &text);
    return result;
}

AfResult af_project_load_file(const char *path, AfAllocator *allocator, AfProject *out_project) {
    AfFileData file = {0};
    AfResult result = af_file_read_limited(path, AF_PROJECT_MAX_FILE_SIZE, allocator, &file);
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_deserialize(file.data, file.size, allocator, out_project);
    }
    af_file_data_destroy(allocator, &file);
    return result;
}

static int af_project_keyframe_compare(const void *left_value, const void *right_value) {
    const AfProjectKeyframe *left = left_value;
    const AfProjectKeyframe *right = right_value;
    if (left->track_id != right->track_id) {
        return left->track_id < right->track_id ? -1 : 1;
    }
    if (left->keyframe.time != right->keyframe.time) {
        return left->keyframe.time < right->keyframe.time ? -1 : 1;
    }
    if (left->id == right->id) {
        return 0;
    }
    return left->id < right->id ? -1 : 1;
}

AfResult af_project_compile(const AfProject *project, AfAllocator *allocator,
                            AfPackageBlob *out_package) {
    AfSceneNode *ir_nodes = NULL;
    AfAnimationClip *ir_clips = NULL;
    AfAnimationTrack *ir_tracks = NULL;
    AfKeyframe *ir_keyframes = NULL;
    AfProjectKeyframe *ordered_keys = NULL;
    AfScene ir_scene = {0};
    AfAnimationData ir_animation = {0};
    AfPackageSource source = {0};
    AfResult result = AF_RESULT_SUCCESS;
    if (allocator == NULL || out_package == NULL ||
        af_project_validate(project) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    ir_nodes =
        af_allocate_array(allocator, project->scene.count, sizeof(*ir_nodes), alignof(AfSceneNode));
    ir_clips = af_allocate_array(allocator, project->clip_count, sizeof(*ir_clips),
                                 alignof(AfAnimationClip));
    ir_tracks = af_allocate_array(allocator, project->track_count, sizeof(*ir_tracks),
                                  alignof(AfAnimationTrack));
    ir_keyframes = af_allocate_array(allocator, project->keyframe_count, sizeof(*ir_keyframes),
                                     alignof(AfKeyframe));
    ordered_keys = af_allocate_array(allocator, project->keyframe_count, sizeof(*ordered_keys),
                                     alignof(AfProjectKeyframe));
    if ((project->scene.count != 0U && ir_nodes == NULL) ||
        (project->clip_count != 0U && ir_clips == NULL) ||
        (project->track_count != 0U && ir_tracks == NULL) ||
        (project->keyframe_count != 0U && (ir_keyframes == NULL || ordered_keys == NULL))) {
        af_deallocate(allocator, ordered_keys);
        af_deallocate(allocator, ir_keyframes);
        af_deallocate(allocator, ir_tracks);
        af_deallocate(allocator, ir_clips);
        af_deallocate(allocator, ir_nodes);
        return AF_RESULT_OUT_OF_MEMORY;
    }
    if (project->scene.count != 0U) {
        memcpy(ir_nodes, project->scene.nodes, (size_t)project->scene.count * sizeof(*ir_nodes));
    }
    ir_scene = (AfScene){.nodes = ir_nodes,
                         .count = project->scene.count,
                         .capacity = project->scene.count,
                         .paths = &project->path_data,
                         .styles = &project->style_data,
                         .layouts = &project->layout_data};
    if (project->keyframe_count != 0U) {
        memcpy(ordered_keys, project->keyframes,
               (size_t)project->keyframe_count * sizeof(*ordered_keys));
        qsort(ordered_keys, project->keyframe_count, sizeof(*ordered_keys),
              af_project_keyframe_compare);
    }
    {
        uint32_t clip_index = 0U;
        uint32_t track_write = 0U;
        uint32_t key_write = 0U;
        for (clip_index = 0U; clip_index < project->clip_count; ++clip_index) {
            const AfProjectClip *source_clip = &project->clips[clip_index];
            uint32_t track_index = 0U;
            const uint32_t track_start = track_write;
            for (track_index = 0U; track_index < project->track_count; ++track_index) {
                const AfProjectTrack *source_track = &project->tracks[track_index];
                uint32_t key_index = 0U;
                const uint32_t key_start = key_write;
                if (source_track->clip_id != source_clip->id) {
                    continue;
                }
                for (key_index = 0U; key_index < project->keyframe_count; ++key_index) {
                    if (ordered_keys[key_index].track_id == source_track->id) {
                        ir_keyframes[key_write++] = ordered_keys[key_index].keyframe;
                    }
                }
                ir_tracks[track_write++] =
                    (AfAnimationTrack){.id = source_track->id,
                                       .node_id = source_track->node_id,
                                       .property = source_track->property,
                                       .keyframe_start = key_start,
                                       .keyframe_count = key_write - key_start};
            }
            ir_clips[clip_index] = (AfAnimationClip){.id = source_clip->id,
                                                     .duration = source_clip->duration,
                                                     .track_start = track_start,
                                                     .track_count = track_write - track_start,
                                                     .loop = source_clip->loop};
        }
        ir_animation = (AfAnimationData){.clips = ir_clips,
                                         .clip_count = project->clip_count,
                                         .tracks = ir_tracks,
                                         .track_count = track_write,
                                         .keyframes = ir_keyframes,
                                         .keyframe_count = key_write};
    }
    result = af_scene_validate(&ir_scene);
    if (result == AF_RESULT_SUCCESS) {
        result = af_animation_validate(&ir_animation, &ir_scene);
    }
    if (result == AF_RESULT_SUCCESS) {
        source = (AfPackageSource){.scene = &ir_scene, .animation = &ir_animation};
        result = af_package_write(&source, allocator, out_package);
    }
    af_deallocate(allocator, ordered_keys);
    af_deallocate(allocator, ir_keyframes);
    af_deallocate(allocator, ir_tracks);
    af_deallocate(allocator, ir_clips);
    af_deallocate(allocator, ir_nodes);
    return result;
}
