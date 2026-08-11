static bool af_test_rigging_and_procedural_motion(void) {
    AfBone bones[5] = {{.id = 1U,
                        .rest_transform = {.scale = {1.0F, 1.0F}},
                        .local_transform = {.scale = {1.0F, 1.0F}},
                        .length = 10.0F},
                       {.id = 2U,
                        .parent_id = 1U,
                        .rest_transform = {.position = {10.0F, 0.0F}, .scale = {1.0F, 1.0F}},
                        .local_transform = {.position = {10.0F, 0.0F}, .scale = {1.0F, 1.0F}},
                        .length = 10.0F},
                       {.id = 3U,
                        .parent_id = 2U,
                        .rest_transform = {.position = {10.0F, 0.0F}, .scale = {1.0F, 1.0F}},
                        .local_transform = {.position = {10.0F, 0.0F}, .scale = {1.0F, 1.0F}},
                        .length = 5.0F}};
    AfSkinWeight weights[4] = {{.vertex_index = 0U, .bone_id = 1U, .weight = 1.0F},
                               {.vertex_index = 1U, .bone_id = 2U, .weight = 1.0F},
                               {.vertex_index = 2U, .bone_id = 3U, .weight = 0.75F},
                               {.vertex_index = 2U, .bone_id = 2U, .weight = 0.25F}};
    AfRigConstraint constraints[4] = {0};
    AfRigData rig = {.bones = bones,
                     .bone_count = 3U,
                     .bone_capacity = 5U,
                     .weights = weights,
                     .weight_count = 4U,
                     .weight_capacity = 4U,
                     .constraints = constraints,
                     .constraint_capacity = 4U};
    AfPath path = {.id = 50U,
                   .node_id = 1U,
                   .command_count = 3U,
                   .fill_rule = AF_PATH_FILL_NON_ZERO,
                   .geometry_generation = 7U};
    AfPathCommand path_commands[3] = {{.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}},
                                      {.type = AF_PATH_LINE, .point = {10.0F, 10.0F}},
                                      {.type = AF_PATH_LINE, .point = {20.0F, 0.0F}}};
    AfPathData path_data = {.paths = &path,
                            .path_count = 1U,
                            .path_capacity = 1U,
                            .commands = path_commands,
                            .command_count = 3U,
                            .command_capacity = 3U};
    AfPathTessellationScratch scratch;
    const AfVec2 bind_vertices[4] = {{0.0F, 0.0F}, {10.0F, 0.0F}, {22.0F, 0.0F}, {30.0F, 5.0F}};
    AfVec2 skinned[4];
    AfRigMetrics metrics = {0};
    const uint64_t path_generation = path.geometry_generation;
    CHECK(af_path_data_validate(&path_data) == AF_RESULT_SUCCESS);
    CHECK(af_rig_validate(&rig, 4U, &path_data) == AF_RESULT_SUCCESS);
    CHECK(af_rig_evaluate(&rig, &path_data, NULL, &metrics) == AF_RESULT_SUCCESS);
    CHECK(af_rig_skin(&rig, bind_vertices, 4U, skinned, 4U, &metrics) == AF_RESULT_SUCCESS);
    for (uint32_t index = 0U; index < 4U; ++index) {
        CHECK_NEAR(skinned[index].x, bind_vertices[index].x, 0.0001F);
        CHECK_NEAR(skinned[index].y, bind_vertices[index].y, 0.0001F);
    }
    CHECK(metrics.deformed_vertices == 4U);

    {
        const AfBone added = {.id = 4U,
                              .parent_id = 3U,
                              .rest_transform = {.position = {5.0F, 0.0F}, .scale = {1.0F, 1.0F}},
                              .local_transform = {.position = {5.0F, 0.0F}, .scale = {1.0F, 1.0F}},
                              .length = 2.0F};
        CHECK(af_rig_add_bone(&rig, &added) == AF_RESULT_SUCCESS);
        CHECK(af_rig_delete_bone(&rig, 3U) == AF_RESULT_INVALID_ARGUMENT);
        CHECK(af_rig_delete_bone(&rig, 4U) == AF_RESULT_SUCCESS);
        CHECK(af_rig_reparent_bone(&rig, 1U, 3U) == AF_RESULT_INVALID_ARGUMENT);
        CHECK(bones[0].parent_id == 0U);
    }

    for (uint32_t type = AF_CONSTRAINT_IK; type <= AF_CONSTRAINT_TRANSFORM_COPY; ++type) {
        for (uint32_t bone = 0U; bone < rig.bone_count; ++bone) {
            bones[bone].local_transform = bones[bone].rest_transform;
        }
        constraints[0] =
            (AfRigConstraint){.id = 10U + type,
                              .type = (AfConstraintType)type,
                              .bone_id = type == AF_CONSTRAINT_IK ? 3U : 1U,
                              .target_bone_id = type == AF_CONSTRAINT_TRANSFORM_COPY ? 2U : 0U,
                              .path_id = 50U,
                              .target_position = {15.0F, 15.0F},
                              .target_rotation = 0.4F,
                              .target_scale = {1.2F, 0.8F},
                              .distance = 5.0F,
                              .path_position = 0.5F,
                              .weight = 0.75F,
                              .chain_length = 3U,
                              .iterations = 3U};
        rig.constraint_count = 1U;
        metrics.solver_iterations = 0U;
        CHECK(af_rig_validate(&rig, 4U, &path_data) == AF_RESULT_SUCCESS);
        CHECK(af_rig_evaluate(&rig, &path_data, &scratch, &metrics) == AF_RESULT_SUCCESS);
        CHECK(metrics.solver_iterations > 0U &&
              metrics.solver_iterations <= AF_MAX_SOLVER_ITERATIONS * AF_MAX_IK_CHAIN_LENGTH);
        for (uint32_t bone = 0U; bone < rig.bone_count; ++bone) {
            CHECK(isfinite(bones[bone].world_transform.tx));
            CHECK(isfinite(bones[bone].world_transform.ty));
        }
    }

    constraints[0] = (AfRigConstraint){.id = 30U,
                                       .type = AF_CONSTRAINT_POSITION,
                                       .bone_id = 1U,
                                       .target_bone_id = 2U,
                                       .weight = 1.0F};
    constraints[1] = (AfRigConstraint){.id = 31U,
                                       .type = AF_CONSTRAINT_POSITION,
                                       .bone_id = 1U,
                                       .target_bone_id = 3U,
                                       .weight = 1.0F};
    constraints[2] = (AfRigConstraint){.id = 32U,
                                       .type = AF_CONSTRAINT_POSITION,
                                       .bone_id = 3U,
                                       .target_bone_id = 1U,
                                       .weight = 1.0F};
    rig.constraint_count = 3U;
    CHECK(af_rig_validate(&rig, 4U, &path_data) == AF_RESULT_INVALID_ARGUMENT);
    rig.constraint_count = 0U;

    {
        AfBoneKeyframe keys[4] = {
            {.bone_id = 1U, .time = 0.0F, .transform = bones[0].rest_transform},
            {.bone_id = 3U, .time = 0.0F, .transform = bones[2].rest_transform},
            {.bone_id = 1U,
             .time = 1.0F,
             .transform = {.position = {2.0F, 0.0F}, .rotation = 0.5F, .scale = {1.0F, 1.0F}}},
            {.bone_id = 3U,
             .time = 1.0F,
             .transform = {.position = {10.0F, 2.0F}, .rotation = -0.25F, .scale = {1.0F, 1.0F}}}};
        AfBoneAnimation animation = {
            .keyframes = keys, .keyframe_count = 4U, .keyframe_capacity = 4U, .duration = 1.0F};
        CHECK(af_bone_animation_validate(&animation, &rig) == AF_RESULT_SUCCESS);
        CHECK(af_bone_animation_apply(&animation, 0.5F, &rig) == AF_RESULT_SUCCESS);
        CHECK_NEAR(bones[0].local_transform.position.x, 1.0F, 0.0001F);
        CHECK_NEAR(bones[0].local_transform.rotation, 0.25F, 0.0001F);
        CHECK(af_rig_skin(&rig, bind_vertices, 4U, skinned, 4U, NULL) == AF_RESULT_SUCCESS);
        CHECK(fabsf(skinned[1].y - bind_vertices[1].y) > 0.1F);
        CHECK_NEAR(bind_vertices[1].x, 10.0F, 0.0001F);
        CHECK(path.geometry_generation == path_generation);
    }

    {
        AfProceduralTrack tracks[6] = {{.bone_id = 1U,
                                        .type = AF_PROCEDURAL_SPRING,
                                        .property = AF_PROCEDURAL_POSITION_X,
                                        .target = 3.0F,
                                        .stiffness = 30.0F,
                                        .damping = 5.0F},
                                       {.bone_id = 1U,
                                        .type = AF_PROCEDURAL_DAMPED,
                                        .property = AF_PROCEDURAL_POSITION_Y,
                                        .target = 2.0F,
                                        .stiffness = 20.0F,
                                        .damping = 8.0F},
                                       {.bone_id = 1U,
                                        .type = AF_PROCEDURAL_OSCILLATION,
                                        .property = AF_PROCEDURAL_ROTATION,
                                        .amplitude = 0.2F,
                                        .frequency = 2.0F},
                                       {.bone_id = 2U,
                                        .type = AF_PROCEDURAL_NOISE,
                                        .property = AF_PROCEDURAL_ROTATION,
                                        .amplitude = 0.1F,
                                        .seed = 1234U},
                                       {.bone_id = 3U,
                                        .source_bone_id = 2U,
                                        .type = AF_PROCEDURAL_FOLLOW_THROUGH,
                                        .property = AF_PROCEDURAL_POSITION_Y,
                                        .target = 0.5F,
                                        .stiffness = 15.0F,
                                        .damping = 4.0F},
                                       {.bone_id = 3U,
                                        .source_bone_id = 1U,
                                        .type = AF_PROCEDURAL_SECONDARY,
                                        .property = AF_PROCEDURAL_ROTATION,
                                        .stiffness = 12.0F,
                                        .damping = 3.0F}};
        AfProceduralData procedural = {.tracks = tracks, .track_count = 6U, .track_capacity = 6U};
        AfBone deterministic_bones[3];
        AfRigData deterministic_rig = rig;
        AfProceduralState first_states[6];
        AfProceduralState second_states[6];
        AfProceduralRuntime first_runtime;
        AfProceduralRuntime second_runtime;
        AfBoneKeyframe baked[64];
        uint32_t baked_count = 0U;
        for (uint32_t bone = 0U; bone < rig.bone_count; ++bone) {
            bones[bone].local_transform = bones[bone].rest_transform;
        }
        memcpy(deterministic_bones, bones, sizeof(deterministic_bones));
        deterministic_rig.bones = deterministic_bones;
        CHECK(af_procedural_validate(&procedural, &rig) == AF_RESULT_SUCCESS);
        CHECK(af_procedural_runtime_init(&first_runtime, &procedural, first_states, 6U) ==
              AF_RESULT_SUCCESS);
        CHECK(af_procedural_runtime_init(&second_runtime, &procedural, second_states, 6U) ==
              AF_RESULT_SUCCESS);
        metrics.fixed_steps = 0U;
        CHECK(af_procedural_update(&first_runtime, 1.0F, &rig, &metrics) == AF_RESULT_SUCCESS);
        CHECK(af_procedural_update(&second_runtime, 1.0F, &deterministic_rig, NULL) ==
              AF_RESULT_SUCCESS);
        CHECK(metrics.fixed_steps == AF_MAX_FIXED_STEPS_PER_UPDATE);
        CHECK(memcmp(bones, deterministic_bones, sizeof(deterministic_bones)) == 0);
        CHECK(first_runtime.tick == second_runtime.tick &&
              memcmp(first_states, second_states, sizeof(first_states)) == 0);
        CHECK(af_procedural_runtime_init(&first_runtime, &procedural, first_states, 6U) ==
              AF_RESULT_SUCCESS);
        CHECK(af_procedural_bake(&first_runtime, &rig, 0.05F, 0.01F, baked, 64U, &baked_count,
                                 &metrics) == AF_RESULT_SUCCESS);
        CHECK(baked_count > 0U && baked_count <= 64U);
        {
            AfBoneAnimation baked_animation = {.keyframes = baked,
                                               .keyframe_count = baked_count,
                                               .keyframe_capacity = 64U,
                                               .duration = 0.05F};
            CHECK(af_bone_animation_validate(&baked_animation, &rig) == AF_RESULT_SUCCESS);
        }
        CHECK(af_procedural_bake(&first_runtime, &rig, 1000.0F, 0.0001F, baked, 64U, &baked_count,
                                 NULL) == AF_RESULT_LIMIT_EXCEEDED);
    }
    CHECK(path.geometry_generation == path_generation);
    return true;
}

static uint32_t af_test_read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) | ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static uint64_t af_test_read_u64(const uint8_t *bytes) {
    return (uint64_t)af_test_read_u32(bytes) | ((uint64_t)af_test_read_u32(bytes + 4U) << 32U);
}

static void af_test_write_u32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    bytes[2] = (uint8_t)(value >> 16U);
    bytes[3] = (uint8_t)(value >> 24U);
}

static void af_test_write_u64(uint8_t *bytes, uint64_t value) {
    af_test_write_u32(bytes, (uint32_t)value);
    af_test_write_u32(bytes + 4U, (uint32_t)(value >> 32U));
}

static uint32_t af_test_checksum(const uint8_t *bytes, size_t size) {
    uint32_t hash = 2166136261U;
    size_t index = 0U;
    for (index = 0U; index < size; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

static void af_test_rechecksum(AfPackageBlob *blob) {
    af_test_write_u32(blob->data + 48U, af_test_checksum(blob->data + AF_PACKAGE_HEADER_SIZE,
                                                         blob->size - AF_PACKAGE_HEADER_SIZE));
}
