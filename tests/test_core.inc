static void af_test_fixture_init(AfTestFixture *fixture) {
    *fixture = (AfTestFixture){
        .nodes = {{.id = 1U,
                   .parent_id = AF_INVALID_NODE_ID,
                   .type = AF_NODE_GROUP,
                   .local_transform = {.position = {10.0F, 20.0F}, .scale = {1.0F, 1.0F}},
                   .opacity = 0.8F,
                   .visible = true},
                  {.id = 2U,
                   .parent_id = 1U,
                   .type = AF_NODE_RECTANGLE,
                   .local_transform = {.position = {5.0F, 7.0F}, .scale = {1.0F, 1.0F}},
                   .bounds = {100.0F, 60.0F},
                   .fill = {0.2F, 0.4F, 0.6F, 1.0F},
                   .opacity = 0.5F,
                   .visible = true},
                  {.id = 3U,
                   .parent_id = AF_INVALID_NODE_ID,
                   .type = AF_NODE_ELLIPSE,
                   .local_transform = {.position = {30.0F, 40.0F}, .scale = {1.0F, 1.0F}},
                   .bounds = {50.0F, 50.0F},
                   .fill = {1.0F, 0.2F, 0.1F, 1.0F},
                   .opacity = 1.0F,
                   .layer_order = 2,
                   .visible = true}},
        .clips =
            {{.id = 10U, .duration = 2.0F, .track_start = 0U, .track_count = 2U, .loop = true}},
        .tracks = {{.id = 11U,
                    .node_id = 2U,
                    .property = AF_PROPERTY_POSITION,
                    .keyframe_start = 0U,
                    .keyframe_count = 3U},
                   {.id = 12U,
                    .node_id = 3U,
                    .property = AF_PROPERTY_OPACITY,
                    .keyframe_start = 3U,
                    .keyframe_count = 2U}},
        .keys = {{.time = 0.0F, .value = {5.0F, 7.0F}, .interpolation = AF_INTERPOLATION_LINEAR},
                 {.time = 1.0F,
                  .value = {15.0F, 17.0F},
                  .interpolation = AF_INTERPOLATION_CUBIC_BEZIER,
                  .easing = {0.42F, 0.0F, 0.58F, 1.0F}},
                 {.time = 2.0F, .value = {5.0F, 7.0F}, .interpolation = AF_INTERPOLATION_LINEAR},
                 {.time = 0.0F, .value = {1.0F}, .interpolation = AF_INTERPOLATION_STEP},
                 {.time = 1.0F, .value = {0.25F}, .interpolation = AF_INTERPOLATION_LINEAR}}};
    fixture->scene = (AfScene){.nodes = fixture->nodes, .count = 3U, .capacity = 3U};
    fixture->animation = (AfAnimationData){.clips = fixture->clips,
                                           .clip_count = 1U,
                                           .tracks = fixture->tracks,
                                           .track_count = 2U,
                                           .keyframes = fixture->keys,
                                           .keyframe_count = 5U};
}

static bool af_test_particles(void) {
    AfTestFixture fixture;
    AfParticleEmitter emitters[2];
    AfParticleData data = {.emitters = emitters, .count = 1U, .capacity = 2U};
    AfParticleSpawnSample first;
    AfParticleSpawnSample second;
    AfVec2 position = {0.0F, 3.0F};
    AfVec2 velocity = {2.0F, -5.0F};
    float depth = -1.0F;
    float depth_velocity = -2.0F;
    af_test_fixture_init(&fixture);
    emitters[0] = af_particle_emitter_default(700U, 2U);
    emitters[0].type = AF_PARTICLE_EMITTER_RECTANGLE;
    emitters[0].seed = 0x12345678U;
    emitters[0].output = AF_PARTICLE_OUTPUT_TRAIL;
    emitters[0].turbulence_strength = 120.0F;
    emitters[0].radial_force = 40.0F;
    emitters[0].vortex_force = 60.0F;
    emitters[0].attractor_position = (AfVec2){20.0F, 10.0F};
    emitters[0].attractor_strength = 90.0F;
    emitters[0].attractor_radius = 100.0F;
    emitters[0].repulsor_position = (AfVec2){-20.0F, -10.0F};
    emitters[0].repulsor_strength = 70.0F;
    emitters[0].repulsor_radius = 80.0F;
    emitters[0].vector_field_direction = (AfVec2){1.0F, 0.0F};
    emitters[0].vector_field_strength = 10.0F;
    emitters[0].collision_plane_enabled = true;
    emitters[0].collision_plane_normal = (AfVec2){0.0F, 1.0F};
    emitters[0].collision_plane_offset = 5.0F;
    emitters[0].depth_collision_enabled = true;
    data.lod = af_vfx_lod_default();
    CHECK(af_particle_data_validate(&data, &fixture.scene) == AF_RESULT_SUCCESS);
    CHECK(isfinite(af_particle_force_sample(&emitters[0], (AfVec2){3.0F, 4.0F}, 1.0F).x));
    CHECK(af_particle_collide_sample(&emitters[0], (AfVec2){0.0F, 7.0F}, &position, &velocity,
                                     &depth, &depth_velocity) == AF_RESULT_SUCCESS);
    CHECK_NEAR(position.y, 5.0F, 0.0001F);
    CHECK(depth == emitters[0].depth_min && depth_velocity >= 0.0F);
    data.lod.host_quality = 0.5F;
    data.lod.view_distance = 3000.0F;
    CHECK_NEAR(af_vfx_lod_factor(&data.lod), 0.25F, 0.0001F);
    data.lod = af_vfx_lod_default();
    CHECK(af_particle_total_capacity(&data) == emitters[0].capacity);
    CHECK(af_particle_spawn_sample(&emitters[0], 42U, &first) == AF_RESULT_SUCCESS);
    CHECK(af_particle_spawn_sample(&emitters[0], 42U, &second) == AF_RESULT_SUCCESS);
    CHECK(memcmp(&first, &second, sizeof(first)) == 0);
    CHECK(af_particle_random_u32(emitters[0].seed, emitters[0].id, 42U, 0U) !=
          af_particle_random_u32(emitters[0].seed, emitters[0].id, 43U, 0U));
    emitters[1] = af_particle_emitter_default(701U, 3U);
    emitters[0].sub_emitter_id = emitters[1].id;
    emitters[0].sub_emit_birth_count = 2U;
    emitters[0].sub_emit_death_count = 1U;
    emitters[0].max_recursion_depth = AF_MAX_PARTICLE_RECURSION_DEPTH;
    data.count = 2U;
    CHECK(af_particle_data_validate(&data, &fixture.scene) == AF_RESULT_SUCCESS);
    emitters[0].max_recursion_depth = AF_MAX_PARTICLE_RECURSION_DEPTH + 1U;
    CHECK(af_particle_data_validate(&data, &fixture.scene) == AF_RESULT_INVALID_ARGUMENT);
    emitters[0].max_recursion_depth = AF_MAX_PARTICLE_RECURSION_DEPTH;
    emitters[0].capacity = AF_MAX_PARTICLES;
    emitters[1].capacity = 1U;
    CHECK(af_particle_total_capacity(&data) == AF_MAX_PARTICLES + 1U);
    CHECK(af_particle_data_validate(&data, &fixture.scene) == AF_RESULT_INVALID_ARGUMENT);
    emitters[0].capacity = 1024U;
    emitters[1].type = AF_PARTICLE_EMITTER_VECTOR_PATH;
    emitters[1].path_id = 99U;
    CHECK(af_particle_data_validate(&data, &fixture.scene) == AF_RESULT_INVALID_ARGUMENT);
    emitters[1].type = AF_PARTICLE_EMITTER_POINT;
    emitters[1].path_id = 0U;
    emitters[1].node_id = emitters[0].node_id;
    CHECK(af_particle_data_validate(&data, &fixture.scene) == AF_RESULT_INVALID_ARGUMENT);
    return true;
}

static bool af_test_core(void) {
    alignas(64) uint8_t memory[512];
    AfArena arena;
    void *first = NULL;
    void *second = NULL;
    AfScratch scratch;
    size_t value = 0U;
    AfHandleSlot slots[2];
    AfHandlePool pool;
    uint64_t first_handle = 0U;
    uint64_t second_handle = 0U;
    uint32_t index = 0U;
    AfAllocator allocator;
    AfArray array;
    int number = 0;
    af_arena_init(&arena, memory, sizeof(memory));
    CHECK(af_arena_allocate(&arena, 7U, 8U, &first) == AF_RESULT_SUCCESS);
    CHECK(((uintptr_t)first & 7U) == 0U);
    scratch = af_scratch_begin(&arena);
    CHECK(af_arena_allocate(&arena, 32U, 32U, &second) == AF_RESULT_SUCCESS);
    CHECK(((uintptr_t)second & 31U) == 0U);
    af_scratch_end(scratch);
    CHECK(arena.offset == scratch.saved_offset);
    CHECK(af_arena_allocate(&arena, 1024U, 8U, &second) == AF_RESULT_OUT_OF_MEMORY);
    CHECK(af_size_add_overflow(SIZE_MAX, 1U, &value));
    CHECK(af_size_mul_overflow(SIZE_MAX, 2U, &value));
    CHECK(!af_size_mul_overflow(16U, 8U, &value) && value == 128U);
    af_handle_pool_init(&pool, slots, 2U);
    CHECK(af_handle_allocate(&pool, &first_handle) == AF_RESULT_SUCCESS);
    CHECK(af_handle_validate(&pool, first_handle, &index) == AF_RESULT_SUCCESS && index == 0U);
    CHECK(af_handle_release(&pool, first_handle) == AF_RESULT_SUCCESS);
    CHECK(af_handle_validate(&pool, first_handle, NULL) == AF_RESULT_STALE_HANDLE);
    CHECK(af_handle_allocate(&pool, &second_handle) == AF_RESULT_SUCCESS);
    CHECK(second_handle != first_handle);
    af_allocator_init_default(&allocator);
    af_array_init(&array, sizeof(int), alignof(int), &allocator);
    for (number = 0; number < 32; ++number) {
        CHECK(af_array_push(&array, &number) == AF_RESULT_SUCCESS);
    }
    CHECK(array.count == 32U && *(int *)af_array_at(&array, 17U) == 17);
    CHECK(af_array_at(&array, 32U) == NULL);
    af_array_destroy(&array);
    CHECK(af_hash_string("Arcflow") == af_hash_string("Arcflow"));
    CHECK(af_hash_string("Arcflow") != af_hash_string("arcflow"));
    return true;
}

static bool af_test_file_limit(void) {
    const char *path = "arcflow_test_file_limit.bin";
    const uint8_t content[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    AfAllocator allocator;
    AfFileData file = {0};
    CHECK(af_file_write(path, content, sizeof(content)) == AF_RESULT_SUCCESS);
    af_allocator_init_default(&allocator);
    CHECK(af_file_read_limited(path, 4U, &allocator, &file) == AF_RESULT_LIMIT_EXCEEDED);
    CHECK(af_file_read_limited(path, 8U, &allocator, &file) == AF_RESULT_SUCCESS);
    CHECK(file.size == sizeof(content) && memcmp(file.data, content, sizeof(content)) == 0);
    af_file_data_destroy(&allocator, &file);
    CHECK(remove(path) == 0);
    return true;
}

static bool af_test_png_writer(void) {
    const char *path = "arcflow_test_image.png";
    const uint8_t bgra[8] = {30U, 20U, 10U, 255U, 60U, 50U, 40U, 128U};
    const uint8_t expected_signature[8] = {137U, 80U, 78U, 71U, 13U, 10U, 26U, 10U};
    const uint8_t expected_scanline[9] = {0U, 10U, 20U, 30U, 255U, 40U, 50U, 60U, 128U};
    uint8_t encoded[128] = {0};
    FILE *file = NULL;
    size_t encoded_size = 0U;
    CHECK(af_png_write_rgba8(path, bgra, 2U, 1U, 8U, AF_PIXEL_ORDER_BGRA) == AF_RESULT_SUCCESS);
    file = fopen(path, "rb");
    CHECK(file != NULL);
    encoded_size = fread(encoded, 1U, sizeof(encoded), file);
    CHECK(fclose(file) == 0);
    CHECK(encoded_size == 77U);
    CHECK(memcmp(encoded, expected_signature, sizeof(expected_signature)) == 0);
    CHECK(memcmp(encoded + 12U, "IHDR", 4U) == 0);
    CHECK(encoded[19] == 2U && encoded[23] == 1U);
    CHECK(memcmp(encoded + 48U, expected_scanline, sizeof(expected_scanline)) == 0);
    CHECK(af_png_write_rgba8(path, bgra, 2U, 1U, 7U, AF_PIXEL_ORDER_BGRA) ==
          AF_RESULT_INVALID_ARGUMENT);
    CHECK(remove(path) == 0);
    return true;
}

static bool af_test_math(void) {
    const AfAffine2 parent = af_affine_from_transform(
        (AfTransform2D){.position = {10.0F, 20.0F}, .rotation = 0.0F, .scale = {2.0F, 2.0F}});
    const AfAffine2 child = af_affine_from_transform(
        (AfTransform2D){.position = {5.0F, 3.0F}, .rotation = 0.0F, .scale = {1.0F, 1.0F}});
    const AfAffine2 world = af_affine_multiply(parent, child);
    AfAffine2 inverse;
    AfVec2 transformed = af_affine_transform_point(world, (AfVec2){1.0F, 2.0F});
    AfVec2 restored;
    const AfVec2 projected = af_canvas_project((AfVec2){100.0F, 50.0F}, (AfVec2){200.0F, 100.0F});
    CHECK_NEAR(transformed.x, 22.0F, 0.0001F);
    CHECK_NEAR(transformed.y, 30.0F, 0.0001F);
    CHECK(af_affine_inverse(world, &inverse));
    restored = af_affine_transform_point(inverse, transformed);
    CHECK_NEAR(restored.x, 1.0F, 0.0001F);
    CHECK_NEAR(restored.y, 2.0F, 0.0001F);
    CHECK_NEAR(projected.x, 0.0F, 0.0001F);
    CHECK_NEAR(projected.y, 0.0F, 0.0001F);
    CHECK(!af_affine_inverse((AfAffine2){0}, &inverse));
    return true;
}

static bool af_test_scene(void) {
    AfTestFixture fixture;
    AfSceneNode extra = {.id = 4U,
                         .parent_id = 2U,
                         .type = AF_NODE_ROUNDED_RECTANGLE,
                         .local_transform = {.scale = {1.0F, 1.0F}},
                         .bounds = {10.0F, 10.0F},
                         .opacity = 1.0F,
                         .visible = true};
    AfSceneNode storage[5];
    af_test_fixture_init(&fixture);
    memcpy(storage, fixture.nodes, sizeof(fixture.nodes));
    fixture.scene.nodes = storage;
    fixture.scene.capacity = 5U;
    CHECK(af_scene_validate(&fixture.scene) == AF_RESULT_SUCCESS);
    CHECK(af_scene_evaluate_world(&fixture.scene) == AF_RESULT_SUCCESS);
    CHECK_NEAR(af_scene_find(&fixture.scene, 2U)->world_transform.tx, 15.0F, 0.0001F);
    CHECK_NEAR(af_scene_find(&fixture.scene, 2U)->world_transform.ty, 27.0F, 0.0001F);
    CHECK_NEAR(af_scene_find(&fixture.scene, 2U)->world_opacity, 0.4F, 0.0001F);
    CHECK(af_scene_add_node(&fixture.scene, &extra) == AF_RESULT_SUCCESS);
    CHECK(af_scene_reparent(&fixture.scene, 1U, 4U) == AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_scene_reparent(&fixture.scene, 3U, 1U) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&fixture.scene, 3U)->parent_id == 1U);
    CHECK(af_scene_delete_node(&fixture.scene, 4U) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&fixture.scene, 4U) == NULL);
    CHECK(fixture.scene.nodes[0].layer_order <= fixture.scene.nodes[2].layer_order);
    return true;
}

static bool af_test_animation(void) {
    AfTestFixture fixture;
    float value[4] = {0};
    AfAnimationTrack empty_track = {.id = 20U,
                                    .node_id = 2U,
                                    .property = AF_PROPERTY_POSITION,
                                    .keyframe_start = 0U,
                                    .keyframe_count = 0U};
    AfPlayback playback = {.time = 1.8F, .speed = 2.0F, .direction = 1, .playing = true};
    AfKeyframe duplicate_keys[3] = {
        {.time = 0.0F, .value = {1.0F}, .interpolation = AF_INTERPOLATION_LINEAR},
        {.time = 0.0F, .value = {2.0F}, .interpolation = AF_INTERPOLATION_LINEAR},
        {.time = 1.0F, .value = {4.0F}, .interpolation = AF_INTERPOLATION_LINEAR}};
    AfAnimationTrack duplicate_track = {
        .id = 30U, .node_id = 2U, .property = AF_PROPERTY_OPACITY, .keyframe_count = 3U};
    AfAnimationData duplicate_animation = {.tracks = &duplicate_track,
                                           .track_count = 1U,
                                           .keyframes = duplicate_keys,
                                           .keyframe_count = 3U};
    AfSceneNode first_result[3];
    af_test_fixture_init(&fixture);
    CHECK(af_animation_validate(&fixture.animation, &fixture.scene) == AF_RESULT_SUCCESS);
    CHECK(af_animation_sample_track(&fixture.animation, &fixture.tracks[0], 0.5F, value) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR(value[0], 10.0F, 0.0001F);
    CHECK_NEAR(value[1], 12.0F, 0.0001F);
    CHECK(af_animation_sample_track(&fixture.animation, &fixture.tracks[0], 1.0F, value) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR(value[0], 15.0F, 0.0001F);
    CHECK(af_animation_sample_track(&fixture.animation, &fixture.tracks[1], 0.5F, value) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR(value[0], 1.0F, 0.0001F);
    CHECK(af_animation_sample_track(&fixture.animation, &empty_track, 0.0F, value) ==
          AF_RESULT_NOT_FOUND);
    CHECK(af_animation_sample_track(&duplicate_animation, &duplicate_track, 0.0F, value) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR(value[0], 2.0F, 0.0001F);
    CHECK(af_animation_apply(&fixture.animation, &fixture.clips[0], 0.5F, &fixture.scene) ==
          AF_RESULT_SUCCESS);
    memcpy(first_result, fixture.scene.nodes, sizeof(first_result));
    af_test_fixture_init(&fixture);
    CHECK(af_animation_apply(&fixture.animation, &fixture.clips[0], 0.5F, &fixture.scene) ==
          AF_RESULT_SUCCESS);
    CHECK(memcmp(first_result, fixture.scene.nodes, sizeof(first_result)) == 0);
    af_playback_update(&playback, &fixture.clips[0], 0.2F);
    CHECK_NEAR(playback.time, 0.2F, 0.0001F);
    playback.direction = -1;
    af_playback_update(&playback, &fixture.clips[0], 0.2F);
    CHECK_NEAR(playback.time, 1.8F, 0.0001F);
    af_playback_pause(&playback);
    CHECK(!playback.playing);
    af_playback_play(&playback);
    CHECK(playback.playing);
    af_playback_stop(&playback);
    CHECK(!playback.playing && playback.time == 0.0F);
    af_playback_seek(&playback, &fixture.clips[0], 9.0F);
    CHECK(playback.time == fixture.clips[0].duration);
    return true;
}

static bool af_test_paths(void) {
    AfPath paths[3] = {{.id = 1U,
                        .node_id = 10U,
                        .command_start = 0U,
                        .command_count = 5U,
                        .fill_rule = AF_PATH_FILL_NON_ZERO,
                        .geometry_generation = 1U},
                       {.id = 2U,
                        .node_id = 11U,
                        .command_start = 5U,
                        .command_count = 10U,
                        .fill_rule = AF_PATH_FILL_EVEN_ODD,
                        .geometry_generation = 1U},
                       {.id = 3U,
                        .node_id = 12U,
                        .command_start = 15U,
                        .command_count = 5U,
                        .fill_rule = AF_PATH_FILL_EVEN_ODD,
                        .geometry_generation = 1U}};
    AfPathCommand commands[20] = {{.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}},
                                  {.type = AF_PATH_LINE, .point = {100.0F, 0.0F}},
                                  {.type = AF_PATH_LINE, .point = {100.0F, 100.0F}},
                                  {.type = AF_PATH_LINE, .point = {0.0F, 100.0F}},
                                  {.type = AF_PATH_CLOSE},
                                  {.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}},
                                  {.type = AF_PATH_LINE, .point = {100.0F, 0.0F}},
                                  {.type = AF_PATH_LINE, .point = {100.0F, 100.0F}},
                                  {.type = AF_PATH_LINE, .point = {0.0F, 100.0F}},
                                  {.type = AF_PATH_CLOSE},
                                  {.type = AF_PATH_MOVE, .point = {25.0F, 25.0F}},
                                  {.type = AF_PATH_LINE, .point = {75.0F, 25.0F}},
                                  {.type = AF_PATH_LINE, .point = {75.0F, 75.0F}},
                                  {.type = AF_PATH_LINE, .point = {25.0F, 75.0F}},
                                  {.type = AF_PATH_CLOSE},
                                  {.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}},
                                  {.type = AF_PATH_LINE, .point = {100.0F, 100.0F}},
                                  {.type = AF_PATH_LINE, .point = {0.0F, 100.0F}},
                                  {.type = AF_PATH_LINE, .point = {100.0F, 0.0F}},
                                  {.type = AF_PATH_CLOSE}};
    AfPathData data = {.paths = paths,
                       .path_count = 3U,
                       .path_capacity = 3U,
                       .commands = commands,
                       .command_count = 20U,
                       .command_capacity = 20U};
    AfPathTessellationScratch scratch;
    AfVec2 vertices[4096];
    AfVec2 cache_vertices[4096];
    AfPathGeometryCache cache = {.vertices = cache_vertices, .vertex_capacity = 4096U};
    AfPath bounded_path = {.id = 4U,
                           .node_id = 13U,
                           .command_count = 2U,
                           .fill_rule = AF_PATH_FILL_NON_ZERO,
                           .geometry_generation = 1U};
    AfPathCommand bounded_commands[2] = {{.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}},
                                         {.type = AF_PATH_CUBIC,
                                          .point = {100.0F, 0.0F},
                                          .control1 = {0.0F, 1000000.0F},
                                          .control2 = {100.0F, -1000000.0F}}};
    AfPathData bounded_data = {.paths = &bounded_path,
                               .path_count = 1U,
                               .path_capacity = 1U,
                               .commands = bounded_commands,
                               .command_count = 2U,
                               .command_capacity = 2U};
    AfFlattenedPath flattened = {.points = scratch.points,
                                 .point_capacity = AF_MAX_PATH_FLATTENED_POINTS,
                                 .contours = scratch.contours,
                                 .contour_capacity = AF_MAX_PATH_CONTOURS};
    uint32_t vertex_count = 0U;
    CHECK(af_path_data_validate(&data) == AF_RESULT_SUCCESS);
    CHECK(af_path_tessellate(&data, &paths[0], 0.25F, &scratch, vertices, 4096U, &vertex_count) ==
          AF_RESULT_SUCCESS);
    CHECK(vertex_count == 6U);
    CHECK(af_path_hit_test(&data, &paths[0], (AfVec2){50.0F, 50.0F}, 0.25F, &scratch));
    CHECK(!af_path_hit_test(&data, &paths[0], (AfVec2){150.0F, 50.0F}, 0.25F, &scratch));
    CHECK(af_path_tessellate(&data, &paths[1], 0.25F, &scratch, vertices, 4096U, &vertex_count) ==
          AF_RESULT_SUCCESS);
    CHECK(vertex_count != 0U);
    CHECK(af_path_hit_test(&data, &paths[1], (AfVec2){10.0F, 10.0F}, 0.25F, &scratch));
    CHECK(!af_path_hit_test(&data, &paths[1], (AfVec2){50.0F, 50.0F}, 0.25F, &scratch));
    CHECK(af_path_tessellate(&data, &paths[2], 0.25F, &scratch, vertices, 4096U, &vertex_count) ==
          AF_RESULT_SUCCESS);
    CHECK(vertex_count != 0U && vertex_count <= 24U);
    CHECK(af_path_cache_update(&data, &paths[2], 0.25F, &scratch, &cache) == AF_RESULT_SUCCESS);
    CHECK(cache.rebuild_count == 1U && cache.vertex_count == vertex_count);
    CHECK(af_path_cache_update(&data, &paths[2], 0.25F, &scratch, &cache) == AF_RESULT_SUCCESS);
    CHECK(cache.rebuild_count == 1U);
    CHECK(af_path_mark_geometry_changed(&paths[2]) == AF_RESULT_SUCCESS);
    CHECK(af_path_cache_update(&data, &paths[2], 0.25F, &scratch, &cache) == AF_RESULT_SUCCESS);
    CHECK(cache.rebuild_count == 2U);
    commands[0].type = AF_PATH_LINE;
    CHECK(af_path_data_validate(&data) == AF_RESULT_INVALID_ARGUMENT);
    commands[0].type = AF_PATH_MOVE;
    commands[1].point.x = INFINITY;
    CHECK(af_path_data_validate(&data) == AF_RESULT_INVALID_ARGUMENT);
    commands[1].point.x = 100.0F;
    CHECK(af_path_data_validate(&data) == AF_RESULT_SUCCESS);
    CHECK(af_path_flatten(&bounded_data, &bounded_path, 0.0001F, &flattened) == AF_RESULT_SUCCESS);
    CHECK(flattened.point_count <= (1U << AF_PATH_MAX_SUBDIVISION_DEPTH) + 1U);
    bounded_path.command_count = AF_MAX_PATH_COMMANDS_PER_PATH + 1U;
    CHECK(af_path_data_validate(&bounded_data) == AF_RESULT_INVALID_ARGUMENT);
    return true;
}

static bool af_test_vector_styling(void) {
    AfPaint gradient = {.type = AF_PAINT_LINEAR_GRADIENT,
                        .start = {0.0F, 0.0F},
                        .end = {100.0F, 0.0F},
                        .transform = {.m00 = 2.0F, .m11 = 2.0F, .tx = 10.0F},
                        .stops = {{.offset = 0.0F, .color = {1.0F, 0.0F, 0.0F, 1.0F}},
                                  {.offset = 0.5F, .color = {0.0F, 1.0F, 0.0F, 0.5F}},
                                  {.offset = 1.0F, .color = {0.0F, 0.0F, 1.0F, 1.0F}}},
                        .stop_count = 3U};
    AfColor sampled;
    AfStyle styles[2] = {af_style_default(1U, 0U, (AfColor){1.0F, 0.0F, 0.0F, 1.0F}),
                         af_style_default(2U, 10U, (AfColor){0.0F, 0.0F, 1.0F, 1.0F})};
    AfStyleData style_data = {.styles = styles, .count = 2U, .capacity = 2U};
    AfStyle resolved;
    AfVec2 points[4] = {{0.0F, 0.0F}, {100.0F, 0.0F}, {100.0F, 100.0F}, {0.0F, 100.0F}};
    AfPathContour contour = {.point_count = 4U, .closed = true};
    AfFlattenedPath flattened = {.points = points,
                                 .point_count = 4U,
                                 .point_capacity = 4U,
                                 .contours = &contour,
                                 .contour_count = 1U,
                                 .contour_capacity = 1U};
    AfVec2 stroke_vertices[4096];
    uint32_t stroke_count = 0U;
    AfPath path = {.id = 1U, .node_id = 10U, .command_count = 5U, .geometry_generation = 1U};
    AfPathCommand commands[5] = {{.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}},
                                 {.type = AF_PATH_LINE, .point = {100.0F, 0.0F}},
                                 {.type = AF_PATH_LINE, .point = {100.0F, 100.0F}},
                                 {.type = AF_PATH_LINE, .point = {0.0F, 100.0F}},
                                 {.type = AF_PATH_CLOSE}};
    AfPathData path_data = {.paths = &path,
                            .path_count = 1U,
                            .path_capacity = 1U,
                            .commands = commands,
                            .command_count = 5U,
                            .command_capacity = 5U};
    AfPathTessellationScratch scratch;
    AfStrokeGeometryCache cache = {.vertices = stroke_vertices, .vertex_capacity = 4096U};

    CHECK(af_paint_sample(&gradient, (AfVec2){110.0F, 0.0F}, 0.5F, &sampled) == AF_RESULT_SUCCESS);
    CHECK_NEAR(sampled.g, 1.0F, 0.0001F);
    CHECK_NEAR(sampled.a, 0.25F, 0.0001F);
    gradient.type = AF_PAINT_RADIAL_GRADIENT;
    gradient.transform = af_affine_identity();
    gradient.start = (AfVec2){0.0F, 0.0F};
    gradient.start_radius = 0.0F;
    gradient.end_radius = 100.0F;
    CHECK(af_paint_sample(&gradient, (AfVec2){50.0F, 0.0F}, 1.0F, &sampled) == AF_RESULT_SUCCESS);
    CHECK_NEAR(sampled.g, 1.0F, 0.0001F);
    gradient.type = AF_PAINT_CONIC_GRADIENT;
    gradient.angle = 0.0F;
    CHECK(af_paint_sample(&gradient, (AfVec2){0.0F, 10.0F}, 1.0F, &sampled) == AF_RESULT_SUCCESS);

    styles[1].parent_style_id = 1U;
    styles[1].inherit_flags = AF_STYLE_INHERIT_FILL;
    styles[1].stroke = (AfStroke){.enabled = true,
                                  .width = 0.25F,
                                  .end_width = 40.0F,
                                  .cap = AF_STROKE_CAP_ROUND,
                                  .join = AF_STROKE_JOIN_MITER,
                                  .miter_limit = 2.0F,
                                  .dashes = {10.0F, 5.0F},
                                  .dash_count = 2U,
                                  .dash_offset = -200005.0F};
    CHECK(af_style_data_validate(&style_data) == AF_RESULT_SUCCESS);
    CHECK(af_style_resolve(&style_data, &styles[1], &resolved) == AF_RESULT_SUCCESS);
    CHECK_NEAR(resolved.fill.solid.r, 1.0F, 0.0001F);
    CHECK(af_stroke_tessellate(&flattened, &resolved.stroke, stroke_vertices, 4096U,
                               &stroke_count) == AF_RESULT_SUCCESS);
    CHECK(stroke_count != 0U && stroke_count <= 4096U);
    CHECK(af_stroke_cache_update(&path_data, &path, &style_data, &styles[1], 0.25F, &scratch,
                                 &cache) == AF_RESULT_SUCCESS);
    CHECK(cache.rebuild_count == 1U);
    CHECK(af_style_mark_paint_changed(&styles[1]) == AF_RESULT_SUCCESS);
    CHECK(af_stroke_cache_update(&path_data, &path, &style_data, &styles[1], 0.25F, &scratch,
                                 &cache) == AF_RESULT_SUCCESS);
    CHECK(cache.rebuild_count == 1U);
    CHECK(af_style_mark_stroke_geometry_changed(&styles[1]) == AF_RESULT_SUCCESS);
    CHECK(af_stroke_cache_update(&path_data, &path, &style_data, &styles[1], 0.25F, &scratch,
                                 &cache) == AF_RESULT_SUCCESS);
    CHECK(cache.rebuild_count == 2U);
    styles[0].parent_style_id = 2U;
    CHECK(af_style_data_validate(&style_data) == AF_RESULT_INVALID_ARGUMENT);
    return true;
}

static bool af_test_responsive_layout(void) {
    AfSceneNode nodes[3] = {{.id = 1U,
                             .parent_id = AF_INVALID_NODE_ID,
                             .type = AF_NODE_GROUP,
                             .local_transform = {.scale = {1.0F, 1.0F}},
                             .bounds = {400.0F, 100.0F},
                             .opacity = 1.0F,
                             .visible = true},
                            {.id = 2U,
                             .parent_id = 1U,
                             .type = AF_NODE_RECTANGLE,
                             .local_transform = {.position = {3.0F, 4.0F}, .scale = {1.0F, 1.0F}},
                             .bounds = {100.0F, 50.0F},
                             .fill = {1.0F, 1.0F, 1.0F, 1.0F},
                             .opacity = 1.0F,
                             .visible = true},
                            {.id = 3U,
                             .parent_id = 1U,
                             .type = AF_NODE_RECTANGLE,
                             .local_transform = {.scale = {1.0F, 1.0F}},
                             .bounds = {20.0F, 20.0F},
                             .fill = {1.0F, 1.0F, 1.0F, 1.0F},
                             .opacity = 1.0F,
                             .visible = true}};
    AfLayoutNode layouts[3] = {af_layout_node_default(1U), af_layout_node_default(2U),
                               af_layout_node_default(3U)};
    AfLayoutData data = {
        .nodes = layouts,
        .count = 3U,
        .capacity = 3U,
        .reference_size = {1000.0F, 500.0F},
        .target_size = {1000.0F, 500.0F},
        .safe_area = {.left = 20.0F, .top = 10.0F, .right = 20.0F, .bottom = 10.0F},
        .scale_mode = AF_CANVAS_SCALE_FIT};
    AfScene scene = {.nodes = nodes, .count = 3U, .capacity = 3U, .layouts = &data};
    layouts[0].width = (AfLayoutValue){.mode = AF_LAYOUT_VALUE_PERCENT, .value = 1.0F};
    layouts[0].height = (AfLayoutValue){.mode = AF_LAYOUT_VALUE_FIXED, .value = 100.0F};
    layouts[0].direction = AF_LAYOUT_DIRECTION_HORIZONTAL;
    layouts[0].child_alignment = AF_LAYOUT_ALIGN_CENTER;
    layouts[0].spacing = 5.0F;
    layouts[0].padding = (AfInsets){.left = 10.0F, .top = 5.0F, .right = 10.0F, .bottom = 5.0F};
    layouts[1].width = (AfLayoutValue){.mode = AF_LAYOUT_VALUE_FIXED, .value = 100.0F};
    layouts[1].height = (AfLayoutValue){.mode = AF_LAYOUT_VALUE_FIXED, .value = 50.0F};
    layouts[2].width = (AfLayoutValue){.mode = AF_LAYOUT_VALUE_PERCENT, .value = 0.5F};
    layouts[2].height = (AfLayoutValue){.mode = AF_LAYOUT_VALUE_FIXED, .value = 80.0F};
    layouts[2].aspect = AF_ASPECT_FIT;
    layouts[2].aspect_ratio = 2.0F;
    CHECK(af_layout_validate(&data, &scene) == AF_RESULT_SUCCESS);
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_SUCCESS);
    CHECK_NEAR(nodes[0].resolved_bounds.x, 960.0F, 0.0001F);
    CHECK_NEAR(nodes[0].world_transform.tx, 20.0F, 0.0001F);
    CHECK_NEAR(nodes[0].world_transform.ty, 10.0F, 0.0001F);
    CHECK_NEAR(nodes[1].world_transform.tx, 33.0F, 0.0001F);
    CHECK_NEAR(nodes[1].world_transform.ty, 39.0F, 0.0001F);
    CHECK_NEAR(nodes[2].world_transform.tx, 135.0F, 0.0001F);
    CHECK_NEAR(nodes[2].resolved_bounds.x, 160.0F, 0.0001F);
    CHECK_NEAR(nodes[2].resolved_bounds.y, 80.0F, 0.0001F);
    nodes[1].local_transform.position.x = 13.0F;
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_SUCCESS);
    CHECK_NEAR(nodes[1].world_transform.tx, 43.0F, 0.0001F);
    data.target_size = (AfVec2){2000.0F, 1000.0F};
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_SUCCESS);
    CHECK_NEAR(nodes[1].world_transform.tx, 66.0F, 0.0001F);
    layouts[1].anchor_min.x = 0.8F;
    layouts[1].anchor_max.x = 0.2F;
    CHECK(af_layout_validate(&data, &scene) == AF_RESULT_INVALID_ARGUMENT);
    return true;
}

static bool af_test_text_shaping(void) {
    AfAllocator allocator;
    AfFileData font_data = {0};
    AfTextEngine *engine = NULL;
    AfGlyphPlacement glyphs[AF_MAX_TEXT_GLYPHS];
    AfTextLine lines[AF_MAX_TEXT_LINES];
    AfTextLayout layout = {.glyphs = glyphs,
                           .glyph_capacity = AF_MAX_TEXT_GLYPHS,
                           .lines = lines,
                           .line_capacity = AF_MAX_TEXT_LINES};
    static const char latin[] = "office interface wraps deterministically";
    static const char rtl[] = "\xD8\xA7\xD8\xB1\xDA\xA9 \xD9\x81\xD9\x84\xD9\x88";
    AfTextRequest request = {.utf8 = latin,
                             .byte_count = sizeof(latin) - 1U,
                             .font_ids = {1U},
                             .font_count = 1U,
                             .font_size = 32.0F,
                             .maximum_width = 180.0F,
                             .line_spacing = 1.25F,
                             .maximum_lines = 2U,
                             .direction = AF_TEXT_DIRECTION_LTR,
                             .alignment = AF_TEXT_ALIGN_CENTER,
                             .wrap = AF_TEXT_WRAP_WORD,
                             .kerning = true,
                             .ligatures = true,
                             .truncate = true};
    AfTextRunCache cache = {.layout = {.glyphs = glyphs,
                                       .glyph_capacity = AF_MAX_TEXT_GLYPHS,
                                       .lines = lines,
                                       .line_capacity = AF_MAX_TEXT_LINES}};
    uint32_t atlas_width = 0U;
    uint32_t atlas_height = 0U;
    uint64_t atlas_generation = 0U;
    AfPathCommand outline[AF_MAX_PATH_COMMANDS_PER_PATH];
    uint32_t outline_count = 0U;
    af_allocator_init_default(&allocator);
    CHECK(af_file_read_limited(ARCFLOW_TEST_FONT, 4U * 1024U * 1024U, &allocator, &font_data) ==
          AF_RESULT_SUCCESS);
    CHECK(af_text_engine_create(&allocator, &engine) == AF_RESULT_SUCCESS);
    CHECK(af_text_font_load_memory(engine, 1U, font_data.data, font_data.size, 0U) ==
          AF_RESULT_SUCCESS);
    CHECK(af_text_shape(engine, &request, &layout) == AF_RESULT_SUCCESS);
    CHECK(layout.glyph_count != 0U && layout.line_count == 2U && layout.truncated);
    CHECK(layout.bounds.x <= request.maximum_width + 0.001F);
    CHECK(af_text_glyph_outline(engine, layout.glyphs[0].font_id, layout.glyphs[0].glyph_id,
                                request.font_size, (AfVec2){20.0F, 40.0F}, outline,
                                AF_MAX_PATH_COMMANDS_PER_PATH,
                                &outline_count) == AF_RESULT_SUCCESS);
    CHECK(outline_count != 0U && outline[0].type == AF_PATH_MOVE &&
          outline[outline_count - 1U].type == AF_PATH_CLOSE);
    cache.layout.glyph_count = 0U;
    cache.layout.line_count = 0U;
    CHECK(af_text_run_cache_update(engine, &request, 1U, &cache) == AF_RESULT_SUCCESS);
    CHECK(cache.rebuild_count == 1U);
    CHECK(af_text_run_cache_update(engine, &request, 1U, &cache) == AF_RESULT_SUCCESS);
    CHECK(cache.rebuild_count == 1U);
    request.utf8 = rtl;
    request.byte_count = sizeof(rtl) - 1U;
    request.direction = AF_TEXT_DIRECTION_RTL;
    request.maximum_width = 400.0F;
    request.maximum_lines = 1U;
    CHECK(af_text_run_cache_update(engine, &request, 2U, &cache) == AF_RESULT_SUCCESS);
    CHECK(cache.rebuild_count == 2U && cache.layout.glyph_count != 0U);
    request.utf8 = "\xC0\xAF";
    request.byte_count = 2U;
    CHECK(af_text_shape(engine, &request, &layout) == AF_RESULT_INVALID_ARGUMENT);
    request.utf8 = "\xED\xA0\x80";
    request.byte_count = 3U;
    CHECK(af_text_shape(engine, &request, &layout) == AF_RESULT_INVALID_ARGUMENT);
    CHECK(af_text_atlas_pixels(engine, &atlas_width, &atlas_height, &atlas_generation) != NULL);
    CHECK(atlas_width == AF_TEXT_ATLAS_WIDTH && atlas_height == AF_TEXT_ATLAS_HEIGHT &&
          atlas_generation > 1U);
    af_text_engine_destroy(engine);
    af_file_data_destroy(&allocator, &font_data);
    return true;
}
