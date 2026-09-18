static bool af_test_editor_text(void) {
    AfAllocator allocator;
    af_allocator_init_default(&allocator);
    AfFileData font = {0};
    AfTextEngine *engine = NULL;
    CHECK(af_file_read_limited(ARCFLOW_TEST_DOCUMENT_FONT, 4U * 1024U * 1024U, &allocator, &font) ==
          AF_RESULT_SUCCESS);
    CHECK(af_text_engine_create(&allocator, &engine) == AF_RESULT_SUCCESS);
    CHECK(af_text_font_load_memory(engine, 1, font.data, font.size, 0) == AF_RESULT_SUCCESS);
    AfProject project = {0}, reopened = {0};
    AfEditorHistory history;
    AfEditorCommand command;
    AfPackageBlob serialized = {0}, package = {0};
    CHECK(af_project_create(&project, &allocator, 32) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_create(&history, &allocator, AF_EDITOR_HISTORY_DEFAULT_BYTES) ==
          AF_RESULT_SUCCESS);
    AfProjectText text = {
        .mode = AF_PROJECT_TEXT_ARTISTIC, .utf8 = "Hello café", .font_size = 36, .width = 180};
    for (uint32_t mode = 1; mode <= 3; ++mode) {
        text.mode = (AfProjectTextMode)mode;
        text.font_family = mode % 3;
        text.bold = mode == 1;
        text.italic = mode == 3;
        text.alignment = mode % 3;
        text.letter_spacing = .5F;
        text.line_spacing = 1.4F;
        if (mode == 3) {
            text.guide_count = 2;
            text.guide[0] = (AfPathCommand){.type = AF_PATH_MOVE, .point = {0, 100}};
            text.guide[1] = (AfPathCommand){.type = AF_PATH_CUBIC,
                                            .point = {500, 100},
                                            .control1 = {150, -40},
                                            .control2 = {350, -40}};
            text.offset = 25;
        }
        AfResult build_result =
            af_editor_command_text(&project, engine, 1, 0, (AfVec2){50, 100}, &text, &command);
        CHECK(build_result == AF_RESULT_SUCCESS);
        CHECK(command.path_command_after_count > 10);
        const uint64_t id = command.target_id;
        CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
              AF_RESULT_SUCCESS);
        CHECK(project.scene.count == mode && af_project_validate(&project) == AF_RESULT_SUCCESS);
        CHECK(memcmp(&project.texts[mode - 1], &text, sizeof(text)) == 0);
        CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS &&
              project.scene.count == mode - 1);
        CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
        (void)snprintf(text.utf8, sizeof(text.utf8), "Edited café");
        CHECK(af_editor_command_text(&project, engine, 1, id, (AfVec2){0}, &text, &command) ==
              AF_RESULT_SUCCESS);
        CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
              AF_RESULT_SUCCESS);
        CHECK(strcmp(project.texts[mode - 1].utf8, "Edited café") == 0);
        CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    }
    CHECK(af_project_serialize(&project, &allocator, &serialized) == AF_RESULT_SUCCESS);
    CHECK(af_project_deserialize(serialized.data, serialized.size, &allocator, &reopened) ==
          AF_RESULT_SUCCESS);
    for (uint32_t i = 0; i < 3; ++i)
        CHECK(memcmp(&project.texts[i], &reopened.texts[i], sizeof(text)) == 0);
    CHECK(af_project_compile(&reopened, &allocator, &package) == AF_RESULT_SUCCESS);
    char svg[262144];
    size_t svg_size = 0;
    AfInterchangeDiagnostics diagnostics = {0};
    CHECK(af_interchange_export_static_svg(&reopened.scene, reopened.canvas_size, svg, sizeof(svg),
                                           &svg_size, &diagnostics) == AF_RESULT_SUCCESS);
    CHECK(svg_size > 100);
    const uint64_t first_id = project.scene.nodes[0].id, last_id = project.scene.nodes[2].id;
    CHECK(af_editor_command_reorder(&project, first_id, last_id, true, &command) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(project.texts[2].mode == AF_PROJECT_TEXT_ARTISTIC);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.texts[0].mode == AF_PROJECT_TEXT_ARTISTIC);
    CHECK(af_editor_command_delete(&project, first_id, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(project.texts[0].mode == AF_PROJECT_TEXT_PARAGRAPH);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.texts[0].mode == AF_PROJECT_TEXT_ARTISTIC);
    CHECK(af_editor_command_convert_path(&project, first_id, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(project.texts[0].mode == AF_PROJECT_TEXT_NONE);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.texts[0].mode == AF_PROJECT_TEXT_ARTISTIC);
    /* Word boundaries and explicit newlines must agree with paragraph authoring. */
    AfGlyphPlacement glyphs[64];
    AfTextLine lines[16];
    AfTextLayout layout = {
        .glyphs = glyphs, .glyph_capacity = 64, .lines = lines, .line_capacity = 16};
    AfTextRequest request = {.utf8 = "one two three",
                             .byte_count = 13,
                             .font_ids = {1},
                             .font_count = 1,
                             .font_size = 24,
                             .maximum_width = 110,
                             .maximum_lines = 16,
                             .line_spacing = 1.2F,
                             .wrap = AF_TEXT_WRAP_WORD,
                             .kerning = true,
                             .ligatures = true};
    CHECK(af_text_shape(engine, &request, &layout) == AF_RESULT_SUCCESS);
    CHECK(layout.line_count >= 2 && layout.bounds.x <= 110);
    for (uint32_t i = 1; i < layout.line_count; ++i) {
        const uint32_t cluster = glyphs[lines[i].glyph_start].cluster;
        CHECK(cluster == 4 || cluster == 8);
    }
    request.utf8 = "one\ntwo";
    request.byte_count = 7;
    CHECK(af_text_shape(engine, &request, &layout) == AF_RESULT_SUCCESS && layout.line_count == 2);
    const float unspaced = layout.bounds.x;
    request.letter_spacing = 3;
    CHECK(af_text_shape(engine, &request, &layout) == AF_RESULT_SUCCESS);
    CHECK(layout.bounds.x > unspaced);
    request.line_spacing = 2;
    CHECK(af_text_shape(engine, &request, &layout) == AF_RESULT_SUCCESS);
    CHECK_NEAR(lines[1].baseline - lines[0].baseline, 48, .01F);
    request.maximum_lines = 1;
    CHECK(af_text_shape(engine, &request, &layout) == AF_RESULT_LIMIT_EXCEEDED);
    text.offset = 10000;
    CHECK(af_editor_command_text(&project, engine, 1, last_id, (AfVec2){0}, &text, &command) ==
          AF_RESULT_LIMIT_EXCEEDED);
    CHECK(project.scene.count == 3 && af_project_validate(&project) == AF_RESULT_SUCCESS);
    af_package_blob_destroy(&allocator, &package);
    af_package_blob_destroy(&allocator, &serialized);
    af_project_destroy(&reopened);
    af_editor_history_destroy(&history);
    af_project_destroy(&project);
    af_text_engine_destroy(engine);
    af_file_data_destroy(&allocator, &font);
    return true;
}

static bool af_test_document_fonts(void) {
    static const char *families[] = {"LiberationSans", "LiberationSerif", "LiberationMono"};
    static const char *styles[] = {"Regular", "Bold", "Italic", "BoldItalic"};
    AfAllocator allocator;
    af_allocator_init_default(&allocator);
    AfTextEngine *engine = NULL;
    AfFileData fonts[12] = {0};
    AfProject project = {0};
    AfEditorCommand command;
    CHECK(af_project_create(&project, &allocator, 16) == AF_RESULT_SUCCESS);
    CHECK(af_text_engine_create(&allocator, &engine) == AF_RESULT_SUCCESS);
    uint64_t hashes[12];
    for (uint32_t i = 0; i < 12; ++i) {
        char path[1024];
        (void)snprintf(path, sizeof(path), "%s/%s-%s.ttf", ARCFLOW_TEST_DOCUMENT_FONT_DIR,
                       families[i / 4], styles[i % 4]);
        CHECK(af_file_read_limited(path, 4U * 1024U * 1024U, &allocator, &fonts[i]) ==
              AF_RESULT_SUCCESS);
        CHECK(af_text_font_load_memory(engine, i + 1U, fonts[i].data, fonts[i].size, 0) ==
              AF_RESULT_SUCCESS);
        AfProjectText text = {.mode = AF_PROJECT_TEXT_ARTISTIC,
                              .utf8 = "Font family",
                              .font_size = 28,
                              .width = 320,
                              .font_family = i / 4,
                              .bold = (i % 4 & 1U) != 0,
                              .italic = (i % 4 & 2U) != 0};
        CHECK(af_editor_command_text(&project, engine, i + 1U, 0, (AfVec2){0}, &text, &command) ==
              AF_RESULT_SUCCESS);
        hashes[i] = af_hash_bytes(command.path_command_after,
                                  command.path_command_after_count * sizeof(AfPathCommand));
        for (uint32_t j = 0; j < i; ++j)
            CHECK(hashes[i] != hashes[j]);
    }
    af_text_engine_destroy(engine);
    for (uint32_t i = 0; i < 12; ++i)
        af_file_data_destroy(&allocator, &fonts[i]);
    af_project_destroy(&project);
    return true;
}
