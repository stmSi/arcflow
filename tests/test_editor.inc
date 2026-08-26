static bool af_test_timeline_authoring(void) {
    static const char legacy_project[] = "arcflow_project 1\n"
                                         "canvas 640 360\n"
                                         "next_id 2\n"
                                         "node\n"
                                         "id 1\n"
                                         "parent none\n"
                                         "type rectangle\n"
                                         "name Legacy Rectangle\n"
                                         "position 0 0\n"
                                         "rotation 0\n"
                                         "scale 1 1\n"
                                         "bounds 100 50\n"
                                         "fill 1 0 0 1\n"
                                         "opacity 1\n"
                                         "corner_radius 0\n"
                                         "layer 0\n"
                                         "visible 1\n"
                                         "end\n";
    AfAllocator allocator;
    AfProject project = {0};
    AfProject migrated = {0};
    AfEditorHistory history = {0};
    AfEditorCommand command = {0};
    AfSceneNode shape = {.id = 1U,
                         .parent_id = AF_INVALID_NODE_ID,
                         .type = AF_NODE_ROUNDED_RECTANGLE,
                         .local_transform = {.position = {10.0F, 20.0F}, .scale = {1.0F, 1.0F}},
                         .bounds = {100.0F, 50.0F},
                         .fill = {0.1F, 0.2F, 0.3F, 1.0F},
                         .opacity = 1.0F,
                         .corner_radius = 4.0F,
                         .visible = true};
    AfProjectClip clip = {.id = 100U,
                          .name = "Authoring Clip",
                          .duration = 4.0F,
                          .loop_start = 0.5F,
                          .loop_end = 3.5F,
                          .loop = true};
    AfProjectTrack tracks[6];
    AfProjectKeyframe keys[12];
    AfSceneNode preview_node;
    AfScene preview = {.nodes = &preview_node, .count = 1U, .capacity = 1U};
    uint64_t large_ids[AF_EDITOR_COMMAND_MAX_KEYFRAMES];
    uint32_t index = 0U;
    af_allocator_init_default(&allocator);
    CHECK(af_project_create(&project, &allocator, 4U) == AF_RESULT_SUCCESS);
    CHECK(af_project_add_node(&project, &shape, "Animated Shape") == AF_RESULT_SUCCESS);
    CHECK(af_project_add_clip(&project, &clip) == AF_RESULT_SUCCESS);
    for (index = 0U; index < 6U; ++index) {
        tracks[index] = (AfProjectTrack){.id = 101U + index,
                                         .clip_id = clip.id,
                                         .node_id = shape.id,
                                         .property = (AfPropertyType)index};
        CHECK(af_project_add_track(&project, &tracks[index]) == AF_RESULT_SUCCESS);
    }
    preview_node = shape;
    CHECK(af_project_animation_apply(&project, clip.id, 0.5F, &preview) == AF_RESULT_SUCCESS);
    CHECK_NEAR(preview_node.local_transform.position.x, 10.0F, 0.0001F);
    for (index = 0U; index < 6U; ++index) {
        AfProjectKeyframe first = {.id = 200U + index * 2U,
                                   .track_id = tracks[index].id,
                                   .keyframe = {.time = 0.0F,
                                                .interpolation = AF_INTERPOLATION_LINEAR,
                                                .easing = {0.42F, 0.0F, 0.58F, 1.0F}}};
        AfProjectKeyframe second = {
            .id = 201U + index * 2U,
            .track_id = tracks[index].id,
            .keyframe = {.time = 1.0F, .interpolation = AF_INTERPOLATION_LINEAR}};
        if (index == AF_PROPERTY_POSITION) {
            first.keyframe.value[0] = 0.0F;
            first.keyframe.value[1] = 10.0F;
            second.keyframe.value[0] = 100.0F;
            second.keyframe.value[1] = 110.0F;
        } else if (index == AF_PROPERTY_ROTATION) {
            first.keyframe.value[0] = 0.0F;
            second.keyframe.value[0] = 1.0F;
            first.keyframe.interpolation = AF_INTERPOLATION_STEP;
        } else if (index == AF_PROPERTY_SCALE) {
            first.keyframe.value[0] = 1.0F;
            first.keyframe.value[1] = 0.5F;
            second.keyframe.value[0] = 2.0F;
            second.keyframe.value[1] = 1.5F;
            first.keyframe.interpolation = AF_INTERPOLATION_CUBIC_BEZIER;
        } else if (index == AF_PROPERTY_OPACITY) {
            first.keyframe.value[0] = 0.2F;
            second.keyframe.value[0] = 1.0F;
        } else if (index == AF_PROPERTY_FILL_COLOR) {
            first.keyframe.value[0] = 0.0F;
            first.keyframe.value[1] = 0.2F;
            first.keyframe.value[2] = 0.4F;
            first.keyframe.value[3] = 1.0F;
            second.keyframe.value[0] = 1.0F;
            second.keyframe.value[1] = 0.8F;
            second.keyframe.value[2] = 0.6F;
            second.keyframe.value[3] = 0.5F;
            first.keyframe.interpolation = AF_INTERPOLATION_CUBIC_BEZIER;
        } else {
            first.keyframe.value[0] = 0.0F;
            second.keyframe.value[0] = 20.0F;
        }
        keys[index * 2U] = first;
        keys[index * 2U + 1U] = second;
        CHECK(af_project_add_keyframe(&project, &second) == AF_RESULT_SUCCESS);
        CHECK(af_project_add_keyframe(&project, &first) == AF_RESULT_SUCCESS);
    }
    af_allocator_begin_frame(&allocator);
    for (index = 0U; index < 64U; ++index) {
        preview_node = shape;
        CHECK(af_project_animation_apply(&project, clip.id, 0.5F, &preview) == AF_RESULT_SUCCESS);
        CHECK_NEAR(preview_node.local_transform.position.x, 50.0F, 0.001F);
        CHECK_NEAR(preview_node.local_transform.position.y, 60.0F, 0.001F);
        CHECK_NEAR(preview_node.local_transform.rotation, 0.0F, 0.001F);
        CHECK_NEAR(preview_node.local_transform.scale.x, 1.5F, 0.002F);
        CHECK_NEAR(preview_node.opacity, 0.6F, 0.001F);
        CHECK_NEAR(preview_node.fill.r, 0.5F, 0.002F);
        CHECK_NEAR(preview_node.corner_radius, 10.0F, 0.001F);
    }
    CHECK(af_allocator_end_frame(&allocator) == 0U);
    project.tracks[0].node_id = 999U;
    CHECK(af_project_validate(&project) == AF_RESULT_INVALID_ARGUMENT);
    project.tracks[0].node_id = shape.id;
    project.keyframes[0].keyframe.time = 5.0F;
    CHECK(af_project_validate(&project) == AF_RESULT_INVALID_ARGUMENT);
    project.keyframes[0].keyframe.time = 1.0F;
    project.keyframes[0].id = tracks[0].id;
    CHECK(af_project_validate(&project) == AF_RESULT_INVALID_ARGUMENT);
    project.keyframes[0].id = keys[1].id;
    CHECK(af_project_validate(&project) == AF_RESULT_SUCCESS);

    CHECK(af_editor_history_create(&history, &allocator, sizeof(AfEditorCommand) * 8U) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_command_animation(AF_EDITOR_MERGE_KEYFRAME_TIME, keys[0].id, &command) ==
          AF_RESULT_SUCCESS);
    CHECK(af_project_keyframe_snapshot(&project, keys[10].id, &command.keyframe_before[0]) ==
          AF_RESULT_SUCCESS);
    CHECK(af_project_keyframe_snapshot(&project, keys[0].id, &command.keyframe_before[1]) ==
          AF_RESULT_SUCCESS);
    command.keyframe_after[0] = command.keyframe_before[0];
    command.keyframe_after[1] = command.keyframe_before[1];
    command.keyframe_after[0].keyframe.keyframe.time = 2.0F;
    command.keyframe_after[1].keyframe.keyframe.time = 0.25F;
    command.keyframe_before_count = 2U;
    command.keyframe_after_count = 2U;
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR(af_project_find_keyframe(&project, keys[10].id)->keyframe.time, 2.0F, 0.0001F);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK_NEAR(af_project_find_keyframe(&project, keys[10].id)->keyframe.time, 0.0F, 0.0001F);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK_NEAR(af_project_find_keyframe(&project, keys[0].id)->keyframe.time, 0.25F, 0.0001F);

    CHECK(af_editor_command_delete(&project, shape.id, &command) == AF_RESULT_SUCCESS);
    CHECK(command.track_before_count == 6U && command.keyframe_before_count == 12U);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(project.scene.count == 0U && project.track_count == 0U && project.keyframe_count == 0U);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.scene.count == 1U && project.track_count == 6U && project.keyframe_count == 12U);

    for (index = 0U; index < AF_EDITOR_COMMAND_MAX_KEYFRAMES; ++index) {
        AfProjectKeyframe key = {.track_id = tracks[0].id,
                                 .keyframe = {.time = (float)(index % 200U) * 0.02F,
                                              .value = {(float)index, 0.0F},
                                              .interpolation = AF_INTERPOLATION_LINEAR}};
        CHECK(af_project_allocate_animation_id(&project, &key.id) == AF_RESULT_SUCCESS);
        large_ids[index] = key.id;
        CHECK(af_project_add_keyframe(&project, &key) == AF_RESULT_SUCCESS);
    }
    CHECK(af_editor_command_animation(AF_EDITOR_MERGE_KEYFRAME_TIME, large_ids[0], &command) ==
          AF_RESULT_SUCCESS);
    for (index = 0U; index < AF_EDITOR_COMMAND_MAX_KEYFRAMES; ++index) {
        CHECK(af_project_keyframe_snapshot(&project,
                                           large_ids[AF_EDITOR_COMMAND_MAX_KEYFRAMES - index - 1U],
                                           &command.keyframe_before[index]) == AF_RESULT_SUCCESS);
        command.keyframe_after[index] = command.keyframe_before[index];
        command.keyframe_after[index].keyframe.keyframe.time =
            4.0F - command.keyframe_after[index].keyframe.keyframe.time;
    }
    command.keyframe_before_count = AF_EDITOR_COMMAND_MAX_KEYFRAMES;
    command.keyframe_after_count = AF_EDITOR_COMMAND_MAX_KEYFRAMES;
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.keyframe_count == 12U + AF_EDITOR_COMMAND_MAX_KEYFRAMES);
    CHECK(af_project_validate(&project) == AF_RESULT_SUCCESS);

    CHECK(af_project_deserialize(legacy_project, strlen(legacy_project), &allocator, &migrated) ==
          AF_RESULT_SUCCESS);
    CHECK(migrated.scene.count == 1U && migrated.clip_count == 0U &&
          migrated.next_animation_id == 1U);
    af_project_destroy(&migrated);
    af_editor_history_destroy(&history);
    af_project_destroy(&project);
    return true;
}

static bool af_test_editor_gizmo(void) {
    AfSceneNode node = {.id = 1U,
                        .parent_id = AF_INVALID_NODE_ID,
                        .type = AF_NODE_RECTANGLE,
                        .local_transform = {.position = {10.0F, 20.0F}, .scale = {1.0F, 1.0F}},
                        .bounds = {100.0F, 50.0F},
                        .opacity = 1.0F,
                        .visible = true,
                        .world_visible = true};
    AfEditorGizmoGeometry geometry = {0};
    node.world_transform = af_affine_from_transform(node.local_transform);
    CHECK(af_editor_gizmo_geometry(&node, af_affine_identity(), &geometry));
    CHECK_NEAR(geometry.corners[0].x, 10.0F, 0.0001F);
    CHECK_NEAR(geometry.corners[2].y, 70.0F, 0.0001F);
    CHECK_NEAR(geometry.scale_x.x, 110.0F, 0.0001F);
    CHECK_NEAR(geometry.scale_y.y, 70.0F, 0.0001F);
    CHECK_NEAR(geometry.skew_x.y, 88.0F, 0.0001F);
    CHECK_NEAR(geometry.skew_y.x, 128.0F, 0.0001F);
    CHECK_NEAR(geometry.rotate.x, 60.0F, 0.0001F);
    CHECK_NEAR(geometry.rotate.y, -12.0F, 0.0001F);
    CHECK(af_editor_gizmo_pick(&geometry, geometry.move_x, 8.0F) == AF_EDITOR_GIZMO_MOVE_X);
    CHECK(af_editor_gizmo_pick(&geometry, geometry.move_y, 8.0F) == AF_EDITOR_GIZMO_MOVE_Y);
    CHECK(af_editor_gizmo_pick(&geometry, geometry.scale_x, 8.0F) == AF_EDITOR_GIZMO_SCALE_X);
    CHECK(af_editor_gizmo_pick(&geometry, geometry.scale_y, 8.0F) == AF_EDITOR_GIZMO_SCALE_Y);
    CHECK(af_editor_gizmo_pick(&geometry, geometry.scale_xy, 8.0F) == AF_EDITOR_GIZMO_SCALE_XY);
    CHECK(af_editor_gizmo_pick(&geometry, geometry.skew_x, 8.0F) == AF_EDITOR_GIZMO_SKEW_X);
    CHECK(af_editor_gizmo_pick(&geometry, geometry.skew_y, 8.0F) == AF_EDITOR_GIZMO_SKEW_Y);
    CHECK(af_editor_gizmo_pick(&geometry, geometry.rotate, 8.0F) == AF_EDITOR_GIZMO_ROTATE);
    CHECK(af_editor_gizmo_pick(&geometry, (AfVec2){500.0F, 500.0F}, 8.0F) == AF_EDITOR_GIZMO_NONE);
    CHECK(af_editor_gizmo_property(AF_EDITOR_GIZMO_MOVE_X) == AF_PROPERTY_POSITION);
    CHECK(af_editor_gizmo_property(AF_EDITOR_GIZMO_MOVE_Y) == AF_PROPERTY_POSITION);
    CHECK(af_editor_gizmo_property(AF_EDITOR_GIZMO_SCALE_X) == AF_PROPERTY_SCALE);
    CHECK(af_editor_gizmo_property(AF_EDITOR_GIZMO_SCALE_Y) == AF_PROPERTY_SCALE);
    CHECK(af_editor_gizmo_property(AF_EDITOR_GIZMO_SCALE_XY) == AF_PROPERTY_SCALE);
    CHECK(af_editor_gizmo_property(AF_EDITOR_GIZMO_SKEW_X) == AF_PROPERTY_SKEW);
    CHECK(af_editor_gizmo_property(AF_EDITOR_GIZMO_SKEW_Y) == AF_PROPERTY_SKEW);
    CHECK(af_editor_gizmo_property(AF_EDITOR_GIZMO_ROTATE) == AF_PROPERTY_ROTATION);
    node.local_transform.skew = (AfVec2){AF_TRANSFORM_MAX_SKEW_RADIANS + 0.01F, 0.0F};
    {
        AfScene scene = {.nodes = &node, .count = 1U, .capacity = 1U};
        CHECK(af_scene_validate(&scene) == AF_RESULT_INVALID_ARGUMENT);
    }
    return true;
}

static bool af_test_editor_commands(void) {
    AfAllocator allocator;
    AfProject project = {0};
    AfEditorHistory history = {0};
    AfProjectNodeSnapshot snapshot = {0};
    AfProjectNodeSnapshot before = {0};
    AfProjectNodeSnapshot after = {0};
    AfEditorCommand command = {0};
    AfProjectNodeSnapshot duplicate = {0};
    AfSceneNode root = {.id = 1U,
                        .parent_id = AF_INVALID_NODE_ID,
                        .type = AF_NODE_GROUP,
                        .local_transform = {.scale = {1.0F, 1.0F}},
                        .opacity = 1.0F,
                        .visible = true};
    AfSceneNode rectangle = {.id = 2U,
                             .parent_id = 1U,
                             .type = AF_NODE_RECTANGLE,
                             .local_transform = {.position = {20.0F, 30.0F}, .scale = {1.0F, 1.0F}},
                             .bounds = {100.0F, 60.0F},
                             .fill = {0.2F, 0.4F, 0.8F, 1.0F},
                             .opacity = 1.0F,
                             .visible = true};
    const size_t history_limit = sizeof(AfEditorCommand) * 3U;
    uint32_t count_before_merge = 0U;
    af_allocator_init_default(&allocator);
    CHECK(af_project_create(&project, &allocator, 16U) == AF_RESULT_SUCCESS);
    CHECK(af_project_add_node(&project, &root, "Root") == AF_RESULT_SUCCESS);
    CHECK(af_project_add_node(&project, &rectangle, "Rectangle") == AF_RESULT_SUCCESS);
    CHECK(af_scene_evaluate_world(&project.scene) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_create(&history, &allocator, history_limit) == AF_RESULT_SUCCESS);
    snapshot = (AfProjectNodeSnapshot){.node = {.id = 3U,
                                                .parent_id = 1U,
                                                .type = AF_NODE_ELLIPSE,
                                                .local_transform = {.scale = {1.0F, 1.0F}},
                                                .bounds = {40.0F, 40.0F},
                                                .fill = {1.0F, 0.2F, 0.3F, 1.0F},
                                                .opacity = 1.0F,
                                                .visible = true},
                                       .index = 2U};
    (void)snprintf(snapshot.name, sizeof(snapshot.name), "Ellipse");
    CHECK(af_editor_command_create(&snapshot, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 3U) != NULL);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 3U) == NULL);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 3U) != NULL);
    CHECK(af_project_snapshot(&project, 2U, &before) == AF_RESULT_SUCCESS);
    af_scene_find(&project.scene, 2U)->local_transform.position.x = 31.0F;
    CHECK(af_project_snapshot(&project, 2U, &after) == AF_RESULT_SUCCESS);
    CHECK(af_editor_command_modify(&before, &after, AF_EDITOR_MERGE_TRANSFORM, &command) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, true, true) == AF_RESULT_SUCCESS);
    count_before_merge = history.count;
    before = after;
    af_scene_find(&project.scene, 2U)->local_transform.position.x = 45.0F;
    CHECK(af_project_snapshot(&project, 2U, &after) == AF_RESULT_SUCCESS);
    CHECK(af_editor_command_modify(&before, &after, AF_EDITOR_MERGE_TRANSFORM, &command) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, true, true) == AF_RESULT_SUCCESS);
    CHECK(history.count == count_before_merge);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK_NEAR(af_scene_find(&project.scene, 2U)->local_transform.position.x, 20.0F, 0.0001F);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK_NEAR(af_scene_find(&project.scene, 2U)->local_transform.position.x, 45.0F, 0.0001F);
    CHECK(af_project_snapshot(&project, 2U, &before) == AF_RESULT_SUCCESS);
    af_scene_find(&project.scene, 2U)->parent_id = AF_INVALID_NODE_ID;
    af_scene_find(&project.scene, 2U)->fill = (AfColor){0.8F, 0.3F, 0.1F, 1.0F};
    af_scene_find(&project.scene, 2U)->opacity = 0.4F;
    af_scene_find(&project.scene, 2U)->corner_radius = 9.0F;
    af_scene_find(&project.scene, 2U)->layer_order = 7;
    af_scene_find(&project.scene, 2U)->local_transform.rotation = 0.3F;
    af_scene_find(&project.scene, 2U)->local_transform.scale = (AfVec2){1.2F, 0.8F};
    CHECK(af_project_set_name(&project, 2U, "Renamed Rectangle") == AF_RESULT_SUCCESS);
    CHECK(af_scene_evaluate_world(&project.scene) == AF_RESULT_SUCCESS);
    CHECK(af_project_snapshot(&project, 2U, &after) == AF_RESULT_SUCCESS);
    CHECK(af_editor_command_modify(&before, &after, AF_EDITOR_MERGE_NONE, &command) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, true, false) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 2U)->parent_id == 1U);
    CHECK(strcmp(af_project_name(&project, 2U), "Rectangle") == 0);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 2U)->parent_id == AF_INVALID_NODE_ID);
    CHECK(strcmp(af_project_name(&project, 2U), "Renamed Rectangle") == 0);
    CHECK_NEAR(af_scene_find(&project.scene, 2U)->opacity, 0.4F, 0.0001F);
    duplicate = after;
    duplicate.node.id = 4U;
    duplicate.node.parent_id = 1U;
    duplicate.index = project.scene.count;
    (void)snprintf(duplicate.name, sizeof(duplicate.name), "Rectangle Copy");
    CHECK(af_editor_command_duplicate(&duplicate, 1U, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 4U) != NULL);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 4U) == NULL);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 4U) != NULL);
    CHECK(af_scene_reparent(&project.scene, 2U, 1U) == AF_RESULT_SUCCESS);
    CHECK(af_editor_command_delete(&project, 1U, &command) == AF_RESULT_SUCCESS);
    CHECK(command.before_count == 4U);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(project.scene.count == 0U);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.scene.count == 4U && af_scene_find(&project.scene, 2U) != NULL);
    command = (AfEditorCommand){.type = AF_EDITOR_COMMAND_MODIFY,
                                .merge_key = AF_EDITOR_MERGE_TRANSFORM,
                                .target_id = 2U,
                                .before_count = 2U,
                                .after_count = 2U};
    CHECK(af_project_snapshot(&project, 2U, &command.before[0]) == AF_RESULT_SUCCESS);
    CHECK(af_project_snapshot(&project, 3U, &command.before[1]) == AF_RESULT_SUCCESS);
    af_scene_find(&project.scene, 2U)->local_transform.position.x += 12.0F;
    af_scene_find(&project.scene, 3U)->local_transform.position.y += 18.0F;
    CHECK(af_project_snapshot(&project, 2U, &command.after[0]) == AF_RESULT_SUCCESS);
    CHECK(af_project_snapshot(&project, 3U, &command.after[1]) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, true, false) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK_NEAR(af_scene_find(&project.scene, 2U)->local_transform.position.x,
               command.before[0].node.local_transform.position.x, 0.0001F);
    CHECK_NEAR(af_scene_find(&project.scene, 3U)->local_transform.position.y,
               command.before[1].node.local_transform.position.y, 0.0001F);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK_NEAR(af_scene_find(&project.scene, 2U)->local_transform.position.x,
               command.after[0].node.local_transform.position.x, 0.0001F);
    CHECK_NEAR(af_scene_find(&project.scene, 3U)->local_transform.position.y,
               command.after[1].node.local_transform.position.y, 0.0001F);
    snapshot = (AfProjectNodeSnapshot){.node = {.id = 5U,
                                                .parent_id = 1U,
                                                .type = AF_NODE_PATH,
                                                .local_transform = {.scale = {1.0F, 1.0F}},
                                                .bounds = {100.0F, 100.0F},
                                                .fill = {0.7F, 0.3F, 0.9F, 1.0F},
                                                .opacity = 1.0F,
                                                .visible = true},
                                       .index = project.scene.count};
    (void)snprintf(snapshot.name, sizeof(snapshot.name), "Undo Path");
    CHECK(af_editor_command_create(&snapshot, &command) == AF_RESULT_SUCCESS);
    command.path_after_count = 1U;
    command.path_after[0] =
        (AfProjectPathSnapshot){.path = {.id = 50U,
                                         .node_id = 5U,
                                         .command_start = project.path_data.command_count,
                                         .command_count = 4U,
                                         .fill_rule = AF_PATH_FILL_NON_ZERO,
                                         .geometry_generation = 1U},
                                .index = project.path_data.path_count};
    command.path_command_after_count = 4U;
    command.path_command_after[0] = (AfPathCommand){.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}};
    command.path_command_after[1] = (AfPathCommand){.type = AF_PATH_LINE, .point = {100.0F, 0.0F}};
    command.path_command_after[2] = (AfPathCommand){.type = AF_PATH_LINE, .point = {50.0F, 100.0F}};
    command.path_command_after[3] = (AfPathCommand){.type = AF_PATH_CLOSE};
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(af_project_find_path(&project, 50U) != NULL &&
          af_project_validate(&project) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 5U) == NULL && af_project_find_path(&project, 50U) == NULL);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 5U) != NULL && af_project_find_path(&project, 50U) != NULL);
    CHECK(af_editor_command_path(AF_EDITOR_MERGE_PATH_POINT, 50U, &command) == AF_RESULT_SUCCESS);
    CHECK(af_project_path_snapshot(&project, 50U, &command.path_before[0],
                                   command.path_command_before,
                                   AF_EDITOR_COMMAND_MAX_PATH_COMMANDS) == AF_RESULT_SUCCESS);
    command.path_before_count = 1U;
    command.path_command_before_count = 4U;
    command.path_after_count = 1U;
    command.path_after[0] = command.path_before[0];
    ++command.path_after[0].path.geometry_generation;
    command.path_command_after_count = 4U;
    memcpy(command.path_command_after, command.path_command_before,
           4U * sizeof(*command.path_command_after));
    command.path_command_after[1].point.x = 125.0F;
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR(project.path_data.commands[1].point.x, 125.0F, 0.0001F);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK_NEAR(project.path_data.commands[1].point.x, 100.0F, 0.0001F);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK_NEAR(project.path_data.commands[1].point.x, 125.0F, 0.0001F);
    {
        AfParticleEmitter emitter = af_particle_emitter_default(900U, 5U);
        emitter.type = AF_PARTICLE_EMITTER_VECTOR_PATH;
        emitter.path_id = 50U;
        CHECK(af_project_add_emitter(&project, &emitter) == AF_RESULT_SUCCESS);
        CHECK(af_project_snapshot(&project, 5U, &before) == AF_RESULT_SUCCESS);
        CHECK(before.has_emitter);
        CHECK(af_project_remove_emitter(&project, emitter.id) == AF_RESULT_SUCCESS);
        CHECK(af_project_snapshot(&project, 5U, &after) == AF_RESULT_SUCCESS);
        CHECK(!after.has_emitter);
        CHECK(af_editor_command_modify(&before, &after, AF_EDITOR_MERGE_PARTICLE, &command) ==
              AF_RESULT_SUCCESS);
        CHECK(af_editor_history_execute(&history, &project, &command, true, false) ==
              AF_RESULT_SUCCESS);
        CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(af_project_find_node_emitter(&project, 5U) != NULL);
        CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(af_project_find_node_emitter(&project, 5U) == NULL);
        CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(af_project_find_node_emitter(&project, 5U) != NULL);
    }
    CHECK(af_scene_reparent(&project.scene, 2U, AF_INVALID_NODE_ID) == AF_RESULT_SUCCESS);
    {
        const uint64_t selected_roots[2] = {1U, 2U};
        CHECK(af_editor_command_delete_many(&project, selected_roots, 2U, &command) ==
              AF_RESULT_SUCCESS);
    }
    CHECK(command.path_before_count == 1U && command.path_command_before_count == 4U);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(project.scene.count == 0U && project.path_data.path_count == 0U);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.scene.count == 5U && project.path_data.path_count == 1U &&
          af_project_find_node_emitter(&project, 5U) != NULL &&
          af_project_validate(&project) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_memory_used(&history) <= history_limit);
    CHECK(history.capacity == 3U);
    af_editor_history_clear(&history);
    CHECK(!af_editor_history_can_undo(&history) && !af_editor_history_can_redo(&history));
    af_editor_history_destroy(&history);
    af_project_destroy(&project);
    return true;
}
