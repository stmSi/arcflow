static bool af_test_package(void) {
    AfTestFixture fixture;
    AfAllocator allocator;
    AfPackageSource source;
    AfSceneNode package_nodes[4];
    AfScene package_scene = {0};
    AfPath package_path = {.id = 40U,
                           .node_id = 4U,
                           .command_count = 4U,
                           .fill_rule = AF_PATH_FILL_EVEN_ODD,
                           .geometry_generation = 3U};
    AfPathCommand package_commands[4] = {{.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}},
                                         {.type = AF_PATH_LINE, .point = {100.0F, 0.0F}},
                                         {.type = AF_PATH_LINE, .point = {50.0F, 100.0F}},
                                         {.type = AF_PATH_CLOSE}};
    AfPathData package_paths = {.paths = &package_path,
                                .path_count = 1U,
                                .path_capacity = 1U,
                                .commands = package_commands,
                                .command_count = 4U,
                                .command_capacity = 4U};
    AfPackageBlob first = {0};
    AfPackageBlob second = {0};
    AfLoadedPackage decoded = {0};
    uint8_t saved[16];
    uint64_t nodes_offset = 0U;
    uint64_t paths_offset = 0U;
    uint64_t commands_offset = 0U;
    af_test_fixture_init(&fixture);
    af_allocator_init_default(&allocator);
    memcpy(package_nodes, fixture.nodes, sizeof(fixture.nodes));
    package_nodes[3] = (AfSceneNode){.id = 4U,
                                     .parent_id = AF_INVALID_NODE_ID,
                                     .type = AF_NODE_PATH,
                                     .local_transform = {.scale = {1.0F, 1.0F}},
                                     .bounds = {100.0F, 100.0F},
                                     .fill = {0.7F, 0.3F, 0.9F, 1.0F},
                                     .opacity = 1.0F,
                                     .visible = true};
    package_scene =
        (AfScene){.nodes = package_nodes, .count = 4U, .capacity = 4U, .paths = &package_paths};
    source = (AfPackageSource){.scene = &package_scene, .animation = &fixture.animation};
    CHECK(af_package_write(&source, &allocator, &first) == AF_RESULT_SUCCESS);
    CHECK(af_package_write(&source, &allocator, &second) == AF_RESULT_SUCCESS);
    CHECK(first.size == second.size && memcmp(first.data, second.data, first.size) == 0);
    CHECK(af_package_validate(first.data, first.size) == AF_RESULT_SUCCESS);
    CHECK(af_package_decode(first.data, first.size, &allocator, &decoded) == AF_RESULT_SUCCESS);
    CHECK(decoded.scene.count == package_scene.count);
    CHECK(decoded.animation.keyframe_count == fixture.animation.keyframe_count);
    CHECK(decoded.path_data.path_count == 1U && decoded.path_data.command_count == 4U);
    CHECK(af_scene_find(&decoded.scene, 2U) != NULL);
    af_loaded_package_destroy(&allocator, &decoded);
    memcpy(saved, first.data, sizeof(saved));
    first.data[0] = 'X';
    CHECK(af_package_validate(first.data, first.size) == AF_RESULT_INVALID_PACKAGE);
    memcpy(first.data, saved, sizeof(saved));
    CHECK(af_package_validate(first.data, 12U) == AF_RESULT_INVALID_PACKAGE);
    af_test_write_u32(first.data + 8U, AF_PACKAGE_FORMAT_VERSION + 1U);
    CHECK(af_package_validate(first.data, first.size) == AF_RESULT_UNSUPPORTED_VERSION);
    af_test_write_u32(first.data + 8U, AF_PACKAGE_FORMAT_VERSION);
    af_test_write_u32(first.data + 12U, AF_RUNTIME_VERSION + 1U);
    CHECK(af_package_validate(first.data, first.size) == AF_RESULT_UNSUPPORTED_VERSION);
    af_test_write_u32(first.data + 12U, AF_RUNTIME_VERSION);
    af_test_write_u32(first.data + 44U, 1U);
    CHECK(af_package_validate(first.data, first.size) == AF_RESULT_UNSUPPORTED_FEATURE);
    af_test_write_u32(first.data + 44U, 0U);
    af_test_write_u64(first.data + 24U, UINT64_MAX);
    CHECK(af_package_validate(first.data, first.size) == AF_RESULT_INVALID_PACKAGE);
    af_test_write_u64(first.data + 24U, first.size);
    nodes_offset = af_test_read_u64(first.data + AF_PACKAGE_HEADER_SIZE + 8U);
    af_test_write_u64(first.data + AF_PACKAGE_HEADER_SIZE + 8U, first.size + 8U);
    af_test_rechecksum(&first);
    CHECK(af_package_validate(first.data, first.size) == AF_RESULT_INVALID_PACKAGE);
    af_test_write_u64(first.data + AF_PACKAGE_HEADER_SIZE + 8U, nodes_offset + 1U);
    af_test_rechecksum(&first);
    CHECK(af_package_validate(first.data, first.size) == AF_RESULT_INVALID_PACKAGE);
    af_test_write_u64(first.data + AF_PACKAGE_HEADER_SIZE + 8U, nodes_offset);
    af_test_write_u64(first.data + AF_PACKAGE_HEADER_SIZE + AF_PACKAGE_SECTION_SIZE + 8U,
                      nodes_offset);
    af_test_rechecksum(&first);
    CHECK(af_package_validate(first.data, first.size) == AF_RESULT_INVALID_PACKAGE);
    af_test_write_u64(
        first.data + AF_PACKAGE_HEADER_SIZE + AF_PACKAGE_SECTION_SIZE + 8U,
        af_test_read_u64(second.data + AF_PACKAGE_HEADER_SIZE + AF_PACKAGE_SECTION_SIZE + 8U));
    af_test_write_u32(first.data + AF_PACKAGE_HEADER_SIZE + 28U, AF_MAX_NODES + 1U);
    af_test_rechecksum(&first);
    CHECK(af_package_validate(first.data, first.size) == AF_RESULT_INVALID_PACKAGE);
    af_test_write_u32(first.data + AF_PACKAGE_HEADER_SIZE + 28U, package_scene.count);
    af_test_write_u64(first.data + nodes_offset + 8U, UINT64_C(999999));
    af_test_rechecksum(&first);
    CHECK(af_package_validate(first.data, first.size) == AF_RESULT_INVALID_PACKAGE);
    memcpy(first.data, second.data, second.size);
    paths_offset =
        af_test_read_u64(first.data + AF_PACKAGE_HEADER_SIZE + 5U * AF_PACKAGE_SECTION_SIZE + 8U);
    commands_offset =
        af_test_read_u64(first.data + AF_PACKAGE_HEADER_SIZE + 6U * AF_PACKAGE_SECTION_SIZE + 8U);
    af_test_write_u32(first.data + paths_offset + 20U, UINT32_MAX);
    af_test_rechecksum(&first);
    CHECK(af_package_validate(first.data, first.size) == AF_RESULT_INVALID_PACKAGE);
    memcpy(first.data, second.data, second.size);
    af_test_write_u32(first.data + commands_offset, AF_PATH_LINE);
    af_test_rechecksum(&first);
    CHECK(af_package_validate(first.data, first.size) == AF_RESULT_INVALID_PACKAGE);
    memcpy(first.data, second.data, second.size);
    af_test_write_u64(first.data + paths_offset + 8U, UINT64_C(999999));
    af_test_rechecksum(&first);
    CHECK(af_package_validate(first.data, first.size) == AF_RESULT_INVALID_PACKAGE);
    memcpy(first.data, second.data, second.size);
    af_test_write_u32(first.data + commands_offset + 8U, UINT32_C(0x7f800000));
    af_test_rechecksum(&first);
    CHECK(af_package_validate(first.data, first.size) == AF_RESULT_INVALID_PACKAGE);
    memcpy(first.data, second.data, second.size);
    CHECK(af_package_validate(first.data, first.size) == AF_RESULT_SUCCESS);
    af_package_blob_destroy(&allocator, &second);
    af_package_blob_destroy(&allocator, &first);
    return true;
}

static bool af_test_runtime(void) {
    AfTestFixture fixture;
    AfAllocator allocator;
    AfPackageSource source;
    AfPackageBlob blob = {0};
    AfRuntime *runtime = NULL;
    AfPackageHandle package = 0U;
    AfInstanceHandle instance = 0U;
    AfRuntimeCreateInfo create_info = {.frames_in_flight = 2U};
    AfParticleEmitter emitter;
    AfParticleData particles;
    af_test_fixture_init(&fixture);
    af_allocator_init_default(&allocator);
    emitter = af_particle_emitter_default(900U, 2U);
    emitter.event_burst_count = 8U;
    particles = (AfParticleData){
        .emitters = &emitter, .count = 1U, .capacity = 1U, .lod = af_vfx_lod_default()};
    fixture.scene.particles = &particles;
    source = (AfPackageSource){.scene = &fixture.scene, .animation = &fixture.animation};
    CHECK(af_package_write(&source, &allocator, &blob) == AF_RESULT_SUCCESS);
    CHECK(af_runtime_create(&create_info, &runtime) == AF_RESULT_SUCCESS);
    CHECK(af_package_load(runtime, blob.data, blob.size, &package) == AF_RESULT_SUCCESS);
    CHECK(af_instance_create(runtime, package, &instance) == AF_RESULT_SUCCESS);
    CHECK(af_instance_set_vfx_quality(runtime, instance, AF_VFX_QUALITY_LOW, 1500.0F) ==
          AF_RESULT_SUCCESS);
    CHECK(af_instance_set_vfx_quality(runtime, instance, (AfVfxQuality)99, 0.0F) ==
          AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_instance_trigger_vfx(runtime, instance, emitter.id, 32U) == AF_RESULT_SUCCESS);
    CHECK(af_instance_trigger_vfx(runtime, instance, emitter.id, emitter.capacity + 1U) ==
          AF_RESULT_LIMIT_EXCEEDED);
    CHECK(af_instance_trigger_vfx(runtime, instance, 999U, 1U) == AF_RESULT_NOT_FOUND);
    CHECK(af_instance_play(runtime, instance) == AF_RESULT_SUCCESS);
    CHECK(af_instance_update(runtime, instance, 0.25F) == AF_RESULT_SUCCESS);
    CHECK(af_runtime_frame_allocation_count(runtime) == 0U);
    CHECK(af_instance_set_direction(runtime, instance, AF_PLAYBACK_REVERSE) == AF_RESULT_SUCCESS);
    CHECK(af_instance_set_playback_speed(runtime, instance, 2.0F) == AF_RESULT_SUCCESS);
    CHECK(af_instance_update(runtime, instance, 0.1F) == AF_RESULT_SUCCESS);
    CHECK(af_runtime_frame_allocation_count(runtime) == 0U);
    {
        AfRuntimeEvent event;
        CHECK(af_instance_queue_animation_event(runtime, instance, 1234U, 2U, 1.0) ==
              AF_RESULT_SUCCESS);
        CHECK(af_instance_poll_event(runtime, instance, &event) == AF_RESULT_SUCCESS);
        CHECK(event.type == AF_EVENT_ANIMATION && event.name_id == 1234U);
        CHECK(af_instance_poll_event(runtime, instance, &event) == AF_RESULT_NOT_FOUND);
    }
    CHECK(af_package_destroy(runtime, package) == AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_instance_destroy(runtime, instance) == AF_RESULT_SUCCESS);
    CHECK(af_instance_update(runtime, instance, 0.1F) == AF_RESULT_STALE_HANDLE);
    CHECK(af_package_destroy(runtime, package) == AF_RESULT_SUCCESS);
    CHECK(af_package_destroy(runtime, package) == AF_RESULT_STALE_HANDLE);
    af_runtime_destroy(runtime);
    af_package_blob_destroy(&allocator, &blob);
    return true;
}

static bool af_test_project_contains(const uint8_t *data, size_t size, const char *text) {
    const size_t text_size = strlen(text);
    size_t index = 0U;
    if (text_size == 0U || text_size > size) {
        return false;
    }
    for (index = 0U; index <= size - text_size; ++index) {
        if (memcmp(data + index, text, text_size) == 0) {
            return true;
        }
    }
    return false;
}
