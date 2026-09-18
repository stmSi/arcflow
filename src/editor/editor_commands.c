#include "editor/editor_commands.h"
#include "editor/editor_vector_tools.h"

#include "scene/scene.h"

#include <math.h>
#include <stdalign.h>
#include <string.h>

static AfResult af_editor_restore(AfProject *project, const AfProjectNodeSnapshot *snapshots,
                                  uint32_t count) {
    uint32_t restored = 0U;
    uint32_t index = 0U;
    if (project == NULL || snapshots == NULL || count == 0U ||
        count > project->scene.capacity - project->scene.count) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    for (index = 0U; index < count; ++index) {
        AfResult result = af_project_insert_snapshot(project, &snapshots[index]);
        if (result != AF_RESULT_SUCCESS) {
            for (uint32_t rollback = 0U; rollback < restored; ++rollback) {
                (void)af_project_remove_node_exact(project, snapshots[rollback].node.id);
            }
            return result;
        }
        ++restored;
    }
    if (af_scene_validate(&project->scene) != AF_RESULT_SUCCESS ||
        af_scene_evaluate_world(&project->scene) != AF_RESULT_SUCCESS) {
        for (index = 0U; index < restored; ++index) {
            (void)af_project_remove_node_exact(project, snapshots[index].node.id);
        }
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return AF_RESULT_SUCCESS;
}

static AfResult af_editor_remove(AfProject *project, const AfProjectNodeSnapshot *snapshots,
                                 uint32_t count) {
    uint32_t index = count;
    if (project == NULL || snapshots == NULL || count == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    while (index > 0U) {
        AfResult result = AF_RESULT_SUCCESS;
        --index;
        result = af_project_remove_node_exact(project, snapshots[index].node.id);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
    }
    if (af_project_validate(project) != AF_RESULT_SUCCESS ||
        af_scene_evaluate_world(&project->scene) != AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return AF_RESULT_SUCCESS;
}

static AfResult af_editor_set_snapshot_emitter(AfProject *project,
                                               const AfProjectNodeSnapshot *snapshot) {
    AfParticleEmitter *current = af_project_find_node_emitter(project, snapshot->node.id);
    if (!snapshot->has_emitter) {
        return current == NULL ? AF_RESULT_SUCCESS
                               : af_project_remove_emitter(project, current->id);
    }
    if (current != NULL) {
        *current = snapshot->emitter;
        return AF_RESULT_SUCCESS;
    }
    return af_project_add_emitter(project, &snapshot->emitter);
}

static AfResult af_editor_modify(AfProject *project, const AfProjectNodeSnapshot *snapshots,
                                 uint32_t count) {
    AfProjectNodeSnapshot rollback[AF_EDITOR_COMMAND_MAX_SNAPSHOTS];
    uint32_t index = 0U;
    if (project == NULL || snapshots == NULL || count == 0U ||
        count > AF_EDITOR_COMMAND_MAX_SNAPSHOTS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < count; ++index) {
        const uint32_t project_index = af_project_find_index(project, snapshots[index].node.id);
        if (project_index == UINT32_MAX ||
            af_project_snapshot(project, snapshots[index].node.id, &rollback[index]) !=
                AF_RESULT_SUCCESS) {
            return AF_RESULT_NOT_FOUND;
        }
        project->scene.nodes[project_index] = snapshots[index].node;
        project->particle_data.lod = snapshots[index].vfx_lod;
        project->texts[project_index] = snapshots[index].text;
        memcpy(project->names[project_index], snapshots[index].name, AF_PROJECT_NAME_CAPACITY);
        *af_project_find_node_style(project, snapshots[index].node.id) = snapshots[index].style;
        if (snapshots[index].has_layout) {
            if (af_project_set_layout(project, &snapshots[index].layout) != AF_RESULT_SUCCESS) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        } else if (af_layout_find_const(&project->layout_data, snapshots[index].node.id) != NULL) {
            (void)af_project_clear_layout(project, snapshots[index].node.id);
        }
        if (af_editor_set_snapshot_emitter(project, &snapshots[index]) != AF_RESULT_SUCCESS) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    if (af_project_validate(project) != AF_RESULT_SUCCESS ||
        af_scene_evaluate_world(&project->scene) != AF_RESULT_SUCCESS) {
        for (index = 0U; index < count; ++index) {
            const uint32_t project_index = af_project_find_index(project, rollback[index].node.id);
            if (project_index != UINT32_MAX) {
                project->scene.nodes[project_index] = rollback[index].node;
                project->particle_data.lod = rollback[index].vfx_lod;
                project->texts[project_index] = rollback[index].text;
                memcpy(project->names[project_index], rollback[index].name,
                       AF_PROJECT_NAME_CAPACITY);
                *af_project_find_node_style(project, rollback[index].node.id) =
                    rollback[index].style;
                if (rollback[index].has_layout) {
                    (void)af_project_set_layout(project, &rollback[index].layout);
                } else if (af_layout_find_const(&project->layout_data, rollback[index].node.id) !=
                           NULL) {
                    (void)af_project_clear_layout(project, rollback[index].node.id);
                }
                (void)af_editor_set_snapshot_emitter(project, &rollback[index]);
            }
        }
        (void)af_scene_evaluate_world(&project->scene);
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return AF_RESULT_SUCCESS;
}

static void af_editor_animation_remove(AfProject *project, const AfEditorCommand *command,
                                       bool before) {
    uint32_t count = before ? command->keyframe_before_count : command->keyframe_after_count;
    const AfProjectKeyframeSnapshot *keys =
        before ? command->keyframe_before : command->keyframe_after;
    while (count > 0U) {
        --count;
        (void)af_project_remove_keyframe_exact(project, keys[count].keyframe.id);
    }
    count = before ? command->track_before_count : command->track_after_count;
    {
        const AfProjectTrackSnapshot *tracks =
            before ? command->track_before : command->track_after;
        while (count > 0U) {
            --count;
            (void)af_project_remove_track_exact(project, tracks[count].track.id);
        }
    }
    count = before ? command->clip_before_count : command->clip_after_count;
    {
        const AfProjectClipSnapshot *clips = before ? command->clip_before : command->clip_after;
        while (count > 0U) {
            --count;
            (void)af_project_remove_clip_exact(project, clips[count].clip.id);
        }
    }
}

static AfResult af_editor_animation_restore(AfProject *project, const AfEditorCommand *command,
                                            bool before) {
    const uint32_t clip_count = before ? command->clip_before_count : command->clip_after_count;
    const uint32_t track_count = before ? command->track_before_count : command->track_after_count;
    const uint32_t key_count =
        before ? command->keyframe_before_count : command->keyframe_after_count;
    AfProjectClipSnapshot clips[AF_EDITOR_COMMAND_MAX_CLIPS];
    AfProjectTrackSnapshot tracks[AF_EDITOR_COMMAND_MAX_TRACKS];
    AfProjectKeyframeSnapshot keys[AF_EDITOR_COMMAND_MAX_KEYFRAMES];
    uint32_t restored_clips = 0U;
    uint32_t restored_tracks = 0U;
    uint32_t restored_keys = 0U;
    uint32_t index = 0U;
    memcpy(clips, before ? command->clip_before : command->clip_after,
           (size_t)clip_count * sizeof(*clips));
    memcpy(tracks, before ? command->track_before : command->track_after,
           (size_t)track_count * sizeof(*tracks));
    memcpy(keys, before ? command->keyframe_before : command->keyframe_after,
           (size_t)key_count * sizeof(*keys));
    for (index = 1U; index < clip_count; ++index) {
        AfProjectClipSnapshot value = clips[index];
        uint32_t position = index;
        while (position > 0U && clips[position - 1U].index > value.index) {
            clips[position] = clips[position - 1U];
            --position;
        }
        clips[position] = value;
    }
    for (index = 1U; index < track_count; ++index) {
        AfProjectTrackSnapshot value = tracks[index];
        uint32_t position = index;
        while (position > 0U && tracks[position - 1U].index > value.index) {
            tracks[position] = tracks[position - 1U];
            --position;
        }
        tracks[position] = value;
    }
    for (index = 1U; index < key_count; ++index) {
        AfProjectKeyframeSnapshot value = keys[index];
        uint32_t position = index;
        while (position > 0U && keys[position - 1U].index > value.index) {
            keys[position] = keys[position - 1U];
            --position;
        }
        keys[position] = value;
    }
    for (index = 0U; index < clip_count; ++index) {
        AfResult result = af_project_insert_clip_snapshot(project, &clips[index]);
        if (result != AF_RESULT_SUCCESS) {
            goto rollback;
        }
        ++restored_clips;
    }
    for (index = 0U; index < track_count; ++index) {
        AfResult result = af_project_insert_track_snapshot(project, &tracks[index]);
        if (result != AF_RESULT_SUCCESS) {
            goto rollback;
        }
        ++restored_tracks;
    }
    for (index = 0U; index < key_count; ++index) {
        AfResult result = af_project_insert_keyframe_snapshot(project, &keys[index]);
        if (result != AF_RESULT_SUCCESS) {
            goto rollback;
        }
        ++restored_keys;
    }
    if (af_project_validate(project) == AF_RESULT_SUCCESS) {
        return AF_RESULT_SUCCESS;
    }
rollback:
    while (restored_keys > 0U) {
        --restored_keys;
        (void)af_project_remove_keyframe_exact(project, keys[restored_keys].keyframe.id);
    }
    while (restored_tracks > 0U) {
        --restored_tracks;
        (void)af_project_remove_track_exact(project, tracks[restored_tracks].track.id);
    }
    while (restored_clips > 0U) {
        --restored_clips;
        (void)af_project_remove_clip_exact(project, clips[restored_clips].clip.id);
    }
    return AF_RESULT_INVALID_ARGUMENT;
}

static AfResult af_editor_animation_apply(AfProject *project, const AfEditorCommand *command,
                                          bool forward) {
    af_editor_animation_remove(project, command, forward);
    return af_editor_animation_restore(project, command, !forward);
}

static void af_editor_path_remove(AfProject *project, const AfEditorCommand *command, bool before) {
    uint32_t count = before ? command->path_before_count : command->path_after_count;
    const AfProjectPathSnapshot *paths = before ? command->path_before : command->path_after;
    while (count > 0U) {
        --count;
        (void)af_project_remove_path_exact(project, paths[count].path.id);
    }
}

static AfResult af_editor_path_restore(AfProject *project, const AfEditorCommand *command,
                                       bool before) {
    const uint32_t count = before ? command->path_before_count : command->path_after_count;
    const AfProjectPathSnapshot *source = before ? command->path_before : command->path_after;
    const AfPathCommand *commands =
        before ? command->path_command_before : command->path_command_after;
    AfProjectPathSnapshot paths[AF_EDITOR_COMMAND_MAX_PATHS];
    uint32_t restored = 0U;
    memcpy(paths, source, (size_t)count * sizeof(*paths));
    for (uint32_t index = 1U; index < count; ++index) {
        const AfProjectPathSnapshot value = paths[index];
        uint32_t position = index;
        while (position > 0U && paths[position - 1U].index > value.index) {
            paths[position] = paths[position - 1U];
            --position;
        }
        paths[position] = value;
    }
    for (uint32_t index = 0U; index < count; ++index) {
        const AfResult result = af_project_insert_path_snapshot(
            project, &paths[index], &commands[paths[index].command_offset]);
        if (result != AF_RESULT_SUCCESS) {
            while (restored > 0U) {
                --restored;
                (void)af_project_remove_path_exact(project, paths[restored].path.id);
            }
            return result;
        }
        ++restored;
    }
    if (af_project_validate(project) == AF_RESULT_SUCCESS) {
        return AF_RESULT_SUCCESS;
    }
    while (restored > 0U) {
        --restored;
        (void)af_project_remove_path_exact(project, paths[restored].path.id);
    }
    return AF_RESULT_INVALID_ARGUMENT;
}

static AfResult af_editor_path_apply(AfProject *project, const AfEditorCommand *command,
                                     bool forward) {
    AfResult result = AF_RESULT_SUCCESS;
    af_editor_path_remove(project, command, forward);
    result = af_editor_path_restore(project, command, !forward);
    if (result != AF_RESULT_SUCCESS) {
        (void)af_editor_path_restore(project, command, forward);
    }
    return result;
}

static AfResult af_editor_command_apply_side(AfProject *project, const AfEditorCommand *command,
                                             bool forward) {
    switch (command->type) {
    case AF_EDITOR_COMMAND_COMPOUND: {
        const AfProjectNodeSnapshot *remove = forward ? command->before : command->after;
        const AfProjectNodeSnapshot *insert = forward ? command->after : command->before;
        const uint32_t remove_count = forward ? command->before_count : command->after_count;
        const uint32_t insert_count = forward ? command->after_count : command->before_count;
        const uint32_t remove_paths =
            forward ? command->path_before_count : command->path_after_count;
        const uint32_t insert_paths =
            forward ? command->path_after_count : command->path_before_count;
        const uint32_t remove_points =
            forward ? command->path_command_before_count : command->path_command_after_count;
        const uint32_t insert_points =
            forward ? command->path_command_after_count : command->path_command_before_count;
        uint32_t remove_layouts = 0U, insert_layouts = 0U;
        if (remove_count == 0U && insert_count == 0U)
            return AF_RESULT_INVALID_ARGUMENT;
        for (uint32_t i = 0; i < remove_count; ++i) {
            if (af_scene_find_const(&project->scene, remove[i].node.id) == NULL)
                return AF_RESULT_NOT_FOUND;
            remove_layouts += remove[i].has_layout;
        }
        for (uint32_t i = 0; i < insert_count; ++i)
            insert_layouts += insert[i].has_layout;
        if (project->scene.count < remove_count || project->style_data.count < remove_count ||
            project->layout_data.count < remove_layouts ||
            project->path_data.path_count < remove_paths ||
            project->path_data.command_count < remove_points)
            return AF_RESULT_INVALID_ARGUMENT;
        if (insert_count > project->scene.capacity - (project->scene.count - remove_count) ||
            insert_count >
                project->style_data.capacity - (project->style_data.count - remove_count) ||
            insert_layouts >
                project->layout_data.capacity - (project->layout_data.count - remove_layouts) ||
            insert_paths >
                project->path_data.path_capacity - (project->path_data.path_count - remove_paths) ||
            insert_points > project->path_data.command_capacity -
                                (project->path_data.command_count - remove_points))
            return AF_RESULT_LIMIT_EXCEEDED;
        const uint64_t next_id = project->next_id;
        af_editor_path_remove(project, command, forward);
        /* Builders accept only independent leaves. No invalid intermediate state is rendered. */
        for (uint32_t i = remove_count; i > 0U; --i)
            (void)af_project_remove_node_exact(project, remove[i - 1U].node.id);
        AfResult result =
            insert_count ? af_editor_restore(project, insert, insert_count) : AF_RESULT_SUCCESS;
        const bool inserted = result == AF_RESULT_SUCCESS;
        if (inserted)
            result = af_editor_path_restore(project, command, !forward);
        if (result == AF_RESULT_SUCCESS)
            result = af_layout_evaluate_scene(&project->scene);
        if (result != AF_RESULT_SUCCESS) {
            if (inserted) {
                af_editor_path_remove(project, command, !forward);
                for (uint32_t i = insert_count; i > 0U; --i)
                    (void)af_project_remove_node_exact(project, insert[i - 1U].node.id);
            }
            (void)af_editor_restore(project, remove, remove_count);
            (void)af_editor_path_restore(project, command, forward);
            (void)af_layout_evaluate_scene(&project->scene);
            project->next_id = next_id;
        }
        return result;
    }
    case AF_EDITOR_COMMAND_CREATE:
    case AF_EDITOR_COMMAND_DUPLICATE:
        if (forward) {
            AfResult result = af_editor_restore(project, command->after, command->after_count);
            if (result == AF_RESULT_SUCCESS) {
                result = af_editor_path_restore(project, command, false);
                if (result != AF_RESULT_SUCCESS) {
                    (void)af_editor_remove(project, command->after, command->after_count);
                }
            }
            return result;
        }
        af_editor_path_remove(project, command, false);
        return af_editor_remove(project, command->after, command->after_count);
    case AF_EDITOR_COMMAND_DELETE:
        if (forward) {
            AfResult result = AF_RESULT_SUCCESS;
            af_editor_animation_remove(project, command, true);
            af_editor_path_remove(project, command, true);
            result = af_editor_remove(project, command->before, command->before_count);
            if (result != AF_RESULT_SUCCESS) {
                (void)af_editor_path_restore(project, command, true);
                (void)af_editor_animation_restore(project, command, true);
            }
            return result;
        }
        {
            AfResult result = af_editor_restore(project, command->before, command->before_count);
            if (result == AF_RESULT_SUCCESS) {
                result = af_editor_path_restore(project, command, true);
            }
            if (result == AF_RESULT_SUCCESS) {
                result = af_editor_animation_restore(project, command, true);
            }
            if (result != AF_RESULT_SUCCESS) {
                af_editor_animation_remove(project, command, false);
                af_editor_path_remove(project, command, true);
                (void)af_editor_remove(project, command->before, command->before_count);
            }
            return result;
        }
    case AF_EDITOR_COMMAND_MODIFY:
        return forward ? af_editor_modify(project, command->after, command->after_count)
                       : af_editor_modify(project, command->before, command->before_count);
    case AF_EDITOR_COMMAND_ANIMATION:
        return af_editor_animation_apply(project, command, forward);
    case AF_EDITOR_COMMAND_PATH:
        return af_editor_path_apply(project, command, forward);
    case AF_EDITOR_COMMAND_GROUP: {
        if (command->before_count == 0U || command->after_count != command->before_count + 1U) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        if (forward) {
            AfResult result = af_editor_restore(project, command->after, 1U);
            if (result == AF_RESULT_SUCCESS) {
                result = af_editor_modify(project, command->after + 1U, command->before_count);
                if (result != AF_RESULT_SUCCESS) {
                    (void)af_editor_remove(project, command->after, 1U);
                }
            }
            return result;
        }
        AfResult result = af_editor_modify(project, command->before, command->before_count);
        if (result == AF_RESULT_SUCCESS) {
            result = af_editor_remove(project, command->after, 1U);
            if (result != AF_RESULT_SUCCESS) {
                (void)af_editor_modify(project, command->after + 1U, command->before_count);
            }
        }
        return result;
    }
    case AF_EDITOR_COMMAND_REORDER: {
        if (command->before_count != 1U || command->after_count != 1U)
            return AF_RESULT_INVALID_ARGUMENT;
        const AfProjectNodeSnapshot *snapshot = forward ? command->after : command->before;
        const uint32_t from = af_project_find_index(project, command->target_id);
        const uint32_t to = snapshot->index;
        if (from == UINT32_MAX || to >= project->scene.count)
            return AF_RESULT_INVALID_ARGUMENT;
        /* Parent and placement belong to the same undoable tree move. */
        if (project->scene.nodes[from].parent_id != snapshot->node.parent_id) {
            const AfResult result = af_editor_modify(project, snapshot, 1U);
            if (result != AF_RESULT_SUCCESS)
                return result;
        }
        AfSceneNode node = project->scene.nodes[from];
        const AfProjectText text = project->texts[from];
        char name[AF_PROJECT_NAME_CAPACITY];
        memcpy(name, project->names[from], sizeof(name));
        if (to > from) {
            memmove(project->scene.nodes + from, project->scene.nodes + from + 1U,
                    (size_t)(to - from) * sizeof(node));
            memmove(project->texts + from, project->texts + from + 1U,
                    (size_t)(to - from) * sizeof(text));
            memmove(project->names + from, project->names + from + 1U,
                    (size_t)(to - from) * sizeof(name));
        } else if (to < from) {
            memmove(project->scene.nodes + to + 1U, project->scene.nodes + to,
                    (size_t)(from - to) * sizeof(node));
            memmove(project->texts + to + 1U, project->texts + to,
                    (size_t)(from - to) * sizeof(text));
            memmove(project->names + to + 1U, project->names + to,
                    (size_t)(from - to) * sizeof(name));
        }
        node.layer_order = snapshot->node.layer_order;
        project->texts[to] = text;
        project->scene.nodes[to] = node;
        memcpy(project->names[to], name, sizeof(name));
        return AF_RESULT_SUCCESS;
    }
    case AF_EDITOR_COMMAND_CONVERT_PATH: {
        if (command->before_count != 1U || command->after_count != 1U ||
            command->path_after_count != 1U || command->path_before_count > 1U) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        AfSceneNode *node = af_scene_find(&project->scene, command->target_id);
        if (node == NULL) {
            return AF_RESULT_NOT_FOUND;
        }
        const AfSceneNode rollback = *node;
        /* Node type and path must change together: neither intermediate state validates. */
        af_editor_path_remove(project, command, forward);
        *node = forward ? command->after[0].node : command->before[0].node;
        AfResult result = af_editor_path_restore(project, command, !forward);
        const bool restored = result == AF_RESULT_SUCCESS;
        if (result == AF_RESULT_SUCCESS) {
            result = af_editor_modify(project, forward ? command->after : command->before, 1U);
        }
        if (result != AF_RESULT_SUCCESS) {
            if (restored)
                af_editor_path_remove(project, command, !forward);
            *node = rollback;
            (void)af_editor_path_restore(project, command, forward);
            (void)af_editor_modify(project, forward ? command->before : command->after, 1U);
            return result;
        }
        return af_scene_evaluate_world(&project->scene);
    }
    default:
        return AF_RESULT_INVALID_ARGUMENT;
    }
}

AfResult af_editor_command_convert_path(const AfProject *project, uint64_t node_id,
                                        AfEditorCommand *out_command) {
    const AfSceneNode *node =
        project == NULL ? NULL : af_scene_find_const(&project->scene, node_id);
    if (node != NULL && out_command != NULL && node->type == AF_NODE_PATH &&
        (node->polygon_sides != 0U ||
         project->texts[af_project_find_index(project, node_id)].mode != AF_PROJECT_TEXT_NONE)) {
        AfProjectNodeSnapshot before = {0}, after = {0};
        AfResult result = af_project_snapshot(project, node_id, &before);
        if (result != AF_RESULT_SUCCESS)
            return result;
        after = before;
        after.node.polygon_sides = 0U;
        after.node.polygon_inset = 0.0F;
        after.text = (AfProjectText){0};
        return af_editor_command_modify(&before, &after, AF_EDITOR_MERGE_NONE, out_command);
    }
    if (node == NULL || out_command == NULL ||
        af_path_find_node_const(&project->path_data, node_id) != NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    /* A path has no parametric corner-radius channel. Keep those animations intact. */
    for (uint32_t index = 0U; index < project->track_count; ++index) {
        if (project->tracks[index].node_id == node_id &&
            project->tracks[index].property == AF_PROPERTY_CORNER_RADIUS) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    *out_command = (AfEditorCommand){.type = AF_EDITOR_COMMAND_CONVERT_PATH,
                                     .target_id = node_id,
                                     .before_count = 1U,
                                     .after_count = 1U};
    const uint32_t count = af_editor_vector_primitive_path(
        node->type, node->bounds, node->corner_radius, out_command->path_command_after,
        AF_EDITOR_COMMAND_MAX_PATH_COMMANDS);
    if (count == 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (project->path_data.path_count == project->path_data.path_capacity ||
        count > project->path_data.command_capacity - project->path_data.command_count) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    AfResult result = af_project_snapshot(project, node_id, &out_command->before[0]);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    out_command->after[0] = out_command->before[0];
    out_command->after[0].node.type = AF_NODE_PATH;
    out_command->after[0].node.corner_radius = 0.0F;
    out_command->path_after_count = 1U;
    out_command->path_command_after_count = count;
    /* Path IDs are scoped to path data, but need not equal their owner's ID. */
    uint64_t path_id = node_id;
    if (af_path_find_const(&project->path_data, path_id) != NULL) {
        path_id = 1U;
        while (af_path_find_const(&project->path_data, path_id) != NULL) {
            ++path_id;
        }
    }
    out_command->path_after[0] =
        (AfProjectPathSnapshot){.path = {.id = path_id,
                                         .node_id = node_id,
                                         .command_start = project->path_data.command_count,
                                         .command_count = count,
                                         .fill_rule = AF_PATH_FILL_NON_ZERO,
                                         .geometry_generation = 1U},
                                .index = project->path_data.path_count};
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_command_group(const AfProject *project, const uint64_t *ids, uint32_t count,
                                 uint64_t group_id, AfEditorCommand *out_command) {
    if (project == NULL || ids == NULL || out_command == NULL || count == 0U ||
        count >= AF_EDITOR_COMMAND_MAX_SNAPSHOTS || group_id == 0U ||
        af_scene_find_const(&project->scene, group_id) != NULL)
        return AF_RESULT_INVALID_ARGUMENT;
    *out_command = (AfEditorCommand){.type = AF_EDITOR_COMMAND_GROUP,
                                     .target_id = group_id,
                                     .before_count = count,
                                     .after_count = count + 1U};
    uint64_t parent_id = AF_INVALID_NODE_ID;
    for (uint32_t i = 0U; i < count; ++i) {
        if (af_project_snapshot(project, ids[i], &out_command->before[i]) != AF_RESULT_SUCCESS)
            return AF_RESULT_NOT_FOUND;
        for (uint32_t j = 0U; j < i; ++j)
            if (ids[i] == ids[j])
                return AF_RESULT_INVALID_ARGUMENT;
        if (i == 0U)
            parent_id = out_command->before[i].node.parent_id;
        if (out_command->before[i].node.parent_id != parent_id)
            return AF_RESULT_INVALID_ARGUMENT;
        out_command->after[i + 1U] = out_command->before[i];
        out_command->after[i + 1U].node.parent_id = group_id;
    }
    const AfSceneNode *parent = af_scene_find_const(&project->scene, parent_id);
    AfProjectNodeSnapshot *group = &out_command->after[0];
    group->node = (AfSceneNode){.id = group_id,
                                .parent_id = parent_id,
                                .type = AF_NODE_GROUP,
                                .local_transform = {.scale = {1, 1}},
                                .bounds = parent == NULL ? project->canvas_size : parent->bounds,
                                .opacity = 1,
                                .visible = true,
                                .layer_order = out_command->before[0].node.layer_order};
    group->index = project->scene.count;
    group->style = af_style_default(group_id, group_id, (AfColor){0});
    memcpy(group->name, "Group", sizeof("Group"));
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_command_polygon(const AfProject *project, uint64_t node_id, AfVec2 size,
                                   uint32_t sides, float inset, AfEditorCommand *out_command) {
    const AfSceneNode *node =
        project == NULL ? NULL : af_scene_find_const(&project->scene, node_id);
    const AfPath *path =
        project == NULL ? NULL : af_path_find_node_const(&project->path_data, node_id);
    if (node == NULL || path == NULL || node->polygon_sides == 0U || out_command == NULL ||
        sides < 3U || sides > 64U || !isfinite(inset) || inset < 0.0F || inset > 0.95F ||
        path->command_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS ||
        path->geometry_generation == UINT64_MAX)
        return AF_RESULT_INVALID_ARGUMENT;
    for (uint32_t i = 0U; i < project->track_count; ++i) {
        if (project->tracks[i].node_id == node_id &&
            project->tracks[i].property >= AF_PROPERTY_PATH_POINT)
            return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_command = (AfEditorCommand){.type = AF_EDITOR_COMMAND_CONVERT_PATH,
                                     .target_id = node_id,
                                     .before_count = 1U,
                                     .after_count = 1U,
                                     .path_before_count = 1U,
                                     .path_after_count = 1U,
                                     .merge_key = AF_EDITOR_MERGE_TRANSFORM};
    AfResult result = af_project_snapshot(project, node_id, &out_command->before[0]);
    if (result != AF_RESULT_SUCCESS)
        return result;
    result = af_project_path_snapshot(project, path->id, &out_command->path_before[0],
                                      out_command->path_command_before,
                                      AF_EDITOR_COMMAND_MAX_PATH_COMMANDS);
    if (result != AF_RESULT_SUCCESS)
        return result;
    const uint32_t count = af_editor_vector_tool_polygon(
        size, sides, inset, out_command->path_command_after, AF_EDITOR_COMMAND_MAX_PATH_COMMANDS);
    if (count == 0U)
        return AF_RESULT_INVALID_ARGUMENT;
    if (count > project->path_data.command_capacity - project->path_data.command_count +
                    path->command_count)
        return AF_RESULT_LIMIT_EXCEEDED;
    out_command->after[0] = out_command->before[0];
    out_command->after[0].node.polygon_sides = sides;
    out_command->after[0].node.polygon_inset = inset;
    out_command->after[0].node.bounds = size;
    if (out_command->after[0].has_layout) {
        out_command->after[0].layout.width = (AfLayoutValue){AF_LAYOUT_VALUE_FIXED, size.x};
        out_command->after[0].layout.height = (AfLayoutValue){AF_LAYOUT_VALUE_FIXED, size.y};
    }
    out_command->path_after[0] = out_command->path_before[0];
    out_command->path_after[0].path.command_count = count;
    ++out_command->path_after[0].path.geometry_generation;
    out_command->path_command_before_count = path->command_count;
    out_command->path_command_after_count = count;
    return AF_RESULT_SUCCESS;
}

bool af_editor_can_reparent(const AfProject *project, uint64_t node_id, uint64_t parent_id) {
    if (project == NULL)
        return false;
    const AfSceneNode *node = af_scene_find_const(&project->scene, node_id);
    const AfSceneNode *parent = af_scene_find_const(&project->scene, parent_id);
    if (node == NULL || node->parent_id == parent_id ||
        (parent_id != AF_INVALID_NODE_ID && (parent == NULL || parent->type != AF_NODE_GROUP)))
        return false;
    for (uint32_t depth = 0U; parent != NULL; ++depth) {
        if (parent->id == node_id || depth >= AF_MAX_HIERARCHY_DEPTH)
            return false;
        parent = af_scene_find_const(&project->scene, parent->parent_id);
    }
    return true;
}

AfResult af_editor_command_reparent(AfProject *project, uint64_t node_id, uint64_t parent_id,
                                    AfEditorCommand *out_command) {
    if (out_command == NULL || !af_editor_can_reparent(project, node_id, parent_id))
        return AF_RESULT_INVALID_ARGUMENT;
    AfProjectNodeSnapshot before = {0}, after = {0};
    AfResult result = af_project_snapshot(project, node_id, &before);
    if (result != AF_RESULT_SUCCESS)
        return result;
    result = af_layout_evaluate_scene(&project->scene);
    if (result != AF_RESULT_SUCCESS)
        return result;
    AfSceneNode *node = af_scene_find(&project->scene, node_id);
    const AfAffine2 world = node->world_transform;
    const AfVec2 bounds = node->resolved_bounds;
    after = before;
    after.node.parent_id = parent_id;
    /* Resolve the destination's layout frame with an identity local transform.
     * Keep the current size and account for anchors, padding, flow and pivot. */
    after.node.local_transform = (AfTransform2D){.scale = {1, 1}};
    after.node.bounds = bounds;
    if (after.has_layout) {
        after.layout.width = (AfLayoutValue){AF_LAYOUT_VALUE_FIXED, bounds.x};
        after.layout.height = (AfLayoutValue){AF_LAYOUT_VALUE_FIXED, bounds.y};
        after.layout.anchor_max = after.layout.anchor_min;
        result = af_project_set_layout(project, &after.layout);
    }
    *node = after.node;
    if (result == AF_RESULT_SUCCESS)
        result = af_layout_evaluate_scene(&project->scene);
    AfAffine2 inverse;
    if (result == AF_RESULT_SUCCESS && !af_affine_inverse(node->world_transform, &inverse))
        result = AF_RESULT_INVALID_ARGUMENT;
    if (result == AF_RESULT_SUCCESS) {
        const AfAffine2 local = af_affine_multiply(inverse, world);
        const float sx = hypotf(local.m00, local.m10);
        const float det = local.m00 * local.m11 - local.m01 * local.m10;
        if (sx < 1.0e-8F || fabsf(det) < 1.0e-8F) {
            result = AF_RESULT_INVALID_ARGUMENT;
        } else {
            const AfVec2 pivot = after.has_layout
                                     ? (AfVec2){node->resolved_bounds.x * after.layout.pivot.x,
                                                node->resolved_bounds.y * after.layout.pivot.y}
                                     : (AfVec2){0};
            after.node.local_transform = (AfTransform2D){
                .position = {local.tx - pivot.x + local.m00 * pivot.x + local.m01 * pivot.y,
                             local.ty - pivot.y + local.m10 * pivot.x + local.m11 * pivot.y},
                .rotation = atan2f(local.m10, local.m00),
                .scale = {sx, det / sx},
                .skew = {atanf((local.m00 * local.m01 + local.m10 * local.m11) / det), 0}};
            if (fabsf(after.node.local_transform.skew.x) > AF_TRANSFORM_MAX_SKEW_RADIANS)
                result = AF_RESULT_INVALID_ARGUMENT;
        }
    }
    *node = before.node;
    if (before.has_layout)
        (void)af_project_set_layout(project, &before.layout);
    (void)af_layout_evaluate_scene(&project->scene);
    if (result != AF_RESULT_SUCCESS)
        return result;
    return af_editor_command_modify(&before, &after, AF_EDITOR_MERGE_PARENT, out_command);
}

bool af_editor_can_reorder(const AfProject *project, uint64_t node_id, uint64_t target_id) {
    if (project == NULL || node_id == target_id)
        return false;
    const AfSceneNode *node = af_scene_find_const(&project->scene, node_id);
    const AfSceneNode *target = af_scene_find_const(&project->scene, target_id);
    return node != NULL && target != NULL &&
           (node->parent_id == target->parent_id ||
            af_editor_can_reparent(project, node_id, target->parent_id));
}

AfResult af_editor_command_reorder(AfProject *project, uint64_t node_id, uint64_t target_id,
                                   bool after, AfEditorCommand *out_command) {
    if (out_command == NULL || !af_editor_can_reorder(project, node_id, target_id))
        return AF_RESULT_INVALID_ARGUMENT;
    const uint32_t from = af_project_find_index(project, node_id);
    const uint32_t target = af_project_find_index(project, target_id);
    AfResult result;
    if (project->scene.nodes[from].parent_id != project->scene.nodes[target].parent_id) {
        result = af_editor_command_reparent(project, node_id,
                                            project->scene.nodes[target].parent_id, out_command);
    } else {
        *out_command =
            (AfEditorCommand){.target_id = node_id, .before_count = 1U, .after_count = 1U};
        result = af_project_snapshot(project, node_id, &out_command->before[0]);
        out_command->after[0] = out_command->before[0];
    }
    if (result != AF_RESULT_SUCCESS)
        return result;
    out_command->type = AF_EDITOR_COMMAND_REORDER;
    out_command->after[0].index = target + (after ? 1U : 0U) - (from < target ? 1U : 0U);
    out_command->after[0].node.layer_order = project->scene.nodes[target].layer_order;
    if (out_command->before[0].node.parent_id != out_command->after[0].node.parent_id) {
        /* Flow layout depends on storage order. Correct the reparented position
         * against the final insertion slot, then restore the authoring state. */
        const AfAffine2 original = project->scene.nodes[from].world_transform;
        result = af_editor_command_apply_side(project, out_command, true);
        if (result != AF_RESULT_SUCCESS)
            return result;
        result = af_layout_evaluate_scene(&project->scene);
        if (result == AF_RESULT_SUCCESS) {
            const AfSceneNode *moved = af_scene_find_const(&project->scene, node_id);
            AfAffine2 inverse;
            if (!af_affine_inverse(moved->world_transform, &inverse)) {
                result = AF_RESULT_INVALID_ARGUMENT;
            } else {
                const AfVec2 delta = {original.tx - moved->world_transform.tx,
                                      original.ty - moved->world_transform.ty};
                const AfVec2 local_delta = {inverse.m00 * delta.x + inverse.m01 * delta.y,
                                            inverse.m10 * delta.x + inverse.m11 * delta.y};
                const AfAffine2 local = af_affine_from_transform(moved->local_transform);
                out_command->after[0].node.local_transform.position.x +=
                    local.m00 * local_delta.x + local.m01 * local_delta.y;
                out_command->after[0].node.local_transform.position.y +=
                    local.m10 * local_delta.x + local.m11 * local_delta.y;
            }
        }
        const AfResult restored = af_editor_command_apply_side(project, out_command, false);
        (void)af_layout_evaluate_scene(&project->scene);
        if (result != AF_RESULT_SUCCESS)
            return result;
        if (restored != AF_RESULT_SUCCESS)
            return restored;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_history_create(AfEditorHistory *history, AfAllocator *allocator,
                                  size_t byte_limit) {
    uint32_t capacity = 0U;
    if (history == NULL || allocator == NULL || byte_limit < sizeof(AfEditorCommand)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *history = (AfEditorHistory){0};
    capacity = (uint32_t)(byte_limit / sizeof(AfEditorCommand));
    if (capacity > AF_EDITOR_HISTORY_MAX_COMMANDS) {
        capacity = AF_EDITOR_HISTORY_MAX_COMMANDS;
    }
    history->commands = af_allocate_array(allocator, capacity, sizeof(*history->commands),
                                          alignof(AfEditorCommand));
    if (history->commands == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    history->capacity = capacity;
    history->byte_limit = byte_limit;
    history->allocator = allocator;
    return AF_RESULT_SUCCESS;
}

void af_editor_history_destroy(AfEditorHistory *history) {
    if (history == NULL) {
        return;
    }
    if (history->allocator != NULL) {
        af_deallocate(history->allocator, history->commands);
    }
    *history = (AfEditorHistory){0};
}

void af_editor_history_clear(AfEditorHistory *history) {
    if (history != NULL) {
        history->count = 0U;
        history->cursor = 0U;
    }
}

size_t af_editor_history_memory_used(const AfEditorHistory *history) {
    return history == NULL ? 0U : (size_t)history->count * sizeof(AfEditorCommand);
}

bool af_editor_history_can_undo(const AfEditorHistory *history) {
    return history != NULL && history->cursor != 0U;
}

bool af_editor_history_can_redo(const AfEditorHistory *history) {
    return history != NULL && history->cursor < history->count;
}

AfResult af_editor_command_create(const AfProjectNodeSnapshot *snapshot,
                                  AfEditorCommand *out_command) {
    if (snapshot == NULL || out_command == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_command = (AfEditorCommand){
        .type = AF_EDITOR_COMMAND_CREATE, .target_id = snapshot->node.id, .after_count = 1U};
    out_command->after[0] = *snapshot;
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_command_delete(const AfProject *project, uint64_t id,
                                  AfEditorCommand *out_command) {
    return af_editor_command_delete_many(project, &id, 1U, out_command);
}

AfResult af_editor_command_delete_many(const AfProject *project, const uint64_t *ids,
                                       uint32_t id_count, AfEditorCommand *out_command) {
    AfResult result = AF_RESULT_SUCCESS;
    if (project == NULL || ids == NULL || id_count == 0U || out_command == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_command = (AfEditorCommand){.type = AF_EDITOR_COMMAND_DELETE, .target_id = ids[0]};
    for (uint32_t id_index = 0U; result == AF_RESULT_SUCCESS && id_index < id_count; ++id_index) {
        uint32_t subtree_count = 0U;
        result = af_project_collect_subtree(
            project, ids[id_index], &out_command->before[out_command->before_count],
            AF_EDITOR_COMMAND_MAX_SNAPSHOTS - out_command->before_count, &subtree_count);
        out_command->before_count += subtree_count;
    }
    for (uint32_t index = 1U; index < out_command->before_count; ++index) {
        const AfProjectNodeSnapshot value = out_command->before[index];
        uint32_t position = index;
        while (position > 0U && out_command->before[position - 1U].index > value.index) {
            out_command->before[position] = out_command->before[position - 1U];
            --position;
        }
        out_command->before[position] = value;
    }
    for (uint32_t track_index = 0U;
         result == AF_RESULT_SUCCESS && track_index < project->track_count; ++track_index) {
        bool affected = false;
        for (uint32_t node_index = 0U; node_index < out_command->before_count; ++node_index) {
            if (project->tracks[track_index].node_id == out_command->before[node_index].node.id) {
                affected = true;
                break;
            }
        }
        if (!affected) {
            continue;
        }
        if (out_command->track_before_count >= AF_EDITOR_COMMAND_MAX_TRACKS) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        out_command->track_before[out_command->track_before_count++] =
            (AfProjectTrackSnapshot){.track = project->tracks[track_index], .index = track_index};
        for (uint32_t key_index = 0U; key_index < project->keyframe_count; ++key_index) {
            if (project->keyframes[key_index].track_id != project->tracks[track_index].id) {
                continue;
            }
            if (out_command->keyframe_before_count >= AF_EDITOR_COMMAND_MAX_KEYFRAMES) {
                return AF_RESULT_LIMIT_EXCEEDED;
            }
            out_command->keyframe_before[out_command->keyframe_before_count++] =
                (AfProjectKeyframeSnapshot){.keyframe = project->keyframes[key_index],
                                            .index = key_index};
        }
    }
    for (uint32_t path_index = 0U;
         result == AF_RESULT_SUCCESS && path_index < project->path_data.path_count; ++path_index) {
        bool affected = false;
        const AfPath *path = &project->path_data.paths[path_index];
        for (uint32_t node_index = 0U; node_index < out_command->before_count; ++node_index) {
            if (path->node_id == out_command->before[node_index].node.id) {
                affected = true;
                break;
            }
        }
        if (!affected) {
            continue;
        }
        if (out_command->path_before_count >= AF_EDITOR_COMMAND_MAX_PATHS ||
            path->command_count >
                AF_EDITOR_COMMAND_MAX_PATH_COMMANDS - out_command->path_command_before_count) {
            return AF_RESULT_LIMIT_EXCEEDED;
        }
        {
            AfProjectPathSnapshot *snapshot =
                &out_command->path_before[out_command->path_before_count];
            snapshot->command_offset = out_command->path_command_before_count;
            result = af_project_path_snapshot(
                project, path->id, snapshot,
                &out_command->path_command_before[out_command->path_command_before_count],
                AF_EDITOR_COMMAND_MAX_PATH_COMMANDS - out_command->path_command_before_count);
            snapshot->command_offset = out_command->path_command_before_count;
            if (result == AF_RESULT_SUCCESS) {
                out_command->path_command_before_count += path->command_count;
                ++out_command->path_before_count;
            }
        }
    }
    return result;
}

AfResult af_editor_command_duplicate(const AfProjectNodeSnapshot *snapshots, uint32_t count,
                                     AfEditorCommand *out_command) {
    if (snapshots == NULL || count == 0U || count > AF_EDITOR_COMMAND_MAX_SNAPSHOTS ||
        out_command == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_command = (AfEditorCommand){.type = AF_EDITOR_COMMAND_DUPLICATE,
                                     .target_id = snapshots[0].node.id,
                                     .after_count = count};
    memcpy(out_command->after, snapshots, (size_t)count * sizeof(*snapshots));
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_command_modify(const AfProjectNodeSnapshot *before,
                                  const AfProjectNodeSnapshot *after, AfEditorMergeKey merge_key,
                                  AfEditorCommand *out_command) {
    if (before == NULL || after == NULL || out_command == NULL ||
        before->node.id != after->node.id) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_command = (AfEditorCommand){.type = AF_EDITOR_COMMAND_MODIFY,
                                     .merge_key = merge_key,
                                     .target_id = before->node.id,
                                     .before_count = 1U,
                                     .after_count = 1U};
    out_command->before[0] = *before;
    out_command->after[0] = *after;
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_command_animation(AfEditorMergeKey merge_key, uint64_t target_id,
                                     AfEditorCommand *out_command) {
    if (out_command == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_command = (AfEditorCommand){
        .type = AF_EDITOR_COMMAND_ANIMATION, .merge_key = merge_key, .target_id = target_id};
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_command_path(AfEditorMergeKey merge_key, uint64_t target_id,
                                AfEditorCommand *out_command) {
    if (out_command == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_command = (AfEditorCommand){
        .type = AF_EDITOR_COMMAND_PATH, .merge_key = merge_key, .target_id = target_id};
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_history_execute(AfEditorHistory *history, AfProject *project,
                                   const AfEditorCommand *command, bool already_applied,
                                   bool allow_merge) {
    AfResult result = AF_RESULT_SUCCESS;
    if (history == NULL || project == NULL || command == NULL || history->commands == NULL ||
        command->before_count > AF_EDITOR_COMMAND_MAX_SNAPSHOTS ||
        command->after_count > AF_EDITOR_COMMAND_MAX_SNAPSHOTS ||
        command->clip_before_count > AF_EDITOR_COMMAND_MAX_CLIPS ||
        command->clip_after_count > AF_EDITOR_COMMAND_MAX_CLIPS ||
        command->track_before_count > AF_EDITOR_COMMAND_MAX_TRACKS ||
        command->track_after_count > AF_EDITOR_COMMAND_MAX_TRACKS ||
        command->keyframe_before_count > AF_EDITOR_COMMAND_MAX_KEYFRAMES ||
        command->keyframe_after_count > AF_EDITOR_COMMAND_MAX_KEYFRAMES ||
        command->path_before_count > AF_EDITOR_COMMAND_MAX_PATHS ||
        command->path_after_count > AF_EDITOR_COMMAND_MAX_PATHS ||
        command->path_command_before_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS ||
        command->path_command_after_count > AF_EDITOR_COMMAND_MAX_PATH_COMMANDS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (!already_applied) {
        result = af_editor_command_apply_side(project, command, true);
        if (result != AF_RESULT_SUCCESS) {
            return result;
        }
    }
    history->count = history->cursor;
    if (allow_merge &&
        (command->type == AF_EDITOR_COMMAND_MODIFY ||
         command->type == AF_EDITOR_COMMAND_ANIMATION || command->type == AF_EDITOR_COMMAND_PATH ||
         (command->type == AF_EDITOR_COMMAND_CONVERT_PATH && command->path_before_count == 1U)) &&
        command->merge_key != AF_EDITOR_MERGE_NONE && history->cursor != 0U) {
        AfEditorCommand *previous = &history->commands[history->cursor - 1U];
        if (previous->type == command->type && previous->target_id == command->target_id &&
            previous->merge_key == command->merge_key) {
            if (command->type == AF_EDITOR_COMMAND_MODIFY) {
                previous->after[0] = command->after[0];
                previous->after_count = 1U;
            } else if (command->type == AF_EDITOR_COMMAND_ANIMATION) {
                previous->clip_after_count = command->clip_after_count;
                previous->track_after_count = command->track_after_count;
                previous->keyframe_after_count = command->keyframe_after_count;
                memcpy(previous->clip_after, command->clip_after,
                       (size_t)command->clip_after_count * sizeof(*command->clip_after));
                memcpy(previous->track_after, command->track_after,
                       (size_t)command->track_after_count * sizeof(*command->track_after));
                memcpy(previous->keyframe_after, command->keyframe_after,
                       (size_t)command->keyframe_after_count * sizeof(*command->keyframe_after));
            } else {
                if (command->type == AF_EDITOR_COMMAND_CONVERT_PATH) {
                    previous->after[0] = command->after[0];
                }
                previous->path_after_count = command->path_after_count;
                previous->path_command_after_count = command->path_command_after_count;
                memcpy(previous->path_after, command->path_after,
                       (size_t)command->path_after_count * sizeof(*command->path_after));
                memcpy(previous->path_command_after, command->path_command_after,
                       (size_t)command->path_command_after_count *
                           sizeof(*command->path_command_after));
            }
            return AF_RESULT_SUCCESS;
        }
    }
    if (history->count == history->capacity) {
        memmove(history->commands, history->commands + 1U,
                (size_t)(history->count - 1U) * sizeof(*history->commands));
        --history->count;
        --history->cursor;
    }
    history->commands[history->cursor] = *command;
    ++history->cursor;
    history->count = history->cursor;
    return AF_RESULT_SUCCESS;
}

AfResult af_editor_history_undo(AfEditorHistory *history, AfProject *project) {
    AfResult result = AF_RESULT_SUCCESS;
    if (!af_editor_history_can_undo(history) || project == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    result = af_editor_command_apply_side(project, &history->commands[history->cursor - 1U], false);
    if (result == AF_RESULT_SUCCESS) {
        --history->cursor;
    }
    return result;
}

AfResult af_editor_history_redo(AfEditorHistory *history, AfProject *project) {
    AfResult result = AF_RESULT_SUCCESS;
    if (!af_editor_history_can_redo(history) || project == NULL) {
        return AF_RESULT_NOT_FOUND;
    }
    result = af_editor_command_apply_side(project, &history->commands[history->cursor], true);
    if (result == AF_RESULT_SUCCESS) {
        ++history->cursor;
    }
    return result;
}
