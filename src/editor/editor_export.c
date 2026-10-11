#include "editor/editor_export.h"
#include <math.h>
#include <string.h>

bool af_editor_export_includes(const AfProject *project, uint64_t id, const uint64_t *ids,
                               uint32_t count) {
    if (!project || (count && !ids))
        return false;
    for (uint32_t depth = 0; id != AF_INVALID_NODE_ID && depth <= AF_MAX_HIERARCHY_DEPTH; ++depth) {
        for (uint32_t i = 0; i < count; ++i)
            if (ids[i] == id)
                return true;
        const AfSceneNode *node = af_scene_find_const(&project->scene, id);
        if (!node)
            break;
        id = node->parent_id;
    }
    return false;
}
AfResult af_editor_export_project(const AfProject *source, const AfEditorExportOptions *options,
                                  const uint64_t *ids, uint32_t count, AfAllocator *allocator,
                                  AfProject *output) {
    if (!source || !options || !allocator || !output || output == source || count > AF_MAX_NODES ||
        (count && !ids) || !isfinite(options->size.x) || !isfinite(options->size.y) ||
        options->size.x < 1 || options->size.y < 1 || options->size.x > 16384 ||
        options->size.y > 16384 || !isfinite(options->origin.x) || !isfinite(options->origin.y) ||
        (options->selection_only && !count))
        return AF_RESULT_INVALID_ARGUMENT;
    AfPackageBlob serialized = {0};
    AfResult result = af_project_serialize(source, allocator, &serialized);
    if (result == AF_RESULT_SUCCESS)
        result = af_project_deserialize(serialized.data, serialized.size, allocator, output);
    af_package_blob_destroy(allocator, &serialized);
    if (result != AF_RESULT_SUCCESS)
        return result;
    /* Project text stores image references, not the decoded in-memory pixels. */
    af_image_data_destroy(&output->image_data);
    result = af_image_data_clone(&source->image_data, allocator, &output->image_data);
    if (result != AF_RESULT_SUCCESS) {
        af_project_destroy(output);
        return result;
    }
    if (options->selection_only) {
        bool found = false;
        for (uint32_t i = 0; i < count; ++i)
            if (af_scene_find_const(&source->scene, ids[i]))
                found = true;
        if (!found) {
            af_project_destroy(output);
            return AF_RESULT_NOT_FOUND;
        }
        /* Resolve inherited appearances before removing their source objects. */
        for (uint32_t i = 0; i < output->style_data.count; ++i) {
            AfStyle resolved;
            result =
                af_style_resolve(&source->style_data, &output->style_data.styles[i], &resolved);
            if (result != AF_RESULT_SUCCESS) {
                af_project_destroy(output);
                return result;
            }
            resolved.parent_style_id = 0;
            resolved.inherit_flags = 0;
            output->style_data.styles[i] = resolved;
        }
        uint64_t kept[AF_MAX_NODES];
        uint32_t kept_count = 0;
        for (uint32_t i = 0; i < source->scene.count; ++i) {
            const AfSceneNode *node = &source->scene.nodes[i];
            bool visible = af_editor_export_includes(source, node->id, ids, count),
                 ancestor = false;
            for (uint32_t j = 0; j < count && !visible && !ancestor; ++j)
                ancestor = af_editor_export_includes(source, ids[j], &node->id, 1);
            /* Flow siblings reserve space even when their artwork is not exported. */
            if (!visible && !ancestor && node->parent_id != AF_INVALID_NODE_ID) {
                const AfLayoutNode *layout =
                    af_layout_find_const(&source->layout_data, node->parent_id);
                if (layout && layout->direction != AF_LAYOUT_DIRECTION_NONE)
                    for (uint32_t j = 0; j < count && !ancestor; ++j)
                        ancestor = af_editor_export_includes(source, ids[j], &node->parent_id, 1);
            }
            if (visible || ancestor)
                kept[kept_count++] = node->id;
            if (ancestor && !visible) {
                uint32_t index = af_project_find_index(output, node->id);
                AfSceneNode *parent = &output->scene.nodes[index];
                parent->type = AF_NODE_GROUP;
                parent->polygon_sides = 0;
                parent->polygon_inset = 0;
                output->texts[index] = (AfProjectText){0};
                (void)af_image_remove_binding(&output->image_data, parent->id);
                for (uint32_t e = 0; e < output->particle_data.count;)
                    if (output->particle_data.emitters[e].node_id == parent->id)
                        (void)af_project_remove_emitter(output,
                                                        output->particle_data.emitters[e].id);
                    else
                        ++e;
            }
        }
        for (uint32_t i = 0; i < output->scene.count;) {
            const uint64_t id = output->scene.nodes[i].id;
            bool keep = false;
            for (uint32_t k = 0; k < kept_count; ++k)
                if (kept[k] == id) {
                    keep = true;
                    break;
                }
            if (!keep)
                (void)af_project_remove_node_exact(output, id);
            else
                ++i;
        }
        for (uint32_t i = 0; i < output->path_data.path_count;) {
            const AfPath *path = &output->path_data.paths[i];
            const AfSceneNode *node = af_scene_find_const(&output->scene, path->node_id);
            if (!node || node->type != AF_NODE_PATH)
                (void)af_project_remove_path_exact(output, path->id);
            else
                ++i;
        }
        for (uint32_t i = 0; i < output->track_count;) {
            const AfProjectTrack *track = &output->tracks[i];
            const AfSceneNode *node = af_scene_find_const(&output->scene, track->node_id);
            if (!node || (af_property_is_path(track->property) && node->type != AF_NODE_PATH))
                (void)af_project_remove_track_exact(output, track->id);
            else
                ++i;
        }
        for (uint32_t i = 0; i < output->keyframe_count;) {
            if (!af_project_find_track_const(output, output->keyframes[i].track_id))
                (void)af_project_remove_keyframe_exact(output, output->keyframes[i].id);
            else
                ++i;
        }
        for (uint32_t i = 0; i < output->particle_data.count; ++i) {
            AfParticleEmitter *emitter = &output->particle_data.emitters[i];
            bool present = false;
            for (uint32_t e = 0; e < output->particle_data.count; ++e)
                if (output->particle_data.emitters[e].id == emitter->sub_emitter_id)
                    present = true;
            if (emitter->sub_emitter_id && !present) {
                emitter->sub_emitter_id = 0;
                emitter->sub_emit_birth_count = 0;
                emitter->sub_emit_death_count = 0;
            }
        }
        /* Exposed bindings that refer outside the export cannot be retained safely. */
        for (uint32_t i = 0; i < output->binding_data.property_count;) {
            const AfExposedProperty *property = &output->binding_data.properties[i];
            bool keep = true;
            for (uint32_t j = 0; j < property->target_count; ++j) {
                const AfBindingTarget *target =
                    &output->binding_data.targets[property->target_start + j];
                if ((target->type == AF_BINDING_TARGET_NODE ||
                     target->type == AF_BINDING_TARGET_TEXT_STORAGE) &&
                    !af_scene_find_const(&output->scene, target->object_id))
                    keep = false;
                if (target->type == AF_BINDING_TARGET_PARTICLE) {
                    bool present = false;
                    for (uint32_t e = 0; e < output->particle_data.count; ++e)
                        if (output->particle_data.emitters[e].id == target->object_id)
                            present = true;
                    if (!present)
                        keep = false;
                }
                if (target->type == AF_BINDING_TARGET_NODE &&
                    target->property == AF_BINDING_TARGET_NODE_IMAGE) {
                    const AfSceneNode *node =
                        af_scene_find_const(&output->scene, target->object_id);
                    if (!node || node->type != AF_NODE_IMAGE)
                        keep = false;
                }
            }
            if (!keep)
                (void)af_project_remove_exposed_property(output, property->id);
            else
                ++i;
        }
    }
    AfVec2 scale = {output->layout_data.target_size.x / output->layout_data.reference_size.x,
                    output->layout_data.target_size.y / output->layout_data.reference_size.y};
    if (output->layout_data.scale_mode == AF_CANVAS_SCALE_NONE)
        scale = (AfVec2){1, 1};
    else if (output->layout_data.scale_mode == AF_CANVAS_SCALE_FIT)
        scale.x = scale.y = fminf(scale.x, scale.y);
    else if (output->layout_data.scale_mode == AF_CANVAS_SCALE_FILL)
        scale.x = scale.y = fmaxf(scale.x, scale.y);
    const AfVec2 offset = {options->origin.x / scale.x, options->origin.y / scale.y};
    for (uint32_t i = 0; i < output->scene.count; ++i) {
        AfSceneNode *node = &output->scene.nodes[i];
        if (node->parent_id == AF_INVALID_NODE_ID) {
            node->local_transform.position.x -= offset.x;
            node->local_transform.position.y -= offset.y;
            for (uint32_t t = 0; t < output->track_count; ++t)
                if (output->tracks[t].node_id == node->id &&
                    output->tracks[t].property == AF_PROPERTY_POSITION)
                    for (uint32_t k = 0; k < output->keyframe_count; ++k)
                        if (output->keyframes[k].track_id == output->tracks[t].id) {
                            output->keyframes[k].keyframe.value[0] -= offset.x;
                            output->keyframes[k].keyframe.value[1] -= offset.y;
                        }
        }
    }
    output->canvas_size = options->size;
    result = af_scene_evaluate_world(&output->scene);
    if (result == AF_RESULT_SUCCESS)
        result = af_project_validate(output);
    if (result != AF_RESULT_SUCCESS)
        af_project_destroy(output);
    return result;
}
