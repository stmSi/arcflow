static bool af_test_editor_boolean_commands(void) {
    for (uint32_t op = 0; op < 3U; ++op) {
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
        AfSceneNode node = af_test_compound_node(1U, AF_NODE_GROUP, (AfVec2){80, 40});
        node.local_transform.rotation = 0.25F;
        node.local_transform.skew.x = 0.15F;
        node.local_transform.scale = (AfVec2){-1.2F, 0.8F};
        CHECK(af_project_add_node(&project, &node, "Parent") == AF_RESULT_SUCCESS);
        for (uint32_t i = 0; i < 2U; ++i) {
            node = af_test_compound_node(2U + i, AF_NODE_RECTANGLE,
                                         (AfVec2){(float)i * 50, (float)i * 50});
            node.parent_id = 1U;
            node.fill = i == 0U ? (AfColor){1, 0, 0, 1} : (AfColor){0, 1, 0, 1};
            CHECK(af_project_add_node(&project, &node, "Operand") == AF_RESULT_SUCCESS);
        }
        CHECK(af_layout_evaluate_scene(&project.scene) == AF_RESULT_SUCCESS);
        const AfAffine2 parent = project.scene.nodes[0].world_transform;
        CHECK(af_editor_command_boolean(&project, 2U, 3U, (AfPathBooleanOperation)op, &command) ==
              AF_RESULT_SUCCESS);
        CHECK(project.scene.count == 3U);
        CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
              AF_RESULT_SUCCESS);
        CHECK(project.scene.count == 2U && af_scene_find_const(&project.scene, 3U) == NULL);
        CHECK(af_scene_find_const(&project.scene, 2U)->fill.r == 1);
        const AfPath *path = af_path_find_node_const(&project.path_data, 2U);
        CHECK(path != NULL && path->fill_rule == AF_PATH_FILL_NON_ZERO);
        CHECK(fabs(af_test_boolean_area(project.path_data.commands + path->command_start,
                                        path->command_count) -
                   (op == 0U   ? 17500
                    : op == 1U ? 7500
                               : 2500)) < 0.1);
        AfAffine2 inverse;
        CHECK(
            af_affine_inverse(af_scene_find_const(&project.scene, 2U)->world_transform, &inverse));
        AfPathTessellationScratch scratch;
        const AfVec2 samples[] = {{20, 20}, {75, 75}, {125, 125}};
        for (uint32_t i = 0; i < 3U; ++i) {
            const AfVec2 point =
                af_affine_transform_point(inverse, af_affine_transform_point(parent, samples[i]));
            const bool expected = op == 0U || (op == 1U && i == 0U) || (op == 2U && i == 1U);
            CHECK(af_path_hit_test(&project.path_data, path, point, 0.1F, &scratch) == expected);
        }
        CHECK(af_project_serialize(&project, &allocator, &text) == AF_RESULT_SUCCESS);
        CHECK(af_project_deserialize(text.data, text.size, &allocator, &reopened) ==
              AF_RESULT_SUCCESS);
        CHECK(af_project_compile(&reopened, &allocator, &package) == AF_RESULT_SUCCESS);
        CHECK(af_package_decode(package.data, package.size, &allocator, &runtime) ==
              AF_RESULT_SUCCESS);
        CHECK(runtime.path_data.command_count == project.path_data.command_count);
        char svg[16384];
        size_t size = 0U;
        AfInterchangeDiagnostics diagnostics = {0};
        CHECK(af_interchange_export_static_svg(&project.scene, project.canvas_size, svg,
                                               sizeof(svg), &size,
                                               &diagnostics) == AF_RESULT_SUCCESS);
        CHECK(diagnostics.warning_count == 0U);
        CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(project.scene.count == 3U && project.path_data.path_count == 0U);
        CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(project.scene.count == 2U && project.path_data.path_count == 1U);
        af_loaded_package_destroy(&allocator, &runtime);
        af_package_blob_destroy(&allocator, &text);
        af_package_blob_destroy(&allocator, &package);
        af_project_destroy(&reopened);
        af_editor_history_destroy(&history);
        af_project_destroy(&project);
    }
    return true;
}

static bool af_test_editor_boolean_curves_and_empty(void) {
    AfAllocator allocator;
    af_allocator_init_default(&allocator);
    AfProject project = {0};
    AfEditorHistory history = {0};
    AfEditorCommand command = {0};
    CHECK(af_project_create(&project, &allocator, 8U) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_create(&history, &allocator, sizeof(command) * 4U) ==
          AF_RESULT_SUCCESS);
    AfSceneNode node = af_test_compound_node(1U, AF_NODE_PATH, (AfVec2){0, 0});
    CHECK(af_project_add_node(&project, &node, "Curve") == AF_RESULT_SUCCESS);
    AfPathCommand curves[16];
    uint32_t count = af_editor_vector_primitive_path(AF_NODE_ELLIPSE, node.bounds, 0, curves, 16U);
    AfPath original = {.id = 51U, .node_id = 1U, .command_count = count, .geometry_generation = 7U};
    CHECK(af_project_add_path(&project, &original, curves) == AF_RESULT_SUCCESS);
    node = af_test_compound_node(2U, AF_NODE_RECTANGLE, (AfVec2){50, 0});
    CHECK(af_project_add_node(&project, &node, "Cutter") == AF_RESULT_SUCCESS);
    CHECK(af_editor_command_boolean(&project, 1U, 2U, AF_PATH_BOOLEAN_SUBTRACT, &command) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    for (uint32_t i = 0; i < project.path_data.command_count; ++i)
        CHECK(project.path_data.commands[i].type != AF_PATH_CUBIC);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.path_data.paths[0].id == 51U &&
          project.path_data.paths[0].geometry_generation == 7U);
    CHECK(memcmp(project.path_data.commands, curves, count * sizeof(*curves)) == 0);
    project.scene.nodes[1].local_transform.position.x = 200;
    const uint32_t cursor = history.cursor;
    const uint64_t next_id = project.next_id;
    CHECK(af_editor_command_boolean(&project, 1U, 2U, AF_PATH_BOOLEAN_INTERSECT, &command) ==
          AF_RESULT_NOT_FOUND);
    CHECK(project.scene.count == 2U && history.cursor == cursor && project.next_id == next_id);
    CHECK(af_project_validate(&project) == AF_RESULT_SUCCESS);
    af_editor_history_destroy(&history);
    af_project_destroy(&project);
    return true;
}
