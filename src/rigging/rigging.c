#include "rigging/rigging.h"

#include <math.h>
#include <string.h>

static AfBone *af_rig_bone(AfRigData *rig, uint64_t id) {
    for (uint32_t index = 0U; rig != NULL && index < rig->bone_count; ++index) {
        if (rig->bones[index].id == id) {
            return &rig->bones[index];
        }
    }
    return NULL;
}

static const AfBone *af_rig_bone_const(const AfRigData *rig, uint64_t id) {
    return af_rig_bone((AfRigData *)rig, id);
}

static bool af_rig_finite_transform(AfTransform2D transform) {
    return isfinite(transform.position.x) && isfinite(transform.position.y) &&
           isfinite(transform.rotation) && isfinite(transform.scale.x) &&
           isfinite(transform.scale.y) && transform.scale.x != 0.0F && transform.scale.y != 0.0F;
}

static bool af_rig_bone_chain_valid(const AfRigData *rig, uint64_t id) {
    uint64_t current = id;
    for (uint32_t depth = 0U; current != 0U; ++depth) {
        const AfBone *bone = af_rig_bone_const(rig, current);
        if (bone == NULL || depth >= AF_MAX_BONES) {
            return false;
        }
        current = bone->parent_id;
        if (current == id) {
            return false;
        }
    }
    return true;
}

static bool af_rig_path_exists(const AfPathData *paths, uint64_t id) {
    return paths != NULL && af_path_find_const(paths, id) != NULL;
}

static bool af_constraint_visit(const AfRigData *rig, uint64_t bone_id, uint8_t *marks) {
    uint32_t bone_index = 0U;
    for (; bone_index < rig->bone_count; ++bone_index) {
        if (rig->bones[bone_index].id == bone_id) {
            break;
        }
    }
    if (bone_index == rig->bone_count || marks[bone_index] == 1U) {
        return false;
    }
    if (marks[bone_index] == 2U) {
        return true;
    }
    marks[bone_index] = 1U;
    for (uint32_t index = 0U; index < rig->constraint_count; ++index) {
        const AfRigConstraint *constraint = &rig->constraints[index];
        if (constraint->bone_id == bone_id && constraint->target_bone_id != 0U &&
            !af_constraint_visit(rig, constraint->target_bone_id, marks)) {
            return false;
        }
    }
    marks[bone_index] = 2U;
    return true;
}

static bool af_constraint_graph_valid(const AfRigData *rig) {
    uint8_t marks[AF_MAX_BONES] = {0};
    for (uint32_t index = 0U; index < rig->bone_count; ++index) {
        if (!af_constraint_visit(rig, rig->bones[index].id, marks)) {
            return false;
        }
    }
    return true;
}

AfResult af_rig_validate(const AfRigData *rig, uint32_t vertex_count, const AfPathData *paths) {
    if (rig == NULL || rig->bone_count > AF_MAX_BONES || rig->weight_count > AF_MAX_SKIN_WEIGHTS ||
        rig->constraint_count > AF_MAX_RIG_CONSTRAINTS || rig->bone_count > rig->bone_capacity ||
        rig->weight_count > rig->weight_capacity ||
        rig->constraint_count > rig->constraint_capacity ||
        (rig->bone_count != 0U && rig->bones == NULL) ||
        (rig->weight_count != 0U && rig->weights == NULL) ||
        (rig->constraint_count != 0U && rig->constraints == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < rig->bone_count; ++index) {
        const AfBone *bone = &rig->bones[index];
        if (bone->id == 0U || bone->parent_id == bone->id ||
            (bone->parent_id != 0U && af_rig_bone_const(rig, bone->parent_id) == NULL) ||
            !af_rig_finite_transform(bone->rest_transform) ||
            !af_rig_finite_transform(bone->local_transform) || !isfinite(bone->length) ||
            bone->length < 0.0F || !af_rig_bone_chain_valid(rig, bone->id)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t other = index + 1U; other < rig->bone_count; ++other) {
            if (bone->id == rig->bones[other].id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    for (uint32_t index = 0U; index < rig->weight_count; ++index) {
        const AfSkinWeight *weight = &rig->weights[index];
        uint32_t weights_for_vertex = 0U;
        float total = 0.0F;
        if (weight->vertex_index >= vertex_count ||
            af_rig_bone_const(rig, weight->bone_id) == NULL || !isfinite(weight->weight) ||
            weight->weight < 0.0F || weight->weight > 1.0F) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t candidate = 0U; candidate < rig->weight_count; ++candidate) {
            if (rig->weights[candidate].vertex_index == weight->vertex_index) {
                ++weights_for_vertex;
                total += rig->weights[candidate].weight;
            }
        }
        if (weights_for_vertex > AF_MAX_WEIGHTS_PER_VERTEX || total > 1.00001F) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (uint32_t index = 0U; index < rig->constraint_count; ++index) {
        const AfRigConstraint *constraint = &rig->constraints[index];
        if (constraint->id == 0U || constraint->type > AF_CONSTRAINT_TRANSFORM_COPY ||
            af_rig_bone_const(rig, constraint->bone_id) == NULL ||
            (constraint->target_bone_id != 0U &&
             af_rig_bone_const(rig, constraint->target_bone_id) == NULL) ||
            constraint->bone_id == constraint->target_bone_id || !isfinite(constraint->weight) ||
            constraint->weight < 0.0F || constraint->weight > 1.0F ||
            !isfinite(constraint->target_position.x) || !isfinite(constraint->target_position.y) ||
            !isfinite(constraint->target_rotation) || !isfinite(constraint->target_scale.x) ||
            !isfinite(constraint->target_scale.y) || !isfinite(constraint->distance) ||
            constraint->distance < 0.0F || !isfinite(constraint->path_position) ||
            constraint->path_position < 0.0F || constraint->path_position > 1.0F ||
            constraint->iterations > AF_MAX_SOLVER_ITERATIONS ||
            constraint->chain_length > AF_MAX_IK_CHAIN_LENGTH ||
            (constraint->type == AF_CONSTRAINT_IK && constraint->chain_length == 0U) ||
            (constraint->target_bone_id == 0U &&
             (constraint->type == AF_CONSTRAINT_SCALE ||
              constraint->type == AF_CONSTRAINT_TRANSFORM_COPY) &&
             (constraint->target_scale.x == 0.0F || constraint->target_scale.y == 0.0F)) ||
            (constraint->type == AF_CONSTRAINT_PATH_FOLLOW &&
             !af_rig_path_exists(paths, constraint->path_id))) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t other = index + 1U; other < rig->constraint_count; ++other) {
            if (constraint->id == rig->constraints[other].id) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    return af_constraint_graph_valid(rig) ? AF_RESULT_SUCCESS : AF_RESULT_INVALID_ARGUMENT;
}

static AfResult af_rig_eval_bone(AfRigData *rig, uint32_t index, uint8_t *marks) {
    AfBone *bone = &rig->bones[index];
    if (marks[index] == 2U) {
        return AF_RESULT_SUCCESS;
    }
    if (marks[index] == 1U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    marks[index] = 1U;
    const AfAffine2 local = af_affine_from_transform(bone->local_transform);
    if (bone->parent_id == 0U) {
        bone->world_transform = local;
    } else {
        uint32_t parent_index = 0U;
        for (; parent_index < rig->bone_count; ++parent_index) {
            if (rig->bones[parent_index].id == bone->parent_id) {
                break;
            }
        }
        if (parent_index == rig->bone_count ||
            af_rig_eval_bone(rig, parent_index, marks) != AF_RESULT_SUCCESS) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        bone->world_transform = af_affine_multiply(rig->bones[parent_index].world_transform, local);
    }
    marks[index] = 2U;
    return AF_RESULT_SUCCESS;
}

static AfResult af_rig_update_world(AfRigData *rig) {
    uint8_t marks[AF_MAX_BONES] = {0};
    for (uint32_t index = 0U; index < rig->bone_count; ++index) {
        if (af_rig_eval_bone(rig, index, marks) != AF_RESULT_SUCCESS) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    return AF_RESULT_SUCCESS;
}

static AfVec2 af_constraint_target(const AfRigData *rig, const AfRigConstraint *constraint) {
    const AfBone *target = af_rig_bone_const(rig, constraint->target_bone_id);
    return target == NULL ? constraint->target_position
                          : (AfVec2){target->world_transform.tx, target->world_transform.ty};
}

static void af_lerp_transform(AfTransform2D *transform, const AfTransform2D *target, float weight,
                              AfConstraintType type) {
    if (type == AF_CONSTRAINT_POSITION || type == AF_CONSTRAINT_TRANSFORM_COPY) {
        transform->position.x += (target->position.x - transform->position.x) * weight;
        transform->position.y += (target->position.y - transform->position.y) * weight;
    }
    if (type == AF_CONSTRAINT_ROTATION || type == AF_CONSTRAINT_TRANSFORM_COPY) {
        transform->rotation += (target->rotation - transform->rotation) * weight;
    }
    if (type == AF_CONSTRAINT_SCALE || type == AF_CONSTRAINT_TRANSFORM_COPY) {
        transform->scale.x += (target->scale.x - transform->scale.x) * weight;
        transform->scale.y += (target->scale.y - transform->scale.y) * weight;
    }
}

static AfResult af_solve_ik(AfRigData *rig, const AfRigConstraint *constraint,
                            AfRigMetrics *metrics) {
    AfBone *end = af_rig_bone(rig, constraint->bone_id);
    const AfVec2 target = af_constraint_target(rig, constraint);
    const uint32_t iterations = constraint->iterations == 0U ? 1U : constraint->iterations;
    for (uint32_t iteration = 0U; iteration < iterations; ++iteration) {
        AfBone *joint = end;
        for (uint32_t link = 0U; link < constraint->chain_length && joint != NULL; ++link) {
            const AfVec2 joint_position = {joint->world_transform.tx, joint->world_transform.ty};
            const AfVec2 tip =
                af_affine_transform_point(end->world_transform, (AfVec2){end->length, 0.0F});
            const float current_angle = atan2f(tip.y - joint_position.y, tip.x - joint_position.x);
            const float target_angle =
                atan2f(target.y - joint_position.y, target.x - joint_position.x);
            joint->local_transform.rotation += (target_angle - current_angle) * constraint->weight;
            if (af_rig_update_world(rig) != AF_RESULT_SUCCESS) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            if (metrics != NULL) {
                ++metrics->solver_iterations;
            }
            joint = af_rig_bone(rig, joint->parent_id);
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_rig_evaluate(AfRigData *rig, const AfPathData *paths,
                         AfPathTessellationScratch *scratch, AfRigMetrics *metrics) {
    if (rig == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < rig->constraint_count; ++index) {
        if (rig->constraints[index].type == AF_CONSTRAINT_PATH_FOLLOW && scratch == NULL) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    AfResult result = af_rig_update_world(rig);
    for (uint32_t index = 0U; result == AF_RESULT_SUCCESS && index < rig->constraint_count;
         ++index) {
        const AfRigConstraint *constraint = &rig->constraints[index];
        AfBone *bone = af_rig_bone(rig, constraint->bone_id);
        const AfBone *target_bone = af_rig_bone_const(rig, constraint->target_bone_id);
        const AfVec2 target_position = af_constraint_target(rig, constraint);
        if (bone == NULL) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        if (constraint->type == AF_CONSTRAINT_IK) {
            result = af_solve_ik(rig, constraint, metrics);
            continue;
        }
        if (constraint->type == AF_CONSTRAINT_AIM) {
            const float angle = atan2f(target_position.y - bone->world_transform.ty,
                                       target_position.x - bone->world_transform.tx);
            bone->local_transform.rotation +=
                (angle - bone->local_transform.rotation) * constraint->weight;
        } else if (constraint->type == AF_CONSTRAINT_DISTANCE) {
            const float dx = bone->local_transform.position.x - target_position.x;
            const float dy = bone->local_transform.position.y - target_position.y;
            const float length = hypotf(dx, dy);
            if (length > 0.000001F) {
                const float desired_x = target_position.x + dx / length * constraint->distance;
                const float desired_y = target_position.y + dy / length * constraint->distance;
                bone->local_transform.position.x +=
                    (desired_x - bone->local_transform.position.x) * constraint->weight;
                bone->local_transform.position.y +=
                    (desired_y - bone->local_transform.position.y) * constraint->weight;
            }
        } else if (constraint->type == AF_CONSTRAINT_PATH_FOLLOW) {
            AfTransform2D transform;
            result = af_path_follow(paths, constraint->path_id, constraint->path_position, 0.25F,
                                    scratch, &transform);
            if (result == AF_RESULT_SUCCESS) {
                af_lerp_transform(&bone->local_transform, &transform, constraint->weight,
                                  AF_CONSTRAINT_TRANSFORM_COPY);
            }
        } else {
            AfTransform2D target = target_bone == NULL
                                       ? (AfTransform2D){.position = target_position,
                                                         .rotation = constraint->target_rotation,
                                                         .scale = constraint->target_scale}
                                       : target_bone->local_transform;
            af_lerp_transform(&bone->local_transform, &target, constraint->weight,
                              constraint->type);
        }
        if (metrics != NULL) {
            ++metrics->solver_iterations;
        }
        if (result == AF_RESULT_SUCCESS) {
            result = af_rig_update_world(rig);
        }
    }
    return result;
}

AfResult af_rig_skin(const AfRigData *rig, const AfVec2 *bind_vertices, uint32_t vertex_count,
                     AfVec2 *out_vertices, uint32_t capacity, AfRigMetrics *metrics) {
    if (rig == NULL || bind_vertices == NULL || out_vertices == NULL || vertex_count > capacity ||
        vertex_count > AF_MAX_MESH_VERTICES) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    AfAffine2 bind_world[AF_MAX_BONES];
    uint8_t marks[AF_MAX_BONES] = {0};
    for (uint32_t completed = 0U; completed < rig->bone_count;) {
        bool progressed = false;
        for (uint32_t index = 0U; index < rig->bone_count; ++index) {
            if (marks[index] != 0U) {
                continue;
            }
            if (rig->bones[index].parent_id == 0U) {
                bind_world[index] = af_affine_from_transform(rig->bones[index].rest_transform);
            } else {
                uint32_t parent = 0U;
                for (; parent < rig->bone_count; ++parent) {
                    if (rig->bones[parent].id == rig->bones[index].parent_id) {
                        break;
                    }
                }
                if (parent == rig->bone_count) {
                    return AF_RESULT_INVALID_ARGUMENT;
                }
                if (marks[parent] == 0U) {
                    continue;
                }
                bind_world[index] = af_affine_multiply(
                    bind_world[parent], af_affine_from_transform(rig->bones[index].rest_transform));
            }
            marks[index] = 1U;
            ++completed;
            progressed = true;
        }
        if (!progressed) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (uint32_t vertex = 0U; vertex < vertex_count; ++vertex) {
        AfVec2 result = {0};
        float total = 0.0F;
        for (uint32_t index = 0U; index < rig->weight_count; ++index) {
            const AfSkinWeight *weight = &rig->weights[index];
            if (weight->vertex_index == vertex) {
                const AfBone *bone = af_rig_bone_const(rig, weight->bone_id);
                AfAffine2 inverse_bind;
                uint32_t bone_index = 0U;
                for (; bone_index < rig->bone_count; ++bone_index) {
                    if (rig->bones[bone_index].id == weight->bone_id) {
                        break;
                    }
                }
                if (bone == NULL || bone_index == rig->bone_count ||
                    !af_affine_inverse(bind_world[bone_index], &inverse_bind)) {
                    return AF_RESULT_INVALID_ARGUMENT;
                }
                const AfAffine2 deformation =
                    af_affine_multiply(bone->world_transform, inverse_bind);
                const AfVec2 transformed =
                    af_affine_transform_point(deformation, bind_vertices[vertex]);
                result.x += transformed.x * weight->weight;
                result.y += transformed.y * weight->weight;
                total += weight->weight;
            }
        }
        if (total < 1.0F) {
            result.x += bind_vertices[vertex].x * (1.0F - total);
            result.y += bind_vertices[vertex].y * (1.0F - total);
        }
        out_vertices[vertex] = result;
    }
    if (metrics != NULL) {
        metrics->deformed_vertices += vertex_count;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_rig_add_bone(AfRigData *rig, const AfBone *bone) {
    if (rig == NULL || bone == NULL || rig->bone_count >= rig->bone_capacity || bone->id == 0U ||
        af_rig_bone(rig, bone->id) != NULL ||
        (bone->parent_id != 0U && af_rig_bone(rig, bone->parent_id) == NULL) ||
        !af_rig_finite_transform(bone->rest_transform) ||
        !af_rig_finite_transform(bone->local_transform) || !isfinite(bone->length) ||
        bone->length < 0.0F) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    rig->bones[rig->bone_count++] = *bone;
    return AF_RESULT_SUCCESS;
}

AfResult af_rig_delete_bone(AfRigData *rig, uint64_t bone_id) {
    if (rig == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < rig->bone_count; ++index) {
        if (rig->bones[index].parent_id == bone_id) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (uint32_t index = 0U; index < rig->weight_count; ++index) {
        if (rig->weights[index].bone_id == bone_id) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (uint32_t index = 0U; index < rig->constraint_count; ++index) {
        if (rig->constraints[index].bone_id == bone_id ||
            rig->constraints[index].target_bone_id == bone_id) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    for (uint32_t index = 0U; index < rig->bone_count; ++index) {
        if (rig->bones[index].id == bone_id) {
            memmove(&rig->bones[index], &rig->bones[index + 1U],
                    (size_t)(rig->bone_count - index - 1U) * sizeof(AfBone));
            --rig->bone_count;
            return AF_RESULT_SUCCESS;
        }
    }
    return AF_RESULT_NOT_FOUND;
}

AfResult af_rig_reparent_bone(AfRigData *rig, uint64_t bone_id, uint64_t parent_id) {
    AfBone *bone = af_rig_bone(rig, bone_id);
    const uint64_t previous = bone == NULL ? 0U : bone->parent_id;
    if (bone == NULL || bone_id == parent_id ||
        (parent_id != 0U && af_rig_bone(rig, parent_id) == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    bone->parent_id = parent_id;
    if (!af_rig_bone_chain_valid(rig, bone_id)) {
        bone->parent_id = previous;
        return AF_RESULT_INVALID_ARGUMENT;
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_bone_animation_validate(const AfBoneAnimation *animation, const AfRigData *rig) {
    if (animation == NULL || rig == NULL || animation->keyframe_count > AF_MAX_BONE_KEYFRAMES ||
        animation->keyframe_count > animation->keyframe_capacity ||
        (animation->keyframe_count != 0U && animation->keyframes == NULL) ||
        !isfinite(animation->duration) || animation->duration < 0.0F) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < animation->keyframe_count; ++index) {
        const AfBoneKeyframe *key = &animation->keyframes[index];
        if (af_rig_bone_const(rig, key->bone_id) == NULL || !isfinite(key->time) ||
            key->time < 0.0F || key->time > animation->duration ||
            !af_rig_finite_transform(key->transform)) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
        for (uint32_t previous = 0U; previous < index; ++previous) {
            if (animation->keyframes[previous].bone_id == key->bone_id &&
                animation->keyframes[previous].time > key->time) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_bone_animation_apply(const AfBoneAnimation *animation, float time, AfRigData *rig) {
    if (animation == NULL || rig == NULL || !isfinite(time)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    time = fminf(fmaxf(time, 0.0F), animation->duration);
    for (uint32_t bone_index = 0U; bone_index < rig->bone_count; ++bone_index) {
        const AfBoneKeyframe *left = NULL;
        const AfBoneKeyframe *right = NULL;
        for (uint32_t key_index = 0U; key_index < animation->keyframe_count; ++key_index) {
            const AfBoneKeyframe *key = &animation->keyframes[key_index];
            if (key->bone_id != rig->bones[bone_index].id) {
                continue;
            }
            if (key->time <= time) {
                left = key;
            }
            if (key->time >= time) {
                right = key;
                break;
            }
        }
        if (left == NULL) {
            left = right;
        }
        if (right == NULL) {
            right = left;
        }
        if (left != NULL) {
            const float amount =
                right->time > left->time ? (time - left->time) / (right->time - left->time) : 0.0F;
            AfTransform2D transform = left->transform;
            transform.position.x += (right->transform.position.x - transform.position.x) * amount;
            transform.position.y += (right->transform.position.y - transform.position.y) * amount;
            transform.rotation += (right->transform.rotation - transform.rotation) * amount;
            transform.scale.x += (right->transform.scale.x - transform.scale.x) * amount;
            transform.scale.y += (right->transform.scale.y - transform.scale.y) * amount;
            rig->bones[bone_index].local_transform = transform;
        }
    }
    return af_rig_update_world(rig);
}

AfResult af_procedural_validate(const AfProceduralData *data, const AfRigData *rig) {
    if (data == NULL || rig == NULL || data->track_count > AF_MAX_PROCEDURAL_TRACKS ||
        data->track_count > data->track_capacity ||
        (data->track_count != 0U && data->tracks == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < data->track_count; ++index) {
        const AfProceduralTrack *track = &data->tracks[index];
        if (af_rig_bone_const(rig, track->bone_id) == NULL ||
            (track->source_bone_id != 0U &&
             af_rig_bone_const(rig, track->source_bone_id) == NULL) ||
            track->type > AF_PROCEDURAL_SECONDARY || track->property > AF_PROCEDURAL_SCALE_Y ||
            !isfinite(track->target) || !isfinite(track->amplitude) ||
            !isfinite(track->frequency) || track->frequency < 0.0F || !isfinite(track->stiffness) ||
            track->stiffness < 0.0F || !isfinite(track->damping) || track->damping < 0.0F) {
            return AF_RESULT_INVALID_ARGUMENT;
        }
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_procedural_runtime_init(AfProceduralRuntime *runtime, const AfProceduralData *data,
                                    AfProceduralState *states, uint32_t capacity) {
    if (runtime == NULL || data == NULL || capacity < data->track_count ||
        (capacity != 0U && states == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *runtime = (AfProceduralRuntime){.data = data,
                                     .states = states,
                                     .state_count = data->track_count,
                                     .state_capacity = capacity};
    if (data->track_count != 0U) {
        memset(states, 0, (size_t)data->track_count * sizeof(AfProceduralState));
    }
    return AF_RESULT_SUCCESS;
}

static float af_procedural_property(const AfBone *bone, AfProceduralProperty property) {
    if (property == AF_PROCEDURAL_POSITION_X) {
        return bone->local_transform.position.x;
    }
    if (property == AF_PROCEDURAL_POSITION_Y) {
        return bone->local_transform.position.y;
    }
    if (property == AF_PROCEDURAL_ROTATION) {
        return bone->local_transform.rotation;
    }
    return property == AF_PROCEDURAL_SCALE_X ? bone->local_transform.scale.x
                                             : bone->local_transform.scale.y;
}

static void af_procedural_set_property(AfBone *bone, AfProceduralProperty property, float value) {
    if (property == AF_PROCEDURAL_POSITION_X) {
        bone->local_transform.position.x = value;
    } else if (property == AF_PROCEDURAL_POSITION_Y) {
        bone->local_transform.position.y = value;
    } else if (property == AF_PROCEDURAL_ROTATION) {
        bone->local_transform.rotation = value;
    } else if (property == AF_PROCEDURAL_SCALE_X) {
        bone->local_transform.scale.x = value;
    } else {
        bone->local_transform.scale.y = value;
    }
}

static uint32_t af_noise_hash(uint32_t value) {
    value ^= value >> 16U;
    value *= 0x7FEB352DU;
    value ^= value >> 15U;
    value *= 0x846CA68BU;
    return value ^ (value >> 16U);
}

static void af_procedural_step(AfProceduralRuntime *runtime, AfRigData *rig) {
    for (uint32_t index = 0U; index < runtime->state_count; ++index) {
        const AfProceduralTrack *track = &runtime->data->tracks[index];
        AfProceduralState *state = &runtime->states[index];
        AfBone *bone = af_rig_bone(rig, track->bone_id);
        const AfBone *source = af_rig_bone_const(rig, track->source_bone_id);
        float target = track->target;
        if (!state->initialized) {
            state->value = af_procedural_property(bone, track->property);
            state->initialized = true;
        }
        if (source != NULL && (track->type == AF_PROCEDURAL_FOLLOW_THROUGH ||
                               track->type == AF_PROCEDURAL_SECONDARY)) {
            target += af_procedural_property(source, track->property);
        }
        if (track->type == AF_PROCEDURAL_OSCILLATION) {
            state->value =
                target + sinf(runtime->time * track->frequency * 6.28318530718F) * track->amplitude;
        } else if (track->type == AF_PROCEDURAL_NOISE) {
            const uint32_t noise = af_noise_hash(track->seed ^ (uint32_t)runtime->tick);
            state->value = target + ((float)(noise & 0xFFFFU) / 32767.5F - 1.0F) * track->amplitude;
        } else {
            const float stiffness = track->type == AF_PROCEDURAL_DAMPED
                                        ? fmaxf(track->stiffness * 0.5F, 0.0001F)
                                        : track->stiffness;
            const float acceleration =
                (target - state->value) * stiffness - state->velocity * track->damping;
            state->velocity += acceleration * AF_PROCEDURAL_FIXED_STEP;
            state->value += state->velocity * AF_PROCEDURAL_FIXED_STEP;
        }
        af_procedural_set_property(bone, track->property, state->value);
    }
    runtime->time += AF_PROCEDURAL_FIXED_STEP;
    ++runtime->tick;
}

AfResult af_procedural_update(AfProceduralRuntime *runtime, float delta_time, AfRigData *rig,
                              AfRigMetrics *metrics) {
    uint32_t steps = 0U;
    if (runtime == NULL || runtime->data == NULL || rig == NULL ||
        runtime->state_count != runtime->data->track_count ||
        runtime->state_count > runtime->state_capacity ||
        (runtime->state_count != 0U && runtime->states == NULL) || !isfinite(delta_time) ||
        delta_time < 0.0F) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    runtime->accumulator += delta_time;
    while (runtime->accumulator >= AF_PROCEDURAL_FIXED_STEP &&
           steps < AF_MAX_FIXED_STEPS_PER_UPDATE) {
        af_procedural_step(runtime, rig);
        runtime->accumulator -= AF_PROCEDURAL_FIXED_STEP;
        ++steps;
    }
    if (steps == AF_MAX_FIXED_STEPS_PER_UPDATE &&
        runtime->accumulator >= AF_PROCEDURAL_FIXED_STEP) {
        runtime->accumulator = fmodf(runtime->accumulator, AF_PROCEDURAL_FIXED_STEP);
    }
    if (metrics != NULL) {
        metrics->fixed_steps += steps;
    }
    return af_rig_update_world(rig);
}

AfResult af_procedural_bake(AfProceduralRuntime *runtime, AfRigData *rig, float duration,
                            float sample_step, AfBoneKeyframe *out_keyframes, uint32_t capacity,
                            uint32_t *out_count, AfRigMetrics *metrics) {
    uint32_t output = 0U;
    if (runtime == NULL || rig == NULL || out_keyframes == NULL || out_count == NULL ||
        !isfinite(duration) || duration < 0.0F || !isfinite(sample_step) || sample_step <= 0.0F) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    const double sample_count = floor((double)duration / (double)sample_step) + 1.0;
    if (sample_count > (double)AF_MAX_BONE_KEYFRAMES) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    const uint32_t samples = (uint32_t)sample_count;
    if (runtime->state_count != 0U && (samples > AF_MAX_BONE_KEYFRAMES / runtime->state_count ||
                                       samples > capacity / runtime->state_count)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    for (uint32_t sample = 0U; sample < samples; ++sample) {
        if (sample != 0U) {
            AfResult result = af_procedural_update(runtime, sample_step, rig, metrics);
            if (result != AF_RESULT_SUCCESS) {
                return result;
            }
        }
        for (uint32_t track = 0U; track < runtime->state_count; ++track) {
            const AfBone *bone = af_rig_bone_const(rig, runtime->data->tracks[track].bone_id);
            out_keyframes[output++] = (AfBoneKeyframe){.bone_id = bone->id,
                                                       .time = (float)sample * sample_step,
                                                       .transform = bone->local_transform};
        }
    }
    *out_count = output;
    return AF_RESULT_SUCCESS;
}
