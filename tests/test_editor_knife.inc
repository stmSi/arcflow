static const AfPathCommand af_test_knife_rectangle[] = {
    {.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}},
    {.type = AF_PATH_LINE, .point = {100.0F, 0.0F}},
    {.type = AF_PATH_LINE, .point = {100.0F, 100.0F}},
    {.type = AF_PATH_LINE, .point = {0.0F, 100.0F}},
    {.type = AF_PATH_CLOSE}};

static bool af_test_knife_contours(AfPathCommand *commands, uint32_t count, uint32_t moves,
                                   uint32_t closes) {
    AfPath path = {.id = 1U, .node_id = 1U, .command_count = count, .geometry_generation = 1U};
    AfPathData data = {.paths = &path,
                       .path_count = 1U,
                       .path_capacity = 1U,
                       .commands = commands,
                       .command_count = count,
                       .command_capacity = count};
    uint32_t actual_moves = 0U;
    uint32_t actual_closes = 0U;
    CHECK(af_path_data_validate(&data) == AF_RESULT_SUCCESS);
    for (uint32_t index = 0U; index < count; ++index) {
        actual_moves += commands[index].type == AF_PATH_MOVE;
        actual_closes += commands[index].type == AF_PATH_CLOSE;
    }
    CHECK(actual_moves == moves && actual_closes == closes);
    return true;
}

static AfVec2 af_test_knife_curve_at(AfVec2 start, AfPathCommand curve, float t) {
    const float u = 1.0F - t;
    return (AfVec2){u * u * u * start.x + 3.0F * u * u * t * curve.control1.x +
                        3.0F * u * t * t * curve.control2.x + t * t * t * curve.point.x,
                    u * u * u * start.y + 3.0F * u * u * t * curve.control1.y +
                        3.0F * u * t * t * curve.control2.y + t * t * t * curve.point.y};
}

static bool af_test_editor_knife_geometry(void) {
    AfPathCommand output[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY] = {0};
    AfPathCommand source[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY] = {0};
    AfEditorKnifeResult result = {0};
    /* A single cut on the implicit left edge opens one contour at the actual intersection. */
    CHECK(af_editor_vector_tool_knife(af_test_knife_rectangle, 5U, (AfVec2){-10.0F, 50.0F},
                                      (AfVec2){10.0F, 50.0F}, output, 6U,
                                      &result) == AF_RESULT_SUCCESS);
    CHECK(result.cut_count == 1U && result.command_count == 6U && result.first_cut_command == 0U);
    CHECK(af_test_knife_contours(output, result.command_count, 1U, 0U));
    CHECK_NEAR(output[0].point.x, 0.0F, 0.0001F);
    CHECK_NEAR(output[0].point.y, 50.0F, 0.0001F);
    CHECK(output[1].type == AF_PATH_LINE && output[1].point.x == 0.0F && output[1].point.y == 0.0F);
    CHECK(memcmp(&output[2], &af_test_knife_rectangle[1], 3U * sizeof(*output)) == 0);
    CHECK(output[5].point.x == output[0].point.x && output[5].point.y == output[0].point.y);
    /* An explicit edge cut must not introduce a second break at the old MOVE. */
    CHECK(af_editor_vector_tool_knife(af_test_knife_rectangle, 5U, (AfVec2){50.0F, -10.0F},
                                      (AfVec2){50.0F, 10.0F}, output, 6U,
                                      &result) == AF_RESULT_SUCCESS);
    CHECK(result.cut_count == 1U && result.command_count == 6U);
    CHECK(af_test_knife_contours(output, result.command_count, 1U, 0U));
    CHECK(output[0].point.x == 50.0F && output[0].point.y == 0.0F);
    CHECK(output[4].point.x == 0.0F && output[4].point.y == 0.0F);
    /* Cross both the explicit right edge and implicit left edge: exactly two open contours. */
    CHECK(af_editor_vector_tool_knife(af_test_knife_rectangle, 5U, (AfVec2){-10.0F, 50.0F},
                                      (AfVec2){110.0F, 50.0F}, output, 8U,
                                      &result) == AF_RESULT_SUCCESS);
    CHECK(result.cut_count == 2U && result.command_count == 8U);
    CHECK(af_test_knife_contours(output, result.command_count, 2U, 0U));
    CHECK(output[0].point.x == 100.0F && output[0].point.y == 50.0F);
    CHECK(output[3].point.x == 0.0F && output[3].point.y == 50.0F);
    CHECK(output[4].type == AF_PATH_MOVE && output[4].point.x == 0.0F &&
          output[4].point.y == 50.0F);
    /* Compound contours retain uncut geometry and winding, even with input/output aliasing. */
    memcpy(source, af_test_knife_rectangle, sizeof(af_test_knife_rectangle));
    memcpy(source + 5U, af_test_knife_rectangle, sizeof(af_test_knife_rectangle));
    for (uint32_t index = 5U; index < 9U; ++index) {
        source[index].point.x += 200.0F;
    }
    memcpy(output, source, 10U * sizeof(*source));
    CHECK(af_editor_vector_tool_knife(output, 10U, (AfVec2){-10.0F, 50.0F}, (AfVec2){110.0F, 50.0F},
                                      output, 13U, &result) == AF_RESULT_SUCCESS);
    CHECK(result.cut_count == 2U && result.command_count == 13U);
    CHECK(af_test_knife_contours(output, result.command_count, 3U, 1U));
    CHECK(memcmp(output + 8U, source + 5U, 5U * sizeof(*source)) == 0);
    CHECK(af_editor_vector_tool_knife(source, 10U, (AfVec2){-10.0F, 50.0F}, (AfVec2){310.0F, 50.0F},
                                      output, 16U, &result) == AF_RESULT_SUCCESS);
    CHECK(result.cut_count == 4U && result.command_count == 16U);
    CHECK(af_test_knife_contours(output, result.command_count, 4U, 0U));
    /* Subdivision preserves the entire cubic, including both tangents. */
    source[0] = (AfPathCommand){.type = AF_PATH_MOVE};
    source[1] = (AfPathCommand){.type = AF_PATH_CUBIC,
                                .point = {100.0F, 0.0F},
                                .control1 = {0.0F, 100.0F},
                                .control2 = {100.0F, 100.0F}};
    CHECK(af_editor_vector_tool_knife(source, 2U, (AfVec2){50.0F, -10.0F}, (AfVec2){50.0F, 80.0F},
                                      output, 4U, &result) == AF_RESULT_SUCCESS);
    CHECK(result.cut_count == 1U && result.command_count == 4U && result.first_cut_command == 1U);
    CHECK(af_test_knife_contours(output, result.command_count, 2U, 0U));
    CHECK(output[1].type == AF_PATH_CUBIC && output[3].type == AF_PATH_CUBIC);
    CHECK_NEAR(output[1].point.x, 50.0F, 0.0001F);
    CHECK_NEAR(output[1].point.y, 75.0F, 0.0001F);
    for (uint32_t sample = 0U; sample <= 20U; ++sample) {
        const float t = (float)sample / 20.0F;
        const AfVec2 expected = af_test_knife_curve_at(source[0].point, source[1], t);
        const AfVec2 actual =
            t <= 0.5F ? af_test_knife_curve_at(output[0].point, output[1], t * 2.0F)
                      : af_test_knife_curve_at(output[2].point, output[3], (t - 0.5F) * 2.0F);
        CHECK_NEAR(actual.x, expected.x, 0.0001F);
        CHECK_NEAR(actual.y, expected.y, 0.0001F);
    }
    source[2] = (AfPathCommand){.type = AF_PATH_CLOSE};
    CHECK(af_editor_vector_tool_knife(source, 3U, (AfVec2){50.0F, -10.0F}, (AfVec2){50.0F, 80.0F},
                                      output, 6U, &result) == AF_RESULT_SUCCESS);
    CHECK(result.cut_count == 2U && result.command_count == 6U);
    CHECK(af_test_knife_contours(output, result.command_count, 2U, 0U));
    CHECK(output[1].type == AF_PATH_CUBIC && output[5].type == AF_PATH_CUBIC);
    CHECK(output[4].point.x == 0.0F && output[4].point.y == 0.0F);
    /* A curve returning to the MOVE has no implicit closing edge to materialize. */
    source[1].point = (AfVec2){0.0F, 0.0F};
    source[1].control1 = (AfVec2){100.0F, -100.0F};
    source[1].control2 = (AfVec2){100.0F, 100.0F};
    CHECK(af_editor_vector_tool_knife(source, 3U, (AfVec2){50.0F, -100.0F}, (AfVec2){50.0F, 100.0F},
                                      output, 3U, &result) == AF_RESULT_SUCCESS);
    CHECK(result.cut_count == 1U && result.command_count == 3U);
    CHECK(af_test_knife_contours(output, result.command_count, 1U, 0U));
    CHECK(output[1].type == AF_PATH_CUBIC && output[2].type == AF_PATH_CUBIC);
    return true;
}

static bool af_test_editor_knife_limits(void) {
    AfPathCommand output[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY] = {0};
    AfPathCommand source[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY] = {0};
    AfPathCommand sentinel[AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY];
    AfEditorKnifeResult result = {.command_count = 37U, .cut_count = 42U, .first_cut_command = 23U};
    const AfEditorKnifeResult saved = result;
    memset(output, 0x5a, sizeof(output));
    memcpy(sentinel, output, sizeof(output));
    const AfVec2 starts[] = {{-10.0F, 50.0F}, {-10.0F, -10.0F}, {-10.0F, 0.0F},
                             {0.0F, 50.0F},   {NAN, 0.0F},      {0.0F, 0.0F}};
    const AfVec2 ends[] = {{110.0F, 50.0F}, {110.0F, 110.0F}, {110.0F, 0.0F},
                           {0.0F, 50.0F},   {10.0F, 10.0F},   {INFINITY, 0.0F}};
    const AfResult expected[] = {AF_RESULT_LIMIT_EXCEEDED,   AF_RESULT_NOT_FOUND,
                                 AF_RESULT_NOT_FOUND,        AF_RESULT_INVALID_ARGUMENT,
                                 AF_RESULT_INVALID_ARGUMENT, AF_RESULT_INVALID_ARGUMENT};
    for (uint32_t index = 0U; index < 6U; ++index) {
        CHECK(af_editor_vector_tool_knife(af_test_knife_rectangle, 5U, starts[index], ends[index],
                                          output,
                                          index == 0U ? 7U : AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY,
                                          &result) == expected[index]);
        CHECK(memcmp(output, sentinel, sizeof(output)) == 0);
        CHECK(memcmp(&result, &saved, sizeof(result)) == 0);
    }
    source[0] = (AfPathCommand){.type = AF_PATH_MOVE};
    for (uint32_t index = 1U; index < AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY; ++index) {
        source[index] = (AfPathCommand){.type = AF_PATH_LINE, .point = {(float)index, 0.0F}};
    }
    CHECK(af_editor_vector_tool_knife(source, 511U, (AfVec2){0.5F, -1.0F}, (AfVec2){0.5F, 1.0F},
                                      output, AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY,
                                      &result) == AF_RESULT_LIMIT_EXCEEDED);
    CHECK(memcmp(output, sentinel, sizeof(output)) == 0);
    CHECK(memcmp(&result, &saved, sizeof(result)) == 0);
    CHECK(af_editor_vector_tool_knife(source, 510U, (AfVec2){0.5F, -1.0F}, (AfVec2){0.5F, 1.0F},
                                      output, AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY,
                                      &result) == AF_RESULT_SUCCESS);
    CHECK(result.command_count == 512U && result.cut_count == 1U);
    CHECK(af_test_knife_contours(output, result.command_count, 2U, 0U));
    source[1].type = AF_PATH_MOVE;
    memcpy(sentinel, output, sizeof(output));
    CHECK(af_editor_vector_tool_knife(source, 510U, (AfVec2){0.5F, -1.0F}, (AfVec2){0.5F, 1.0F},
                                      output, AF_EDITOR_VECTOR_TOOL_POINT_CAPACITY,
                                      &result) == AF_RESULT_INVALID_ARGUMENT);
    CHECK(memcmp(output, sentinel, sizeof(output)) == 0);
    return true;
}

static bool af_test_editor_knife_persistence(void) {
    AfAllocator allocator;
    AfProject project = {0};
    AfProject reopened = {0};
    AfEditorHistory history = {0};
    AfEditorCommand command = {0};
    AfEditorKnifeResult result = {0};
    AfPackageBlob text = {0};
    AfPackageBlob package = {0};
    AfLoadedPackage runtime = {0};
    AfSceneNode node = {.id = 1U,
                        .parent_id = AF_INVALID_NODE_ID,
                        .type = AF_NODE_PATH,
                        .local_transform = {.scale = {1.0F, 1.0F}},
                        .bounds = {100.0F, 100.0F},
                        .fill = {0.5F, 0.2F, 0.8F, 1.0F},
                        .opacity = 1.0F,
                        .visible = true};
    AfPath path = {.id = 2U,
                   .node_id = 1U,
                   .command_count = 5U,
                   .fill_rule = AF_PATH_FILL_EVEN_ODD,
                   .geometry_generation = 1U};
    af_allocator_init_default(&allocator);
    CHECK(af_project_create(&project, &allocator, 8U) == AF_RESULT_SUCCESS);
    CHECK(af_project_add_node(&project, &node, "Knife rectangle") == AF_RESULT_SUCCESS);
    CHECK(af_project_add_path(&project, &path, af_test_knife_rectangle) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_create(&history, &allocator, sizeof(command) * 4U) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_command_path(AF_EDITOR_MERGE_NONE, path.id, &command) == AF_RESULT_SUCCESS);
    CHECK(af_project_path_snapshot(&project, path.id, &command.path_before[0],
                                   command.path_command_before,
                                   AF_EDITOR_COMMAND_MAX_PATH_COMMANDS) == AF_RESULT_SUCCESS);
    command.path_before_count = command.path_after_count = 1U;
    command.path_command_before_count = 5U;
    command.path_after[0] = command.path_before[0];
    CHECK(af_editor_vector_tool_knife(command.path_command_before, 5U, (AfVec2){-10.0F, 50.0F},
                                      (AfVec2){110.0F, 50.0F}, command.path_command_after,
                                      AF_EDITOR_COMMAND_MAX_PATH_COMMANDS,
                                      &result) == AF_RESULT_SUCCESS);
    command.path_command_after_count = result.command_count;
    command.path_after[0].path.command_count = result.command_count;
    ++command.path_after[0].path.geometry_generation;
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(history.count == 1U && history.cursor == 1U);
    CHECK(af_project_validate(&project) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.path_data.command_count == 5U);
    CHECK(memcmp(project.path_data.commands, af_test_knife_rectangle,
                 sizeof(af_test_knife_rectangle)) == 0);
    CHECK(af_project_find_path(&project, path.id)->geometry_generation == 1U);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.path_data.command_count == result.command_count);
    CHECK(memcmp(project.path_data.commands, command.path_command_after,
                 result.command_count * sizeof(AfPathCommand)) == 0);
    CHECK(af_project_serialize(&project, &allocator, &text) == AF_RESULT_SUCCESS);
    CHECK(af_project_deserialize(text.data, text.size, &allocator, &reopened) == AF_RESULT_SUCCESS);
    CHECK(reopened.path_data.command_count == result.command_count);
    CHECK(memcmp(reopened.path_data.commands, command.path_command_after,
                 result.command_count * sizeof(AfPathCommand)) == 0);
    CHECK(af_project_find_path(&reopened, path.id)->fill_rule == AF_PATH_FILL_EVEN_ODD);
    CHECK(af_project_compile(&reopened, &allocator, &package) == AF_RESULT_SUCCESS);
    CHECK(af_package_decode(package.data, package.size, &allocator, &runtime) == AF_RESULT_SUCCESS);
    CHECK(runtime.path_data.command_count == result.command_count);
    CHECK(memcmp(runtime.path_data.commands, command.path_command_after,
                 result.command_count * sizeof(AfPathCommand)) == 0);
    CHECK(af_test_knife_contours(runtime.path_data.commands, result.command_count, 2U, 0U));
    af_loaded_package_destroy(&allocator, &runtime);
    af_package_blob_destroy(&allocator, &package);
    af_package_blob_destroy(&allocator, &text);
    af_project_destroy(&reopened);
    af_editor_history_destroy(&history);
    af_project_destroy(&project);
    return true;
}
