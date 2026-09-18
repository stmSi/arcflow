static bool af_test_vector_expansion(void) {
    AfAllocator allocator;
    af_allocator_init_default(&allocator);
    AfPath paths[2];
    AfPathCommand input[12], output[512];
    af_test_boolean_rectangle(input, 0, 0, 100, 100);
    af_test_boolean_rectangle(input + 5, 20, 20, 60, 60);
    AfPathData data = af_test_boolean_data(paths, input, 5, 5);
    uint32_t count = 0;
    CHECK(af_path_offset(&allocator, &data, paths, 10, AF_STROKE_JOIN_MITER, 4, .1F, output, 512,
                         &count) == AF_RESULT_SUCCESS);
    CHECK_NEAR((float)af_test_boolean_area(output, count), 14400, .1F);
    CHECK(af_path_offset(&allocator, &data, paths, 10, AF_STROKE_JOIN_BEVEL, 4, .1F, output, 512,
                         &count) == AF_RESULT_SUCCESS);
    CHECK_NEAR((float)af_test_boolean_area(output, count), 14200, .1F);
    CHECK(af_path_offset(&allocator, &data, paths, 10, AF_STROKE_JOIN_ROUND, 4, .1F, output, 512,
                         &count) == AF_RESULT_SUCCESS);
    CHECK_NEAR((float)af_test_boolean_area(output, count), 14314.159F, 2.0F);
    CHECK(af_path_offset(&allocator, &data, paths, -10, AF_STROKE_JOIN_MITER, 4, .1F, output, 512,
                         &count) == AF_RESULT_SUCCESS);
    CHECK_NEAR((float)af_test_boolean_area(output, count), 6400, .1F);
    CHECK(af_path_offset(&allocator, &data, paths, -60, AF_STROKE_JOIN_MITER, 4, .1F, output, 512,
                         &count) == AF_RESULT_SUCCESS &&
          count == 0);
    data.path_count = 1;
    paths[0].command_count = 10;
    paths[0].fill_rule = AF_PATH_FILL_EVEN_ODD;
    CHECK(af_path_offset(&allocator, &data, paths, 5, AF_STROKE_JOIN_MITER, 4, .1F, output, 512,
                         &count) == AF_RESULT_SUCCESS);
    CHECK_NEAR((float)af_test_boolean_area(output, count), 12100 - 2500, .1F);
    input[0] = (AfPathCommand){.type = AF_PATH_MOVE, .point = {0, 0}};
    input[1] = (AfPathCommand){.type = AF_PATH_LINE, .point = {100, 0}};
    paths[0].command_count = data.command_count = 2;
    AfStroke stroke = {.enabled = true, .width = 10, .end_width = 10, .miter_limit = 4};
    CHECK(af_path_outline(&allocator, &data, paths, &stroke, .1F, output, 512, &count) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR((float)af_test_boolean_area(output, count), 1000, .1F);
    stroke.cap = AF_STROKE_CAP_ROUND;
    CHECK(af_path_outline(&allocator, &data, paths, &stroke, .1F, output, 512, &count) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR((float)af_test_boolean_area(output, count), 1078.54F, 2.0F);
    stroke.cap = AF_STROKE_CAP_SQUARE;
    CHECK(af_path_outline(&allocator, &data, paths, &stroke, .1F, output, 512, &count) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR((float)af_test_boolean_area(output, count), 1100, .1F);
    stroke.cap = AF_STROKE_CAP_BUTT;
    stroke.dashes[0] = stroke.dashes[1] = 10;
    stroke.dash_count = 2;
    CHECK(af_path_outline(&allocator, &data, paths, &stroke, .1F, output, 512, &count) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR((float)af_test_boolean_area(output, count), 500, .1F);
    AfPathCommand saved = output[0];
    count = 777;
    CHECK(af_path_outline(&allocator, &data, paths, &stroke, .1F, output, 1, &count) ==
          AF_RESULT_LIMIT_EXCEEDED);
    CHECK(count == 777 && memcmp(&saved, output, sizeof(saved)) == 0);
    CHECK(af_path_offset(&allocator, &data, paths, 5, AF_STROKE_JOIN_MITER, 4, .1F, output, 512,
                         &count) == AF_RESULT_UNSUPPORTED_FEATURE);
    /* Every orientation of two cubic contours; untouched contour survives and controls swap. */
    input[0] = (AfPathCommand){.type = AF_PATH_MOVE, .point = {0, 0}};
    input[1] = (AfPathCommand){
        .type = AF_PATH_CUBIC, .point = {20, 0}, .control1 = {5, 10}, .control2 = {15, 10}};
    input[2] = (AfPathCommand){.type = AF_PATH_MOVE, .point = {30, 0}};
    input[3] = (AfPathCommand){
        .type = AF_PATH_CUBIC, .point = {50, 0}, .control1 = {35, -10}, .control2 = {45, -10}};
    for (uint32_t a = 0; a < 2; ++a)
        for (uint32_t b = 2; b < 4; ++b) {
            CHECK(af_editor_vector_join(input, 4, a, b, output, 512, &count) == AF_RESULT_SUCCESS);
            CHECK(count == 4 && output[2].type == AF_PATH_LINE);
            CHECK_NEAR(output[1].control1.x, a == 0 ? 15 : 5, .001F);
            CHECK_NEAR(output[3].control1.x, b == 2 ? 35 : 45, .001F);
        }
    CHECK(af_editor_vector_join(input, 4, 0, 1, output, 512, &count) == AF_RESULT_SUCCESS);
    CHECK(count == 5 && output[2].type == AF_PATH_CLOSE && output[3].type == AF_PATH_MOVE);
    CHECK(af_editor_vector_join(output, count, 0, 1, input, 12, &count) ==
          AF_RESULT_INVALID_ARGUMENT);
    return true;
}

static bool af_test_expansion_commands(void) {
    AfAllocator allocator;
    af_allocator_init_default(&allocator);
    for (uint32_t offset = 0; offset < 2; ++offset) {
        AfProject project = {0}, reopened = {0};
        CHECK(af_project_create(&project, &allocator, 8) == AF_RESULT_SUCCESS);
        AfSceneNode node = af_test_compound_node(1, AF_NODE_GROUP, (AfVec2){80, 40});
        node.local_transform.rotation = .25F;
        node.local_transform.scale = (AfVec2){-1.2F, .8F};
        CHECK(af_project_add_node(&project, &node, "Parent") == AF_RESULT_SUCCESS);
        for (uint32_t i = 2; i <= 3; ++i) {
            node = af_test_compound_node(i, AF_NODE_RECTANGLE, (AfVec2){0, 0});
            node.parent_id = 1;
            node.bounds = (AfVec2){100, 100};
            CHECK(af_project_add_node(&project, &node, "Rectangle") == AF_RESULT_SUCCESS);
        }
        AfStyle *style = af_project_find_node_style(&project, 2);
        style->stroke.enabled = true;
        style->stroke.width = style->stroke.end_width = 10;
        style->stroke_paint = af_paint_solid((AfColor){0, 1, 0, 1});
        AfEditorCommand command;
        AfEditorHistory history;
        CHECK(af_editor_history_create(&history, &allocator, AF_EDITOR_HISTORY_DEFAULT_BYTES) ==
              AF_RESULT_SUCCESS);
        CHECK(af_editor_command_outline(&project, 2, offset != 0, -10, &command) ==
              AF_RESULT_SUCCESS);
        const uint64_t output = command.after[command.after_count - 1].node.id;
        CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
              AF_RESULT_SUCCESS);
        CHECK(project.scene.count == (offset ? 3U : 4U));
        const AfPath *path = af_path_find_node_const(&project.path_data, output);
        CHECK(path && path->fill_rule == AF_PATH_FILL_NON_ZERO);
        CHECK_NEAR((float)af_test_boolean_area(project.path_data.commands + path->command_start,
                                               path->command_count),
                   offset ? 6400 : 4000, .1F);
        if (!offset) {
            CHECK(!af_project_find_node_style(&project, 2)->stroke.enabled);
            CHECK(af_project_find_node_style(&project, output)->fill.solid.g == 1);
        }
        AfPackageBlob text = {0}, package = {0};
        CHECK(af_project_serialize(&project, &allocator, &text) == AF_RESULT_SUCCESS);
        CHECK(af_project_deserialize(text.data, text.size, &allocator, &reopened) ==
              AF_RESULT_SUCCESS);
        CHECK(af_project_compile(&reopened, &allocator, &package) == AF_RESULT_SUCCESS);
        CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(project.scene.count == 3 && project.path_data.path_count == 0);
        CHECK(af_project_find_node_style(&project, 2)->stroke.enabled);
        CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(af_project_validate(&project) == AF_RESULT_SUCCESS);
        af_package_blob_destroy(&allocator, &text);
        af_package_blob_destroy(&allocator, &package);
        af_project_destroy(&reopened);
        af_editor_history_destroy(&history);
        af_project_destroy(&project);
    }
    return true;
}
