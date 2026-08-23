#include "animation/animation.h"
#include "arcflow/arcflow_build_info.h"
#include "assets/package.h"
#include "core/allocator.h"
#include "core/file_io.h"
#include "core/hash.h"
#include "onboarding/onboarding.h"
#include "project/image_import.h"
#include "project/project.h"
#include "project/project_collaboration.h"
#include "project/project_templates.h"
#include "scene/scene.h"
#include "stability/diagnostics.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#endif

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
    particle.spawn_rate = 90.0F;
    particle.capacity = 4096U;
    particle.output = AF_PARTICLE_OUTPUT_TRAIL;
    particle.turbulence_strength = 160.0F;
    particle.turbulence_frequency = 0.018F;
    particle.vortex_force = 90.0F;
    particle.vector_field_direction = (AfVec2){1.0F, 0.2F};
    particle.vector_field_strength = 22.0F;
    particle.trail_length = 52.0F;
    particle.trail_width = 7.0F;
    particle_data.lod = af_vfx_lod_default();
    af_allocator_init_default(&allocator);
    result = af_package_write(&source, &allocator, &blob);
    if (result == AF_RESULT_SUCCESS) {
        result = af_file_write_atomic(path, blob.data, blob.size, 1U);
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

static AfResult af_report_project_conflicts(const char *path, AfAllocator *allocator) {
    AfFileData file = {0};
    AfProjectConflictReport conflicts = {0};
    AfResult result = af_file_read_limited(path, AF_PROJECT_MAX_FILE_SIZE, allocator, &file);
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_conflicts_scan(file.data, file.size, &conflicts);
    }
    if (result == AF_RESULT_INVALID_PACKAGE && conflicts.count != 0U) {
        for (uint32_t index = 0U; index < conflicts.count; ++index) {
            (void)fprintf(stderr,
                          "%s:%u: merge conflict marker (%s); resolve this source-control "
                          "conflict before validation\n",
                          path, conflicts.entries[index].line,
                          af_project_conflict_marker_name(conflicts.entries[index].marker));
        }
        if (conflicts.truncated) {
            (void)fprintf(stderr, "%s: additional conflict markers omitted\n", path);
        }
    }
    af_file_data_destroy(allocator, &file);
    return result;
}

static AfResult af_load_project_checked_versioned(const char *path, AfAllocator *allocator,
                                                  uint32_t *out_source_format_version,
                                                  AfProject *out_project) {
    AfResult result = af_report_project_conflicts(path, allocator);
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_project_load_file_versioned(path, allocator, out_source_format_version, out_project);
    }
    if (result == AF_RESULT_SUCCESS) {
        for (uint32_t index = 0U; index < out_project->image_data.asset_count; ++index) {
            const AfImageAsset *asset = &out_project->image_data.assets[index];
            if (asset->missing) {
                (void)fprintf(stderr, "%s: asset %llu source '%s' is missing or changed\n", path,
                              (unsigned long long)asset->id, asset->source_path);
                result = AF_RESULT_NOT_FOUND;
            }
        }
    }
    return result;
}

static AfResult af_load_project_checked(const char *path, AfAllocator *allocator,
                                        AfProject *out_project) {
    return af_load_project_checked_versioned(path, allocator, NULL, out_project);
}

static AfResult af_validate_project_file(const char *path) {
    AfAllocator allocator = {0};
    AfProject project = {0};
    AfResult result = AF_RESULT_SUCCESS;
    af_allocator_init_default(&allocator);
    result = af_load_project_checked(path, &allocator, &project);
    if (result == AF_RESULT_SUCCESS) {
        (void)fprintf(stdout, "%s: valid project '%s' revision %llu (%u objects, %u assets)\n",
                      path, project.metadata.project_id,
                      (unsigned long long)project.metadata.revision, project.scene.count,
                      project.image_data.asset_count);
    }
    af_project_destroy(&project);
    return result;
}

static AfResult af_write_new_project(const char *path) {
    AfAllocator allocator = {0};
    AfProject project = {0};
    AfResult result = AF_RESULT_SUCCESS;
    af_allocator_init_default(&allocator);
    result = af_project_create(&project, &allocator, 1U);
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_save_file(&project, path, &allocator);
    }
    af_project_destroy(&project);
    return result;
}

static AfResult af_compile_project(const char *input_path, const char *output_path, bool release) {
    AfAllocator allocator = {0};
    AfProject project = {0};
    AfPackageBlob package = {0};
    AfResult result = AF_RESULT_SUCCESS;
    af_allocator_init_default(&allocator);
    result = af_load_project_checked(input_path, &allocator, &project);
    if (result == AF_RESULT_SUCCESS) {
        result = release ? af_project_compile_release(&project, &allocator, &package)
                         : af_project_compile(&project, &allocator, &package);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_file_write_atomic(output_path, package.data, package.size, 1U);
    }
    af_package_blob_destroy(&allocator, &package);
    af_project_destroy(&project);
    return result;
}

static AfResult af_check_project_determinism(const char *input_path) {
    AfAllocator allocator = {0};
    AfProject project = {0};
    AfPackageBlob first = {0};
    AfPackageBlob second = {0};
    AfResult result = AF_RESULT_SUCCESS;
    af_allocator_init_default(&allocator);
    result = af_load_project_checked(input_path, &allocator, &project);
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_compile_release(&project, &allocator, &first);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_compile_release(&project, &allocator, &second);
    }
    if (result == AF_RESULT_SUCCESS &&
        (first.size != second.size || memcmp(first.data, second.data, first.size) != 0)) {
        (void)fprintf(stderr, "%s: identical inputs produced different package bytes\n",
                      input_path);
        result = AF_RESULT_INVALID_PACKAGE;
    }
    if (result == AF_RESULT_SUCCESS) {
        (void)fprintf(stdout, "%s: deterministic package (%zu bytes)\n", input_path, first.size);
    }
    af_package_blob_destroy(&allocator, &second);
    af_package_blob_destroy(&allocator, &first);
    af_project_destroy(&project);
    return result;
}

static bool af_package_blobs_equal(const AfPackageBlob *left, const AfPackageBlob *right) {
    return left->size == right->size &&
           (left->size == 0U || memcmp(left->data, right->data, left->size) == 0);
}

static AfResult af_migrate_project(const char *input_path, const char *output_path) {
    AfAllocator allocator = {0};
    AfProject source = {0};
    AfProject migrated = {0};
    AfPackageBlob serialized = {0};
    AfPackageBlob normalized = {0};
    AfPackageBlob source_package = {0};
    AfPackageBlob migrated_package = {0};
    uint32_t source_format_version = 0U;
    uint32_t migrated_format_version = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    af_allocator_init_default(&allocator);
    result =
        af_load_project_checked_versioned(input_path, &allocator, &source_format_version, &source);
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_serialize(&source, &allocator, &serialized);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_deserialize_versioned(serialized.data, serialized.size, &allocator,
                                                  &migrated_format_version, &migrated);
    }
    if (result == AF_RESULT_SUCCESS && migrated_format_version != AF_PROJECT_FORMAT_VERSION) {
        result = AF_RESULT_INVALID_PACKAGE;
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_reimport_images(&migrated, output_path, NULL);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_serialize(&migrated, &allocator, &normalized);
    }
    if (result == AF_RESULT_SUCCESS && !af_package_blobs_equal(&serialized, &normalized)) {
        (void)fprintf(stderr,
                      "%s: migration changed normalized editable-project data; output was not "
                      "written\n",
                      input_path);
        result = AF_RESULT_INVALID_PACKAGE;
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_compile_release(&source, &allocator, &source_package);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_compile_release(&migrated, &allocator, &migrated_package);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_package_validate(source_package.data, source_package.size);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_package_validate(migrated_package.data, migrated_package.size);
    }
    if (result == AF_RESULT_SUCCESS &&
        !af_package_blobs_equal(&source_package, &migrated_package)) {
        (void)fprintf(stderr,
                      "%s: migration changed deterministic runtime package bytes; output was "
                      "not written\n",
                      input_path);
        result = AF_RESULT_INVALID_PACKAGE;
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_file_write_atomic(output_path, serialized.data, serialized.size, 3U);
    }
    if (result == AF_RESULT_SUCCESS) {
        (void)fprintf(stdout,
                      "%s: migrated project format %u -> %u to %s (%zu bytes, "
                      "runtime-equivalent)\n",
                      input_path, source_format_version, migrated_format_version, output_path,
                      serialized.size);
    }
    af_package_blob_destroy(&allocator, &migrated_package);
    af_package_blob_destroy(&allocator, &source_package);
    af_package_blob_destroy(&allocator, &normalized);
    af_package_blob_destroy(&allocator, &serialized);
    af_project_destroy(&migrated);
    af_project_destroy(&source);
    return result;
}

static void af_beta_audit_report(AfStabilityDiagnostics *diagnostics, uint64_t id,
                                 AfStabilitySeverity severity, AfStabilityCategory category,
                                 AfResult result, const char *message) {
    (void)af_stability_diagnostics_report(diagnostics, id, severity, category, result, "compiler",
                                          message);
}

static AfResult af_beta_audit_project(const char *input_path) {
    AfAllocator allocator = {0};
    AfProject source = {0};
    AfProject roundtrip = {0};
    AfPackageBlob serialized = {0};
    AfPackageBlob roundtrip_serialized = {0};
    AfPackageBlob source_package = {0};
    AfPackageBlob roundtrip_package = {0};
    AfStabilityDiagnostic diagnostic_storage[8];
    AfStabilityDiagnostics diagnostics = {0};
    AfStabilitySummary summary = {0};
    uint32_t source_format_version = 0U;
    uint32_t roundtrip_format_version = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    af_allocator_init_default(&allocator);
    result = af_stability_diagnostics_init(&diagnostics, diagnostic_storage, 8U);
    if (result != AF_RESULT_SUCCESS) {
        return result;
    }
    result =
        af_load_project_checked_versioned(input_path, &allocator, &source_format_version, &source);
    if (result != AF_RESULT_SUCCESS) {
        af_beta_audit_report(&diagnostics, 1U, AF_STABILITY_CRITICAL, AF_STABILITY_SAVE_LOAD,
                             result, "Project failed strict load and asset validation");
        goto report;
    }
    result = af_project_serialize(&source, &allocator, &serialized);
    if (result != AF_RESULT_SUCCESS) {
        af_beta_audit_report(&diagnostics, 2U, AF_STABILITY_CRITICAL, AF_STABILITY_SAVE_LOAD,
                             result, "Project could not be serialized for round-trip testing");
        goto report;
    }
    result = af_project_deserialize_versioned(serialized.data, serialized.size, &allocator,
                                              &roundtrip_format_version, &roundtrip);
    if (result != AF_RESULT_SUCCESS) {
        af_beta_audit_report(&diagnostics, 3U, AF_STABILITY_CRITICAL, AF_STABILITY_MIGRATION,
                             result, "Serialized beta project could not be migrated and reopened");
        goto report;
    }
    if (roundtrip_format_version != AF_PROJECT_FORMAT_VERSION) {
        result = AF_RESULT_INVALID_PACKAGE;
        af_beta_audit_report(&diagnostics, 7U, AF_STABILITY_HIGH, AF_STABILITY_MIGRATION, result,
                             "Migration did not emit the current editable-project format");
        goto report;
    }
    result = af_project_reimport_images(&roundtrip, input_path, NULL);
    if (result != AF_RESULT_SUCCESS) {
        af_beta_audit_report(&diagnostics, 8U, AF_STABILITY_CRITICAL, AF_STABILITY_SAVE_LOAD,
                             result, "Migrated project assets could not be reopened");
        goto report;
    }
    result = af_project_serialize(&roundtrip, &allocator, &roundtrip_serialized);
    if (result != AF_RESULT_SUCCESS ||
        !af_package_blobs_equal(&serialized, &roundtrip_serialized)) {
        result = result == AF_RESULT_SUCCESS ? AF_RESULT_INVALID_PACKAGE : result;
        af_beta_audit_report(&diagnostics, 9U, AF_STABILITY_CRITICAL, AF_STABILITY_SAVE_LOAD,
                             result, "Save/load round trip changed editable-project data");
        goto report;
    }
    result = af_project_compile_release(&source, &allocator, &source_package);
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_compile_release(&roundtrip, &allocator, &roundtrip_package);
    }
    if (result != AF_RESULT_SUCCESS) {
        af_beta_audit_report(&diagnostics, 4U, AF_STABILITY_HIGH,
                             AF_STABILITY_PACKAGE_COMPATIBILITY, result,
                             "Project or round-trip project did not compile as a release package");
        goto report;
    }
    result = af_package_validate(source_package.data, source_package.size);
    if (result == AF_RESULT_SUCCESS) {
        result = af_package_validate(roundtrip_package.data, roundtrip_package.size);
    }
    if (result != AF_RESULT_SUCCESS) {
        af_beta_audit_report(&diagnostics, 5U, AF_STABILITY_HIGH,
                             AF_STABILITY_PACKAGE_COMPATIBILITY, result,
                             "Compiled package failed strict package validation");
        goto report;
    }
    if (!af_package_blobs_equal(&source_package, &roundtrip_package)) {
        result = AF_RESULT_INVALID_PACKAGE;
        af_beta_audit_report(&diagnostics, 6U, AF_STABILITY_HIGH,
                             AF_STABILITY_PACKAGE_COMPATIBILITY, result,
                             "Save/load round trip changed deterministic runtime package bytes");
    }

report:
    summary = af_stability_diagnostics_summarize(&diagnostics);
    for (uint32_t index = 0U; index < diagnostics.count; ++index) {
        const AfStabilityDiagnostic *diagnostic = &diagnostics.items[index];
        if (!diagnostic->resolved) {
            (void)fprintf(stderr, "[%s] [%s] %s: %s (%s)\n",
                          af_stability_severity_name(diagnostic->severity),
                          af_stability_category_name(diagnostic->category), diagnostic->component,
                          diagnostic->message, af_result_string(diagnostic->result));
        }
    }
    (void)fprintf(
        stdout,
        "%s: beta audit %s (project format %u -> %u, %u release blocker%s, %llu diagnostic "
        "occurrence%s)\n",
        input_path, summary.release_ready && result == AF_RESULT_SUCCESS ? "READY" : "BLOCKED",
        source_format_version, AF_PROJECT_FORMAT_VERSION, summary.release_blocker_count,
        summary.release_blocker_count == 1U ? "" : "s",
        (unsigned long long)summary.total_occurrence_count,
        summary.total_occurrence_count == 1U ? "" : "s");
    af_package_blob_destroy(&allocator, &roundtrip_package);
    af_package_blob_destroy(&allocator, &source_package);
    af_package_blob_destroy(&allocator, &roundtrip_serialized);
    af_package_blob_destroy(&allocator, &serialized);
    af_project_destroy(&roundtrip);
    af_project_destroy(&source);
    if (summary.release_ready && result == AF_RESULT_SUCCESS) {
        return AF_RESULT_SUCCESS;
    }
    return result == AF_RESULT_SUCCESS ? AF_RESULT_INVALID_PACKAGE : result;
}

static bool af_update_manifest_token_valid(const char *token, size_t capacity) {
    size_t length = 0U;
    if (token == NULL || token[0] == '\0') {
        return false;
    }
    while (length < capacity && token[length] != '\0') {
        const unsigned char byte = (unsigned char)token[length];
        if (byte <= 32U || byte == 127U) {
            return false;
        }
        ++length;
    }
    return length < capacity;
}

static AfResult af_write_update_manifest(const char *artifact_path, const char *url,
                                         const char *signature, const char *output_path) {
    AfAllocator allocator = {0};
    AfFileData artifact = {0};
    char manifest[2048];
    int written = 0;
    AfResult result = AF_RESULT_SUCCESS;
    if (!af_update_manifest_token_valid(url, 512U) ||
        !af_update_manifest_token_valid(signature, 256U)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    af_allocator_init_default(&allocator);
    result = af_file_read_limited(artifact_path, 512U * 1024U * 1024U, &allocator, &artifact);
    if (result == AF_RESULT_SUCCESS) {
        written = snprintf(
            manifest, sizeof(manifest),
            "arcflow_update 1\nversion %s\nchannel %s\nruntime_min %u\npackage_min %u\n"
            "size %zu\nhash %016llx\nurl %s\nsignature %s\n",
            AF_VERSION_STRING, AF_RELEASE_CHANNEL, AF_RUNTIME_VERSION, AF_PACKAGE_FORMAT_VERSION,
            artifact.size, (unsigned long long)af_hash_bytes(artifact.data, artifact.size), url,
            signature);
        result = written < 0 || (size_t)written >= sizeof(manifest) ? AF_RESULT_LIMIT_EXCEEDED
                                                                    : AF_RESULT_SUCCESS;
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_file_write_atomic(output_path, manifest, (size_t)written, 1U);
    }
    af_file_data_destroy(&allocator, &artifact);
    return result;
}

#include "compiler/compiler_onboarding.inc"

static void af_print_usage(FILE *stream) {
    (void)fprintf(stream,
                  "usage:\n"
                  "  arcflow_compiler --compile INPUT.afproj OUTPUT.afp\n"
                  "  arcflow_compiler --compile-release INPUT.afproj OUTPUT.afp\n"
                  "  arcflow_compiler --validate-project INPUT.afproj\n"
                  "  arcflow_compiler --check-determinism INPUT.afproj\n"
                  "  arcflow_compiler --beta-audit INPUT.afproj\n"
                  "  arcflow_compiler --migrate-project INPUT.afproj OUTPUT.afproj\n"
                  "  arcflow_compiler --validate INPUT.afp\n"
                  "  arcflow_compiler --new-project OUTPUT.afproj\n"
                  "  arcflow_compiler --new-project-template TEMPLATE OUTPUT.afproj\n"
                  "  arcflow_compiler --generate-examples DIRECTORY\n"
                  "  arcflow_compiler --list-templates\n"
                  "  arcflow_compiler --list-guides\n"
                  "  arcflow_compiler --guide TOPIC\n"
                  "  arcflow_compiler --make-update-manifest ARTIFACT URL SIGNATURE OUTPUT\n"
                  "  arcflow_compiler --version\n"
                  "  arcflow_compiler --help\n"
                  "  arcflow_compiler --demo OUTPUT.afp\n");
}

int main(int argument_count, char **arguments) {
    AfResult result = AF_RESULT_INVALID_ARGUMENT;
    if (argument_count == 2 && strcmp(arguments[1], "--version") == 0) {
        (void)fprintf(stdout,
                      "Arcflow %s channel %s build %s project-format %u package-format %u runtime "
                      "%u\n",
                      AF_VERSION_STRING, AF_RELEASE_CHANNEL, AF_BUILD_ID, AF_PROJECT_FORMAT_VERSION,
                      AF_PACKAGE_FORMAT_VERSION, AF_RUNTIME_VERSION);
        return 0;
    }
    if (argument_count == 2 &&
        (strcmp(arguments[1], "--help") == 0 || strcmp(arguments[1], "-h") == 0)) {
        af_print_usage(stdout);
        return 0;
    }
    if (argument_count == 2 && strcmp(arguments[1], "--list-templates") == 0) {
        af_compiler_templates_print();
        return 0;
    }
    if (argument_count == 2 && strcmp(arguments[1], "--list-guides") == 0) {
        af_compiler_guides_print();
        return 0;
    }
    if (argument_count == 3 && strcmp(arguments[1], "--guide") == 0) {
        result = af_compiler_guide_print(arguments[2]);
    } else if (argument_count == 4 && strcmp(arguments[1], "--new-project-template") == 0) {
        const AfProjectTemplateInfo *info = af_project_template_find(arguments[2]);
        result =
            info == NULL ? AF_RESULT_NOT_FOUND : af_compiler_template_write(info, arguments[3]);
    } else if (argument_count == 3 && strcmp(arguments[1], "--generate-examples") == 0) {
        result = af_compiler_examples_generate(arguments[2]);
    } else if (argument_count == 6 && strcmp(arguments[1], "--make-update-manifest") == 0) {
        result = af_write_update_manifest(arguments[2], arguments[3], arguments[4], arguments[5]);
    } else if (argument_count == 3 && strcmp(arguments[1], "--demo") == 0) {
        result = af_write_demo(arguments[2]);
    } else if (argument_count == 3 && strcmp(arguments[1], "--new-project") == 0) {
        result = af_write_new_project(arguments[2]);
    } else if (argument_count == 3 && strcmp(arguments[1], "--validate") == 0) {
        result = af_validate_file(arguments[2]);
    } else if (argument_count == 3 && strcmp(arguments[1], "--validate-project") == 0) {
        result = af_validate_project_file(arguments[2]);
    } else if (argument_count == 3 && strcmp(arguments[1], "--check-determinism") == 0) {
        result = af_check_project_determinism(arguments[2]);
    } else if (argument_count == 3 && strcmp(arguments[1], "--beta-audit") == 0) {
        result = af_beta_audit_project(arguments[2]);
    } else if (argument_count == 4 && strcmp(arguments[1], "--migrate-project") == 0) {
        result = af_migrate_project(arguments[2], arguments[3]);
    } else if (argument_count == 4 && (strcmp(arguments[1], "--release") == 0 ||
                                       strcmp(arguments[1], "--compile-release") == 0)) {
        result = af_compile_project(arguments[2], arguments[3], true);
    } else if (argument_count == 4 && strcmp(arguments[1], "--compile") == 0) {
        result = af_compile_project(arguments[2], arguments[3], false);
    } else if (argument_count == 3) {
        result = af_compile_project(arguments[1], arguments[2], false);
    } else {
        af_print_usage(stderr);
        return 2;
    }
    if (result != AF_RESULT_SUCCESS) {
        (void)fprintf(stderr, "Arcflow Compiler: %s\n", af_result_string(result));
        return 1;
    }
    return 0;
}
