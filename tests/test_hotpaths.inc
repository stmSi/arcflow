static bool af_test_id_index(void) {
    uint64_t ids[1024];
    uint32_t slots[2048];
    AfIdIndex index;
    for (uint32_t i = 0U; i < 1024U; ++i) {
        ids[i] = UINT64_MAX - (uint64_t)i * UINT64_C(4294967296);
    }
    CHECK(af_id_index_build(&index, ids, 1024U, sizeof(*ids), slots, 2048U) == AF_RESULT_SUCCESS);
    for (uint32_t i = 0U; i < 1024U; ++i) {
        CHECK(af_id_index_find(&index, ids[i]) == i);
    }
    CHECK(af_id_index_find(&index, 0U) == UINT32_MAX);
    ids[1023] = ids[0];
    CHECK(af_id_index_build(&index, ids, 1024U, sizeof(*ids), slots, 2048U) ==
          AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_id_index_build(&index, NULL, 0U, sizeof(*ids), slots, 2048U) == AF_RESULT_SUCCESS);
    CHECK(af_id_index_find(&index, UINT64_MAX) == UINT32_MAX);
    return true;
}

static bool af_test_hierarchy_hotpaths(void) {
    AfSceneNode nodes[70];
    AfLayoutNode layouts[70];
    AfLayoutData data = {.nodes = layouts,
                         .count = 70U,
                         .capacity = 70U,
                         .reference_size = {1000.0F, 1000.0F},
                         .target_size = {1000.0F, 1000.0F}};
    AfScene scene = {.nodes = nodes, .count = 70U, .capacity = 70U};
    /* Parents follow their children; IDs cannot be interpreted as array indices. */
    for (uint32_t i = 0U; i < 70U; ++i) {
        nodes[i] =
            (AfSceneNode){.id = UINT64_C(1) << (i % 60U),
                          .parent_id = AF_INVALID_NODE_ID,
                          .local_transform = {.position = {1.0F, 2.0F}, .scale = {1.0F, 1.0F}},
                          .bounds = {10.0F, 20.0F},
                          .opacity = 1.0F,
                          .visible = true};
        nodes[i].id += (uint64_t)i * 100U;
        layouts[i] = af_layout_node_default(nodes[i].id);
    }
    for (uint32_t i = 0U; i < 69U; ++i) {
        nodes[i].parent_id = nodes[i + 1U].id;
    }
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_INVALID_ARGUMENT);
    scene.count = 64U;
    nodes[63].parent_id = AF_INVALID_NODE_ID;
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_SUCCESS);
    CHECK_NEAR(nodes[0].world_transform.tx, 64.0F, 0.0001F);
    nodes[63].parent_id = nodes[0].id;
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_INVALID_ARGUMENT);
    nodes[63].parent_id = UINT64_MAX - 1U;
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_NOT_FOUND);
    nodes[63].parent_id = AF_INVALID_NODE_ID;
    for (uint32_t i = 0U; i < 63U; ++i) {
        nodes[i].parent_id = nodes[63].id;
    }
    scene.layouts = &data;
    data.count = 64U;
    layouts[63].direction = AF_LAYOUT_DIRECTION_HORIZONTAL;
    layouts[63].spacing = 0.5F;
    /* A descendant of the last sibling forces out-of-order sibling evaluation. */
    nodes[0].parent_id = nodes[62].id;
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_SUCCESS);
    CHECK_NEAR(nodes[62].world_transform.tx, 2.0F + 61.0F * 10.5F, 0.0001F);
    CHECK_NEAR(nodes[0].world_transform.tx, nodes[62].world_transform.tx + 1.0F, 0.0001F);
    nodes[1].bounds.x = 30.0F;
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_SUCCESS);
    CHECK_NEAR(nodes[62].world_transform.tx, 22.0F + 61.0F * 10.5F, 0.0001F);
    layouts[63].direction = AF_LAYOUT_DIRECTION_VERTICAL;
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_SUCCESS);
    CHECK_NEAR(nodes[62].world_transform.ty, 4.0F + 61.0F * 20.5F, 0.0001F);
    layouts[2].node_id = layouts[1].node_id;
    CHECK(af_layout_validate(&data, &scene) == AF_RESULT_INVALID_ARGUMENT);
    layouts[2].node_id = nodes[2].id;
    nodes[63].parent_id = nodes[0].id;
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_INVALID_ARGUMENT);
    scene.count = AF_MAX_NODES + 1U;
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_INVALID_ARGUMENT);
    return true;
}

static bool af_test_animation_hotpaths(void) {
    AfSceneNode nodes[2] = {{.id = 1U,
                             .parent_id = AF_INVALID_NODE_ID,
                             .local_transform = {.scale = {1.0F, 1.0F}},
                             .opacity = 1.0F},
                            {.id = 2U,
                             .parent_id = AF_INVALID_NODE_ID,
                             .local_transform = {.scale = {1.0F, 1.0F}},
                             .opacity = 1.0F}};
    AfScene scene = {.nodes = nodes, .count = 2U, .capacity = 2U};
    AfProjectClip clip = {.id = 7U, .duration = 10.0F};
    AfProjectTrack tracks[3] = {
        {.id = 101U, .clip_id = 7U, .node_id = 1U, .property = AF_PROPERTY_POSITION},
        {.id = 102U, .clip_id = 7U, .node_id = 2U, .property = AF_PROPERTY_ROTATION},
        {.id = 103U, .clip_id = 8U, .node_id = 1U, .property = AF_PROPERTY_POSITION}};
    AfProjectKeyframe keys[] = {
        {.id = 30U,
         .track_id = 101U,
         .keyframe = {.time = 4.0F,
                      .value = {40.0F, 2.0F},
                      .interpolation = AF_INTERPOLATION_LINEAR}},
        {.id = 99U, .track_id = 103U, .keyframe = {.time = 0.0F, .value = {999.0F}}},
        {.id = 10U,
         .track_id = 101U,
         .keyframe = {.time = 2.0F, .value = {20.0F}, .interpolation = AF_INTERPOLATION_LINEAR}},
        {.id = 20U,
         .track_id = 101U,
         .keyframe = {.time = 2.0F, .value = {22.0F}, .interpolation = AF_INTERPOLATION_LINEAR}},
        {.id = 40U,
         .track_id = 101U,
         .keyframe = {.time = 4.0F, .value = {44.0F}, .interpolation = AF_INTERPOLATION_LINEAR}}};
    AfProject project = {.clips = &clip,
                         .clip_count = 1U,
                         .tracks = tracks,
                         .track_count = 3U,
                         .keyframes = keys,
                         .keyframe_count = 5U};
    nodes[1].local_transform.rotation = 0.75F;
    const float times[] = {-1.0F, 2.0F, 3.0F, 4.0F, 12.0F};
    const float expected[] = {20.0F, 22.0F, 31.0F, 44.0F, 44.0F};
    for (uint32_t i = 0U; i < 5U; ++i) {
        CHECK(af_project_animation_apply(&project, 7U, times[i], &scene) == AF_RESULT_SUCCESS);
        CHECK_NEAR(nodes[0].local_transform.position.x, expected[i], 0.0001F);
        CHECK_NEAR(nodes[1].local_transform.rotation, 0.75F, 0.0001F);
    }
    /* Reorder storage and change a key: each evaluation must reflect edits immediately. */
    AfProjectKeyframe swap = keys[0];
    keys[0] = keys[4];
    keys[4] = swap;
    keys[3].keyframe.value[0] = 24.0F;
    CHECK(af_project_animation_apply(&project, 7U, 3.0F, &scene) == AF_RESULT_SUCCESS);
    CHECK_NEAR(nodes[0].local_transform.position.x, 32.0F, 0.0001F);
    AfKeyframe runtime_keys[4] = {keys[2].keyframe, keys[3].keyframe, keys[4].keyframe,
                                  keys[0].keyframe};
    AfAnimationTrack track = {.keyframe_count = 4U};
    AfAnimationData animation = {.keyframes = runtime_keys, .keyframe_count = 4U};
    float value[4];
    for (uint32_t i = 0U; i < 5U; ++i) {
        CHECK(af_animation_sample_track(&animation, &track, times[i], value) == AF_RESULT_SUCCESS);
        CHECK_NEAR(value[0], i == 1U ? 24.0F : i == 2U ? 32.0F : expected[i], 0.0001F);
    }
    runtime_keys[1].interpolation = AF_INTERPOLATION_STEP;
    CHECK(af_animation_sample_track(&animation, &track, 3.0F, value) == AF_RESULT_SUCCESS);
    CHECK_NEAR(value[0], 24.0F, 0.0001F);
    return true;
}

static bool af_test_render_batches(void) {
    AfVkRenderItem items[513];
    AfVkRenderItem scratch[513];
    AfVkRenderItem reference[513];
    for (uint32_t count = 0U; count <= 513U; count += count < 20U ? 1U : 17U) {
        for (uint32_t i = 0U; i < count; ++i) {
            const AfVkRenderItem item = {.type = (AfVkRenderItemType)(i % 3U),
                                         .layer_order = i % 19U == 0U ? INT32_MIN
                                                        : i % 17U == 0U
                                                            ? INT32_MAX
                                                            : (int32_t)(i * 73U % 31U) - 15,
                                         .first = i * 10U,
                                         .count = 3U,
                                         .atlas_index = i % 4U};
            items[i] = item;
            uint32_t at = i;
            while (at != 0U && reference[at - 1U].layer_order > item.layer_order) {
                reference[at] = reference[at - 1U];
                --at;
            }
            reference[at] = item;
        }
        CHECK(af_vk_prepare_render_items(items, scratch, count) == count);
        CHECK(memcmp(items, reference, (size_t)count * sizeof(*items)) == 0);
    }
    for (uint32_t i = 0U; i < 513U; ++i) {
        items[i] = (AfVkRenderItem){
            .type = AF_VK_RENDER_ANALYTIC, .layer_order = 4, .first = i, .count = 1U};
    }
    CHECK(af_vk_prepare_render_items(items, scratch, 513U) == 1U);
    CHECK(items[0].first == 0U && items[0].count == 513U);
    const AfVkRenderItem boundaries[] = {
        {.type = AF_VK_RENDER_ANALYTIC, .layer_order = -1, .first = 0U, .count = 1U},
        {.type = AF_VK_RENDER_ANALYTIC, .layer_order = 0, .first = 1U, .count = 1U},
        {.type = AF_VK_RENDER_PATH, .layer_order = 0, .first = 0U, .count = 3U},
        {.type = AF_VK_RENDER_PATH, .layer_order = 0, .first = 3U, .count = 6U},
        {.type = AF_VK_RENDER_IMAGE, .layer_order = 0, .first = 0U, .count = 6U, .atlas_index = 0U},
        {.type = AF_VK_RENDER_IMAGE, .layer_order = 0, .first = 6U, .count = 6U, .atlas_index = 1U},
        {.type = AF_VK_RENDER_IMAGE,
         .layer_order = 0,
         .first = 12U,
         .count = 6U,
         .atlas_index = 1U},
        {.type = AF_VK_RENDER_IMAGE,
         .layer_order = 0,
         .first = 24U,
         .count = 6U,
         .atlas_index = 1U}};
    memcpy(items, boundaries, sizeof(boundaries));
    CHECK(af_vk_prepare_render_items(items, scratch, 8U) == 6U);
    CHECK(items[2].type == AF_VK_RENDER_PATH && items[2].count == 9U);
    CHECK(items[4].type == AF_VK_RENDER_IMAGE && items[4].count == 12U &&
          items[4].atlas_index == 1U);
    CHECK(items[5].first == 24U && items[5].count == 6U);
    return true;
}
