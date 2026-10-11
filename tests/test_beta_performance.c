#include "af/core/allocator.h"
#include "af/core/hash.h"
#include "af/scene/scene.h"
#include "arcflow/arcflow_runtime.h"
#include "assets/package.h"
#include "project/project.h"
#include "project/project_templates.h"
#include "runtime/runtime_internal.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <SDL3/SDL_timer.h>

#define AF_BETA_PACKAGE_LOAD_ITERATIONS 16U
#define AF_BETA_RUNTIME_FRAME_COUNT 60000U
#define AF_BETA_PACKAGE_LOAD_MEAN_LIMIT_NS UINT64_C(50000000)
#define AF_BETA_RUNTIME_UPDATE_MEAN_LIMIT_NS UINT64_C(1000000)

static bool af_beta_elapsed(uint64_t start, uint64_t end, uint64_t count, uint64_t *out_mean) {
    if (end < start || count == 0U || out_mean == NULL) {
        return false;
    }
    *out_mean = (end - start) / count;
    return true;
}

int main(void) {
    AfAllocator allocator = {0};
    AfProject project = {0};
    AfPackageBlob package_blob = {0};
    AfRuntime *runtime = NULL;
    AfPackageHandle package = 0U;
    AfInstanceHandle instance = 0U;
    const AfScene *scene = NULL;
    uint64_t load_start = 0U;
    uint64_t load_end = 0U;
    uint64_t update_start = 0U;
    uint64_t update_end = 0U;
    uint64_t load_mean = UINT64_MAX;
    uint64_t update_mean = UINT64_MAX;
    uint64_t frame_allocations = 0U;
    uint64_t workload_hash = 0U;
    bool passed = false;
    AfResult result = AF_RESULT_SUCCESS;

    af_allocator_init_default(&allocator);
    result =
        af_project_template_build(AF_PROJECT_TEMPLATE_VECTOR_ANIMATION, &allocator, 64U, &project);
    if (result != AF_RESULT_SUCCESS) {
        goto cleanup;
    }
    result = af_project_compile_release(&project, &allocator, &package_blob);
    if (result != AF_RESULT_SUCCESS ||
        af_package_validate(package_blob.data, package_blob.size) != AF_RESULT_SUCCESS) {
        result = result == AF_RESULT_SUCCESS ? AF_RESULT_INVALID_PACKAGE : result;
        goto cleanup;
    }
    workload_hash = af_hash_bytes(package_blob.data, package_blob.size);
    {
        const AfRuntimeCreateInfo create_info = {.frames_in_flight = 2U};
        result = af_runtime_create(&create_info, &runtime);
    }
    if (result != AF_RESULT_SUCCESS) {
        goto cleanup;
    }
    load_start = SDL_GetTicksNS();
    for (uint32_t index = 0U; index < AF_BETA_PACKAGE_LOAD_ITERATIONS; ++index) {
        result = af_package_load(runtime, package_blob.data, package_blob.size, &package);
        if (result != AF_RESULT_SUCCESS) {
            goto cleanup;
        }
        result = af_package_destroy(runtime, package);
        package = 0U;
        if (result != AF_RESULT_SUCCESS) {
            goto cleanup;
        }
    }
    load_end = SDL_GetTicksNS();
    if (!af_beta_elapsed(load_start, load_end, AF_BETA_PACKAGE_LOAD_ITERATIONS, &load_mean)) {
        result = AF_RESULT_INVALID_ARGUMENT;
        goto cleanup;
    }
    result = af_package_load(runtime, package_blob.data, package_blob.size, &package);
    if (result == AF_RESULT_SUCCESS) {
        result = af_instance_create(runtime, package, &instance);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_instance_play(runtime, instance);
    }
    for (uint32_t index = 0U; result == AF_RESULT_SUCCESS && index < 120U; ++index) {
        result = af_instance_update(runtime, instance, 1.0F / 60.0F);
    }
    if (result != AF_RESULT_SUCCESS) {
        goto cleanup;
    }
    update_start = SDL_GetTicksNS();
    for (uint32_t index = 0U; index < AF_BETA_RUNTIME_FRAME_COUNT; ++index) {
        result = af_instance_update(runtime, instance, 1.0F / 60.0F);
        if (result != AF_RESULT_SUCCESS) {
            goto cleanup;
        }
        if (UINT64_MAX - frame_allocations < af_runtime_frame_allocation_count(runtime)) {
            result = AF_RESULT_LIMIT_EXCEEDED;
            goto cleanup;
        }
        frame_allocations += af_runtime_frame_allocation_count(runtime);
    }
    update_end = SDL_GetTicksNS();
    if (!af_beta_elapsed(update_start, update_end, AF_BETA_RUNTIME_FRAME_COUNT, &update_mean)) {
        result = AF_RESULT_INVALID_ARGUMENT;
        goto cleanup;
    }
    result = af_instance_get_scene(runtime, instance, &scene);
    if (result != AF_RESULT_SUCCESS || af_scene_validate(scene) != AF_RESULT_SUCCESS) {
        result = result == AF_RESULT_SUCCESS ? AF_RESULT_INVALID_PACKAGE : result;
        goto cleanup;
    }
    passed = load_mean <= AF_BETA_PACKAGE_LOAD_MEAN_LIMIT_NS &&
             update_mean <= AF_BETA_RUNTIME_UPDATE_MEAN_LIMIT_NS && frame_allocations == 0U;

cleanup:
    (void)fprintf(stdout,
                  "{\"workload\":\"vector-animation\",\"workload_hash\":\"%016llx\","
                  "\"package_load_mean_ns\":%llu,"
                  "\"package_load_limit_ns\":%llu,\"runtime_update_mean_ns\":%llu,"
                  "\"runtime_update_limit_ns\":%llu,\"frames\":%u,"
                  "\"frame_allocations\":%llu,\"passed\":%s}\n",
                  (unsigned long long)workload_hash, (unsigned long long)load_mean,
                  (unsigned long long)AF_BETA_PACKAGE_LOAD_MEAN_LIMIT_NS,
                  (unsigned long long)update_mean,
                  (unsigned long long)AF_BETA_RUNTIME_UPDATE_MEAN_LIMIT_NS,
                  AF_BETA_RUNTIME_FRAME_COUNT, (unsigned long long)frame_allocations,
                  passed && result == AF_RESULT_SUCCESS ? "true" : "false");
    if (instance != 0U) {
        (void)af_instance_destroy(runtime, instance);
    }
    if (package != 0U) {
        (void)af_package_destroy(runtime, package);
    }
    af_runtime_destroy(runtime);
    af_package_blob_destroy(&allocator, &package_blob);
    af_project_destroy(&project);
    return passed && result == AF_RESULT_SUCCESS ? 0 : 1;
}
