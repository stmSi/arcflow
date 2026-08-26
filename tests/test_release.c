#include "animation/animation.h"
#include "arcflow/arcflow_build_info.h"
#include "arcflow/arcflow_host_sdk.h"
#include "arcflow/arcflow_runtime.h"
#include "assets/package.h"
#include "core/allocator.h"
#include "core/hash.h"
#include "project/project.h"
#include "project/project_templates.h"
#include "runtime/runtime_internal.h"
#include "scene/scene.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AF_RELEASE_FRAMES_PER_TEMPLATE 600U

typedef struct AfReleaseHostCounters {
    uint32_t allocations;
    uint32_t deallocations;
    uint32_t record_calls;
} AfReleaseHostCounters;

static void *af_release_host_allocate(void *user, size_t size, size_t alignment) {
    AfReleaseHostCounters *counters = user;
    (void)alignment;
    if (counters != NULL) {
        ++counters->allocations;
    }
    return malloc(size);
}

static void af_release_host_deallocate(void *user, void *memory) {
    AfReleaseHostCounters *counters = user;
    if (counters != NULL) {
        ++counters->deallocations;
    }
    free(memory);
}

static AfResult af_release_record_commands(void *user, const AfHostSdkVulkanDevice *vulkan,
                                           const AfHostSdkRecordRequest *request) {
    AfReleaseHostCounters *counters = user;
    if (counters == NULL || vulkan == NULL || request == NULL ||
        request->command_buffer == VK_NULL_HANDLE || request->target_view == VK_NULL_HANDLE) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    ++counters->record_calls;
    return AF_RESULT_SUCCESS;
}

static bool af_release_channel_valid(void) {
    return strcmp(AF_RELEASE_CHANNEL, "Development") == 0 ||
           strcmp(AF_RELEASE_CHANNEL, "Preview") == 0 || strcmp(AF_RELEASE_CHANNEL, "Beta") == 0 ||
           strcmp(AF_RELEASE_CHANNEL, "Stable") == 0;
}

static bool af_release_metadata_and_host_sdk(void) {
    AfReleaseHostCounters counters = {0};
    AfHostSdkAllocatorCallbacks allocator = {.struct_size = (uint32_t)sizeof(allocator),
                                             .api_version = AF_HOST_SDK_API_VERSION,
                                             .user = &counters,
                                             .allocate = af_release_host_allocate,
                                             .deallocate = af_release_host_deallocate};
    AfHostSdkCreateInfo create_info = {.struct_size = (uint32_t)sizeof(create_info),
                                       .api_version = AF_HOST_SDK_API_VERSION,
                                       .allocator = &allocator};
    AfHostSdkVersion version = {.struct_size = (uint32_t)sizeof(version),
                                .api_version = AF_HOST_SDK_API_VERSION};
    AfHostPackageCompatibility compatibility = {.struct_size = (uint32_t)sizeof(compatibility),
                                                .api_version = AF_HOST_SDK_API_VERSION};
    AfHostSdkFeatureQuery features = {.struct_size = (uint32_t)sizeof(features),
                                      .api_version = AF_HOST_SDK_API_VERSION};
    AfHostSdkContract contract = {.struct_size = (uint32_t)sizeof(contract),
                                  .api_version = AF_HOST_SDK_API_VERSION};
    AfHostSdk *sdk = NULL;
    bool passed = AF_VERSION_MAJOR == 1U && AF_VERSION_MINOR == 0U && AF_VERSION_PATCH == 0U &&
                  strcmp(AF_VERSION_STRING, "1.0.0") == 0 && AF_BUILD_ID[0] != '\0' &&
                  AF_SOURCE_REVISION[0] != '\0' && af_release_channel_valid() &&
                  AF_HOST_SDK_VERSION_MAJOR == 1U && AF_HOST_SDK_VERSION_MINOR == 0U &&
                  AF_HOST_SDK_VERSION_PATCH == 0U;
    passed = passed && af_host_sdk_query_version(&version) == AF_RESULT_SUCCESS &&
             version.major == 1U && version.minor == 0U && version.patch == 0U &&
             version.runtime_version == AF_RUNTIME_VERSION &&
             version.current_package_version == AF_PACKAGE_FORMAT_VERSION &&
             version.previous_package_version + 1U == version.current_package_version;
    passed = passed && af_host_sdk_semantic_version_compatible(1U, 0U, 0U) &&
             !af_host_sdk_semantic_version_compatible(1U, 0U, 1U) &&
             !af_host_sdk_semantic_version_compatible(1U, 1U, 0U) &&
             !af_host_sdk_semantic_version_compatible(0U, 21U, 0U) &&
             !af_host_sdk_semantic_version_compatible(2U, 0U, 0U);
    passed = passed &&
             af_host_sdk_query_package_version(AF_PACKAGE_FORMAT_VERSION, AF_RUNTIME_VERSION,
                                               &compatibility) == AF_RESULT_SUCCESS &&
             compatibility.kind == AF_HOST_PACKAGE_CURRENT && compatibility.can_load_directly &&
             !compatibility.requires_upgrade;
    compatibility = (AfHostPackageCompatibility){.struct_size = (uint32_t)sizeof(compatibility),
                                                 .api_version = AF_HOST_SDK_API_VERSION};
    passed = passed &&
             af_host_sdk_query_package_version(AF_PACKAGE_FORMAT_VERSION - 1U, AF_RUNTIME_VERSION,
                                               &compatibility) == AF_RESULT_SUCCESS &&
             compatibility.kind == AF_HOST_PACKAGE_PREVIOUS_REQUIRES_UPGRADE &&
             !compatibility.can_load_directly && compatibility.requires_upgrade;
    compatibility = (AfHostPackageCompatibility){.struct_size = (uint32_t)sizeof(compatibility),
                                                 .api_version = AF_HOST_SDK_API_VERSION};
    passed = passed &&
             af_host_sdk_query_package_version(AF_PACKAGE_FORMAT_VERSION, AF_RUNTIME_VERSION + 1U,
                                               &compatibility) == AF_RESULT_SUCCESS &&
             compatibility.kind == AF_HOST_PACKAGE_REQUIRES_NEWER_RUNTIME;
    passed = passed && af_host_sdk_create(&create_info, &sdk) == AF_RESULT_SUCCESS && sdk != NULL;
    passed = passed && af_host_sdk_query_features(sdk, &features) == AF_RESULT_SUCCESS &&
             features.supported == AF_HOST_SDK_FEATURE_ALL &&
             (features.enabled & AF_HOST_SDK_FEATURE_ALLOCATOR_CALLBACKS) != 0U &&
             (features.enabled & AF_HOST_SDK_FEATURE_HOST_COMMAND_BUFFER) == 0U;
    passed = passed && af_host_sdk_query_contract(sdk, &contract) == AF_RESULT_SUCCESS &&
             !contract.creates_vulkan_device && !contract.submits_command_buffers &&
             !contract.waits_for_device_idle;
    af_host_sdk_destroy(sdk);
    sdk = NULL;
    {
        AfHostSdkVulkanDevice vulkan = {.struct_size = (uint32_t)sizeof(vulkan),
                                        .api_version = AF_HOST_SDK_API_VERSION,
                                        .instance = (VkInstance)(uintptr_t)1U,
                                        .physical_device = (VkPhysicalDevice)(uintptr_t)2U,
                                        .device = (VkDevice)(uintptr_t)3U,
                                        .graphics_queue = (VkQueue)(uintptr_t)4U,
                                        .graphics_queue_family = 0U,
                                        .frames_in_flight = 2U};
        AfHostSdkRecordRequest request = {.struct_size = (uint32_t)sizeof(request),
                                          .api_version = AF_HOST_SDK_API_VERSION,
                                          .command_buffer = (VkCommandBuffer)(uintptr_t)5U,
                                          .target_image = (VkImage)(uintptr_t)6U,
                                          .target_view = (VkImageView)(uintptr_t)7U,
                                          .target_format = VK_FORMAT_R8G8B8A8_UNORM,
                                          .target_extent = {640U, 480U},
                                          .render_area = {.offset = {0, 0}, .extent = {640U, 480U}},
                                          .initial_layout =
                                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                          .final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                          .sample_count = VK_SAMPLE_COUNT_1_BIT,
                                          .frame_index = 1U,
                                          .flags = AF_HOST_RECORD_COMMAND_BUFFER_IS_RECORDING |
                                                   AF_HOST_RECORD_ALLOW_DYNAMIC_RENDERING |
                                                   AF_HOST_RECORD_ALLOW_SYNCHRONIZATION_2};
        create_info.vulkan = &vulkan;
        create_info.record_commands = af_release_record_commands;
        create_info.record_user = &counters;
        passed =
            passed && af_host_sdk_create(&create_info, &sdk) == AF_RESULT_SUCCESS && sdk != NULL;
        features = (AfHostSdkFeatureQuery){.struct_size = (uint32_t)sizeof(features),
                                           .api_version = AF_HOST_SDK_API_VERSION};
        passed = passed && af_host_sdk_query_features(sdk, &features) == AF_RESULT_SUCCESS &&
                 (features.enabled & AF_HOST_SDK_FEATURE_EXISTING_VULKAN_DEVICE) != 0U &&
                 (features.enabled & AF_HOST_SDK_FEATURE_HOST_COMMAND_BUFFER) != 0U;
        passed = passed && af_host_sdk_record_commands(sdk, &request) == AF_RESULT_SUCCESS &&
                 counters.record_calls == 1U;
        af_host_sdk_destroy(sdk);
    }
    return passed && counters.allocations == 2U && counters.deallocations == 2U;
}

static AfResult af_release_run_template(AfRuntime *runtime, AfProjectTemplateId template_id,
                                        uint64_t *workload_hash, uint64_t *frame_count,
                                        uint64_t *frame_allocations) {
    static const AfCanvasInfo canvases[] = {{.width = 1280.0F, .height = 720.0F},
                                            {.width = 1920.0F,
                                             .height = 1080.0F,
                                             .safe_left = 48.0F,
                                             .safe_top = 24.0F,
                                             .safe_right = 48.0F,
                                             .safe_bottom = 24.0F},
                                            {.width = 720.0F,
                                             .height = 1280.0F,
                                             .safe_left = 24.0F,
                                             .safe_top = 48.0F,
                                             .safe_right = 24.0F,
                                             .safe_bottom = 48.0F}};
    AfAllocator allocator = {0};
    AfProject project = {0};
    AfPackageBlob blob = {0};
    AfPackageHandle package = 0U;
    AfInstanceHandle instance = 0U;
    const AfScene *scene = NULL;
    AfResult result = AF_RESULT_SUCCESS;
    af_allocator_init_default(&allocator);
    result = af_project_template_build(template_id, &allocator, 64U, &project);
    if (result == AF_RESULT_SUCCESS) {
        result = af_project_compile_release(&project, &allocator, &blob);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_package_validate(blob.data, blob.size);
    }
    if (result == AF_RESULT_SUCCESS) {
        *workload_hash ^= af_hash_bytes(blob.data, blob.size) + UINT64_C(0x9e3779b97f4a7c15) +
                          (*workload_hash << 6U) + (*workload_hash >> 2U);
        result = af_package_load(runtime, blob.data, blob.size, &package);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_instance_create(runtime, package, &instance);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_instance_play(runtime, instance);
    }
    for (uint32_t frame = 0U; result == AF_RESULT_SUCCESS && frame < AF_RELEASE_FRAMES_PER_TEMPLATE;
         ++frame) {
        if (frame % 200U == 0U) {
            result = af_instance_resize(runtime, instance, &canvases[(frame / 200U) % 3U]);
        }
        if (result == AF_RESULT_SUCCESS) {
            result = af_instance_update(runtime, instance, 1.0F / 60.0F);
        }
        if (result == AF_RESULT_SUCCESS) {
            *frame_allocations += af_runtime_frame_allocation_count(runtime);
            ++*frame_count;
        }
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_instance_get_scene(runtime, instance, &scene);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_scene_validate(scene);
    }
    if (instance != 0U) {
        const AfResult cleanup = af_instance_destroy(runtime, instance);
        result = result == AF_RESULT_SUCCESS ? cleanup : result;
    }
    if (package != 0U) {
        const AfResult cleanup = af_package_destroy(runtime, package);
        result = result == AF_RESULT_SUCCESS ? cleanup : result;
    }
    af_package_blob_destroy(&allocator, &blob);
    af_project_destroy(&project);
    return result;
}

int main(void) {
    const AfRuntimeCreateInfo create_info = {.frames_in_flight = 2U};
    AfRuntime *runtime = NULL;
    uint64_t workload_hash = 0U;
    uint64_t frame_count = 0U;
    uint64_t frame_allocations = 0U;
    uint32_t templates_passed = 0U;
    bool passed = af_release_metadata_and_host_sdk();
    AfResult result = af_runtime_create(&create_info, &runtime);
    for (uint32_t index = 0U;
         passed && result == AF_RESULT_SUCCESS && index < (uint32_t)AF_PROJECT_TEMPLATE_COUNT;
         ++index) {
        result = af_release_run_template(runtime, (AfProjectTemplateId)index, &workload_hash,
                                         &frame_count, &frame_allocations);
        if (result == AF_RESULT_SUCCESS) {
            ++templates_passed;
        }
    }
    passed = passed && result == AF_RESULT_SUCCESS &&
             templates_passed == (uint32_t)AF_PROJECT_TEMPLATE_COUNT && frame_allocations == 0U;
    af_runtime_destroy(runtime);
    (void)printf("{\"version\":\"%s\",\"channel\":\"%s\",\"host_sdk\":\"%u.%u.%u\","
                 "\"templates\":%u,\"frames\":%llu,\"frame_allocations\":%llu,"
                 "\"workload_hash\":\"%016llx\",\"passed\":%s}\n",
                 AF_VERSION_STRING, AF_RELEASE_CHANNEL, AF_HOST_SDK_VERSION_MAJOR,
                 AF_HOST_SDK_VERSION_MINOR, AF_HOST_SDK_VERSION_PATCH, templates_passed,
                 (unsigned long long)frame_count, (unsigned long long)frame_allocations,
                 (unsigned long long)workload_hash, passed ? "true" : "false");
    return passed ? 0 : 1;
}
