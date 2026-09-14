static void af_test_boolean_rectangle(AfPathCommand *p, float x, float y, float w, float h) {
    p[0] = (AfPathCommand){.type = AF_PATH_MOVE, .point = {x, y}};
    p[1] = (AfPathCommand){.type = AF_PATH_LINE, .point = {x + w, y}};
    p[2] = (AfPathCommand){.type = AF_PATH_LINE, .point = {x + w, y + h}};
    p[3] = (AfPathCommand){.type = AF_PATH_LINE, .point = {x, y + h}};
    p[4] = (AfPathCommand){.type = AF_PATH_CLOSE};
}

static double af_test_boolean_area(const AfPathCommand *commands, uint32_t count) {
    AfVec2 previous = {0}, first = {0};
    double area = 0;
    for (uint32_t i = 0; i < count; ++i) {
        const AfPathCommand command = commands[i];
        if (command.type == AF_PATH_MOVE)
            first = previous = command.point;
        else {
            const AfVec2 point = command.type == AF_PATH_CLOSE ? first : command.point;
            area += (double)previous.x * point.y - (double)point.x * previous.y;
            previous = point;
        }
    }
    return area * 0.5;
}

static AfPathData af_test_boolean_data(AfPath *paths, AfPathCommand *commands, uint32_t a,
                                       uint32_t b) {
    paths[0] = (AfPath){.id = 1U,
                        .node_id = 1U,
                        .command_count = a,
                        .fill_rule = AF_PATH_FILL_NON_ZERO,
                        .geometry_generation = 1U};
    paths[1] = (AfPath){.id = 2U,
                        .node_id = 2U,
                        .command_start = a,
                        .command_count = b,
                        .fill_rule = AF_PATH_FILL_NON_ZERO,
                        .geometry_generation = 1U};
    return (AfPathData){.paths = paths,
                        .path_count = 2U,
                        .path_capacity = 2U,
                        .commands = commands,
                        .command_count = a + b,
                        .command_capacity = a + b};
}

static bool af_test_boolean_rectangles(void) {
    AfAllocator allocator;
    af_allocator_init_default(&allocator);
    AfPath paths[2];
    AfPathCommand input[10], output[512];
    AfPathData data = af_test_boolean_data(paths, input, 5U, 5U);
    /* Overlap, containment, disjoint, shared edge, point contact, identical, thin overlap. */
    const float cases[][4] = {{50, 50, 100, 100},    {25, 25, 50, 50},     {200, 0, 100, 100},
                              {100, 0, 100, 100},    {100, 100, 100, 100}, {0, 0, 100, 100},
                              {99.99F, 0, 100, 100}, {25, 0, 50, 100},     {-25, -25, 150, 150}};
    for (uint32_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const float *r = cases[i];
        af_test_boolean_rectangle(input, 0, 0, 100, 100);
        af_test_boolean_rectangle(input + 5U, r[0], r[1], r[2], r[3]);
        const double overlap = fmax(0, fmin(100, r[0] + r[2]) - fmax(0, r[0])) *
                               fmax(0, fmin(100, r[1] + r[3]) - fmax(0, r[1]));
        const double expected[] = {10000 + (double)r[2] * r[3] - overlap, 10000 - overlap, overlap};
        for (uint32_t op = 0; op < 3U; ++op) {
            uint32_t count = 777U;
            CHECK(af_path_boolean(&allocator, &data, paths, paths + 1U, (AfPathBooleanOperation)op,
                                  0.1F, output, 512U, &count) == AF_RESULT_SUCCESS);
            CHECK(fabs(af_test_boolean_area(output, count) - expected[op]) < 0.1);
            CHECK((count == 0U) == (expected[op] == 0));
            if (count != 0U) {
                AfPath result = {
                    .id = 3U, .node_id = 3U, .command_count = count, .geometry_generation = 1U};
                AfPathData out = {.paths = &result,
                                  .path_count = 1U,
                                  .path_capacity = 1U,
                                  .commands = output,
                                  .command_count = count,
                                  .command_capacity = 512U};
                CHECK(af_path_data_validate(&out) == AF_RESULT_SUCCESS);
            }
        }
    }
    /* Deterministic varied rectangles check Boolean identities independently of clipping. */
    uint32_t seed = 19283U;
    for (uint32_t i = 0; i < 200U; ++i) {
        float r[8];
        for (uint32_t j = 0; j < 8U; ++j) {
            seed = seed * 1664525U + 1013904223U;
            r[j] = (float)(seed % 1500U) * 0.1F + 0.031F;
        }
        af_test_boolean_rectangle(input, r[0], r[1], r[2], r[3]);
        af_test_boolean_rectangle(input + 5U, r[4], r[5], r[6], r[7]);
        const double overlap = fmax(0, fmin(r[0] + r[2], r[4] + r[6]) - fmax(r[0], r[4])) *
                               fmax(0, fmin(r[1] + r[3], r[5] + r[7]) - fmax(r[1], r[5]));
        const double expected[] = {(double)r[2] * r[3] + (double)r[6] * r[7] - overlap,
                                   (double)r[2] * r[3] - overlap, overlap};
        for (uint32_t op = 0; op < 3U; ++op) {
            uint32_t count = 0U;
            const AfResult result =
                af_path_boolean(&allocator, &data, paths, paths + 1U, (AfPathBooleanOperation)op,
                                0.1F, output, 512U, &count);
            if (result != AF_RESULT_SUCCESS)
                (void)fprintf(
                    stderr,
                    "Boolean rectangle case %u op %u result %d: %g %g %g %g / %g %g %g %g\n", i, op,
                    result, (double)r[0], (double)r[1], (double)r[2], (double)r[3], (double)r[4],
                    (double)r[5], (double)r[6], (double)r[7]);
            CHECK(result == AF_RESULT_SUCCESS);
            CHECK(fabs(af_test_boolean_area(output, count) - expected[op]) < 0.1);
        }
    }
    return true;
}

static bool af_test_boolean_membership(AfPathData *data, AfPathBooleanOperation operation) {
    AfAllocator allocator;
    af_allocator_init_default(&allocator);
    AfPathCommand output[512];
    uint32_t count = 0U;
    CHECK(af_path_boolean(&allocator, data, data->paths, data->paths + 1U, operation, 0.05F, output,
                          512U, &count) == AF_RESULT_SUCCESS);
    AfPath result = {.id = 3U, .node_id = 3U, .command_count = count, .geometry_generation = 1U};
    AfPathData out = {.paths = &result,
                      .path_count = 1U,
                      .path_capacity = 1U,
                      .commands = output,
                      .command_count = count,
                      .command_capacity = 512U};
    AfPathTessellationScratch scratch;
    for (uint32_t y = 0; y < 24U; ++y) {
        for (uint32_t x = 0; x < 24U; ++x) {
            const AfVec2 p = {(float)x * 5.317F - 15.17F, (float)y * 5.213F - 15.39F};
            const bool a = af_path_hit_test(data, data->paths, p, 0.05F, &scratch);
            const bool b = af_path_hit_test(data, data->paths + 1U, p, 0.05F, &scratch);
            const bool expected = operation == AF_PATH_BOOLEAN_UNION      ? a || b
                                  : operation == AF_PATH_BOOLEAN_SUBTRACT ? a && !b
                                                                          : a && b;
            const bool actual = count != 0U && af_path_hit_test(&out, &result, p, 0.05F, &scratch);
            CHECK(actual == expected);
        }
    }
    return true;
}

static bool af_test_boolean_contours_and_curves(void) {
    AfPath paths[2];
    AfPathCommand input[128];
    af_test_boolean_rectangle(input, 0, 0, 100, 100);
    af_test_boolean_rectangle(input + 5U, 25, 25, 50, 50);
    af_test_boolean_rectangle(input + 10U, 50, -10, 75, 100);
    AfPathData data = af_test_boolean_data(paths, input, 10U, 5U);
    for (uint32_t rule = 0; rule < 2U; ++rule) {
        paths[0].fill_rule = (AfPathFillRule)rule;
        for (uint32_t op = 0; op < 3U; ++op)
            CHECK(af_test_boolean_membership(&data, (AfPathBooleanOperation)op));
    }
    /* Opposite contour winding makes a nonzero hole. */
    af_test_boolean_rectangle(input + 5U, 75, 25, -50, 50);
    paths[0].fill_rule = AF_PATH_FILL_NON_ZERO;
    for (uint32_t op = 0; op < 3U; ++op)
        CHECK(af_test_boolean_membership(&data, (AfPathBooleanOperation)op));
    /* A self-crossing bow tie, and a concave star. */
    input[0] = (AfPathCommand){.type = AF_PATH_MOVE, .point = {0, 0}};
    input[1] = (AfPathCommand){.type = AF_PATH_LINE, .point = {100, 100}};
    input[2] = (AfPathCommand){.type = AF_PATH_LINE, .point = {100, 0}};
    input[3] = (AfPathCommand){.type = AF_PATH_LINE, .point = {0, 100}};
    input[4] = (AfPathCommand){.type = AF_PATH_CLOSE};
    uint32_t second =
        af_editor_vector_tool_polygon((AfVec2){100, 100}, 5U, 0.45F, input + 5U, 123U);
    data = af_test_boolean_data(paths, input, 5U, second);
    for (uint32_t op = 0; op < 3U; ++op)
        CHECK(af_test_boolean_membership(&data, (AfPathBooleanOperation)op));
    const uint32_t first =
        af_editor_vector_primitive_path(AF_NODE_ELLIPSE, (AfVec2){100, 80}, 0, input, 128U);
    second = af_editor_vector_primitive_path(AF_NODE_ROUNDED_RECTANGLE, (AfVec2){100, 100}, 20,
                                             input + first, 128U - first);
    for (uint32_t i = first; i < first + second; ++i) {
        input[i].point.x += 30;
        input[i].control1.x += 30;
        input[i].control2.x += 30;
    }
    data = af_test_boolean_data(paths, input, first, second);
    for (uint32_t op = 0; op < 3U; ++op)
        CHECK(af_test_boolean_membership(&data, (AfPathBooleanOperation)op));
    for (uint32_t trial = 0; trial < 24U; ++trial) {
        uint32_t counts[2];
        uint32_t offset = 0U;
        for (uint32_t shape = 0; shape < 2U; ++shape) {
            counts[shape] = af_editor_vector_tool_polygon(
                (AfVec2){90, 80}, 3U + (trial + shape) % 6U, 0.2F + (float)(trial % 5U) * 0.13F,
                input + offset, 128U - offset);
            const AfAffine2 transform = af_affine_from_transform(
                (AfTransform2D){.position = {(float)shape * 23.713F, (float)shape * 13.251F},
                                .rotation = (float)(trial + shape * 3U) * 0.213F,
                                .scale = {shape == 0U ? 1 : -1, 1}});
            for (uint32_t i = 0; i < counts[shape]; ++i)
                input[offset + i].point =
                    af_affine_transform_point(transform, input[offset + i].point);
            offset += counts[shape];
        }
        data = af_test_boolean_data(paths, input, counts[0], counts[1]);
        paths[trial % 2U].fill_rule = AF_PATH_FILL_EVEN_ODD;
        for (uint32_t op = 0; op < 3U; ++op)
            CHECK(af_test_boolean_membership(&data, (AfPathBooleanOperation)op));
    }
    return true;
}

static bool af_test_boolean_limits(void) {
    AfAllocator allocator;
    af_allocator_init_default(&allocator);
    AfPath paths[2];
    AfPathCommand input[10], output[16], saved[16];
    af_test_boolean_rectangle(input, 0, 0, 100, 100);
    af_test_boolean_rectangle(input + 5U, 50, 50, 100, 100);
    AfPathData data = af_test_boolean_data(paths, input, 5U, 5U);
    memset(output, 0x5a, sizeof(output));
    memcpy(saved, output, sizeof(output));
    uint32_t count = 99U;
    CHECK(af_path_boolean(&allocator, &data, paths, paths + 1U, AF_PATH_BOOLEAN_UNION, 0.1F, output,
                          3U, &count) == AF_RESULT_LIMIT_EXCEEDED);
    CHECK(count == 99U && memcmp(saved, output, sizeof(output)) == 0);
    input[4] = (AfPathCommand){.type = AF_PATH_LINE, .point = {0, 0}};
    CHECK(af_path_boolean(&allocator, &data, paths, paths + 1U, AF_PATH_BOOLEAN_UNION, 0.1F, output,
                          16U, &count) == AF_RESULT_UNSUPPORTED_FEATURE);
    CHECK(count == 99U && memcmp(saved, output, sizeof(output)) == 0);
    input[4] = (AfPathCommand){.type = AF_PATH_CLOSE};
    CHECK(af_path_boolean(&allocator, &data, paths, paths + 1U, AF_PATH_BOOLEAN_INTERSECT, 0.1F,
                          input, 10U, &count) == AF_RESULT_SUCCESS);
    CHECK(fabs(af_test_boolean_area(input, count) - 2500) < 0.01);
    /* A grid creates many intersections from a modest authored command count. */
    AfPathCommand grid[320];
    for (uint32_t i = 0; i < 32U; ++i) {
        af_test_boolean_rectangle(grid + i * 5U, (float)i * 3, -5, 1, 110);
        af_test_boolean_rectangle(grid + 160U + i * 5U, -5, (float)i * 3, 110, 1);
    }
    data = af_test_boolean_data(paths, grid, 160U, 160U);
    count = 99U;
    CHECK(af_path_boolean(&allocator, &data, paths, paths + 1U, AF_PATH_BOOLEAN_UNION, 0.1F, output,
                          16U, &count) == AF_RESULT_LIMIT_EXCEEDED);
    CHECK(count == 99U && memcmp(saved, output, sizeof(output)) == 0);
    return true;
}
