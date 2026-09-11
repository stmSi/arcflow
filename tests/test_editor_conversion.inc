static bool af_test_editor_conversion(void) {
    AfAllocator allocator;
    af_allocator_init_default(&allocator);
    const AfNodeType types[] = {AF_NODE_RECTANGLE, AF_NODE_ROUNDED_RECTANGLE, AF_NODE_ELLIPSE};
    const uint32_t counts[] = {5U, 10U, 6U};
    for (uint32_t kind = 0U; kind < 3U; ++kind) {
        AfProject project = {0}, reopened = {0};
        AfEditorHistory history = {0};
        AfEditorCommand command = {0};
        AfPackageBlob text = {0};
        AfSceneNode shape = {.id = 1U,
                             .type = types[kind],
                             .parent_id = AF_INVALID_NODE_ID,
                             .local_transform = {.position = {23.0F, -11.0F},
                                                 .rotation = 0.6F,
                                                 .scale = {1.7F, 0.8F},
                                                 .skew = {0.12F, 0.0F}},
                             .bounds = {100.0F, 60.0F},
                             .corner_radius = 12.0F,
                             .fill = {0.1F, 0.3F, 0.8F, 1.0F},
                             .opacity = 0.8F,
                             .visible = true};
        CHECK(af_project_create(&project, &allocator, 8U) == AF_RESULT_SUCCESS);
        CHECK(af_project_add_node(&project, &shape, "Convertible") == AF_RESULT_SUCCESS);
        CHECK(af_scene_evaluate_world(&project.scene) == AF_RESULT_SUCCESS);
        const AfAffine2 world = project.scene.nodes[0].world_transform;
        const AfStyle style = *af_project_find_node_style(&project, 1U);
        CHECK(af_editor_history_create(&history, &allocator, sizeof(command) * 4U) ==
              AF_RESULT_SUCCESS);
        CHECK(af_editor_command_convert_path(&project, 1U, &command) == AF_RESULT_SUCCESS);
        CHECK(project.scene.nodes[0].type == types[kind] && project.path_data.path_count == 0U);
        CHECK(command.path_command_after_count == counts[kind]);
        CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
              AF_RESULT_SUCCESS);
        CHECK(project.scene.nodes[0].type == AF_NODE_PATH && history.count == 1U);
        CHECK(af_project_validate(&project) == AF_RESULT_SUCCESS);
        CHECK(memcmp(&world, &project.scene.nodes[0].world_transform, sizeof(world)) == 0);
        CHECK(memcmp(&style, af_project_find_node_style(&project, 1U), sizeof(style)) == 0);
        CHECK(strcmp(af_project_name(&project, 1U), "Convertible") == 0);
        CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(project.scene.nodes[0].type == types[kind]);
        CHECK(project.scene.nodes[0].corner_radius == 12.0F && project.path_data.path_count == 0U);
        CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(af_project_serialize(&project, &allocator, &text) == AF_RESULT_SUCCESS);
        CHECK(af_project_deserialize(text.data, text.size, &allocator, &reopened) ==
              AF_RESULT_SUCCESS);
        CHECK(reopened.scene.nodes[0].type == AF_NODE_PATH);
        CHECK(reopened.path_data.command_count == counts[kind]);
        CHECK(memcmp(reopened.path_data.commands, command.path_command_after,
                     counts[kind] * sizeof(AfPathCommand)) == 0);
        af_package_blob_destroy(&allocator, &text);
        af_project_destroy(&reopened);
        CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
        const uint32_t capacity = project.path_data.command_capacity;
        project.path_data.command_capacity = counts[kind] - 1U;
        CHECK(af_editor_history_redo(&history, &project) != AF_RESULT_SUCCESS);
        CHECK(project.scene.nodes[0].type == types[kind] && project.path_data.path_count == 0U);
        CHECK(history.cursor == 0U);
        project.path_data.command_capacity = capacity;
        CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
        af_editor_history_destroy(&history);
        af_project_destroy(&project);
    }
    AfPathCommand commands[10], unchanged[10];
    memset(commands, 0x5a, sizeof(commands));
    memcpy(unchanged, commands, sizeof(commands));
    CHECK(af_editor_vector_primitive_path(AF_NODE_ELLIPSE, (AfVec2){100.0F, 60.0F}, 0.0F, commands,
                                          5U) == 0U);
    CHECK(memcmp(commands, unchanged, sizeof(commands)) == 0);
    CHECK(af_editor_vector_primitive_path(AF_NODE_GROUP, (AfVec2){100.0F, 60.0F}, 0.0F, commands,
                                          10U) == 0U);
    CHECK(af_editor_vector_primitive_path(AF_NODE_ROUNDED_RECTANGLE, (AfVec2){100.0F, 60.0F},
                                          500.0F, commands, 10U) == 10U);
    CHECK_NEAR(commands[0].point.x, 30.0F, 0.0001F);
    CHECK(af_editor_vector_primitive_path(AF_NODE_ELLIPSE, (AfVec2){100.0F, 60.0F}, 0.0F, commands,
                                          10U) == 6U);
    const AfVec2 middle = af_test_knife_curve_at(commands[0].point, commands[1], 0.5F);
    CHECK_NEAR((middle.x - 50.0F) * (middle.x - 50.0F) / 2500.0F +
                   (middle.y - 30.0F) * (middle.y - 30.0F) / 900.0F,
               1.0F, 0.0001F);
    return true;
}

static bool af_test_editor_shape_settings_and_hierarchy(void) {
    AfAllocator allocator;
    AfProject project = {0}, reopened = {0};
    AfEditorHistory history = {0};
    AfEditorCommand command = {0};
    AfPathCommand points[130];
    AfPackageBlob text = {0}, package = {0};
    AfLoadedPackage runtime = {0};
    af_allocator_init_default(&allocator);
    CHECK(af_project_create(&project, &allocator, 16U) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_create(&history, &allocator, sizeof(command) * 8U) ==
          AF_RESULT_SUCCESS);
    AfSceneNode node = {.id = 1U,
                        .parent_id = AF_INVALID_NODE_ID,
                        .type = AF_NODE_PATH,
                        .bounds = {100, 80},
                        .local_transform = {.position = {30, 20}, .scale = {1, 1}},
                        .fill = {0.6F, 0.2F, 0.8F, 1},
                        .opacity = 1,
                        .visible = true,
                        .polygon_sides = 5U,
                        .polygon_inset = 0.45F};
    CHECK(af_project_add_node(&project, &node, "Star") == AF_RESULT_SUCCESS);
    uint32_t count = af_editor_vector_tool_polygon(node.bounds, 5U, 0.45F, points, 130U);
    AfPath path = {.id = 1U, .node_id = 1U, .command_count = count, .geometry_generation = 1U};
    CHECK(af_project_add_path(&project, &path, points) == AF_RESULT_SUCCESS);
    CHECK(af_editor_command_polygon(&project, 1U, (AfVec2){120, 90}, 7U, 0.6F, &command) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(project.scene.nodes[0].polygon_sides == 7U && project.path_data.command_count == 15U);
    CHECK(project.scene.nodes[0].bounds.x == 120.0F);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.scene.nodes[0].polygon_sides == 5U && project.path_data.command_count == count);
    CHECK(memcmp(project.path_data.commands, points, count * sizeof(*points)) == 0);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_project_serialize(&project, &allocator, &text) == AF_RESULT_SUCCESS);
    CHECK(af_project_deserialize(text.data, text.size, &allocator, &reopened) == AF_RESULT_SUCCESS);
    CHECK(reopened.scene.nodes[0].polygon_sides == 7U);
    CHECK_NEAR(reopened.scene.nodes[0].polygon_inset, 0.6F, 0.0001F);
    CHECK(af_project_compile(&reopened, &allocator, &package) == AF_RESULT_SUCCESS);
    CHECK(af_package_decode(package.data, package.size, &allocator, &runtime) == AF_RESULT_SUCCESS);
    CHECK(runtime.path_data.command_count == 15U);
    CHECK(af_editor_command_convert_path(&project, 1U, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(project.scene.nodes[0].polygon_sides == 0U && project.path_data.command_count == 15U);
    CHECK(af_editor_command_polygon(&project, 1U, (AfVec2){120, 90}, 5U, 0.4F, &command) !=
          AF_RESULT_SUCCESS);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.scene.nodes[0].polygon_sides == 7U);
    node.id = 2U;
    node.type = AF_NODE_RECTANGLE;
    node.polygon_sides = 0U;
    node.polygon_inset = 0.0F;
    node.local_transform.position.x = 160.0F;
    CHECK(af_project_add_node(&project, &node, "Rectangle") == AF_RESULT_SUCCESS);
    node.id = 3U;
    CHECK(af_project_add_node(&project, &node, "Third") == AF_RESULT_SUCCESS);
    CHECK(af_scene_evaluate_world(&project.scene) == AF_RESULT_SUCCESS);
    const AfAffine2 before = af_scene_find_const(&project.scene, 1U)->world_transform;
    uint64_t group_id = 0U;
    CHECK(af_project_allocate_id(&project, &group_id) == AF_RESULT_SUCCESS);
    const uint64_t ids[] = {1U, 2U};
    CHECK(af_editor_command_group(&project, ids, 2U, group_id, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(project.scene.count == 4U);
    CHECK(af_scene_find_const(&project.scene, 1U)->parent_id == group_id);
    CHECK(memcmp(&before, &af_scene_find_const(&project.scene, 1U)->world_transform,
                 sizeof(before)) == 0);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.scene.count == 3U &&
          af_scene_find_const(&project.scene, 1U)->parent_id == AF_INVALID_NODE_ID);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_editor_command_reorder(&project, group_id, 1U, true, &command) != AF_RESULT_SUCCESS);
    for (uint32_t placement = 0U; placement < 2U; ++placement) {
        CHECK(af_editor_can_reorder(&project, 3U, 1U));
        CHECK(af_editor_command_reorder(&project, 3U, 1U, placement != 0U, &command) == AF_RESULT_SUCCESS);
        CHECK(af_scene_find_const(&project.scene, 3U)->parent_id == AF_INVALID_NODE_ID);
        CHECK(af_editor_history_execute(&history, &project, &command, false, false) == AF_RESULT_SUCCESS);
        CHECK(af_scene_find_const(&project.scene, 3U)->parent_id == group_id);
        CHECK((af_project_find_index(&project, 3U) > af_project_find_index(&project, 1U)) == (placement != 0U));
        CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(af_scene_find_const(&project.scene, 3U)->parent_id == AF_INVALID_NODE_ID);
        CHECK(af_project_find_index(&project, 3U) == 2U);
        CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(af_scene_find_const(&project.scene, 3U)->parent_id == group_id);
        CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    }
    CHECK(af_editor_command_reorder(&project, 1U, 3U, true, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find_const(&project.scene, 1U)->parent_id == AF_INVALID_NODE_ID);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find_const(&project.scene, 1U)->parent_id == group_id);
    CHECK(af_editor_command_reorder(&project, 1U, 2U, true, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(project.scene.nodes[0].id == 2U && project.scene.nodes[1].id == 1U);
    CHECK(strcmp(af_project_name(&project, 1U), "Star") == 0);
    CHECK(af_project_validate(&project) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.scene.nodes[0].id == 1U && project.scene.nodes[1].id == 2U);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.scene.nodes[0].id == 2U);
    af_loaded_package_destroy(&allocator, &runtime);
    af_package_blob_destroy(&allocator, &package);
    af_package_blob_destroy(&allocator, &text);
    af_project_destroy(&reopened);
    af_editor_history_destroy(&history);
    af_project_destroy(&project);
    return true;
}

static bool af_test_editor_reparent(void) {
    AfAllocator allocator;
    af_allocator_init_default(&allocator);
    AfProject project = {0};
    AfEditorHistory history = {0};
    AfEditorCommand command = {0};
    CHECK(af_project_create(&project, &allocator, 8U) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_create(&history, &allocator, sizeof(command) * 4U) ==
          AF_RESULT_SUCCESS);
    AfSceneNode node = {
        .id = 1U,
        .parent_id = AF_INVALID_NODE_ID,
        .type = AF_NODE_GROUP,
        .bounds = {400, 300},
        .local_transform = {.position = {70, -25}, .rotation = 0.3F, .scale = {1.2F, 0.8F}},
        .fill = {1, 1, 1, 1},
        .opacity = 1,
        .visible = true};
    CHECK(af_project_add_node(&project, &node, "Scene") == AF_RESULT_SUCCESS);
    node.id = 2U;
    node.local_transform =
        (AfTransform2D){.position = {-80, 90}, .rotation = -0.2F, .scale = {-0.9F, 1.3F}};
    CHECK(af_project_add_node(&project, &node, "Destination") == AF_RESULT_SUCCESS);
    AfLayoutNode parent_layout = af_layout_node_default(2U);
    parent_layout.padding = (AfInsets){10, 12, 14, 16};
    CHECK(af_project_set_layout(&project, &parent_layout) == AF_RESULT_SUCCESS);
    node.id = 3U;
    node.type = AF_NODE_RECTANGLE;
    node.parent_id = 1U;
    node.bounds = (AfVec2){100, 60};
    node.local_transform = (AfTransform2D){
        .position = {90, 50}, .rotation = 0.15F, .scale = {1.1F, 0.9F}, .skew = {0.1F, 0}};
    CHECK(af_project_add_node(&project, &node, "Child") == AF_RESULT_SUCCESS);
    AfLayoutNode layout = af_layout_node_default(3U);
    layout.pivot = (AfVec2){0.5F, 0.5F};
    layout.offset = (AfVec2){15, -8};
    layout.anchor_min = layout.anchor_max = (AfVec2){0.5F, 0.5F};
    CHECK(af_project_set_layout(&project, &layout) == AF_RESULT_SUCCESS);
    CHECK(af_layout_evaluate_scene(&project.scene) == AF_RESULT_SUCCESS);
    const AfAffine2 world = af_scene_find(&project.scene, 3U)->world_transform;
    const AfVec2 bounds = af_scene_find(&project.scene, 3U)->resolved_bounds;
    CHECK(!af_editor_can_reparent(&project, 3U, 3U));
    CHECK(!af_editor_can_reparent(&project, 1U, 3U));
    CHECK(af_editor_command_reparent(&project, 3U, 2U, &command) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 3U)->parent_id == 1U);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    for (uint32_t pass = 0U; pass < 3U; ++pass) {
        CHECK(af_layout_evaluate_scene(&project.scene) == AF_RESULT_SUCCESS);
        const AfSceneNode *child = af_scene_find(&project.scene, 3U);
        CHECK(child->parent_id == (pass == 1U ? 1U : 2U));
        CHECK_NEAR(child->resolved_bounds.x, bounds.x, 0.001F);
        CHECK_NEAR(child->resolved_bounds.y, bounds.y, 0.001F);
        CHECK_NEAR(child->world_transform.m00, world.m00, 0.001F);
        CHECK_NEAR(child->world_transform.m01, world.m01, 0.001F);
        CHECK_NEAR(child->world_transform.m10, world.m10, 0.001F);
        CHECK_NEAR(child->world_transform.m11, world.m11, 0.001F);
        CHECK_NEAR(child->world_transform.tx, world.tx, 0.001F);
        CHECK_NEAR(child->world_transform.ty, world.ty, 0.001F);
        if (pass == 0U)
            CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
        if (pass == 1U)
            CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    }
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    parent_layout.direction = AF_LAYOUT_DIRECTION_HORIZONTAL;
    CHECK(af_project_set_layout(&project, &parent_layout) == AF_RESULT_SUCCESS);
    node.id = 4U;
    node.parent_id = 2U;
    CHECK(af_project_add_node(&project, &node, "Peer") == AF_RESULT_SUCCESS);
    CHECK(af_layout_evaluate_scene(&project.scene) == AF_RESULT_SUCCESS);
    const AfAffine2 before_insert = af_scene_find(&project.scene, 3U)->world_transform;
    CHECK(af_editor_command_reorder(&project, 3U, 4U, true, &command) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 3U)->parent_id == 1U);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) == AF_RESULT_SUCCESS);
    CHECK(af_layout_evaluate_scene(&project.scene) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 3U)->parent_id == 2U);
    CHECK(af_project_find_index(&project, 3U) > af_project_find_index(&project, 4U));
    CHECK_NEAR(af_scene_find(&project.scene, 3U)->world_transform.tx, before_insert.tx, 0.001F);
    CHECK_NEAR(af_scene_find(&project.scene, 3U)->world_transform.ty, before_insert.ty, 0.001F);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 3U)->parent_id == 1U);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_editor_command_reparent(&project, 2U, 1U, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(!af_editor_can_reparent(&project, 1U, 2U));
    CHECK(af_editor_command_reparent(&project, 1U, 2U, &command) != AF_RESULT_SUCCESS);
    af_scene_find(&project.scene, 1U)->local_transform.scale.x = 0;
    CHECK(af_editor_command_reparent(&project, 3U, 1U, &command) != AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 3U)->parent_id == 2U);
    CHECK(af_project_validate(&project) == AF_RESULT_SUCCESS);
    af_editor_history_destroy(&history);
    af_project_destroy(&project);
    return true;
}
