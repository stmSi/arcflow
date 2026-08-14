static bool af_test_project(void) {
    const char *path = "arcflow_test_project.afproj";
    AfAllocator allocator;
    AfProject project = {0};
    AfProject loaded = {0};
    AfProject reopened = {0};
    AfPackageBlob text = {0};
    AfPackageBlob package = {0};
    AfLoadedPackage runtime = {0};
    AfSceneNode root = {.id = 1U,
                        .parent_id = AF_INVALID_NODE_ID,
                        .type = AF_NODE_GROUP,
                        .local_transform = {.scale = {1.0F, 1.0F}},
                        .opacity = 1.0F,
                        .visible = true};
    AfSceneNode shape = {
        .id = 2U,
        .parent_id = 1U,
        .type = AF_NODE_ROUNDED_RECTANGLE,
        .local_transform = {.position = {12.0F, 34.0F}, .rotation = 0.25F, .scale = {1.5F, 0.75F}},
        .bounds = {140.0F, 90.0F},
        .fill = {0.2F, 0.5F, 0.8F, 1.0F},
        .opacity = 0.7F,
        .corner_radius = 18.0F,
        .layer_order = 4,
        .visible = true};
    AfSceneNode path_node = {
        .id = 3U,
        .parent_id = 1U,
        .type = AF_NODE_PATH,
        .local_transform = {.position = {300.0F, 120.0F}, .scale = {1.0F, 1.0F}},
        .bounds = {120.0F, 100.0F},
        .fill = {0.8F, 0.3F, 0.6F, 1.0F},
        .opacity = 1.0F,
        .visible = true};
    AfPath vector_path = {.id = 500U,
                          .node_id = 3U,
                          .command_count = 5U,
                          .fill_rule = AF_PATH_FILL_EVEN_ODD,
                          .geometry_generation = 7U};
    AfPathCommand path_commands[5] = {{.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}},
                                      {.type = AF_PATH_CUBIC,
                                       .point = {120.0F, 0.0F},
                                       .control1 = {30.0F, 80.0F},
                                       .control2 = {90.0F, -40.0F}},
                                      {.type = AF_PATH_LINE, .point = {100.0F, 100.0F}},
                                      {.type = AF_PATH_LINE, .point = {10.0F, 90.0F}},
                                      {.type = AF_PATH_CLOSE}};
    AfProjectClip clip = {.id = 100U,
                          .name = "Main Clip",
                          .duration = 2.0F,
                          .loop_start = 0.25F,
                          .loop_end = 1.75F,
                          .loop = true};
    AfProjectTrack track = {
        .id = 101U, .clip_id = 100U, .node_id = 2U, .property = AF_PROPERTY_POSITION};
    AfProjectKeyframe first_key = {.id = 102U,
                                   .track_id = 101U,
                                   .keyframe = {.time = 0.0F,
                                                .value = {12.0F, 34.0F},
                                                .interpolation = AF_INTERPOLATION_LINEAR}};
    AfProjectKeyframe second_key = {.id = 103U,
                                    .track_id = 101U,
                                    .keyframe = {.time = 1.0F,
                                                 .value = {112.0F, 84.0F},
                                                 .interpolation = AF_INTERPOLATION_CUBIC_BEZIER,
                                                 .easing = {0.42F, 0.0F, 0.58F, 1.0F}}};
    AfProjectTrack path_tracks[3] = {{.id = 104U,
                                      .clip_id = 100U,
                                      .node_id = 3U,
                                      .property = AF_PROPERTY_PATH_POINT,
                                      .element_index = 1U},
                                     {.id = 107U,
                                      .clip_id = 100U,
                                      .node_id = 3U,
                                      .property = AF_PROPERTY_PATH_CONTROL_IN,
                                      .element_index = 1U},
                                     {.id = 110U,
                                      .clip_id = 100U,
                                      .node_id = 3U,
                                      .property = AF_PROPERTY_PATH_CONTROL_OUT,
                                      .element_index = 1U}};
    AfProjectKeyframe path_keys[6] = {{.id = 105U,
                                       .track_id = 104U,
                                       .keyframe = {.time = 0.0F,
                                                    .value = {120.0F, 0.0F},
                                                    .interpolation = AF_INTERPOLATION_LINEAR}},
                                      {.id = 106U,
                                       .track_id = 104U,
                                       .keyframe = {.time = 1.0F,
                                                    .value = {160.0F, 40.0F},
                                                    .interpolation = AF_INTERPOLATION_LINEAR}},
                                      {.id = 108U,
                                       .track_id = 107U,
                                       .keyframe = {.time = 0.0F,
                                                    .value = {90.0F, -40.0F},
                                                    .interpolation = AF_INTERPOLATION_LINEAR}},
                                      {.id = 109U,
                                       .track_id = 107U,
                                       .keyframe = {.time = 1.0F,
                                                    .value = {110.0F, -20.0F},
                                                    .interpolation = AF_INTERPOLATION_LINEAR}},
                                      {.id = 111U,
                                       .track_id = 110U,
                                       .keyframe = {.time = 0.0F,
                                                    .value = {30.0F, 80.0F},
                                                    .interpolation = AF_INTERPOLATION_LINEAR}},
                                      {.id = 112U,
                                       .track_id = 110U,
                                       .keyframe = {.time = 1.0F,
                                                    .value = {50.0F, 100.0F},
                                                    .interpolation = AF_INTERPOLATION_LINEAR}}};
    char with_unknown[8192];
    char invalid_parent[8192];
    char unsupported[8192];
    int written = 0;
    af_allocator_init_default(&allocator);
    CHECK(af_project_create(&project, &allocator, 8U) == AF_RESULT_SUCCESS);
    project.canvas_size = (AfVec2){1920.0F, 1080.0F};
    CHECK(af_project_add_node(&project, &root, "Root Group") == AF_RESULT_SUCCESS);
    CHECK(af_project_add_node(&project, &shape, "Editor Only Name") == AF_RESULT_SUCCESS);
    CHECK(af_project_add_node(&project, &path_node, "Editable Path") == AF_RESULT_SUCCESS);
    CHECK(af_project_add_path(&project, &vector_path, path_commands) == AF_RESULT_SUCCESS);
    CHECK(af_project_add_clip(&project, &clip) == AF_RESULT_SUCCESS);
    CHECK(af_project_add_track(&project, &track) == AF_RESULT_SUCCESS);
    CHECK(af_project_add_keyframe(&project, &first_key) == AF_RESULT_SUCCESS);
    CHECK(af_project_add_keyframe(&project, &second_key) == AF_RESULT_SUCCESS);
    for (uint32_t index = 0U; index < 3U; ++index) {
        CHECK(af_project_add_track(&project, &path_tracks[index]) == AF_RESULT_SUCCESS);
    }
    for (uint32_t index = 0U; index < 6U; ++index) {
        CHECK(af_project_add_keyframe(&project, &path_keys[index]) == AF_RESULT_SUCCESS);
    }
    {
        uint64_t emitter_id = 0U;
        CHECK(af_project_allocate_animation_id(&project, &emitter_id) == AF_RESULT_SUCCESS);
        AfParticleEmitter emitter = af_particle_emitter_default(emitter_id, shape.id);
        emitter.type = AF_PARTICLE_EMITTER_RECTANGLE;
        emitter.output = AF_PARTICLE_OUTPUT_FLIPBOOK;
        emitter.flipbook_columns = 4U;
        emitter.flipbook_rows = 2U;
        emitter.flipbook_frames_per_second = 12.0F;
        emitter.turbulence_strength = 175.0F;
        emitter.radial_force = -40.0F;
        emitter.vortex_force = 90.0F;
        emitter.collision_plane_enabled = true;
        emitter.collision_plane_offset = -320.0F;
        emitter.output = AF_PARTICLE_OUTPUT_RIBBON;
        emitter.sub_emitter_id = emitter.id;
        emitter.sub_emit_death_count = 1U;
        emitter.max_recursion_depth = 2U;
        CHECK(af_project_add_emitter(&project, &emitter) == AF_RESULT_SUCCESS);
    }
    {
        AfLayoutNode layout = af_layout_node_default(3U);
        project.layout_data.safe_area =
            (AfInsets){.left = 24.0F, .top = 12.0F, .right = 24.0F, .bottom = 12.0F};
        project.layout_data.reference_size = project.canvas_size;
        project.layout_data.target_size = project.canvas_size;
        layout.anchor_min = (AfVec2){0.5F, 0.5F};
        layout.anchor_max = layout.anchor_min;
        layout.pivot = (AfVec2){0.5F, 0.5F};
        layout.width = (AfLayoutValue){.mode = AF_LAYOUT_VALUE_PERCENT, .value = 0.25F};
        layout.height = (AfLayoutValue){.mode = AF_LAYOUT_VALUE_FIXED, .value = 100.0F};
        layout.minimum_size = (AfVec2){80.0F, 50.0F};
        layout.maximum_size = (AfVec2){800.0F, 400.0F};
        CHECK(af_project_set_layout(&project, &layout) == AF_RESULT_SUCCESS);
    }
    {
        AfStyle *style = af_project_find_node_style(&project, 3U);
        CHECK(style != NULL);
        style->fill = (AfPaint){.type = AF_PAINT_LINEAR_GRADIENT,
                                .start = {0.0F, 0.0F},
                                .end = {120.0F, 0.0F},
                                .transform = {.m00 = 1.0F, .m11 = 1.0F},
                                .stops = {{.offset = 0.0F, .color = {1.0F, 0.0F, 0.0F, 0.8F}},
                                          {.offset = 1.0F, .color = {0.0F, 0.0F, 1.0F, 0.4F}}},
                                .stop_count = 2U};
        style->stroke.enabled = true;
        style->stroke.width = 3.0F;
        style->stroke.end_width = 6.0F;
        style->stroke.cap = AF_STROKE_CAP_ROUND;
        style->stroke.join = AF_STROKE_JOIN_BEVEL;
        style->stroke.dashes[0] = 8.0F;
        style->stroke.dashes[1] = 4.0F;
        style->stroke.dash_count = 2U;
    }
    CHECK(af_project_serialize(&project, &allocator, &text) == AF_RESULT_SUCCESS);
    CHECK(af_test_project_contains(text.data, text.size, "arcflow_project 10"));
    CHECK(af_test_project_contains(text.data, text.size, "property path_control_in"));
    CHECK(af_test_project_contains(text.data, text.size, "element 1"));
    CHECK(af_test_project_contains(text.data, text.size, "particle "));
    CHECK(af_test_project_contains(text.data, text.size, "vfx_emitter "));
    CHECK(af_test_project_contains(text.data, text.size, "vfx_lod "));
    CHECK(af_test_project_contains(text.data, text.size, "name Editor Only Name"));
    CHECK(af_test_project_contains(text.data, text.size, "interpolation cubic_bezier"));
    CHECK(af_project_deserialize(text.data, text.size, &allocator, &loaded) == AF_RESULT_SUCCESS);
    CHECK(loaded.scene.count == 3U && loaded.canvas_size.x == 1920.0F);
    CHECK(strcmp(af_project_name(&loaded, 2U), "Editor Only Name") == 0);
    CHECK(loaded.clip_count == 1U && loaded.track_count == 4U && loaded.keyframe_count == 8U);
    CHECK(loaded.path_data.path_count == 1U && loaded.path_data.command_count == 5U);
    CHECK(af_project_find_path(&loaded, 500U)->geometry_generation == 7U);
    CHECK(af_project_find_node_style(&loaded, 3U)->fill.type == AF_PAINT_LINEAR_GRADIENT);
    CHECK(af_project_find_node_style(&loaded, 3U)->stroke.dash_count == 2U);
    CHECK(loaded.layout_data.count == 1U);
    CHECK(loaded.particle_data.count == 1U);
    CHECK(loaded.particle_data.emitters[0].output == AF_PARTICLE_OUTPUT_RIBBON);
    CHECK_NEAR(loaded.particle_data.emitters[0].turbulence_strength, 175.0F, 0.0001F);
    CHECK(loaded.particle_data.emitters[0].collision_plane_enabled);
    CHECK(loaded.particle_data.emitters[0].sub_emit_death_count == 1U);
    CHECK_NEAR(af_layout_find_const(&loaded.layout_data, 3U)->width.value, 0.25F, 0.0001F);
    CHECK_NEAR(af_scene_find(&loaded.scene, 2U)->local_transform.rotation, 0.25F, 0.0001F);
    {
        AfSceneNode animated_nodes[3];
        AfPath animated_path;
        AfPathCommand animated_commands[5];
        AfPathData animated_path_data = {.paths = &animated_path,
                                         .path_count = 1U,
                                         .path_capacity = 1U,
                                         .commands = animated_commands,
                                         .command_count = 5U,
                                         .command_capacity = 5U};
        AfScene animated = {
            .nodes = animated_nodes, .count = 3U, .capacity = 3U, .paths = &animated_path_data};
        memcpy(animated_nodes, loaded.scene.nodes, sizeof(animated_nodes));
        animated_path = loaded.path_data.paths[0];
        memcpy(animated_commands, loaded.path_data.commands, sizeof(animated_commands));
        af_allocator_begin_frame(&allocator);
        CHECK(af_project_animation_apply(&loaded, 100U, 0.5F, &animated) == AF_RESULT_SUCCESS);
        CHECK(af_allocator_end_frame(&allocator) == 0U);
        CHECK_NEAR(af_scene_find(&animated, 2U)->local_transform.position.x, 62.0F, 0.0001F);
        CHECK_NEAR(af_scene_find(&animated, 2U)->local_transform.position.y, 59.0F, 0.0001F);
        CHECK_NEAR(animated_commands[1].point.x, 140.0F, 0.0001F);
        CHECK_NEAR(animated_commands[1].point.y, 20.0F, 0.0001F);
        CHECK_NEAR(animated_commands[1].control1.x, 40.0F, 0.0001F);
        CHECK_NEAR(animated_commands[1].control1.y, 90.0F, 0.0001F);
        CHECK_NEAR(animated_commands[1].control2.x, 100.0F, 0.0001F);
        CHECK_NEAR(animated_commands[1].control2.y, -30.0F, 0.0001F);
    }
    CHECK(af_project_save_file(&loaded, path, &allocator) == AF_RESULT_SUCCESS);
    CHECK(af_project_load_file(path, &allocator, &reopened) == AF_RESULT_SUCCESS);
    CHECK(strcmp(af_project_name(&reopened, 1U), "Root Group") == 0);
    CHECK(remove(path) == 0);
    CHECK(af_project_compile(&loaded, &allocator, &package) == AF_RESULT_SUCCESS);
    CHECK(af_package_validate(package.data, package.size) == AF_RESULT_SUCCESS);
    {
        const uint64_t tracks_offset = af_test_read_u64(package.data + AF_PACKAGE_HEADER_SIZE +
                                                        2U * AF_PACKAGE_SECTION_SIZE + 8U);
        const uint64_t element_offset = tracks_offset + 32U + 28U;
        const uint32_t saved_element = af_test_read_u32(package.data + element_offset);
        af_test_write_u32(package.data + element_offset, UINT32_MAX);
        af_test_rechecksum(&package);
        CHECK(af_package_validate(package.data, package.size) == AF_RESULT_INVALID_PACKAGE);
        af_test_write_u32(package.data + element_offset, saved_element);
        af_test_rechecksum(&package);
        CHECK(af_package_validate(package.data, package.size) == AF_RESULT_SUCCESS);
    }
    {
        const uint64_t particles_offset = af_test_read_u64(package.data + AF_PACKAGE_HEADER_SIZE +
                                                           11U * AF_PACKAGE_SECTION_SIZE + 8U);
        const uint64_t record_offset = particles_offset + 64U;
        const uint64_t saved_sub_emitter = af_test_read_u64(package.data + record_offset + 328U);
        af_test_write_u64(package.data + record_offset + 328U, UINT64_C(999999));
        af_test_rechecksum(&package);
        CHECK(af_package_validate(package.data, package.size) == AF_RESULT_INVALID_PACKAGE);
        af_test_write_u64(package.data + record_offset + 328U, saved_sub_emitter);
        af_test_rechecksum(&package);
        CHECK(af_package_validate(package.data, package.size) == AF_RESULT_SUCCESS);
    }
    CHECK(!af_test_project_contains(package.data, package.size, "Editor Only Name"));
    CHECK(af_package_decode(package.data, package.size, &allocator, &runtime) == AF_RESULT_SUCCESS);
    CHECK(runtime.scene.count == loaded.scene.count && runtime.animation.clip_count == 1U);
    CHECK(runtime.animation.track_count == 4U && runtime.animation.keyframe_count == 8U);
    CHECK(runtime.animation.tracks[1].element_index == 1U);
    CHECK(runtime.path_data.path_count == 1U && runtime.path_data.command_count == 5U);
    CHECK(runtime.style_data.count == loaded.style_data.count);
    CHECK(runtime.layout_data.count == 1U);
    CHECK(runtime.particle_data.count == 1U);
    CHECK(runtime.scene.particles == &runtime.particle_data);
    CHECK(runtime.particle_data.emitters[0].output == AF_PARTICLE_OUTPUT_RIBBON);
    CHECK_NEAR(runtime.particle_data.emitters[0].vortex_force, 90.0F, 0.0001F);
    CHECK(af_style_find_node_const(&runtime.style_data, 3U)->stroke.end_width == 6.0F);
    {
        AfSceneNode layered_nodes[3];
        AfPath layered_path = runtime.path_data.paths[0];
        AfPathCommand layered_commands[5];
        AfPathData layered_paths = {.paths = &layered_path,
                                    .path_count = 1U,
                                    .path_capacity = 1U,
                                    .commands = layered_commands,
                                    .command_count = 5U,
                                    .command_capacity = 5U};
        AfScene layered_scene = {
            .nodes = layered_nodes, .count = 3U, .capacity = 3U, .paths = &layered_paths};
        AfAnimationLayer layer = {
            .clip_id = 100U, .time = 1.0F, .weight = 0.5F, .order = 0, .enabled = true};
        AfAdvancedAnimationData advanced = {
            .layers = &layer, .layer_count = 1U, .layer_capacity = 1U};
        memcpy(layered_nodes, runtime.scene.nodes, sizeof(layered_nodes));
        memcpy(layered_commands, runtime.path_data.commands, sizeof(layered_commands));
        CHECK(af_advanced_apply_layers(&advanced, &runtime.animation, &layered_scene) ==
              AF_RESULT_SUCCESS);
        CHECK_NEAR(layered_commands[1].point.x, 140.0F, 0.0001F);
        CHECK_NEAR(layered_commands[1].control1.y, 90.0F, 0.0001F);
        CHECK_NEAR(layered_commands[1].control2.y, -30.0F, 0.0001F);
    }
    af_loaded_package_destroy(&allocator, &runtime);
    {
        AfRuntime *public_runtime = NULL;
        AfPackageHandle public_package = 0U;
        AfInstanceHandle first_instance = 0U;
        AfInstanceHandle second_instance = 0U;
        const AfScene *first_scene = NULL;
        const AfScene *second_scene = NULL;
        const AfRuntimeCreateInfo create_info = {.frames_in_flight = 2U};
        CHECK(af_runtime_create(&create_info, &public_runtime) == AF_RESULT_SUCCESS);
        CHECK(af_package_load(public_runtime, package.data, package.size, &public_package) ==
              AF_RESULT_SUCCESS);
        CHECK(af_instance_create(public_runtime, public_package, &first_instance) ==
              AF_RESULT_SUCCESS);
        CHECK(af_instance_create(public_runtime, public_package, &second_instance) ==
              AF_RESULT_SUCCESS);
        CHECK(af_instance_seek(public_runtime, first_instance, 1.0F) == AF_RESULT_SUCCESS);
        CHECK(af_instance_seek(public_runtime, second_instance, 0.0F) == AF_RESULT_SUCCESS);
        CHECK(af_instance_get_scene(public_runtime, first_instance, &first_scene) ==
              AF_RESULT_SUCCESS);
        CHECK(af_instance_get_scene(public_runtime, second_instance, &second_scene) ==
              AF_RESULT_SUCCESS);
        CHECK(first_scene->paths != second_scene->paths);
        CHECK_NEAR(first_scene->paths->commands[1].point.x, 160.0F, 0.0001F);
        CHECK_NEAR(second_scene->paths->commands[1].point.x, 120.0F, 0.0001F);
        CHECK_NEAR(first_scene->paths->commands[1].control1.y, 100.0F, 0.0001F);
        CHECK_NEAR(second_scene->paths->commands[1].control2.y, -40.0F, 0.0001F);
        CHECK(af_instance_destroy(public_runtime, first_instance) == AF_RESULT_SUCCESS);
        CHECK(af_instance_destroy(public_runtime, second_instance) == AF_RESULT_SUCCESS);
        CHECK(af_package_destroy(public_runtime, public_package) == AF_RESULT_SUCCESS);
        af_runtime_destroy(public_runtime);
    }
    written = snprintf(with_unknown, sizeof(with_unknown), "unknown_editor_field retained\n%.*s",
                       (int)text.size, (const char *)text.data);
    CHECK(written > 0 && (size_t)written < sizeof(with_unknown));
    af_project_destroy(&reopened);
    CHECK(af_project_deserialize(with_unknown, (size_t)written, &allocator, &reopened) ==
          AF_RESULT_INVALID_PACKAGE);
    written =
        snprintf(with_unknown, sizeof(with_unknown), "arcflow_project 3\nunknown_field 7\n%.*s",
                 (int)(text.size - strlen("arcflow_project 3\n")),
                 (const char *)text.data + strlen("arcflow_project 3\n"));
    CHECK(written > 0 && (size_t)written < sizeof(with_unknown));
    CHECK(af_project_deserialize(with_unknown, (size_t)written, &allocator, &reopened) ==
          AF_RESULT_SUCCESS);
    af_project_destroy(&reopened);
    CHECK(text.size < sizeof(invalid_parent));
    memcpy(invalid_parent, text.data, text.size);
    invalid_parent[text.size] = '\0';
    {
        char *parent = strstr(invalid_parent, "parent 1");
        CHECK(parent != NULL);
        parent[7] = '9';
    }
    CHECK(af_project_deserialize(invalid_parent, text.size, &allocator, &reopened) ==
          AF_RESULT_INVALID_PACKAGE);
    CHECK(text.size < sizeof(unsupported));
    memcpy(unsupported, text.data, text.size);
    unsupported[text.size] = '\0';
    unsupported[strlen("arcflow_project ")] = '0';
    CHECK(af_project_deserialize(unsupported, text.size, &allocator, &reopened) ==
          AF_RESULT_UNSUPPORTED_VERSION);
    af_package_blob_destroy(&allocator, &package);
    af_package_blob_destroy(&allocator, &text);
    af_project_destroy(&reopened);
    af_project_destroy(&loaded);
    af_project_destroy(&project);
    return true;
}
