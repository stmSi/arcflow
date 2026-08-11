#include "animation/animation.h"
#include "assets/package.h"
#include "core/allocator.h"
#include "core/file_io.h"
#include "project/project.h"
#include "scene/scene.h"

#include <stdio.h>
#include <string.h>

static AfKeyframe af_key(float time, float x, float y, float z, float w,
                         AfInterpolation interpolation) {
    return (AfKeyframe){.time = time,
                        .value = {x, y, z, w},
                        .interpolation = interpolation,
                        .easing = {0.42F, 0.0F, 0.58F, 1.0F}};
}

static AfResult af_write_demo(const char *path) {
    AfAllocator allocator = {0};
    AfSceneNode nodes[5] = {
        {.id = 1U,
         .parent_id = AF_INVALID_NODE_ID,
         .type = AF_NODE_GROUP,
         .local_transform = {.position = {180.0F, 120.0F}, .scale = {1.0F, 1.0F}},
         .opacity = 1.0F,
         .visible = true},
        {.id = 2U,
         .parent_id = 1U,
         .type = AF_NODE_RECTANGLE,
         .local_transform = {.position = {0.0F, 0.0F}, .scale = {1.0F, 1.0F}},
         .bounds = {260.0F, 140.0F},
         .fill = {0.15F, 0.55F, 0.95F, 1.0F},
         .opacity = 1.0F,
         .layer_order = 0,
         .visible = true},
        {.id = 3U,
         .parent_id = 1U,
         .type = AF_NODE_ROUNDED_RECTANGLE,
         .local_transform = {.position = {350.0F, 100.0F}, .scale = {1.0F, 1.0F}},
         .bounds = {250.0F, 150.0F},
         .fill = {0.95F, 0.35F, 0.25F, 1.0F},
         .opacity = 1.0F,
         .corner_radius = 32.0F,
         .layer_order = 1,
         .visible = true},
        {.id = 4U,
         .parent_id = 3U,
         .type = AF_NODE_ELLIPSE,
         .local_transform = {.position = {70.0F, 210.0F}, .scale = {1.0F, 1.0F}},
         .bounds = {150.0F, 150.0F},
         .fill = {0.35F, 0.9F, 0.55F, 1.0F},
         .opacity = 1.0F,
         .layer_order = 2,
         .visible = true},
        {.id = 5U,
         .parent_id = 1U,
         .type = AF_NODE_PATH,
         .local_transform = {.position = {700.0F, 70.0F}, .scale = {1.0F, 1.0F}},
         .bounds = {240.0F, 220.0F},
         .fill = {0.78F, 0.35F, 0.96F, 1.0F},
         .opacity = 0.92F,
         .layer_order = 3,
         .visible = true}};
    AfPath vector_path = {.id = 200U,
                          .node_id = 5U,
                          .command_count = 10U,
                          .fill_rule = AF_PATH_FILL_EVEN_ODD,
                          .geometry_generation = 1U};
    AfPathCommand path_commands[10] = {{.type = AF_PATH_MOVE, .point = {120.0F, 220.0F}},
                                       {.type = AF_PATH_CUBIC,
                                        .point = {0.0F, 70.0F},
                                        .control1 = {45.0F, 180.0F},
                                        .control2 = {0.0F, 140.0F}},
                                       {.type = AF_PATH_CUBIC,
                                        .point = {120.0F, 40.0F},
                                        .control1 = {0.0F, 0.0F},
                                        .control2 = {70.0F, 10.0F}},
                                       {.type = AF_PATH_CUBIC,
                                        .point = {240.0F, 70.0F},
                                        .control1 = {170.0F, 10.0F},
                                        .control2 = {240.0F, 0.0F}},
                                       {.type = AF_PATH_CUBIC,
                                        .point = {120.0F, 220.0F},
                                        .control1 = {240.0F, 140.0F},
                                        .control2 = {195.0F, 180.0F}},
                                       {.type = AF_PATH_CLOSE},
                                       {.type = AF_PATH_MOVE, .point = {90.0F, 90.0F}},
                                       {.type = AF_PATH_LINE, .point = {150.0F, 90.0F}},
                                       {.type = AF_PATH_LINE, .point = {120.0F, 140.0F}},
                                       {.type = AF_PATH_CLOSE}};
    AfPathData path_data = {.paths = &vector_path,
                            .path_count = 1U,
                            .path_capacity = 1U,
                            .commands = path_commands,
                            .command_count = 10U,
                            .command_capacity = 10U};
    AfParticleEmitter particle = af_particle_emitter_default(300U, 5U);
    AfParticleData particle_data = {.emitters = &particle, .count = 1U, .capacity = 1U};
    AfAnimationClip clips[1] = {
        {.id = 100U, .duration = 4.0F, .track_start = 0U, .track_count = 4U, .loop = true}};
    AfAnimationTrack tracks[4] = {{.id = 101U,
                                   .node_id = 2U,
                                   .property = AF_PROPERTY_POSITION,
                                   .keyframe_start = 0U,
                                   .keyframe_count = 3U},
                                  {.id = 102U,
                                   .node_id = 3U,
                                   .property = AF_PROPERTY_ROTATION,
                                   .keyframe_start = 3U,
                                   .keyframe_count = 3U},
                                  {.id = 103U,
                                   .node_id = 4U,
                                   .property = AF_PROPERTY_OPACITY,
                                   .keyframe_start = 6U,
                                   .keyframe_count = 3U},
                                  {.id = 104U,
                                   .node_id = 2U,
                                   .property = AF_PROPERTY_FILL_COLOR,
                                   .keyframe_start = 9U,
                                   .keyframe_count = 3U}};
    AfKeyframe keys[12] = {af_key(0.0F, 0.0F, 0.0F, 0.0F, 0.0F, AF_INTERPOLATION_CUBIC_BEZIER),
                           af_key(2.0F, 80.0F, 170.0F, 0.0F, 0.0F, AF_INTERPOLATION_CUBIC_BEZIER),
                           af_key(4.0F, 0.0F, 0.0F, 0.0F, 0.0F, AF_INTERPOLATION_LINEAR),
                           af_key(0.0F, 0.0F, 0.0F, 0.0F, 0.0F, AF_INTERPOLATION_LINEAR),
                           af_key(2.0F, 0.35F, 0.0F, 0.0F, 0.0F, AF_INTERPOLATION_LINEAR),
                           af_key(4.0F, 0.0F, 0.0F, 0.0F, 0.0F, AF_INTERPOLATION_LINEAR),
                           af_key(0.0F, 1.0F, 0.0F, 0.0F, 0.0F, AF_INTERPOLATION_LINEAR),
                           af_key(2.0F, 0.2F, 0.0F, 0.0F, 0.0F, AF_INTERPOLATION_LINEAR),
                           af_key(4.0F, 1.0F, 0.0F, 0.0F, 0.0F, AF_INTERPOLATION_LINEAR),
                           af_key(0.0F, 0.15F, 0.55F, 0.95F, 1.0F, AF_INTERPOLATION_LINEAR),
                           af_key(2.0F, 0.72F, 0.28F, 0.95F, 1.0F, AF_INTERPOLATION_LINEAR),
                           af_key(4.0F, 0.15F, 0.55F, 0.95F, 1.0F, AF_INTERPOLATION_LINEAR)};
    AfScene scene = {.nodes = nodes,
                     .count = 5U,
                     .capacity = 5U,
                     .paths = &path_data,
                     .particles = &particle_data};
    AfAnimationData animation = {.clips = clips,
                                 .clip_count = 1U,
                                 .tracks = tracks,
                                 .track_count = 4U,
                                 .keyframes = keys,
                                 .keyframe_count = 12U};
    AfPackageSource source = {.scene = &scene, .animation = &animation};
    AfPackageBlob blob = {0};
    AfResult result = AF_RESULT_SUCCESS;
    particle.type = AF_PARTICLE_EMITTER_VECTOR_PATH;
    particle.path_id = vector_path.id;
    particle.output = AF_PARTICLE_OUTPUT_UI_BILLBOARD;
    particle.spawn_rate = 90.0F;
    particle.capacity = 4096U;
    af_allocator_init_default(&allocator);
    result = af_package_write(&source, &allocator, &blob);
    if (result == AF_RESULT_SUCCESS) {
        result = af_file_write(path, blob.data, blob.size);
    }
    af_package_blob_destroy(&allocator, &blob);
    return result;
}

static AfResult af_validate_file(const char *path) {
    AfAllocator allocator = {0};
    AfFileData file = {0};
    AfResult result = AF_RESULT_SUCCESS;
    af_allocator_init_default(&allocator);
    result = af_file_read_limited(path, AF_PACKAGE_MAX_SIZE, &allocator, &file);
    if (result == AF_RESULT_SUCCESS) {
        result = af_package_validate(file.data, file.size);
    }
    af_file_data_destroy(&allocator, &file);
    return result;
}

static AfResult af_compile_project(const char *input_path, const char *output_path) {
    AfAllocator allocator = {0};
    AfProject project = {0};
    AfPackageBlob package = {0};
    AfResult result = AF_RESULT_SUCCESS;
    af_allocator_init_default(&allocator);
    result = af_project_load_file(input_path, &allocator, &project);
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_compile(&project, &allocator, &package);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_file_write(output_path, package.data, package.size);
    }
    af_package_blob_destroy(&allocator, &package);
    af_project_destroy(&project);
    return result;
}

int main(int argument_count, char **arguments) {
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    if (argument_count == 3 && strcmp(arguments[1], "--demo") == 0) {
        result = af_write_demo(arguments[2]);
    } else if (argument_count == 3 && strcmp(arguments[1], "--validate") == 0) {
        result = af_validate_file(arguments[2]);
    } else if (argument_count == 3) {
        result = af_compile_project(arguments[1], arguments[2]);
    } else {
        (void)fprintf(stderr,
                      "usage: arcflow_compiler INPUT.afproj OUTPUT.afp | --demo OUTPUT.afp | "
                      "--validate INPUT.afp\n");
        return 2;
    }
    if (result != AF_RESULT_SUCCESS) {
        (void)fprintf(stderr, "Arcflow Compiler: %s\n", af_result_string(result));
        return 1;
    }
    return 0;
}
