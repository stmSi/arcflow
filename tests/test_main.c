#include "animation/advanced_animation.h"
#include "animation/animation.h"
#include "arcflow/arcflow_runtime.h"
#include "assets/package.h"
#include "component/component.h"
#include "composition/composition.h"
#include "core/allocator.h"
#include "core/arena.h"
#include "core/array.h"
#include "core/file_io.h"
#include "core/handles.h"
#include "core/hash.h"
#include "core/math.h"
#include "core/png.h"
#include "core/safe_math.h"
#include "deformation/deformation.h"
#include "editor/editor_commands.h"
#include "interaction/interaction.h"
#include "layout/layout.h"
#include "particle/particle.h"
#include "path/path.h"
#include "project/project.h"
#include "rigging/rigging.h"
#include "scene/scene.h"
#include "state_machine/state_machine.h"
#include "style/style.h"
#include "text/text.h"

#include <volk.h>

#include <math.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            (void)fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expression);   \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK_NEAR(a, b, epsilon) CHECK(fabsf((a) - (b)) <= (epsilon))

typedef struct AfTestFixture {
    AfSceneNode nodes[3];
    AfScene scene;
    AfAnimationClip clips[1];
    AfAnimationTrack tracks[2];
    AfKeyframe keys[5];
    AfAnimationData animation;
} AfTestFixture;

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
    af_test_fixture_init(&fixture);
    emitters[0] = af_particle_emitter_default(700U, 2U);
    emitters[0].type = AF_PARTICLE_EMITTER_RECTANGLE;
    emitters[0].seed = 0x12345678U;
    CHECK(af_particle_data_validate(&data, &fixture.scene) == AF_RESULT_SUCCESS);
    CHECK(af_particle_total_capacity(&data) == emitters[0].capacity);
    CHECK(af_particle_spawn_sample(&emitters[0], 42U, &first) == AF_RESULT_SUCCESS);
    CHECK(af_particle_spawn_sample(&emitters[0], 42U, &second) == AF_RESULT_SUCCESS);
    CHECK(memcmp(&first, &second, sizeof(first)) == 0);
    CHECK(af_particle_random_u32(emitters[0].seed, emitters[0].id, 42U, 0U) !=
          af_particle_random_u32(emitters[0].seed, emitters[0].id, 43U, 0U));
    emitters[1] = af_particle_emitter_default(701U, 3U);
    emitters[0].capacity = AF_MAX_PARTICLES;
    emitters[1].capacity = 1U;
    data.count = 2U;
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

static bool af_test_components(void) {
    AfTestFixture fixture;
    AfComponentDefinition definitions[4] = {{.id = 100U,
                                             .root_node_id = 1U,
                                             .clip_id = 10U,
                                             .reference_start = 0U,
                                             .reference_count = 1U,
                                             .property_start = 0U,
                                             .property_count = 5U},
                                            {.id = 200U,
                                             .root_node_id = 2U,
                                             .clip_id = 10U,
                                             .reference_start = 1U,
                                             .property_start = 5U}};
    AfComponentReference references[8] = {{.owner_definition_id = 100U, .component_id = 200U}};
    AfComponentProperty properties[16] = {{.definition_id = 100U,
                                           .id = 1U,
                                           .type = AF_COMPONENT_PROPERTY_NUMBER,
                                           .default_value.number = 2.0F},
                                          {.definition_id = 100U,
                                           .id = 2U,
                                           .type = AF_COMPONENT_PROPERTY_BOOLEAN,
                                           .default_value.boolean = true},
                                          {.definition_id = 100U,
                                           .id = 3U,
                                           .type = AF_COMPONENT_PROPERTY_COLOR,
                                           .default_value.color = {0.1F, 0.2F, 0.3F, 1.0F}},
                                          {.definition_id = 100U,
                                           .id = 4U,
                                           .type = AF_COMPONENT_PROPERTY_IMAGE,
                                           .default_value.image_id = 55U},
                                          {.definition_id = 100U,
                                           .id = 5U,
                                           .type = AF_COMPONENT_PROPERTY_TEXT,
                                           .default_value.text = "label"}};
    AfComponentInstance instances[2] = {{.id = 300U,
                                         .definition_id = 100U,
                                         .node_id = 2U,
                                         .override_start = 0U,
                                         .override_count = 2U,
                                         .playback_speed = 1.0F,
                                         .independent_playback = true},
                                        {.id = 301U,
                                         .definition_id = 100U,
                                         .node_id = 3U,
                                         .override_start = 2U,
                                         .playback_time = 0.25F,
                                         .playback_speed = 1.0F}};
    AfComponentOverride overrides[4] = {{.instance_id = 300U,
                                         .property_id = 1U,
                                         .type = AF_COMPONENT_PROPERTY_NUMBER,
                                         .value.number = 7.0F},
                                        {.instance_id = 300U,
                                         .property_id = 5U,
                                         .type = AF_COMPONENT_PROPERTY_TEXT,
                                         .value.text = "overridden"}};
    AfComponentData data = {.definitions = definitions,
                            .definition_count = 2U,
                            .definition_capacity = 4U,
                            .references = references,
                            .reference_count = 1U,
                            .reference_capacity = 8U,
                            .properties = properties,
                            .property_count = 5U,
                            .property_capacity = 16U,
                            .instances = instances,
                            .instance_count = 2U,
                            .instance_capacity = 2U,
                            .overrides = overrides,
                            .override_count = 2U,
                            .override_capacity = 4U};
    AfComponentInstanceState states[2];
    AfComponentRuntime runtime;
    AfComponentValue value;
    AfDetachedComponent detached;
    AfComponentEditSession edit_session;
    AfAllocator allocator;
    AfPackageBlob blob = {0};
    AfLoadedPackage loaded = {0};
    AfPackageSource source;
    af_test_fixture_init(&fixture);
    af_allocator_init_default(&allocator);
    CHECK(af_component_validate(&data, &fixture.scene, &fixture.animation) == AF_RESULT_SUCCESS);
    CHECK(af_component_runtime_init(&runtime, &data, &fixture.animation, states, 2U) ==
          AF_RESULT_SUCCESS);
    CHECK(af_component_runtime_update(&runtime, 0.5F) == AF_RESULT_SUCCESS);
    CHECK_NEAR(states[0].playback_time, 0.5F, 0.0001F);
    CHECK_NEAR(states[1].playback_time, 0.25F, 0.0001F);
    CHECK(af_component_runtime_set_playing(&runtime, 300U, false) == AF_RESULT_SUCCESS);
    CHECK(af_component_runtime_update(&runtime, 1.0F) == AF_RESULT_SUCCESS);
    CHECK_NEAR(states[0].playback_time, 0.5F, 0.0001F);
    CHECK(af_component_resolve_property(&data, 300U, 1U, AF_COMPONENT_PROPERTY_NUMBER, &value) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR(value.number, 7.0F, 0.0001F);
    CHECK(af_component_resolve_property(&data, 301U, 5U, AF_COMPONENT_PROPERTY_TEXT, &value) ==
          AF_RESULT_SUCCESS);
    CHECK(strcmp(value.text, "label") == 0);
    CHECK(af_component_detach(&data, 300U, &detached) == AF_RESULT_SUCCESS);
    CHECK(detached.root_node_id == 1U && detached.override_count == 2U);
    CHECK(af_component_edit_begin(&data, 100U, &edit_session) == AF_RESULT_SUCCESS);
    CHECK(edit_session.active && edit_session.definition_id == 100U);
    af_component_edit_end(&edit_session);
    CHECK(!edit_session.active);
    CHECK(af_component_duplicate_definition(&data, 100U, 201U) == AF_RESULT_SUCCESS);
    CHECK(data.definition_count == 3U && data.property_count == 10U && data.reference_count == 2U);
    data.definition_count = 2U;
    data.property_count = 5U;
    data.reference_count = 2U;
    definitions[1].reference_count = 1U;
    references[1] = (AfComponentReference){.owner_definition_id = 200U, .component_id = 100U};
    CHECK(af_component_validate(&data, &fixture.scene, &fixture.animation) ==
          AF_RESULT_INVALID_ARGUMENT);
    definitions[1].reference_count = 0U;
    data.reference_count = 1U;
    source = (AfPackageSource){
        .scene = &fixture.scene, .animation = &fixture.animation, .components = &data};
    CHECK(af_package_write(&source, &allocator, &blob) == AF_RESULT_SUCCESS);
    CHECK(af_package_validate(blob.data, blob.size) == AF_RESULT_SUCCESS);
    CHECK(af_package_decode(blob.data, blob.size, &allocator, &loaded) == AF_RESULT_SUCCESS);
    CHECK(loaded.component_data.definition_count == 2U &&
          loaded.component_data.instance_count == 2U &&
          loaded.component_data.definitions[0].id == 100U &&
          strcmp(loaded.component_data.overrides[1].value.text, "overridden") == 0);
    af_loaded_package_destroy(&allocator, &loaded);
    af_package_blob_destroy(&allocator, &blob);
    return true;
}

static bool af_test_interaction(void) {
    AfSceneNode nodes[3] = {{.id = 1U,
                             .parent_id = AF_INVALID_NODE_ID,
                             .type = AF_NODE_GROUP,
                             .local_transform = {.position = {10.0F, 10.0F}, .scale = {1.0F, 1.0F}},
                             .bounds = {80.0F, 80.0F},
                             .opacity = 1.0F,
                             .visible = true},
                            {.id = 2U,
                             .parent_id = 1U,
                             .type = AF_NODE_RECTANGLE,
                             .local_transform = {.position = {5.0F, 5.0F}, .scale = {1.0F, 1.0F}},
                             .bounds = {100.0F, 100.0F},
                             .opacity = 1.0F,
                             .visible = true},
                            {.id = 3U,
                             .parent_id = AF_INVALID_NODE_ID,
                             .type = AF_NODE_ELLIPSE,
                             .local_transform = {.position = {30.0F, 30.0F}, .scale = {1.0F, 1.0F}},
                             .bounds = {30.0F, 30.0F},
                             .opacity = 1.0F,
                             .layer_order = 2,
                             .visible = true}};
    AfScene scene = {.nodes = nodes, .count = 3U, .capacity = 3U};
    AfInteractionNode interaction_nodes[3] = {
        {.node_id = 1U, .enabled = false, .clip_children = true},
        {.node_id = 2U, .enabled = true, .focusable = true, .opacity_threshold = 0.01F},
        {.node_id = 3U, .enabled = true, .focusable = true, .opacity_threshold = 0.01F}};
    AfInteractionSystem system;
    AfInteractionData data = {.nodes = interaction_nodes, .count = 3U};
    AfRuntimeEvent event;
    uint32_t event_count = 0U;
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_SUCCESS);
    CHECK(af_interaction_validate(&data, &scene) == AF_RESULT_SUCCESS);
    af_interaction_init(&system, interaction_nodes, 3U);
    CHECK(af_interaction_hit_test(&system, &scene, 35.0F, 35.0F) == 3U);
    interaction_nodes[2].pointer_passthrough = true;
    CHECK(af_interaction_hit_test(&system, &scene, 35.0F, 35.0F) == 2U);
    CHECK(af_interaction_hit_test(&system, &scene, 100.0F, 20.0F) == 0U);
    CHECK(af_interaction_pointer_move(&system, &scene, 20.0F, 20.0F, 1.0) == AF_RESULT_SUCCESS);
    CHECK(af_interaction_pointer_down(&system, &scene, 20.0F, 20.0F, 1.1) == AF_RESULT_SUCCESS);
    CHECK(af_interaction_pointer_move(&system, &scene, 200.0F, 200.0F, 1.2) == AF_RESULT_SUCCESS);
    CHECK(system.captured_node_id == 2U && system.dragging);
    CHECK(af_interaction_pointer_up(&system, &scene, 200.0F, 200.0F, 1.3) == AF_RESULT_SUCCESS);
    while (af_interaction_poll(&system, &event) == AF_RESULT_SUCCESS) {
        ++event_count;
    }
    CHECK(event_count >= 9U);
    CHECK(af_interaction_pointer_down(&system, &scene, 20.0F, 20.0F, 2.0) == AF_RESULT_SUCCESS);
    CHECK(af_interaction_pointer_up(&system, &scene, 20.0F, 20.0F, 2.1) == AF_RESULT_SUCCESS);
    CHECK(af_interaction_pointer_down(&system, &scene, 20.0F, 20.0F, 2.2) == AF_RESULT_SUCCESS);
    CHECK(af_interaction_pointer_up(&system, &scene, 20.0F, 20.0F, 2.3) == AF_RESULT_SUCCESS);
    CHECK(af_interaction_keyboard_activate(&system, 2.4) == AF_RESULT_SUCCESS);
    CHECK(af_interaction_controller_navigate(&system, &scene, 1, 2.5) == AF_RESULT_SUCCESS);
    while (af_interaction_poll(&system, &event) == AF_RESULT_SUCCESS) {
        if (event.type == AF_EVENT_DOUBLE_CLICK) {
            ++event_count;
        }
    }
    CHECK(event_count >= 10U);
    af_interaction_init(&system, interaction_nodes, 3U);
    for (uint32_t index = 0U; index < AF_MAX_RUNTIME_EVENTS; ++index) {
        CHECK(af_interaction_queue_animation_event(&system, 1000U + index, 2U, (double)index) ==
              AF_RESULT_SUCCESS);
    }
    CHECK(af_interaction_queue_animation_event(&system, 9999U, 2U, 999.0) ==
          AF_RESULT_LIMIT_EXCEEDED);
    CHECK(af_interaction_poll(&system, &event) == AF_RESULT_SUCCESS);
    CHECK(event.type == AF_EVENT_ANIMATION && event.name_id == 1000U);
    nodes[0].visible = false;
    CHECK(af_scene_evaluate_world(&scene) == AF_RESULT_SUCCESS);
    CHECK(af_interaction_hit_test(&system, &scene, 20.0F, 20.0F) == 0U);
    return true;
}

static bool af_test_state_machines(void) {
    AfTestFixture fixture;
    AfStateMachineDefinition machines[2] = {
        {.id = 100U,
         .entry_state_id = 1000U,
         .state_start = 0U,
         .state_count = 4U,
         .transition_start = 0U,
         .transition_count = 3U},
        {.id = 200U, .entry_state_id = 2000U, .state_start = 4U, .state_count = 1U}};
    AfStateDefinition states[5] = {
        {.id = 1000U, .machine_id = 100U, .clip_id = 10U, .type = AF_STATE_ANIMATION},
        {.id = 1001U,
         .machine_id = 100U,
         .clip_id = 10U,
         .tag_id = 77U,
         .type = AF_STATE_ANIMATION},
        {.id = 1002U, .machine_id = 100U, .type = AF_STATE_EXIT},
        {.id = 1003U,
         .machine_id = 100U,
         .nested_machine_id = 200U,
         .type = AF_STATE_NESTED_MACHINE},
        {.id = 2000U, .machine_id = 200U, .clip_id = 10U, .type = AF_STATE_ANIMATION}};
    AfStateTransition transitions[4] = {{.source_state_id = 1000U,
                                         .target_state_id = 1001U,
                                         .condition_start = 0U,
                                         .condition_count = 1U,
                                         .priority = 1},
                                        {.source_state_id = 1001U,
                                         .target_state_id = 1000U,
                                         .condition_start = 1U,
                                         .condition_count = 1U,
                                         .priority = 1},
                                        {.target_state_id = 1002U,
                                         .event_id = 99U,
                                         .priority = 10,
                                         .flags = AF_TRANSITION_REQUIRE_EVENT,
                                         .duration = 0.2F,
                                         .easing = {0.25F, 0.1F, 0.25F, 1.0F}}};
    AfStateInputDefinition input_definitions[4] = {
        {.machine_id = 100U,
         .id = 1U,
         .type = AF_STATE_INPUT_BOOLEAN,
         .default_value.boolean = false},
        {.machine_id = 100U, .id = 2U, .type = AF_STATE_INPUT_INTEGER, .default_value.integer = 0},
        {.machine_id = 100U, .id = 3U, .type = AF_STATE_INPUT_FLOAT, .default_value.real = 0.0F},
        {.machine_id = 100U, .id = 4U, .type = AF_STATE_INPUT_TRIGGER}};
    AfTransitionCondition conditions[2] = {{.input_id = 1U,
                                            .type = AF_STATE_INPUT_BOOLEAN,
                                            .comparison = AF_STATE_COMPARE_EQUAL,
                                            .value.boolean = true},
                                           {.input_id = 4U,
                                            .type = AF_STATE_INPUT_TRIGGER,
                                            .comparison = AF_STATE_COMPARE_EQUAL,
                                            .value.boolean = true}};
    AfStateMachineData data = {.machines = machines,
                               .machine_count = 2U,
                               .machine_capacity = 2U,
                               .states = states,
                               .state_count = 5U,
                               .state_capacity = 5U,
                               .transitions = transitions,
                               .transition_count = 3U,
                               .transition_capacity = 4U,
                               .inputs = input_definitions,
                               .input_count = 4U,
                               .input_capacity = 4U,
                               .conditions = conditions,
                               .condition_count = 2U,
                               .condition_capacity = 2U};
    AfStateMachineStatus statuses[2];
    AfStateInputState inputs[4];
    AfStateMachineRuntime runtime;
    AfStateMachineStatus debug;
    AfComponentData no_components = {0};
    AfAllocator allocator;
    AfPackageBlob blob = {0};
    AfLoadedPackage loaded = {0};
    AfRuntime *host_runtime = NULL;
    AfPackageHandle package = 0U;
    AfInstanceHandle instance = 0U;
    AfRuntimeCreateInfo create_info = {.frames_in_flight = 2U};
    AfPackageSource source;
    af_test_fixture_init(&fixture);
    af_allocator_init_default(&allocator);
    CHECK(af_state_machine_validate(&data, &fixture.animation, &no_components) ==
          AF_RESULT_SUCCESS);
    CHECK(af_state_machine_runtime_init(&runtime, &data, &fixture.animation, statuses, 2U, inputs,
                                        4U) == AF_RESULT_SUCCESS);
    CHECK(af_state_machine_set_boolean(&runtime, 1U, true) == AF_RESULT_SUCCESS);
    CHECK(af_state_machine_set_integer(&runtime, 2U, 8) == AF_RESULT_SUCCESS);
    CHECK(af_state_machine_set_float(&runtime, 3U, 0.75F) == AF_RESULT_SUCCESS);
    CHECK(af_state_machine_update(&runtime, 0.1F) == AF_RESULT_SUCCESS);
    CHECK(af_state_machine_debug(&runtime, 100U, &debug) == AF_RESULT_SUCCESS);
    CHECK(debug.current_state_id == 1001U && debug.transitions_last_update == 1U);
    CHECK(af_state_machine_set_boolean(&runtime, 1U, false) == AF_RESULT_SUCCESS);
    CHECK(af_state_machine_fire_trigger(&runtime, 4U) == AF_RESULT_SUCCESS);
    CHECK(af_state_machine_update(&runtime, 0.0F) == AF_RESULT_SUCCESS);
    CHECK(statuses[0].current_state_id == 1000U && !inputs[3].triggered);
    CHECK(af_state_machine_notify_event(&runtime, 99U) == AF_RESULT_SUCCESS);
    CHECK(af_state_machine_update(&runtime, 0.0F) == AF_RESULT_SUCCESS);
    CHECK(statuses[0].pending_state_id == 1002U);
    CHECK(af_state_machine_update(&runtime, 0.1F) == AF_RESULT_SUCCESS);
    CHECK(statuses[0].pending_state_id == 1002U);
    CHECK(af_state_machine_update(&runtime, 0.1F) == AF_RESULT_SUCCESS);
    CHECK(statuses[0].current_state_id == 1002U && statuses[0].exited);
    source = (AfPackageSource){
        .scene = &fixture.scene, .animation = &fixture.animation, .state_machines = &data};
    CHECK(af_package_write(&source, &allocator, &blob) == AF_RESULT_SUCCESS);
    CHECK(af_package_decode(blob.data, blob.size, &allocator, &loaded) == AF_RESULT_SUCCESS);
    CHECK(loaded.state_machine_data.machine_count == 2U &&
          loaded.state_machine_data.transition_count == 3U &&
          loaded.state_machine_data.inputs[3].type == AF_STATE_INPUT_TRIGGER);
    af_loaded_package_destroy(&allocator, &loaded);
    CHECK(af_runtime_create(&create_info, &host_runtime) == AF_RESULT_SUCCESS);
    CHECK(af_package_load(host_runtime, blob.data, blob.size, &package) == AF_RESULT_SUCCESS);
    CHECK(af_instance_create(host_runtime, package, &instance) == AF_RESULT_SUCCESS);
    CHECK(af_instance_set_boolean_input(host_runtime, instance, 1U, 1) == AF_RESULT_SUCCESS);
    CHECK(af_instance_set_integer_input(host_runtime, instance, 2U, 9) == AF_RESULT_SUCCESS);
    CHECK(af_instance_set_float_input(host_runtime, instance, 3U, 0.5F) == AF_RESULT_SUCCESS);
    CHECK(af_instance_update(host_runtime, instance, 0.01F) == AF_RESULT_SUCCESS);
    CHECK(af_instance_set_boolean_input(host_runtime, instance, 1U, 0) == AF_RESULT_SUCCESS);
    CHECK(af_instance_fire_trigger(host_runtime, instance, 4U) == AF_RESULT_SUCCESS);
    CHECK(af_instance_update(host_runtime, instance, 0.01F) == AF_RESULT_SUCCESS);
    CHECK(af_runtime_frame_allocation_count(host_runtime) == 0U);
    CHECK(af_instance_destroy(host_runtime, instance) == AF_RESULT_SUCCESS);
    CHECK(af_package_destroy(host_runtime, package) == AF_RESULT_SUCCESS);
    af_runtime_destroy(host_runtime);
    af_package_blob_destroy(&allocator, &blob);
    states[3].nested_machine_id = 100U;
    CHECK(af_state_machine_validate(&data, &fixture.animation, &no_components) ==
          AF_RESULT_INVALID_ARGUMENT);
    states[3].nested_machine_id = 200U;
    transitions[0] = (AfStateTransition){.source_state_id = 1000U, .target_state_id = 1001U};
    transitions[1] = (AfStateTransition){.source_state_id = 1001U, .target_state_id = 1000U};
    machines[0].transition_count = 2U;
    CHECK(af_state_machine_runtime_init(&runtime, &data, &fixture.animation, statuses, 2U, inputs,
                                        4U) == AF_RESULT_SUCCESS);
    CHECK(af_state_machine_update(&runtime, 0.0F) == AF_RESULT_LIMIT_EXCEEDED);
    return true;
}

static bool af_test_advanced_animation(void) {
    AfTestFixture fixture;
    AfAnimationClip clips[4] = {0};
    AfAnimationTrack tracks[8] = {0};
    AfKeyframe keys[32] = {0};
    AfAnimationData animation = {.clips = clips,
                                 .clip_count = 1U,
                                 .tracks = tracks,
                                 .track_count = 2U,
                                 .keyframes = keys,
                                 .keyframe_count = 5U};
    AfClipMarker markers[2] = {{.clip_id = 10U, .event_id = 900U, .time = 0.5F}};
    AfTimelineRegion regions[2] = {
        {.clip_id = 10U, .id = 901U, .start_time = 0.25F, .end_time = 1.5F}};
    AfNestedTimeline nested[2] = {{.owner_clip_id = 10U,
                                   .child_clip_id = 20U,
                                   .start_time = 0.0F,
                                   .duration = 2.0F,
                                   .child_start_time = 0.0F,
                                   .child_end_time = 2.0F,
                                   .speed = 1.0F,
                                   .hold = true}};
    AfAnimationLayer layers[2] = {{.clip_id = 10U,
                                   .time = 1.0F,
                                   .weight = 0.5F,
                                   .transition_duration = 0.25F,
                                   .order = 0,
                                   .enabled = true},
                                  {.clip_id = 20U,
                                   .time = 0.5F,
                                   .weight = 0.25F,
                                   .order = 1,
                                   .additive = true,
                                   .enabled = true}};
    AfSpeedPoint speed_points[2] = {{.clip_id = 10U, .time = 0.0F, .speed = 1.0F},
                                    {.clip_id = 10U, .time = 1.0F, .speed = 0.5F}};
    AfAdvancedAnimationData advanced = {.markers = markers,
                                        .marker_count = 1U,
                                        .marker_capacity = 2U,
                                        .regions = regions,
                                        .region_count = 1U,
                                        .region_capacity = 2U,
                                        .nested = nested,
                                        .nested_count = 1U,
                                        .nested_capacity = 2U,
                                        .layers = layers,
                                        .layer_count = 2U,
                                        .layer_capacity = 2U,
                                        .speed_points = speed_points,
                                        .speed_point_count = 2U,
                                        .speed_point_capacity = 2U};
    AfAnimationClip snapshot_clips[4];
    AfAnimationTrack snapshot_tracks[8];
    AfKeyframe snapshot_keys[32];
    AfAnimationEditSnapshot snapshot = {.clips = snapshot_clips,
                                        .clip_capacity = 4U,
                                        .tracks = snapshot_tracks,
                                        .track_capacity = 8U,
                                        .keyframes = snapshot_keys,
                                        .keyframe_capacity = 32U};
    AfMotionPathPoint motion[8];
    float ghosts[8];
    float mapped_time = 0.0F;
    uint32_t output_count = 0U;
    const uint32_t retime_indices[2] = {0U, 1U};
    const float custom[4] = {0.42F, 0.0F, 0.58F, 1.0F};
    af_test_fixture_init(&fixture);
    memcpy(clips, fixture.clips, sizeof(fixture.clips));
    memcpy(tracks, fixture.tracks, sizeof(fixture.tracks));
    memcpy(keys, fixture.keys, sizeof(fixture.keys));
    CHECK(af_advanced_snapshot_capture(&animation, &snapshot) == AF_RESULT_SUCCESS);
    CHECK(af_advanced_clip_duplicate(&animation, 4U, 8U, 32U, 10U, 20U, 100U) == AF_RESULT_SUCCESS);
    CHECK(animation.clip_count == 2U && animation.track_count == 4U &&
          animation.keyframe_count == 10U);
    CHECK(af_advanced_animation_validate(&advanced, &animation) == AF_RESULT_SUCCESS);
    CHECK(af_advanced_map_nested_time(&advanced, &animation, 10U, 20U, 1.0F, false, &mapped_time) ==
          AF_RESULT_SUCCESS);
    CHECK(mapped_time >= 0.0F && mapped_time <= 2.0F);
    CHECK(af_advanced_apply_layers(&advanced, &animation, &fixture.scene) == AF_RESULT_SUCCESS);
    CHECK(isfinite(fixture.nodes[1].local_transform.position.x));
    CHECK(af_advanced_motion_path(&animation, 10U, 2U, 8U, motion, 8U, &output_count) ==
          AF_RESULT_SUCCESS);
    CHECK(output_count == 8U && motion[0].time == 0.0F && motion[7].time == 2.0F);
    CHECK(af_advanced_motion_path_edit(&animation, 11U, 1U, (AfVec2){22.0F, 23.0F}) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR(animation.keyframes[1].value[0], 22.0F, 0.0001F);
    CHECK(af_advanced_retime_keys(&animation, retime_indices, 2U, 0.5F, 1.0F, true) ==
          AF_RESULT_SUCCESS);
    CHECK(animation.keyframes[0].time <= animation.keyframes[1].time);
    CHECK(af_advanced_clip_split(&animation, 4U, 8U, 32U, 10U, 30U, 200U, 1.0F) ==
          AF_RESULT_SUCCESS);
    CHECK(animation.clip_count == 3U && clips[0].duration == 1.0F && clips[2].duration == 1.0F);
    CHECK(af_advanced_ghost_frames(1.0F, 1.0F / 60.0F, 2U, 3U, ghosts, 8U, &output_count) ==
          AF_RESULT_SUCCESS);
    CHECK(output_count == 5U && ghosts[0] < 1.0F && ghosts[4] > 1.0F);
    for (uint32_t preset = AF_EASING_EASE_IN; preset <= AF_EASING_CUSTOM_CUBIC; ++preset) {
        const float eased = af_advanced_easing_evaluate((AfEasingPreset)preset, custom, 0.5F);
        CHECK(isfinite(eased));
    }
    CHECK(af_advanced_snapshot_restore(&animation, &snapshot) == AF_RESULT_SUCCESS);
    CHECK(animation.clip_count == 1U && animation.track_count == 2U &&
          animation.keyframe_count == 5U);
    CHECK(af_advanced_clip_duplicate(&animation, 4U, 8U, 32U, 10U, 20U, 100U) == AF_RESULT_SUCCESS);
    nested[1] = (AfNestedTimeline){.owner_clip_id = 20U,
                                   .child_clip_id = 10U,
                                   .duration = 1.0F,
                                   .child_end_time = 1.0F,
                                   .speed = 1.0F};
    advanced.nested_count = 2U;
    CHECK(af_advanced_animation_validate(&advanced, &animation) == AF_RESULT_INVALID_ARGUMENT);
    return true;
}

static bool af_test_composition(void) {
    AfTestFixture fixture;
    AfCompositionClip clips[2] = {
        {.id = 10U, .node_id = 1U, .type = AF_CLIP_ROUNDED_RECTANGLE, .corner_radius = 8.0F},
        {.id = 11U, .node_id = 3U, .type = AF_CLIP_RECTANGLE}};
    AfCompositionMask masks[2] = {
        {.id = 20U, .owner_node_id = 2U, .mask_node_id = 3U, .alpha = 1.0F},
        {.id = 21U,
         .owner_node_id = 2U,
         .mask_node_id = 3U,
         .parent_mask_id = 20U,
         .alpha = 0.75F,
         .inverted = true}};
    AfLayerFilter filters[5] = {
        {.type = AF_FILTER_BLUR, .radius = 4.0F},
        {.type = AF_FILTER_DROP_SHADOW,
         .radius = 8.0F,
         .offset = {3.0F, 5.0F},
         .color = {0.0F, 0.0F, 0.0F, 0.5F}},
        {.type = AF_FILTER_INNER_SHADOW, .radius = 3.0F},
        {.type = AF_FILTER_GLOW, .radius = 6.0F, .color = {1.0F, 0.5F, 0.2F, 1.0F}},
        {.type = AF_FILTER_COLOR_ADJUSTMENT}};
    AfCompositionLayer layers[2] = {{.id = 100U,
                                     .node_id = 2U,
                                     .clip_id = 10U,
                                     .mask_id = 21U,
                                     .generation = 1U,
                                     .filter_start = 0U,
                                     .filter_count = 5U,
                                     .group_opacity = 0.6F,
                                     .blend_mode = AF_BLEND_NORMAL,
                                     .cache_enabled = true},
                                    {.id = 101U,
                                     .node_id = 3U,
                                     .parent_layer_id = 100U,
                                     .clip_id = 11U,
                                     .generation = 1U,
                                     .group_opacity = 1.0F,
                                     .blend_mode = AF_BLEND_SCREEN,
                                     .offscreen = true}};
    AfCompositionData data = {.layers = layers,
                              .layer_count = 2U,
                              .layer_capacity = 2U,
                              .clips = clips,
                              .clip_count = 2U,
                              .clip_capacity = 2U,
                              .masks = masks,
                              .mask_count = 2U,
                              .mask_capacity = 2U,
                              .filters = filters,
                              .filter_count = 5U,
                              .filter_capacity = 5U};
    AfRenderTargetPool pool = {0};
    AfCompositionCommand commands[2];
    AfCompositionPlan plan = {.commands = commands, .capacity = 2U};
    AfColor destination = {0.2F, 0.3F, 0.4F, 0.8F};
    AfColor source = {0.3F, 0.2F, 0.1F, 0.5F};
    af_test_fixture_init(&fixture);
    fixture.nodes[0].bounds = (AfVec2){300.0F, 300.0F};
    CHECK(af_scene_evaluate_world(&fixture.scene) == AF_RESULT_SUCCESS);
    CHECK(af_composition_validate(&data, &fixture.scene) == AF_RESULT_SUCCESS);
    CHECK(af_composition_build_plan(&data, &fixture.scene, 640U, 480U, &pool, &plan) ==
          AF_RESULT_SUCCESS);
    CHECK(plan.count == 2U && commands[0].target_index >= 0 && commands[1].target_index >= 0 &&
          !commands[0].target_reused);
    CHECK(af_composition_build_plan(&data, &fixture.scene, 640U, 480U, &pool, &plan) ==
          AF_RESULT_SUCCESS);
    CHECK(commands[0].target_reused && commands[1].target_reused);
    fixture.nodes[2].local_transform.position.x += 10.0F;
    CHECK(af_scene_evaluate_world(&fixture.scene) == AF_RESULT_SUCCESS);
    CHECK(af_composition_build_plan(&data, &fixture.scene, 640U, 480U, &pool, &plan) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR(commands[0].mask_transform.tx, fixture.nodes[2].world_transform.tx, 0.0001F);
    for (uint32_t mode = AF_BLEND_NORMAL; mode <= AF_BLEND_SCREEN; ++mode) {
        const AfColor blended = af_composition_blend((AfBlendMode)mode, source, destination);
        CHECK(isfinite(blended.r) && isfinite(blended.g) && isfinite(blended.b) &&
              isfinite(blended.a));
    }
    {
        const AfVec2 expansion = af_composition_filter_expansion(&filters[1]);
        CHECK(expansion.x >= 16.0F && expansion.y >= 16.0F);
    }
    CHECK(af_render_target_acquire(&pool, AF_MAX_RENDER_TARGET_DIMENSION + 1U, 16U, 0U, 0U, false,
                                   &(uint32_t){0}, &(bool){false}) == AF_RESULT_INVALID_ARGUMENT);
    layers[1].blend_mode = AF_BLEND_UNSUPPORTED;
    CHECK(af_composition_validate(&data, &fixture.scene) == AF_RESULT_UNSUPPORTED_FEATURE);
    layers[1].blend_mode = AF_BLEND_SCREEN;
    layers[0].parent_layer_id = 101U;
    CHECK(af_composition_validate(&data, &fixture.scene) == AF_RESULT_INVALID_ARGUMENT);
    layers[0].parent_layer_id = 0U;
    masks[0].parent_mask_id = 21U;
    CHECK(af_composition_validate(&data, &fixture.scene) == AF_RESULT_INVALID_ARGUMENT);
    return true;
}

static bool af_test_shape_deformation(void) {
    AfPath paths[3] = {{.id = 1U,
                        .node_id = 1U,
                        .command_start = 0U,
                        .command_count = 4U,
                        .fill_rule = AF_PATH_FILL_NON_ZERO,
                        .geometry_generation = 1U},
                       {.id = 2U,
                        .node_id = 2U,
                        .command_start = 4U,
                        .command_count = 4U,
                        .fill_rule = AF_PATH_FILL_NON_ZERO,
                        .geometry_generation = 1U},
                       {.id = 3U,
                        .node_id = 3U,
                        .command_start = 8U,
                        .command_count = 3U,
                        .fill_rule = AF_PATH_FILL_NON_ZERO,
                        .geometry_generation = 1U}};
    AfPathCommand commands[11] = {{.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}},
                                  {.type = AF_PATH_LINE, .point = {10.0F, 0.0F}},
                                  {.type = AF_PATH_LINE, .point = {10.0F, 10.0F}},
                                  {.type = AF_PATH_CLOSE},
                                  {.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}},
                                  {.type = AF_PATH_LINE, .point = {20.0F, 0.0F}},
                                  {.type = AF_PATH_LINE, .point = {20.0F, 20.0F}},
                                  {.type = AF_PATH_CLOSE},
                                  {.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}},
                                  {.type = AF_PATH_LINE, .point = {5.0F, 5.0F}},
                                  {.type = AF_PATH_LINE, .point = {10.0F, 0.0F}}};
    AfPathData path_data = {.paths = paths,
                            .path_count = 3U,
                            .path_capacity = 3U,
                            .commands = commands,
                            .command_count = 11U,
                            .command_capacity = 11U};
    AfPathCommand morphed[4];
    AfMorphCache morph = {.commands = morphed, .command_capacity = 4U};
    AfVec2 trimmed[64];
    AfPathTessellationScratch scratch;
    AfTransform2D followed;
    uint32_t trimmed_count = 0U;
    AfStyle style = af_style_default(10U, 1U, (AfColor){1.0F, 1.0F, 1.0F, 1.0F});
    AfStyleData style_data = {.styles = &style, .count = 1U, .capacity = 1U};
    AfCompositionMask mask = {.id = 20U, .owner_node_id = 1U, .mask_node_id = 2U, .alpha = 1.0F};
    AfCompositionData composition = {.mask_count = 1U, .mask_capacity = 1U, .masks = &mask};
    AfShapeAnimationTrack shape_tracks[5] = {{.target_id = 1U,
                                              .property = AF_SHAPE_PROPERTY_PATH_POINT,
                                              .element_index = 1U,
                                              .sample_start = 0U,
                                              .sample_count = 2U},
                                             {.target_id = 10U,
                                              .property = AF_SHAPE_PROPERTY_STROKE_WIDTH,
                                              .sample_start = 2U,
                                              .sample_count = 2U},
                                             {.target_id = 10U,
                                              .property = AF_SHAPE_PROPERTY_GRADIENT_STOP,
                                              .sample_start = 4U,
                                              .sample_count = 2U},
                                             {.target_id = 10U,
                                              .property = AF_SHAPE_PROPERTY_GRADIENT_TRANSFORM,
                                              .sample_start = 6U,
                                              .sample_count = 2U},
                                             {.target_id = 20U,
                                              .property = AF_SHAPE_PROPERTY_MASK_ALPHA,
                                              .sample_start = 8U,
                                              .sample_count = 2U}};
    AfShapeAnimationSample samples[10] = {
        {.time = 0.0F, .value = {10.0F, 0.0F}},
        {.time = 1.0F, .value = {12.0F, 2.0F}},
        {.time = 0.0F, .value = {2.0F}},
        {.time = 1.0F, .value = {6.0F}},
        {.time = 0.0F, .value = {0.0F, 1.0F, 0.0F, 0.0F, 1.0F}},
        {.time = 1.0F, .value = {0.5F, 0.0F, 1.0F, 0.0F, 1.0F}},
        {.time = 0.0F, .value = {1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F}},
        {.time = 1.0F, .value = {1.0F, 0.0F, 0.0F, 1.0F, 5.0F, 3.0F}},
        {.time = 0.0F, .value = {1.0F}},
        {.time = 1.0F, .value = {0.5F}}};
    AfShapeAnimationData shape_animation = {.tracks = shape_tracks,
                                            .track_count = 5U,
                                            .track_capacity = 5U,
                                            .samples = samples,
                                            .sample_count = 10U,
                                            .sample_capacity = 10U};
    AfVec2 mesh_vertices[4] = {{0.0F, 0.0F}, {1.0F, 0.0F}, {1.0F, 1.0F}, {0.0F, 1.0F}};
    uint32_t mesh_indices[6] = {0U, 1U, 2U, 0U, 2U, 3U};
    AfDeformMesh mesh = {.vertices = mesh_vertices,
                         .vertex_count = 4U,
                         .vertex_capacity = 4U,
                         .indices = mesh_indices,
                         .index_count = 6U,
                         .index_capacity = 6U};
    AfShapeKey shape_keys[2] = {
        {.id = 100U, .vertex_start = 0U, .vertex_count = 4U, .delta_start = 0U},
        {.id = 101U, .vertex_start = 0U, .vertex_count = 4U, .delta_start = 4U}};
    AfVec2 deltas[8] = {{0.0F, 0.0F}, {0.5F, 0.0F},   {0.5F, 0.5F},    {0.0F, 0.5F},
                        {0.0F, 0.0F}, {-0.25F, 0.0F}, {-0.25F, 0.25F}, {0.0F, 0.25F}};
    AfShapeKeyData key_data = {.keys = shape_keys,
                               .key_count = 2U,
                               .key_capacity = 2U,
                               .deltas = deltas,
                               .delta_count = 8U,
                               .delta_capacity = 8U};
    AfShapeKeyInfluence influences[2] = {{.key_id = 100U, .weight = 0.5F},
                                         {.key_id = 101U, .weight = 0.25F}};
    AfVec2 deformed[4];
    AfVec2 interpolated[4];
    uint32_t interpolated_count = 0U;
    style.fill.type = AF_PAINT_LINEAR_GRADIENT;
    style.fill.stop_count = 1U;
    style.fill.stops[0] = (AfGradientStop){.color = {1.0F, 0.0F, 0.0F, 1.0F}};
    style.stroke.enabled = true;
    style.stroke.width = 2.0F;
    CHECK(af_path_data_validate(&path_data) == AF_RESULT_SUCCESS);
    CHECK(af_shape_morph_validate(&path_data, 1U, 2U) == AF_RESULT_SUCCESS);
    CHECK(af_shape_morph_update(&path_data, 1U, 2U, 0.5F, &morph) == AF_RESULT_SUCCESS);
    CHECK(morph.rebuild_count == 1U && morph.command_count == 4U);
    CHECK_NEAR(morphed[1].point.x, 15.0F, 0.0001F);
    CHECK(af_shape_morph_update(&path_data, 1U, 2U, 0.5F, &morph) == AF_RESULT_SUCCESS);
    CHECK(morph.rebuild_count == 1U);
    CHECK(af_trim_path(&path_data, 1U, 0.2F, 0.8F, 0.25F, &scratch, trimmed, 64U, &trimmed_count) ==
          AF_RESULT_SUCCESS);
    CHECK(trimmed_count >= 2U);
    CHECK(af_trim_path(&path_data, 3U, 0.1F, 0.9F, 0.25F, &scratch, trimmed, 64U, &trimmed_count) ==
          AF_RESULT_SUCCESS);
    CHECK(af_path_follow(&path_data, 1U, 0.5F, 0.25F, &scratch, &followed) == AF_RESULT_SUCCESS);
    CHECK(isfinite(followed.position.x) && isfinite(followed.rotation));
    CHECK(af_shape_animation_validate(&shape_animation, &path_data, &style_data, &composition) ==
          AF_RESULT_SUCCESS);
    CHECK(af_shape_animation_apply(&shape_animation, 0.5F, &path_data, &style_data, &composition) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR(commands[1].point.x, 11.0F, 0.0001F);
    CHECK_NEAR(style.stroke.width, 4.0F, 0.0001F);
    CHECK_NEAR(mask.alpha, 0.75F, 0.0001F);
    {
        const uint64_t path_generation = paths[0].geometry_generation;
        const uint64_t paint_generation = style.paint_generation;
        const uint64_t stroke_generation = style.stroke_geometry_generation;
        CHECK(af_shape_animation_apply(&shape_animation, 0.5F, &path_data, &style_data,
                                       &composition) == AF_RESULT_SUCCESS);
        CHECK(paths[0].geometry_generation == path_generation &&
              style.paint_generation == paint_generation &&
              style.stroke_geometry_generation == stroke_generation);
    }
    CHECK(af_deform_mesh_validate(&mesh, &key_data) == AF_RESULT_SUCCESS);
    CHECK(af_deform_mesh_apply(&mesh, &key_data, influences, 2U, deformed, 4U) ==
          AF_RESULT_SUCCESS);
    CHECK(deformed[1].x > mesh_vertices[1].x);
    CHECK(af_shape_key_interpolate(&key_data, 100U, 101U, 0.5F, interpolated, 4U,
                                   &interpolated_count) == AF_RESULT_SUCCESS);
    CHECK(interpolated_count == 4U && isfinite(interpolated[2].x));
    commands[5].type = AF_PATH_CUBIC;
    CHECK(af_shape_morph_validate(&path_data, 1U, 2U) == AF_RESULT_INVALID_ARGUMENT);
    return true;
}

static bool af_test_rigging_and_procedural_motion(void) {
    AfBone bones[5] = {{.id = 1U,
                        .rest_transform = {.scale = {1.0F, 1.0F}},
                        .local_transform = {.scale = {1.0F, 1.0F}},
                        .length = 10.0F},
                       {.id = 2U,
                        .parent_id = 1U,
                        .rest_transform = {.position = {10.0F, 0.0F}, .scale = {1.0F, 1.0F}},
                        .local_transform = {.position = {10.0F, 0.0F}, .scale = {1.0F, 1.0F}},
                        .length = 10.0F},
                       {.id = 3U,
                        .parent_id = 2U,
                        .rest_transform = {.position = {10.0F, 0.0F}, .scale = {1.0F, 1.0F}},
                        .local_transform = {.position = {10.0F, 0.0F}, .scale = {1.0F, 1.0F}},
                        .length = 5.0F}};
    AfSkinWeight weights[4] = {{.vertex_index = 0U, .bone_id = 1U, .weight = 1.0F},
                               {.vertex_index = 1U, .bone_id = 2U, .weight = 1.0F},
                               {.vertex_index = 2U, .bone_id = 3U, .weight = 0.75F},
                               {.vertex_index = 2U, .bone_id = 2U, .weight = 0.25F}};
    AfRigConstraint constraints[4] = {0};
    AfRigData rig = {.bones = bones,
                     .bone_count = 3U,
                     .bone_capacity = 5U,
                     .weights = weights,
                     .weight_count = 4U,
                     .weight_capacity = 4U,
                     .constraints = constraints,
                     .constraint_capacity = 4U};
    AfPath path = {.id = 50U,
                   .node_id = 1U,
                   .command_count = 3U,
                   .fill_rule = AF_PATH_FILL_NON_ZERO,
                   .geometry_generation = 7U};
    AfPathCommand path_commands[3] = {{.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}},
                                      {.type = AF_PATH_LINE, .point = {10.0F, 10.0F}},
                                      {.type = AF_PATH_LINE, .point = {20.0F, 0.0F}}};
    AfPathData path_data = {.paths = &path,
                            .path_count = 1U,
                            .path_capacity = 1U,
                            .commands = path_commands,
                            .command_count = 3U,
                            .command_capacity = 3U};
    AfPathTessellationScratch scratch;
    const AfVec2 bind_vertices[4] = {{0.0F, 0.0F}, {10.0F, 0.0F}, {22.0F, 0.0F}, {30.0F, 5.0F}};
    AfVec2 skinned[4];
    AfRigMetrics metrics = {0};
    const uint64_t path_generation = path.geometry_generation;
    CHECK(af_path_data_validate(&path_data) == AF_RESULT_SUCCESS);
    CHECK(af_rig_validate(&rig, 4U, &path_data) == AF_RESULT_SUCCESS);
    CHECK(af_rig_evaluate(&rig, &path_data, NULL, &metrics) == AF_RESULT_SUCCESS);
    CHECK(af_rig_skin(&rig, bind_vertices, 4U, skinned, 4U, &metrics) == AF_RESULT_SUCCESS);
    for (uint32_t index = 0U; index < 4U; ++index) {
        CHECK_NEAR(skinned[index].x, bind_vertices[index].x, 0.0001F);
        CHECK_NEAR(skinned[index].y, bind_vertices[index].y, 0.0001F);
    }
    CHECK(metrics.deformed_vertices == 4U);

    {
        const AfBone added = {.id = 4U,
                              .parent_id = 3U,
                              .rest_transform = {.position = {5.0F, 0.0F}, .scale = {1.0F, 1.0F}},
                              .local_transform = {.position = {5.0F, 0.0F}, .scale = {1.0F, 1.0F}},
                              .length = 2.0F};
        CHECK(af_rig_add_bone(&rig, &added) == AF_RESULT_SUCCESS);
        CHECK(af_rig_delete_bone(&rig, 3U) == AF_RESULT_INVALID_ARGUMENT);
        CHECK(af_rig_delete_bone(&rig, 4U) == AF_RESULT_SUCCESS);
        CHECK(af_rig_reparent_bone(&rig, 1U, 3U) == AF_RESULT_INVALID_ARGUMENT);
        CHECK(bones[0].parent_id == 0U);
    }

    for (uint32_t type = AF_CONSTRAINT_IK; type <= AF_CONSTRAINT_TRANSFORM_COPY; ++type) {
        for (uint32_t bone = 0U; bone < rig.bone_count; ++bone) {
            bones[bone].local_transform = bones[bone].rest_transform;
        }
        constraints[0] =
            (AfRigConstraint){.id = 10U + type,
                              .type = (AfConstraintType)type,
                              .bone_id = type == AF_CONSTRAINT_IK ? 3U : 1U,
                              .target_bone_id = type == AF_CONSTRAINT_TRANSFORM_COPY ? 2U : 0U,
                              .path_id = 50U,
                              .target_position = {15.0F, 15.0F},
                              .target_rotation = 0.4F,
                              .target_scale = {1.2F, 0.8F},
                              .distance = 5.0F,
                              .path_position = 0.5F,
                              .weight = 0.75F,
                              .chain_length = 3U,
                              .iterations = 3U};
        rig.constraint_count = 1U;
        metrics.solver_iterations = 0U;
        CHECK(af_rig_validate(&rig, 4U, &path_data) == AF_RESULT_SUCCESS);
        CHECK(af_rig_evaluate(&rig, &path_data, &scratch, &metrics) == AF_RESULT_SUCCESS);
        CHECK(metrics.solver_iterations > 0U &&
              metrics.solver_iterations <= AF_MAX_SOLVER_ITERATIONS * AF_MAX_IK_CHAIN_LENGTH);
        for (uint32_t bone = 0U; bone < rig.bone_count; ++bone) {
            CHECK(isfinite(bones[bone].world_transform.tx));
            CHECK(isfinite(bones[bone].world_transform.ty));
        }
    }

    constraints[0] = (AfRigConstraint){.id = 30U,
                                       .type = AF_CONSTRAINT_POSITION,
                                       .bone_id = 1U,
                                       .target_bone_id = 2U,
                                       .weight = 1.0F};
    constraints[1] = (AfRigConstraint){.id = 31U,
                                       .type = AF_CONSTRAINT_POSITION,
                                       .bone_id = 1U,
                                       .target_bone_id = 3U,
                                       .weight = 1.0F};
    constraints[2] = (AfRigConstraint){.id = 32U,
                                       .type = AF_CONSTRAINT_POSITION,
                                       .bone_id = 3U,
                                       .target_bone_id = 1U,
                                       .weight = 1.0F};
    rig.constraint_count = 3U;
    CHECK(af_rig_validate(&rig, 4U, &path_data) == AF_RESULT_INVALID_ARGUMENT);
    rig.constraint_count = 0U;

    {
        AfBoneKeyframe keys[4] = {
            {.bone_id = 1U, .time = 0.0F, .transform = bones[0].rest_transform},
            {.bone_id = 3U, .time = 0.0F, .transform = bones[2].rest_transform},
            {.bone_id = 1U,
             .time = 1.0F,
             .transform = {.position = {2.0F, 0.0F}, .rotation = 0.5F, .scale = {1.0F, 1.0F}}},
            {.bone_id = 3U,
             .time = 1.0F,
             .transform = {.position = {10.0F, 2.0F}, .rotation = -0.25F, .scale = {1.0F, 1.0F}}}};
        AfBoneAnimation animation = {
            .keyframes = keys, .keyframe_count = 4U, .keyframe_capacity = 4U, .duration = 1.0F};
        CHECK(af_bone_animation_validate(&animation, &rig) == AF_RESULT_SUCCESS);
        CHECK(af_bone_animation_apply(&animation, 0.5F, &rig) == AF_RESULT_SUCCESS);
        CHECK_NEAR(bones[0].local_transform.position.x, 1.0F, 0.0001F);
        CHECK_NEAR(bones[0].local_transform.rotation, 0.25F, 0.0001F);
        CHECK(af_rig_skin(&rig, bind_vertices, 4U, skinned, 4U, NULL) == AF_RESULT_SUCCESS);
        CHECK(fabsf(skinned[1].y - bind_vertices[1].y) > 0.1F);
        CHECK_NEAR(bind_vertices[1].x, 10.0F, 0.0001F);
        CHECK(path.geometry_generation == path_generation);
    }

    {
        AfProceduralTrack tracks[6] = {{.bone_id = 1U,
                                        .type = AF_PROCEDURAL_SPRING,
                                        .property = AF_PROCEDURAL_POSITION_X,
                                        .target = 3.0F,
                                        .stiffness = 30.0F,
                                        .damping = 5.0F},
                                       {.bone_id = 1U,
                                        .type = AF_PROCEDURAL_DAMPED,
                                        .property = AF_PROCEDURAL_POSITION_Y,
                                        .target = 2.0F,
                                        .stiffness = 20.0F,
                                        .damping = 8.0F},
                                       {.bone_id = 1U,
                                        .type = AF_PROCEDURAL_OSCILLATION,
                                        .property = AF_PROCEDURAL_ROTATION,
                                        .amplitude = 0.2F,
                                        .frequency = 2.0F},
                                       {.bone_id = 2U,
                                        .type = AF_PROCEDURAL_NOISE,
                                        .property = AF_PROCEDURAL_ROTATION,
                                        .amplitude = 0.1F,
                                        .seed = 1234U},
                                       {.bone_id = 3U,
                                        .source_bone_id = 2U,
                                        .type = AF_PROCEDURAL_FOLLOW_THROUGH,
                                        .property = AF_PROCEDURAL_POSITION_Y,
                                        .target = 0.5F,
                                        .stiffness = 15.0F,
                                        .damping = 4.0F},
                                       {.bone_id = 3U,
                                        .source_bone_id = 1U,
                                        .type = AF_PROCEDURAL_SECONDARY,
                                        .property = AF_PROCEDURAL_ROTATION,
                                        .stiffness = 12.0F,
                                        .damping = 3.0F}};
        AfProceduralData procedural = {.tracks = tracks, .track_count = 6U, .track_capacity = 6U};
        AfBone deterministic_bones[3];
        AfRigData deterministic_rig = rig;
        AfProceduralState first_states[6];
        AfProceduralState second_states[6];
        AfProceduralRuntime first_runtime;
        AfProceduralRuntime second_runtime;
        AfBoneKeyframe baked[64];
        uint32_t baked_count = 0U;
        for (uint32_t bone = 0U; bone < rig.bone_count; ++bone) {
            bones[bone].local_transform = bones[bone].rest_transform;
        }
        memcpy(deterministic_bones, bones, sizeof(deterministic_bones));
        deterministic_rig.bones = deterministic_bones;
        CHECK(af_procedural_validate(&procedural, &rig) == AF_RESULT_SUCCESS);
        CHECK(af_procedural_runtime_init(&first_runtime, &procedural, first_states, 6U) ==
              AF_RESULT_SUCCESS);
        CHECK(af_procedural_runtime_init(&second_runtime, &procedural, second_states, 6U) ==
              AF_RESULT_SUCCESS);
        metrics.fixed_steps = 0U;
        CHECK(af_procedural_update(&first_runtime, 1.0F, &rig, &metrics) == AF_RESULT_SUCCESS);
        CHECK(af_procedural_update(&second_runtime, 1.0F, &deterministic_rig, NULL) ==
              AF_RESULT_SUCCESS);
        CHECK(metrics.fixed_steps == AF_MAX_FIXED_STEPS_PER_UPDATE);
        CHECK(memcmp(bones, deterministic_bones, sizeof(deterministic_bones)) == 0);
        CHECK(first_runtime.tick == second_runtime.tick &&
              memcmp(first_states, second_states, sizeof(first_states)) == 0);
        CHECK(af_procedural_runtime_init(&first_runtime, &procedural, first_states, 6U) ==
              AF_RESULT_SUCCESS);
        CHECK(af_procedural_bake(&first_runtime, &rig, 0.05F, 0.01F, baked, 64U, &baked_count,
                                 &metrics) == AF_RESULT_SUCCESS);
        CHECK(baked_count > 0U && baked_count <= 64U);
        {
            AfBoneAnimation baked_animation = {.keyframes = baked,
                                               .keyframe_count = baked_count,
                                               .keyframe_capacity = 64U,
                                               .duration = 0.05F};
            CHECK(af_bone_animation_validate(&baked_animation, &rig) == AF_RESULT_SUCCESS);
        }
        CHECK(af_procedural_bake(&first_runtime, &rig, 1000.0F, 0.0001F, baked, 64U, &baked_count,
                                 NULL) == AF_RESULT_LIMIT_EXCEEDED);
    }
    CHECK(path.geometry_generation == path_generation);
    return true;
}

static uint32_t af_test_read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) | ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static uint64_t af_test_read_u64(const uint8_t *bytes) {
    return (uint64_t)af_test_read_u32(bytes) | ((uint64_t)af_test_read_u32(bytes + 4U) << 32U);
}

static void af_test_write_u32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    bytes[2] = (uint8_t)(value >> 16U);
    bytes[3] = (uint8_t)(value >> 24U);
}

static void af_test_write_u64(uint8_t *bytes, uint64_t value) {
    af_test_write_u32(bytes, (uint32_t)value);
    af_test_write_u32(bytes + 4U, (uint32_t)(value >> 32U));
}

static uint32_t af_test_checksum(const uint8_t *bytes, size_t size) {
    uint32_t hash = 2166136261U;
    size_t index = 0U;
    for (index = 0U; index < size; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

static void af_test_rechecksum(AfPackageBlob *blob) {
    af_test_write_u32(blob->data + 48U, af_test_checksum(blob->data + AF_PACKAGE_HEADER_SIZE,
                                                         blob->size - AF_PACKAGE_HEADER_SIZE));
}

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
    af_test_fixture_init(&fixture);
    af_allocator_init_default(&allocator);
    source = (AfPackageSource){.scene = &fixture.scene, .animation = &fixture.animation};
    CHECK(af_package_write(&source, &allocator, &blob) == AF_RESULT_SUCCESS);
    CHECK(af_runtime_create(&create_info, &runtime) == AF_RESULT_SUCCESS);
    CHECK(af_package_load(runtime, blob.data, blob.size, &package) == AF_RESULT_SUCCESS);
    CHECK(af_instance_create(runtime, package, &instance) == AF_RESULT_SUCCESS);
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
    {
        uint64_t emitter_id = 0U;
        CHECK(af_project_allocate_animation_id(&project, &emitter_id) == AF_RESULT_SUCCESS);
        AfParticleEmitter emitter = af_particle_emitter_default(emitter_id, shape.id);
        emitter.type = AF_PARTICLE_EMITTER_RECTANGLE;
        emitter.output = AF_PARTICLE_OUTPUT_FLIPBOOK;
        emitter.flipbook_columns = 4U;
        emitter.flipbook_rows = 2U;
        emitter.flipbook_frames_per_second = 12.0F;
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
    CHECK(af_test_project_contains(text.data, text.size, "arcflow_project 6"));
    CHECK(af_test_project_contains(text.data, text.size, "particle "));
    CHECK(af_test_project_contains(text.data, text.size, "name Editor Only Name"));
    CHECK(af_test_project_contains(text.data, text.size, "interpolation cubic_bezier"));
    CHECK(af_project_deserialize(text.data, text.size, &allocator, &loaded) == AF_RESULT_SUCCESS);
    CHECK(loaded.scene.count == 3U && loaded.canvas_size.x == 1920.0F);
    CHECK(strcmp(af_project_name(&loaded, 2U), "Editor Only Name") == 0);
    CHECK(loaded.clip_count == 1U && loaded.track_count == 1U && loaded.keyframe_count == 2U);
    CHECK(loaded.path_data.path_count == 1U && loaded.path_data.command_count == 5U);
    CHECK(af_project_find_path(&loaded, 500U)->geometry_generation == 7U);
    CHECK(af_project_find_node_style(&loaded, 3U)->fill.type == AF_PAINT_LINEAR_GRADIENT);
    CHECK(af_project_find_node_style(&loaded, 3U)->stroke.dash_count == 2U);
    CHECK(loaded.layout_data.count == 1U);
    CHECK(loaded.particle_data.count == 1U);
    CHECK(loaded.particle_data.emitters[0].output == AF_PARTICLE_OUTPUT_FLIPBOOK);
    CHECK_NEAR(af_layout_find_const(&loaded.layout_data, 3U)->width.value, 0.25F, 0.0001F);
    CHECK_NEAR(af_scene_find(&loaded.scene, 2U)->local_transform.rotation, 0.25F, 0.0001F);
    {
        AfSceneNode animated_nodes[3];
        AfScene animated = {
            .nodes = animated_nodes, .count = 3U, .capacity = 3U, .paths = &loaded.path_data};
        memcpy(animated_nodes, loaded.scene.nodes, sizeof(animated_nodes));
        af_allocator_begin_frame(&allocator);
        CHECK(af_project_animation_apply(&loaded, 100U, 0.5F, &animated) == AF_RESULT_SUCCESS);
        CHECK(af_allocator_end_frame(&allocator) == 0U);
        CHECK_NEAR(af_scene_find(&animated, 2U)->local_transform.position.x, 62.0F, 0.0001F);
        CHECK_NEAR(af_scene_find(&animated, 2U)->local_transform.position.y, 59.0F, 0.0001F);
    }
    CHECK(af_project_save_file(&loaded, path, &allocator) == AF_RESULT_SUCCESS);
    CHECK(af_project_load_file(path, &allocator, &reopened) == AF_RESULT_SUCCESS);
    CHECK(strcmp(af_project_name(&reopened, 1U), "Root Group") == 0);
    CHECK(remove(path) == 0);
    CHECK(af_project_compile(&loaded, &allocator, &package) == AF_RESULT_SUCCESS);
    CHECK(af_package_validate(package.data, package.size) == AF_RESULT_SUCCESS);
    CHECK(!af_test_project_contains(package.data, package.size, "Editor Only Name"));
    CHECK(af_package_decode(package.data, package.size, &allocator, &runtime) == AF_RESULT_SUCCESS);
    CHECK(runtime.scene.count == loaded.scene.count && runtime.animation.clip_count == 1U);
    CHECK(runtime.animation.track_count == 1U && runtime.animation.keyframe_count == 2U);
    CHECK(runtime.path_data.path_count == 1U && runtime.path_data.command_count == 5U);
    CHECK(runtime.style_data.count == loaded.style_data.count);
    CHECK(runtime.layout_data.count == 1U);
    CHECK(runtime.particle_data.count == 1U);
    CHECK(runtime.scene.particles == &runtime.particle_data);
    CHECK(af_style_find_node_const(&runtime.style_data, 3U)->stroke.end_width == 6.0F);
    af_loaded_package_destroy(&allocator, &runtime);
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
    unsupported[strlen("arcflow_project ")] = '9';
    CHECK(af_project_deserialize(unsupported, text.size, &allocator, &reopened) ==
          AF_RESULT_UNSUPPORTED_VERSION);
    af_package_blob_destroy(&allocator, &package);
    af_package_blob_destroy(&allocator, &text);
    af_project_destroy(&reopened);
    af_project_destroy(&loaded);
    af_project_destroy(&project);
    return true;
}

static bool af_test_timeline_authoring(void) {
    static const char legacy_project[] = "arcflow_project 1\n"
                                         "canvas 640 360\n"
                                         "next_id 2\n"
                                         "node\n"
                                         "id 1\n"
                                         "parent none\n"
                                         "type rectangle\n"
                                         "name Legacy Rectangle\n"
                                         "position 0 0\n"
                                         "rotation 0\n"
                                         "scale 1 1\n"
                                         "bounds 100 50\n"
                                         "fill 1 0 0 1\n"
                                         "opacity 1\n"
                                         "corner_radius 0\n"
                                         "layer 0\n"
                                         "visible 1\n"
                                         "end\n";
    AfAllocator allocator;
    AfProject project = {0};
    AfProject migrated = {0};
    AfEditorHistory history = {0};
    AfEditorCommand command = {0};
    AfSceneNode shape = {.id = 1U,
                         .parent_id = AF_INVALID_NODE_ID,
                         .type = AF_NODE_ROUNDED_RECTANGLE,
                         .local_transform = {.position = {10.0F, 20.0F}, .scale = {1.0F, 1.0F}},
                         .bounds = {100.0F, 50.0F},
                         .fill = {0.1F, 0.2F, 0.3F, 1.0F},
                         .opacity = 1.0F,
                         .corner_radius = 4.0F,
                         .visible = true};
    AfProjectClip clip = {.id = 100U,
                          .name = "Authoring Clip",
                          .duration = 4.0F,
                          .loop_start = 0.5F,
                          .loop_end = 3.5F,
                          .loop = true};
    AfProjectTrack tracks[6];
    AfProjectKeyframe keys[12];
    AfSceneNode preview_node;
    AfScene preview = {.nodes = &preview_node, .count = 1U, .capacity = 1U};
    uint64_t large_ids[AF_EDITOR_COMMAND_MAX_KEYFRAMES];
    uint32_t index = 0U;
    af_allocator_init_default(&allocator);
    CHECK(af_project_create(&project, &allocator, 4U) == AF_RESULT_SUCCESS);
    CHECK(af_project_add_node(&project, &shape, "Animated Shape") == AF_RESULT_SUCCESS);
    CHECK(af_project_add_clip(&project, &clip) == AF_RESULT_SUCCESS);
    for (index = 0U; index < 6U; ++index) {
        tracks[index] = (AfProjectTrack){.id = 101U + index,
                                         .clip_id = clip.id,
                                         .node_id = shape.id,
                                         .property = (AfPropertyType)index};
        CHECK(af_project_add_track(&project, &tracks[index]) == AF_RESULT_SUCCESS);
    }
    preview_node = shape;
    CHECK(af_project_animation_apply(&project, clip.id, 0.5F, &preview) == AF_RESULT_SUCCESS);
    CHECK_NEAR(preview_node.local_transform.position.x, 10.0F, 0.0001F);
    for (index = 0U; index < 6U; ++index) {
        AfProjectKeyframe first = {.id = 200U + index * 2U,
                                   .track_id = tracks[index].id,
                                   .keyframe = {.time = 0.0F,
                                                .interpolation = AF_INTERPOLATION_LINEAR,
                                                .easing = {0.42F, 0.0F, 0.58F, 1.0F}}};
        AfProjectKeyframe second = {
            .id = 201U + index * 2U,
            .track_id = tracks[index].id,
            .keyframe = {.time = 1.0F, .interpolation = AF_INTERPOLATION_LINEAR}};
        if (index == AF_PROPERTY_POSITION) {
            first.keyframe.value[0] = 0.0F;
            first.keyframe.value[1] = 10.0F;
            second.keyframe.value[0] = 100.0F;
            second.keyframe.value[1] = 110.0F;
        } else if (index == AF_PROPERTY_ROTATION) {
            first.keyframe.value[0] = 0.0F;
            second.keyframe.value[0] = 1.0F;
            first.keyframe.interpolation = AF_INTERPOLATION_STEP;
        } else if (index == AF_PROPERTY_SCALE) {
            first.keyframe.value[0] = 1.0F;
            first.keyframe.value[1] = 0.5F;
            second.keyframe.value[0] = 2.0F;
            second.keyframe.value[1] = 1.5F;
            first.keyframe.interpolation = AF_INTERPOLATION_CUBIC_BEZIER;
        } else if (index == AF_PROPERTY_OPACITY) {
            first.keyframe.value[0] = 0.2F;
            second.keyframe.value[0] = 1.0F;
        } else if (index == AF_PROPERTY_FILL_COLOR) {
            first.keyframe.value[0] = 0.0F;
            first.keyframe.value[1] = 0.2F;
            first.keyframe.value[2] = 0.4F;
            first.keyframe.value[3] = 1.0F;
            second.keyframe.value[0] = 1.0F;
            second.keyframe.value[1] = 0.8F;
            second.keyframe.value[2] = 0.6F;
            second.keyframe.value[3] = 0.5F;
            first.keyframe.interpolation = AF_INTERPOLATION_CUBIC_BEZIER;
        } else {
            first.keyframe.value[0] = 0.0F;
            second.keyframe.value[0] = 20.0F;
        }
        keys[index * 2U] = first;
        keys[index * 2U + 1U] = second;
        CHECK(af_project_add_keyframe(&project, &second) == AF_RESULT_SUCCESS);
        CHECK(af_project_add_keyframe(&project, &first) == AF_RESULT_SUCCESS);
    }
    af_allocator_begin_frame(&allocator);
    for (index = 0U; index < 64U; ++index) {
        preview_node = shape;
        CHECK(af_project_animation_apply(&project, clip.id, 0.5F, &preview) == AF_RESULT_SUCCESS);
        CHECK_NEAR(preview_node.local_transform.position.x, 50.0F, 0.001F);
        CHECK_NEAR(preview_node.local_transform.position.y, 60.0F, 0.001F);
        CHECK_NEAR(preview_node.local_transform.rotation, 0.0F, 0.001F);
        CHECK_NEAR(preview_node.local_transform.scale.x, 1.5F, 0.002F);
        CHECK_NEAR(preview_node.opacity, 0.6F, 0.001F);
        CHECK_NEAR(preview_node.fill.r, 0.5F, 0.002F);
        CHECK_NEAR(preview_node.corner_radius, 10.0F, 0.001F);
    }
    CHECK(af_allocator_end_frame(&allocator) == 0U);
    project.tracks[0].node_id = 999U;
    CHECK(af_project_validate(&project) == AF_RESULT_INVALID_ARGUMENT);
    project.tracks[0].node_id = shape.id;
    project.keyframes[0].keyframe.time = 5.0F;
    CHECK(af_project_validate(&project) == AF_RESULT_INVALID_ARGUMENT);
    project.keyframes[0].keyframe.time = 1.0F;
    project.keyframes[0].id = tracks[0].id;
    CHECK(af_project_validate(&project) == AF_RESULT_INVALID_ARGUMENT);
    project.keyframes[0].id = keys[1].id;
    CHECK(af_project_validate(&project) == AF_RESULT_SUCCESS);

    CHECK(af_editor_history_create(&history, &allocator, sizeof(AfEditorCommand) * 8U) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_command_animation(AF_EDITOR_MERGE_KEYFRAME_TIME, keys[0].id, &command) ==
          AF_RESULT_SUCCESS);
    CHECK(af_project_keyframe_snapshot(&project, keys[10].id, &command.keyframe_before[0]) ==
          AF_RESULT_SUCCESS);
    CHECK(af_project_keyframe_snapshot(&project, keys[0].id, &command.keyframe_before[1]) ==
          AF_RESULT_SUCCESS);
    command.keyframe_after[0] = command.keyframe_before[0];
    command.keyframe_after[1] = command.keyframe_before[1];
    command.keyframe_after[0].keyframe.keyframe.time = 2.0F;
    command.keyframe_after[1].keyframe.keyframe.time = 0.25F;
    command.keyframe_before_count = 2U;
    command.keyframe_after_count = 2U;
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR(af_project_find_keyframe(&project, keys[10].id)->keyframe.time, 2.0F, 0.0001F);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK_NEAR(af_project_find_keyframe(&project, keys[10].id)->keyframe.time, 0.0F, 0.0001F);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK_NEAR(af_project_find_keyframe(&project, keys[0].id)->keyframe.time, 0.25F, 0.0001F);

    CHECK(af_editor_command_delete(&project, shape.id, &command) == AF_RESULT_SUCCESS);
    CHECK(command.track_before_count == 6U && command.keyframe_before_count == 12U);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(project.scene.count == 0U && project.track_count == 0U && project.keyframe_count == 0U);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.scene.count == 1U && project.track_count == 6U && project.keyframe_count == 12U);

    for (index = 0U; index < AF_EDITOR_COMMAND_MAX_KEYFRAMES; ++index) {
        AfProjectKeyframe key = {.track_id = tracks[0].id,
                                 .keyframe = {.time = (float)(index % 200U) * 0.02F,
                                              .value = {(float)index, 0.0F},
                                              .interpolation = AF_INTERPOLATION_LINEAR}};
        CHECK(af_project_allocate_animation_id(&project, &key.id) == AF_RESULT_SUCCESS);
        large_ids[index] = key.id;
        CHECK(af_project_add_keyframe(&project, &key) == AF_RESULT_SUCCESS);
    }
    CHECK(af_editor_command_animation(AF_EDITOR_MERGE_KEYFRAME_TIME, large_ids[0], &command) ==
          AF_RESULT_SUCCESS);
    for (index = 0U; index < AF_EDITOR_COMMAND_MAX_KEYFRAMES; ++index) {
        CHECK(af_project_keyframe_snapshot(&project,
                                           large_ids[AF_EDITOR_COMMAND_MAX_KEYFRAMES - index - 1U],
                                           &command.keyframe_before[index]) == AF_RESULT_SUCCESS);
        command.keyframe_after[index] = command.keyframe_before[index];
        command.keyframe_after[index].keyframe.keyframe.time =
            4.0F - command.keyframe_after[index].keyframe.keyframe.time;
    }
    command.keyframe_before_count = AF_EDITOR_COMMAND_MAX_KEYFRAMES;
    command.keyframe_after_count = AF_EDITOR_COMMAND_MAX_KEYFRAMES;
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.keyframe_count == 12U + AF_EDITOR_COMMAND_MAX_KEYFRAMES);
    CHECK(af_project_validate(&project) == AF_RESULT_SUCCESS);

    CHECK(af_project_deserialize(legacy_project, strlen(legacy_project), &allocator, &migrated) ==
          AF_RESULT_SUCCESS);
    CHECK(migrated.scene.count == 1U && migrated.clip_count == 0U &&
          migrated.next_animation_id == 1U);
    af_project_destroy(&migrated);
    af_editor_history_destroy(&history);
    af_project_destroy(&project);
    return true;
}

static bool af_test_editor_commands(void) {
    AfAllocator allocator;
    AfProject project = {0};
    AfEditorHistory history = {0};
    AfProjectNodeSnapshot snapshot = {0};
    AfProjectNodeSnapshot before = {0};
    AfProjectNodeSnapshot after = {0};
    AfEditorCommand command = {0};
    AfProjectNodeSnapshot duplicate = {0};
    AfSceneNode root = {.id = 1U,
                        .parent_id = AF_INVALID_NODE_ID,
                        .type = AF_NODE_GROUP,
                        .local_transform = {.scale = {1.0F, 1.0F}},
                        .opacity = 1.0F,
                        .visible = true};
    AfSceneNode rectangle = {.id = 2U,
                             .parent_id = 1U,
                             .type = AF_NODE_RECTANGLE,
                             .local_transform = {.position = {20.0F, 30.0F}, .scale = {1.0F, 1.0F}},
                             .bounds = {100.0F, 60.0F},
                             .fill = {0.2F, 0.4F, 0.8F, 1.0F},
                             .opacity = 1.0F,
                             .visible = true};
    const size_t history_limit = sizeof(AfEditorCommand) * 3U;
    uint32_t count_before_merge = 0U;
    af_allocator_init_default(&allocator);
    CHECK(af_project_create(&project, &allocator, 16U) == AF_RESULT_SUCCESS);
    CHECK(af_project_add_node(&project, &root, "Root") == AF_RESULT_SUCCESS);
    CHECK(af_project_add_node(&project, &rectangle, "Rectangle") == AF_RESULT_SUCCESS);
    CHECK(af_scene_evaluate_world(&project.scene) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_create(&history, &allocator, history_limit) == AF_RESULT_SUCCESS);
    snapshot = (AfProjectNodeSnapshot){.node = {.id = 3U,
                                                .parent_id = 1U,
                                                .type = AF_NODE_ELLIPSE,
                                                .local_transform = {.scale = {1.0F, 1.0F}},
                                                .bounds = {40.0F, 40.0F},
                                                .fill = {1.0F, 0.2F, 0.3F, 1.0F},
                                                .opacity = 1.0F,
                                                .visible = true},
                                       .index = 2U};
    (void)snprintf(snapshot.name, sizeof(snapshot.name), "Ellipse");
    CHECK(af_editor_command_create(&snapshot, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 3U) != NULL);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 3U) == NULL);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 3U) != NULL);
    CHECK(af_project_snapshot(&project, 2U, &before) == AF_RESULT_SUCCESS);
    af_scene_find(&project.scene, 2U)->local_transform.position.x = 31.0F;
    CHECK(af_project_snapshot(&project, 2U, &after) == AF_RESULT_SUCCESS);
    CHECK(af_editor_command_modify(&before, &after, AF_EDITOR_MERGE_TRANSFORM, &command) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, true, true) == AF_RESULT_SUCCESS);
    count_before_merge = history.count;
    before = after;
    af_scene_find(&project.scene, 2U)->local_transform.position.x = 45.0F;
    CHECK(af_project_snapshot(&project, 2U, &after) == AF_RESULT_SUCCESS);
    CHECK(af_editor_command_modify(&before, &after, AF_EDITOR_MERGE_TRANSFORM, &command) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, true, true) == AF_RESULT_SUCCESS);
    CHECK(history.count == count_before_merge);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK_NEAR(af_scene_find(&project.scene, 2U)->local_transform.position.x, 20.0F, 0.0001F);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK_NEAR(af_scene_find(&project.scene, 2U)->local_transform.position.x, 45.0F, 0.0001F);
    CHECK(af_project_snapshot(&project, 2U, &before) == AF_RESULT_SUCCESS);
    af_scene_find(&project.scene, 2U)->parent_id = AF_INVALID_NODE_ID;
    af_scene_find(&project.scene, 2U)->fill = (AfColor){0.8F, 0.3F, 0.1F, 1.0F};
    af_scene_find(&project.scene, 2U)->opacity = 0.4F;
    af_scene_find(&project.scene, 2U)->corner_radius = 9.0F;
    af_scene_find(&project.scene, 2U)->layer_order = 7;
    af_scene_find(&project.scene, 2U)->local_transform.rotation = 0.3F;
    af_scene_find(&project.scene, 2U)->local_transform.scale = (AfVec2){1.2F, 0.8F};
    CHECK(af_project_set_name(&project, 2U, "Renamed Rectangle") == AF_RESULT_SUCCESS);
    CHECK(af_scene_evaluate_world(&project.scene) == AF_RESULT_SUCCESS);
    CHECK(af_project_snapshot(&project, 2U, &after) == AF_RESULT_SUCCESS);
    CHECK(af_editor_command_modify(&before, &after, AF_EDITOR_MERGE_NONE, &command) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, true, false) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 2U)->parent_id == 1U);
    CHECK(strcmp(af_project_name(&project, 2U), "Rectangle") == 0);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 2U)->parent_id == AF_INVALID_NODE_ID);
    CHECK(strcmp(af_project_name(&project, 2U), "Renamed Rectangle") == 0);
    CHECK_NEAR(af_scene_find(&project.scene, 2U)->opacity, 0.4F, 0.0001F);
    duplicate = after;
    duplicate.node.id = 4U;
    duplicate.node.parent_id = 1U;
    duplicate.index = project.scene.count;
    (void)snprintf(duplicate.name, sizeof(duplicate.name), "Rectangle Copy");
    CHECK(af_editor_command_duplicate(&duplicate, 1U, &command) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 4U) != NULL);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 4U) == NULL);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 4U) != NULL);
    CHECK(af_scene_reparent(&project.scene, 2U, 1U) == AF_RESULT_SUCCESS);
    CHECK(af_editor_command_delete(&project, 1U, &command) == AF_RESULT_SUCCESS);
    CHECK(command.before_count == 4U);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(project.scene.count == 0U);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.scene.count == 4U && af_scene_find(&project.scene, 2U) != NULL);
    command = (AfEditorCommand){.type = AF_EDITOR_COMMAND_MODIFY,
                                .merge_key = AF_EDITOR_MERGE_TRANSFORM,
                                .target_id = 2U,
                                .before_count = 2U,
                                .after_count = 2U};
    CHECK(af_project_snapshot(&project, 2U, &command.before[0]) == AF_RESULT_SUCCESS);
    CHECK(af_project_snapshot(&project, 3U, &command.before[1]) == AF_RESULT_SUCCESS);
    af_scene_find(&project.scene, 2U)->local_transform.position.x += 12.0F;
    af_scene_find(&project.scene, 3U)->local_transform.position.y += 18.0F;
    CHECK(af_project_snapshot(&project, 2U, &command.after[0]) == AF_RESULT_SUCCESS);
    CHECK(af_project_snapshot(&project, 3U, &command.after[1]) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_execute(&history, &project, &command, true, false) ==
          AF_RESULT_SUCCESS);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK_NEAR(af_scene_find(&project.scene, 2U)->local_transform.position.x,
               command.before[0].node.local_transform.position.x, 0.0001F);
    CHECK_NEAR(af_scene_find(&project.scene, 3U)->local_transform.position.y,
               command.before[1].node.local_transform.position.y, 0.0001F);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK_NEAR(af_scene_find(&project.scene, 2U)->local_transform.position.x,
               command.after[0].node.local_transform.position.x, 0.0001F);
    CHECK_NEAR(af_scene_find(&project.scene, 3U)->local_transform.position.y,
               command.after[1].node.local_transform.position.y, 0.0001F);
    snapshot = (AfProjectNodeSnapshot){.node = {.id = 5U,
                                                .parent_id = 1U,
                                                .type = AF_NODE_PATH,
                                                .local_transform = {.scale = {1.0F, 1.0F}},
                                                .bounds = {100.0F, 100.0F},
                                                .fill = {0.7F, 0.3F, 0.9F, 1.0F},
                                                .opacity = 1.0F,
                                                .visible = true},
                                       .index = project.scene.count};
    (void)snprintf(snapshot.name, sizeof(snapshot.name), "Undo Path");
    CHECK(af_editor_command_create(&snapshot, &command) == AF_RESULT_SUCCESS);
    command.path_after_count = 1U;
    command.path_after[0] =
        (AfProjectPathSnapshot){.path = {.id = 50U,
                                         .node_id = 5U,
                                         .command_start = project.path_data.command_count,
                                         .command_count = 4U,
                                         .fill_rule = AF_PATH_FILL_NON_ZERO,
                                         .geometry_generation = 1U},
                                .index = project.path_data.path_count};
    command.path_command_after_count = 4U;
    command.path_command_after[0] = (AfPathCommand){.type = AF_PATH_MOVE, .point = {0.0F, 0.0F}};
    command.path_command_after[1] = (AfPathCommand){.type = AF_PATH_LINE, .point = {100.0F, 0.0F}};
    command.path_command_after[2] = (AfPathCommand){.type = AF_PATH_LINE, .point = {50.0F, 100.0F}};
    command.path_command_after[3] = (AfPathCommand){.type = AF_PATH_CLOSE};
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(af_project_find_path(&project, 50U) != NULL &&
          af_project_validate(&project) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 5U) == NULL && af_project_find_path(&project, 50U) == NULL);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(af_scene_find(&project.scene, 5U) != NULL && af_project_find_path(&project, 50U) != NULL);
    CHECK(af_editor_command_path(AF_EDITOR_MERGE_PATH_POINT, 50U, &command) == AF_RESULT_SUCCESS);
    CHECK(af_project_path_snapshot(&project, 50U, &command.path_before[0],
                                   command.path_command_before,
                                   AF_EDITOR_COMMAND_MAX_PATH_COMMANDS) == AF_RESULT_SUCCESS);
    command.path_before_count = 1U;
    command.path_command_before_count = 4U;
    command.path_after_count = 1U;
    command.path_after[0] = command.path_before[0];
    ++command.path_after[0].path.geometry_generation;
    command.path_command_after_count = 4U;
    memcpy(command.path_command_after, command.path_command_before,
           4U * sizeof(*command.path_command_after));
    command.path_command_after[1].point.x = 125.0F;
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK_NEAR(project.path_data.commands[1].point.x, 125.0F, 0.0001F);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK_NEAR(project.path_data.commands[1].point.x, 100.0F, 0.0001F);
    CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK_NEAR(project.path_data.commands[1].point.x, 125.0F, 0.0001F);
    {
        AfParticleEmitter emitter = af_particle_emitter_default(900U, 5U);
        emitter.type = AF_PARTICLE_EMITTER_VECTOR_PATH;
        emitter.path_id = 50U;
        CHECK(af_project_add_emitter(&project, &emitter) == AF_RESULT_SUCCESS);
        CHECK(af_project_snapshot(&project, 5U, &before) == AF_RESULT_SUCCESS);
        CHECK(before.has_emitter);
        CHECK(af_project_remove_emitter(&project, emitter.id) == AF_RESULT_SUCCESS);
        CHECK(af_project_snapshot(&project, 5U, &after) == AF_RESULT_SUCCESS);
        CHECK(!after.has_emitter);
        CHECK(af_editor_command_modify(&before, &after, AF_EDITOR_MERGE_PARTICLE, &command) ==
              AF_RESULT_SUCCESS);
        CHECK(af_editor_history_execute(&history, &project, &command, true, false) ==
              AF_RESULT_SUCCESS);
        CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(af_project_find_node_emitter(&project, 5U) != NULL);
        CHECK(af_editor_history_redo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(af_project_find_node_emitter(&project, 5U) == NULL);
        CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
        CHECK(af_project_find_node_emitter(&project, 5U) != NULL);
    }
    CHECK(af_scene_reparent(&project.scene, 2U, AF_INVALID_NODE_ID) == AF_RESULT_SUCCESS);
    {
        const uint64_t selected_roots[2] = {1U, 2U};
        CHECK(af_editor_command_delete_many(&project, selected_roots, 2U, &command) ==
              AF_RESULT_SUCCESS);
    }
    CHECK(command.path_before_count == 1U && command.path_command_before_count == 4U);
    CHECK(af_editor_history_execute(&history, &project, &command, false, false) ==
          AF_RESULT_SUCCESS);
    CHECK(project.scene.count == 0U && project.path_data.path_count == 0U);
    CHECK(af_editor_history_undo(&history, &project) == AF_RESULT_SUCCESS);
    CHECK(project.scene.count == 5U && project.path_data.path_count == 1U &&
          af_project_find_node_emitter(&project, 5U) != NULL &&
          af_project_validate(&project) == AF_RESULT_SUCCESS);
    CHECK(af_editor_history_memory_used(&history) <= history_limit);
    CHECK(history.capacity == 3U);
    af_editor_history_clear(&history);
    CHECK(!af_editor_history_can_undo(&history) && !af_editor_history_can_redo(&history));
    af_editor_history_destroy(&history);
    af_project_destroy(&project);
    return true;
}

#if defined(AF_BUILD_GPU_TESTS)
static bool af_test_load_shader(const char *path, uint32_t **out_code, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    long length = 0L;
    uint32_t *code = NULL;
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) <= 0L ||
        (length & 3L) != 0L || fseek(file, 0L, SEEK_SET) != 0) {
        if (file != NULL) {
            (void)fclose(file);
        }
        return false;
    }
    code = malloc((size_t)length);
    if (code == NULL || fread(code, 1U, (size_t)length, file) != (size_t)length) {
        free(code);
        (void)fclose(file);
        return false;
    }
    if (fclose(file) != 0) {
        free(code);
        return false;
    }
    *out_code = code;
    *out_size = (size_t)length;
    return true;
}
#endif

static bool af_test_gpu_smoke(void) {
#if defined(AF_BUILD_GPU_TESTS)
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    VkShaderModule vertex_shader = VK_NULL_HANDLE;
    VkShaderModule fragment_shader = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    uint32_t *vertex_code = NULL;
    uint32_t *fragment_code = NULL;
    size_t vertex_size = 0U;
    size_t fragment_size = 0U;
    uint32_t graphics_family = UINT32_MAX;
    bool passed = false;
    VkApplicationInfo application = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                     .pApplicationName = "Arcflow Tests",
                                     .apiVersion = VK_API_VERSION_1_3};
    VkInstanceCreateInfo instance_info = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                          .pApplicationInfo = &application};
    if (volkInitialize() != VK_SUCCESS ||
        vkCreateInstance(&instance_info, NULL, &instance) != VK_SUCCESS) {
        (void)printf("[SKIP] gpu smoke (Vulkan 1.3 unavailable)\n");
        return true;
    }
    volkLoadInstance(instance);
    {
        VkPhysicalDevice devices[16];
        uint32_t device_count = 16U;
        uint32_t device_index = 0U;
        if (vkEnumeratePhysicalDevices(instance, &device_count, devices) != VK_SUCCESS) {
            goto cleanup;
        }
        for (device_index = 0U; device_index < device_count; ++device_index) {
            VkPhysicalDeviceProperties properties;
            VkPhysicalDeviceVulkan13Features features13 = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
            VkPhysicalDeviceVulkan12Features features12 = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
                .pNext = &features13};
            VkPhysicalDeviceFeatures2 features = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &features12};
            VkQueueFamilyProperties families[64];
            uint32_t family_count = 64U;
            uint32_t family_index = 0U;
            vkGetPhysicalDeviceProperties(devices[device_index], &properties);
            vkGetPhysicalDeviceFeatures2(devices[device_index], &features);
            if (properties.apiVersion < VK_API_VERSION_1_3 || !features13.dynamicRendering ||
                !features13.synchronization2 || !features12.timelineSemaphore) {
                continue;
            }
            vkGetPhysicalDeviceQueueFamilyProperties(devices[device_index], &family_count,
                                                     families);
            for (family_index = 0U; family_index < family_count; ++family_index) {
                if ((families[family_index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
                    physical_device = devices[device_index];
                    graphics_family = family_index;
                    break;
                }
            }
            if (physical_device != VK_NULL_HANDLE) {
                break;
            }
        }
    }
    if (physical_device == VK_NULL_HANDLE) {
        (void)printf("[SKIP] gpu smoke (required Vulkan features unavailable)\n");
        passed = true;
        goto cleanup;
    }
    {
        float priority = 1.0F;
        VkDeviceQueueCreateInfo queue_info = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                              .queueFamilyIndex = graphics_family,
                                              .queueCount = 1U,
                                              .pQueuePriorities = &priority};
        VkPhysicalDeviceVulkan13Features features13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .synchronization2 = VK_TRUE,
            .dynamicRendering = VK_TRUE};
        VkPhysicalDeviceVulkan12Features features12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &features13,
            .timelineSemaphore = VK_TRUE};
        VkDeviceCreateInfo device_info = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                          .pNext = &features12,
                                          .queueCreateInfoCount = 1U,
                                          .pQueueCreateInfos = &queue_info};
        if (vkCreateDevice(physical_device, &device_info, NULL, &device) != VK_SUCCESS) {
            goto cleanup;
        }
    }
    volkLoadDevice(device);
    vkGetDeviceQueue(device, graphics_family, 0U, &queue);
    if (queue == VK_NULL_HANDLE ||
        !af_test_load_shader(ARCFLOW_TEST_VERTEX_SHADER, &vertex_code, &vertex_size) ||
        !af_test_load_shader(ARCFLOW_TEST_FRAGMENT_SHADER, &fragment_code, &fragment_size)) {
        goto cleanup;
    }
    {
        VkShaderModuleCreateInfo vertex_info = {.sType =
                                                    VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                                .codeSize = vertex_size,
                                                .pCode = vertex_code};
        VkShaderModuleCreateInfo fragment_info = {.sType =
                                                      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                                  .codeSize = fragment_size,
                                                  .pCode = fragment_code};
        if (vkCreateShaderModule(device, &vertex_info, NULL, &vertex_shader) != VK_SUCCESS ||
            vkCreateShaderModule(device, &fragment_info, NULL, &fragment_shader) != VK_SUCCESS) {
            goto cleanup;
        }
    }
    {
        VkPushConstantRange push = {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                                    .size = 2U * sizeof(float)};
        VkPipelineLayoutCreateInfo layout_info = {.sType =
                                                      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                  .pushConstantRangeCount = 1U,
                                                  .pPushConstantRanges = &push};
        VkPipelineShaderStageCreateInfo stages[2] = {
            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
             .stage = VK_SHADER_STAGE_VERTEX_BIT,
             .module = vertex_shader,
             .pName = "main"},
            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
             .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
             .module = fragment_shader,
             .pName = "main"}};
        VkVertexInputBindingDescription binding = {
            .binding = 0U, .stride = 68U, .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE};
        VkVertexInputAttributeDescription attributes[4] = {
            {.location = 0U, .format = VK_FORMAT_R32G32B32A32_SFLOAT},
            {.location = 1U, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = 16U},
            {.location = 2U, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = 32U},
            {.location = 3U, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = 48U}};
        VkPipelineVertexInputStateCreateInfo vertex_input = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1U,
            .pVertexBindingDescriptions = &binding,
            .vertexAttributeDescriptionCount = 4U,
            .pVertexAttributeDescriptions = attributes};
        VkPipelineInputAssemblyStateCreateInfo assembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
        VkPipelineViewportStateCreateInfo viewport = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1U,
            .scissorCount = 1U};
        VkPipelineRasterizationStateCreateInfo raster = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .lineWidth = 1.0F};
        VkPipelineMultisampleStateCreateInfo multisample = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
        VkPipelineColorBlendAttachmentState attachment = {
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
        VkPipelineColorBlendStateCreateInfo blend = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1U,
            .pAttachments = &attachment};
        VkDynamicState states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = 2U,
            .pDynamicStates = states};
        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
        VkPipelineRenderingCreateInfo rendering = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1U,
            .pColorAttachmentFormats = &format};
        VkGraphicsPipelineCreateInfo pipeline_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &rendering,
            .stageCount = 2U,
            .pStages = stages,
            .pVertexInputState = &vertex_input,
            .pInputAssemblyState = &assembly,
            .pViewportState = &viewport,
            .pRasterizationState = &raster,
            .pMultisampleState = &multisample,
            .pColorBlendState = &blend,
            .pDynamicState = &dynamic};
        if (vkCreatePipelineLayout(device, &layout_info, NULL, &layout) != VK_SUCCESS) {
            goto cleanup;
        }
        pipeline_info.layout = layout;
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1U, &pipeline_info, NULL,
                                      &pipeline) != VK_SUCCESS) {
            goto cleanup;
        }
    }
    passed = true;

cleanup:
    free(fragment_code);
    free(vertex_code);
    if (device != VK_NULL_HANDLE) {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline, NULL);
        }
        if (layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, layout, NULL);
        }
        if (fragment_shader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, fragment_shader, NULL);
        }
        if (vertex_shader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, vertex_shader, NULL);
        }
        vkDestroyDevice(device, NULL);
    }
    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, NULL);
    }
    return passed;
#else
    (void)printf("[SKIP] gpu smoke (disabled)\n");
    return true;
#endif
}

typedef bool (*AfTestFunction)(void);

typedef struct AfTestCase {
    const char *name;
    AfTestFunction function;
} AfTestCase;

int main(void) {
    static const AfTestCase tests[] = {
        {"core", af_test_core},
        {"file limits", af_test_file_limit},
        {"PNG image export", af_test_png_writer},
        {"math and transforms", af_test_math},
        {"scene hierarchy", af_test_scene},
        {"GPU particle data", af_test_particles},
        {"animation", af_test_animation},
        {"vector paths", af_test_paths},
        {"vector styling", af_test_vector_styling},
        {"responsive layout", af_test_responsive_layout},
        {"text shaping and cache", af_test_text_shaping},
        {"reusable components", af_test_components},
        {"interaction and events", af_test_interaction},
        {"animation state machines", af_test_state_machines},
        {"advanced animation", af_test_advanced_animation},
        {"masks and composition", af_test_composition},
        {"shape animation and deformation", af_test_shape_deformation},
        {"rigging and procedural motion", af_test_rigging_and_procedural_motion},
        {"runtime package", af_test_package},
        {"runtime handles and allocations", af_test_runtime},
        {"editable projects", af_test_project},
        {"timeline authoring", af_test_timeline_authoring},
        {"editor commands", af_test_editor_commands},
        {"Vulkan pipeline smoke", af_test_gpu_smoke}};
    size_t index = 0U;
    size_t passed = 0U;
    for (index = 0U; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        if (tests[index].function()) {
            ++passed;
            (void)printf("[PASS] %s\n", tests[index].name);
        } else {
            (void)printf("[FAIL] %s\n", tests[index].name);
        }
    }
    (void)printf("%zu/%zu tests passed\n", passed, sizeof(tests) / sizeof(tests[0]));
    return passed == sizeof(tests) / sizeof(tests[0]) ? 0 : 1;
}
