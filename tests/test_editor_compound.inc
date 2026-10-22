static AfSceneNode af_test_compound_node(uint64_t id, AfNodeType type, AfVec2 position) {
    return (AfSceneNode){.id = id,
                         .parent_id = AF_INVALID_NODE_ID,
                         .type = type,
                         .local_transform = {.position = position, .scale = {1, 1}},
                         .bounds = {100, 100},
                         .fill = {0.2F, 0.5F, 0.8F, 1},
                         .opacity = 1,
                         .visible = true};
}

static bool af_test_compound_world_commands(const AfPathCommand *before, AfAffine2 before_world,
                                            const AfPathCommand *after, AfAffine2 after_world,
                                            uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        CHECK(before[i].type == after[i].type);
        if (before[i].type == AF_PATH_CLOSE)
            continue;
        const AfVec2 expected[] = {before[i].point, before[i].control1, before[i].control2};
        const AfVec2 actual[] = {after[i].point, after[i].control1, after[i].control2};
        for (uint32_t j = 0; j < (before[i].type == AF_PATH_CUBIC ? 3U : 1U); ++j) {
            const AfVec2 a = af_affine_transform_point(before_world, expected[j]);
            const AfVec2 b = af_affine_transform_point(after_world, actual[j]);
            CHECK_NEAR(a.x, b.x, 0.002F);
            CHECK_NEAR(a.y, b.y, 0.002F);
        }
    }
    return true;
}

static bool af_test_editor_compound_geometry(void) {
    /* Parent padding, pivots, reflections, skew and canvas scaling all preserve curve controls. */
    for (uint32_t parented = 0; parented < 2U; ++parented) {
        AfAllocator allocator;
        af_allocator_init_default(&allocator);
        AfProject project = {0};
        AfEditorHistory history = {0};
        AfEditorCommand command = {0};
        CHECK(af_project_create(&project, &allocator, 12U) == AF_RESULT_SUCCESS);
        CHECK(af_editor_history_create(&history, &allocator, sizeof(command) * 4U) ==
              AF_RESULT_SUCCESS);
        project.layout_data.target_size = (AfVec2){2100, 1100};
        project.layout_data.safe_area = (AfInsets){12, 18, 4, 5};
        AfSceneNode node = af_test_compound_node(1U, AF_NODE_GROUP, (AfVec2){80, 30});
        node.local_transform.rotation = 0.25F;
        CHECK(af_project_add_node(&project, &node, "Parent") == AF_RESULT_SUCCESS);
        AfLayoutNode layout = af_layout_node_default(1U);
        layout.padding = (AfInsets){10, 15, 3, 5};
        CHECK(af_project_set_layout(&project, &layout) == AF_RESULT_SUCCESS);
        AfPathCommand curves[2][16] = {0};
        uint32_t counts[2] = {0};
        for (uint32_t i = 0; i < 2U; ++i) {
            node = af_test_compound_node(i + 2U, i == 0U ? AF_NODE_ELLIPSE : AF_NODE_PATH,
                                         (AfVec2){(float)i * 190 - 50, (float)i * 80});
            node.parent_id = parented ? 1U : AF_INVALID_NODE_ID;
            node.local_transform.rotation = i == 0U ? 0.3F : -0.2F;
            node.local_transform.scale = i == 0U ? (AfVec2){-1.2F, 0.7F} : (AfVec2){0.8F, 1.4F};
            node.local_transform.skew.x = 0.15F;
            CHECK(af_project_add_node(&project, &node, "Shape") == AF_RESULT_SUCCESS);
            layout = af_layout_node_default(node.id);
            layout.pivot = (AfVec2){0.5F, 0.5F};
            layout.anchor_min = layout.anchor_max = (AfVec2){0.2F, 0.3F};
            layout.offset = (AfVec2){25, 10};
            CHECK(af_project_set_layout(&project, &layout) == AF_RESULT_SUCCESS);
            counts[i] =
                af_editor_vector_primitive_path(AF_NODE_ELLIPSE, node.bounds, 0, curves[i], 16U);
            if (i == 1U) {
                AfPath path = {.id = 100U,
                               .node_id = node.id,
                               .command_count = counts[i],
                               .fill_rule = AF_PATH_FILL_NON_ZERO,
                               .geometry_generation = 7U};
                CHECK(af_project_add_path(&project, &path, curves[i]) == AF_RESULT_SUCCESS);
            }
        }
        CHECK(af_layout_evaluate_scene(&project.scene) == AF_RESULT_SUCCESS);
        const AfAffine2 worlds[2] = {project.scene.nodes[1].world_transform,
                                     project.scene.nodes[2].world_transform};
        const uint64_t ids[] = {3U, 2U};
        const uint64_t next = project.next_id;
        CHECK(af_editor_command_combine(&project, ids, 2U, 3U, &command) == AF_RESULT_SUCCESS);
        CHECK(project.scene.count == 3U && project.next_id == next);
        CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
              AF_RESULT_SUCCESS);
        CHECK(project.scene.count == 2U);
        const AfPath *combined = af_path_find_node_const(&project.path_data, 3U);
        CHECK(combined != NULL && combined->id == 100U && combined->geometry_generation == 8U);
        CHECK(combined->command_count == counts[0] + counts[1]);
        const AfAffine2 combined_world = af_scene_find_const(&project.scene, 3U)->world_transform;
        CHECK(af_test_compound_world_commands(curves[1], worlds[1],
                                              project.path_data.commands + combined->command_start,
                                              combined_world, counts[1]));
        CHECK(af_test_compound_world_commands(
            curves[0], worlds[0], project.path_data.commands + combined->command_start + counts[1],
            combined_world, counts[0]));
        CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(project.scene.count == 3U && project.scene.nodes[1].id == 2U &&
              project.scene.nodes[2].id == 3U);
        CHECK(project.scene.nodes[1].type == AF_NODE_ELLIPSE);
        CHECK(project.path_data.paths[0].id == 100U &&
              project.path_data.paths[0].geometry_generation == 7U);
        CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(af_editor_command_break_apart(&project, 3U, &command) == AF_RESULT_SUCCESS);
        const uint64_t second = command.after[1].node.id;
        CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
              AF_RESULT_SUCCESS);
        const uint64_t split_ids[] = {second, 3U};
        for (uint32_t i = 0; i < 2U; ++i) {
            const AfPath *part = af_path_find_node_const(&project.path_data, split_ids[i]);
            CHECK(part != NULL && part->command_count == counts[i]);
            CHECK(af_test_compound_world_commands(
                curves[i], worlds[i], project.path_data.commands + part->command_start,
                af_scene_find_const(&project.scene, split_ids[i])->world_transform, counts[i]));
        }
        CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(project.scene.count == 2U);
        CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(af_project_validate(&project) == AF_RESULT_SUCCESS);
        af_editor_history_destroy(&history);
        af_project_destroy(&project);
    }
    return true;
}

static bool af_test_editor_compound_persistence(void) {
    AfAllocator allocator;
    af_allocator_init_default(&allocator);
    AfProject project = {0}, reopened = {0};
    AfEditorHistory history = {0};
    AfEditorCommand command = {0};
    AfPackageBlob text = {0}, package = {0};
    AfLoadedPackage runtime = {0};
    CHECK(af_project_create(&project, &allocator, 8U) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_create(&history, &allocator, sizeof(command) * 4U) ==
          AF_RESULT_SUCCESS);
    AfSceneNode node = af_test_compound_node(1U, AF_NODE_RECTANGLE, (AfVec2){0, 0});
    CHECK(af_project_add_node(&project, &node, "Outer") == AF_RESULT_SUCCESS);
    node.id = 2U;
    node.bounds = (AfVec2){50, 50};
    node.local_transform.position = (AfVec2){25, 25};
    node.fill = (AfColor){0.8F, 0.1F, 0.2F, 1};
    CHECK(af_project_add_node(&project, &node, "Inner") == AF_RESULT_SUCCESS);
    AfStyle *style = af_project_find_node_style(&project, 2U);
    style->stroke.enabled = true;
    style->stroke.width = style->stroke.end_width = 3;
    const uint64_t ids[] = {1U, 2U};
    CHECK(af_editor_command_combine(&project, ids, 2U, 2U, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(project.scene.count == 1U && project.scene.nodes[0].id == 2U);
    CHECK(af_project_find_node_style(&project, 2U)->stroke.enabled);
    CHECK_NEAR(project.scene.nodes[0].fill.r, 0.8F, 0.0001F);
    AfPathTessellationScratch scratch;
    const AfPath *path = af_path_find_node_const(&project.path_data, 2U);
    CHECK(path != NULL && path->fill_rule == AF_PATH_FILL_EVEN_ODD);
    CHECK(af_path_hit_test(&project.path_data, path, (AfVec2){10, 10}, 0.1F, &scratch));
    CHECK(!af_path_hit_test(&project.path_data, path, (AfVec2){50, 50}, 0.1F, &scratch));
    CHECK(af_project_serialize(&project, &allocator, &text) == AF_RESULT_SUCCESS);
    CHECK(af_project_deserialize(text.data, text.size, &allocator, &reopened) == AF_RESULT_SUCCESS);
    CHECK(reopened.path_data.paths[0].fill_rule == AF_PATH_FILL_EVEN_ODD);
    CHECK(reopened.path_data.command_count == 10U);
    CHECK(af_project_compile(&reopened, &allocator, &package) == AF_RESULT_SUCCESS);
    CHECK(af_package_decode(package.data, package.size, &allocator, &runtime) == AF_RESULT_SUCCESS);
    CHECK(runtime.path_data.command_count == 10U);
    CHECK(runtime.path_data.paths[0].fill_rule == AF_PATH_FILL_EVEN_ODD);
    char svg[8192];
    size_t svg_size = 0U;
    AfInterchangeDiagnostics diagnostics = {0};
    CHECK(af_interchange_export_static_svg(&project.scene, project.canvas_size, svg, sizeof(svg),
                                           &svg_size, &diagnostics) == AF_RESULT_SUCCESS);
    CHECK(diagnostics.warning_count == 0U && strstr(svg, "fill-rule=\"evenodd\"") != NULL);
    CHECK(af_editor_command_break_apart(&project, 2U, &command) == AF_RESULT_SUCCESS);
    const uint64_t next = project.next_id;
    const uint32_t cursor = history.cursor;
    const uint32_t capacity = project.path_data.path_capacity;
    project.path_data.path_capacity = 1U;
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_LIMIT_EXCEEDED);
    CHECK(project.next_id == next && history.cursor == cursor && project.scene.count == 1U);
    CHECK(project.path_data.command_count == 10U);
    project.path_data.path_capacity = capacity;
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(project.scene.count == 2U && project.path_data.path_count == 2U);
    af_loaded_package_destroy(&allocator, &runtime);
    af_package_blob_destroy(&allocator, &package);
    af_package_blob_destroy(&allocator, &text);
    af_project_destroy(&reopened);
    af_editor_history_destroy(&history);
    af_project_destroy(&project);
    return true;
}

static bool af_test_editor_compound_rejections(void) {
    AfAllocator allocator;
    af_allocator_init_default(&allocator);
    AfProject project = {0};
    AfEditorCommand command = {0};
    CHECK(af_project_create(&project, &allocator, 8U) == AF_RESULT_SUCCESS);
    for (uint64_t i = 1U; i <= 2U; ++i) {
        AfSceneNode node = af_test_compound_node(i, AF_NODE_RECTANGLE, (AfVec2){0, 0});
        CHECK(af_project_add_node(&project, &node, "Shape") == AF_RESULT_SUCCESS);
    }
    const uint64_t ids[] = {1U, 2U}, duplicate[] = {1U, 1U};
    CHECK(af_editor_command_combine(&project, duplicate, 2U, 1U, &command) ==
          AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_editor_command_break_apart(&project, 1U, &command) == AF_RESULT_INVALID_ARGUMENT);
    AfProjectClip clip = {.id = 1U, .name = "Clip", .duration = 1};
    CHECK(af_project_add_clip(&project, &clip) == AF_RESULT_SUCCESS);
    AfProjectTrack track = {
        .id = 2U, .clip_id = 1U, .node_id = 1U, .property = AF_PROPERTY_POSITION};
    CHECK(af_project_add_track(&project, &track) == AF_RESULT_SUCCESS);
    const uint64_t next = project.next_id;
    CHECK(af_editor_command_combine(&project, ids, 2U, 2U, &command) ==
          AF_RESULT_UNSUPPORTED_FEATURE);
    CHECK(project.scene.count == 2U && project.next_id == next && project.track_count == 1U);
    CHECK(af_project_remove_track_exact(&project, 2U) == AF_RESULT_SUCCESS);
    project.scene.nodes[0].local_transform.scale.x = 0;
    CHECK(af_editor_command_combine(&project, ids, 2U, 1U, &command) == AF_RESULT_INVALID_ARGUMENT);
    project.scene.nodes[0].local_transform.scale.x = 1;
    project.scene.nodes[0].parent_id = 2U;
    CHECK(af_editor_command_combine(&project, ids, 2U, 1U, &command) ==
          AF_RESULT_UNSUPPORTED_FEATURE);
    project.scene.nodes[0].parent_id = AF_INVALID_NODE_ID;
    /* Reject bindings and dependent styles without silently redirecting their targets. */
    project.binding_data.targets[0] =
        (AfBindingTarget){.type = AF_BINDING_TARGET_NODE, .object_id = 1U};
    project.binding_data.target_count = 1U;
    CHECK(af_editor_command_combine(&project, ids, 2U, 2U, &command) ==
          AF_RESULT_UNSUPPORTED_FEATURE);
    project.binding_data.target_count = 0U;
    AfStyle *dependent = af_project_find_node_style(&project, 2U);
    dependent->parent_style_id = af_project_find_node_style(&project, 1U)->id;
    dependent->inherit_flags = AF_STYLE_INHERIT_FILL;
    CHECK(af_editor_command_combine(&project, ids, 2U, 2U, &command) ==
          AF_RESULT_UNSUPPORTED_FEATURE);
    dependent->parent_style_id = 0U;
    dependent->inherit_flags = 0U;
    /* More contours than an undo command can represent must not partially split a path. */
    AfPathCommand points[AF_EDITOR_COMMAND_MAX_PATH_COMMANDS] = {0};
    const uint32_t contours = AF_EDITOR_COMMAND_MAX_PATHS + 1U;
    for (uint32_t i = 0; i < contours; ++i) {
        points[i * 2U] = (AfPathCommand){.type = AF_PATH_MOVE, .point = {(float)i * 10, 0}};
        points[i * 2U + 1U] =
            (AfPathCommand){.type = AF_PATH_LINE, .point = {(float)i * 10 + 5, 5}};
    }
    project.scene.nodes[0].type = AF_NODE_PATH;
    AfPath path = {.id = 51U, .node_id = 1U, .command_count = contours * 2U, .geometry_generation = 1U};
    CHECK(af_project_add_path(&project, &path, points) == AF_RESULT_SUCCESS);
    CHECK(af_editor_command_break_apart(&project, 1U, &command) == AF_RESULT_LIMIT_EXCEEDED);
    CHECK(project.path_data.command_count == contours * 2U && project.scene.count == 2U);
    CHECK(af_project_remove_path_exact(&project, 51U) == AF_RESULT_SUCCESS);
    points[0] = (AfPathCommand){.type = AF_PATH_MOVE, .point = {0, 0}};
    for (uint32_t i = 1U; i < AF_EDITOR_COMMAND_MAX_PATH_COMMANDS; ++i)
        points[i] = (AfPathCommand){.type = AF_PATH_LINE, .point = {(float)i, (float)(i % 2U)}};
    path.command_count = AF_EDITOR_COMMAND_MAX_PATH_COMMANDS;
    CHECK(af_project_add_path(&project, &path, points) == AF_RESULT_SUCCESS);
    CHECK(af_editor_command_combine(&project, ids, 2U, 2U, &command) == AF_RESULT_LIMIT_EXCEEDED);
    CHECK(project.path_data.command_count == AF_EDITOR_COMMAND_MAX_PATH_COMMANDS && project.next_id == next);
    CHECK(af_project_validate(&project) == AF_RESULT_SUCCESS);
    af_project_destroy(&project);
    return true;
}
