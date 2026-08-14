#include "arcflow/arcflow_host_sdk.h"

#include "arcflow/arcflow_version.h"

#include <stdalign.h>
#include <string.h>

struct AfHostSdk {
    AfHostSdkAllocatorCallbacks allocator;
    AfHostSdkLogCallbacks log;
    AfHostSdkFileCallbacks files;
    AfHostSdkResourceCallbacks resources;
    AfHostSdkVulkanDevice vulkan;
    AfHostSdkRecordCommandsFn record_commands;
    void *record_user;
    size_t maximum_transfer_size;
    AfHostSdkFeatures enabled_features;
    bool recording;
};

static bool af_host_sdk_table_valid(uint32_t struct_size, uint32_t expected_size,
                                    uint32_t api_version) {
    return struct_size >= expected_size && api_version == AF_HOST_SDK_API_VERSION;
}

static bool af_host_sdk_text_bounded(const char *text, size_t maximum_size) {
    size_t index = 0U;
    if (text == NULL || maximum_size == 0U) {
        return false;
    }
    for (index = 0U; index < maximum_size; ++index) {
        if (text[index] == '\0') {
            return true;
        }
    }
    return false;
}

static bool af_host_sdk_vulkan_valid(const AfHostSdkVulkanDevice *vulkan) {
    return vulkan != NULL &&
           af_host_sdk_table_valid(vulkan->struct_size, (uint32_t)sizeof(*vulkan),
                                   vulkan->api_version) &&
           vulkan->instance != VK_NULL_HANDLE && vulkan->physical_device != VK_NULL_HANDLE &&
           vulkan->device != VK_NULL_HANDLE && vulkan->graphics_queue != VK_NULL_HANDLE &&
           vulkan->frames_in_flight > 0U &&
           vulkan->frames_in_flight <= AF_HOST_SDK_MAX_FRAMES_IN_FLIGHT;
}

static bool af_host_sdk_sample_count_valid(VkSampleCountFlagBits sample_count) {
    return sample_count == VK_SAMPLE_COUNT_1_BIT || sample_count == VK_SAMPLE_COUNT_2_BIT ||
           sample_count == VK_SAMPLE_COUNT_4_BIT || sample_count == VK_SAMPLE_COUNT_8_BIT ||
           sample_count == VK_SAMPLE_COUNT_16_BIT || sample_count == VK_SAMPLE_COUNT_32_BIT ||
           sample_count == VK_SAMPLE_COUNT_64_BIT;
}

static uint32_t af_host_sdk_read_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) | ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

AfResult af_host_sdk_create(const AfHostSdkCreateInfo *create_info, AfHostSdk **out_sdk) {
    AfHostSdk *sdk = NULL;
    bool has_vulkan = false;
    if (out_sdk == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_sdk = NULL;
    if (create_info == NULL ||
        !af_host_sdk_table_valid(create_info->struct_size, (uint32_t)sizeof(*create_info),
                                 create_info->api_version) ||
        create_info->allocator == NULL ||
        !af_host_sdk_table_valid(create_info->allocator->struct_size,
                                 (uint32_t)sizeof(*create_info->allocator),
                                 create_info->allocator->api_version) ||
        create_info->allocator->allocate == NULL || create_info->allocator->deallocate == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (create_info->log != NULL && (!af_host_sdk_table_valid(create_info->log->struct_size,
                                                              (uint32_t)sizeof(*create_info->log),
                                                              create_info->log->api_version) ||
                                     create_info->log->log == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (create_info->files != NULL &&
        (!af_host_sdk_table_valid(create_info->files->struct_size,
                                  (uint32_t)sizeof(*create_info->files),
                                  create_info->files->api_version) ||
         create_info->files->load == NULL || create_info->files->release == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (create_info->resources != NULL &&
        (!af_host_sdk_table_valid(create_info->resources->struct_size,
                                  (uint32_t)sizeof(*create_info->resources),
                                  create_info->resources->api_version) ||
         create_info->resources->resolve == NULL || create_info->resources->release == NULL)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    has_vulkan = create_info->vulkan != NULL;
    if (has_vulkan != (create_info->record_commands != NULL) ||
        (has_vulkan && !af_host_sdk_vulkan_valid(create_info->vulkan)) ||
        create_info->maximum_transfer_size > AF_HOST_SDK_MAX_TRANSFER_SIZE) {
        return AF_RESULT_INVALID_ARGUMENT;
    }

    sdk = create_info->allocator->allocate(create_info->allocator->user, sizeof(*sdk),
                                           alignof(AfHostSdk));
    if (sdk == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    memset(sdk, 0, sizeof(*sdk));
    sdk->allocator = *create_info->allocator;
    sdk->enabled_features =
        AF_HOST_SDK_FEATURE_ALLOCATOR_CALLBACKS | AF_HOST_SDK_FEATURE_PACKAGE_COMPATIBILITY_QUERY;
    if (create_info->log != NULL) {
        sdk->log = *create_info->log;
        sdk->enabled_features |= AF_HOST_SDK_FEATURE_LOG_CALLBACK;
    }
    if (create_info->files != NULL) {
        sdk->files = *create_info->files;
        sdk->enabled_features |= AF_HOST_SDK_FEATURE_FILE_CALLBACKS;
    }
    if (create_info->resources != NULL) {
        sdk->resources = *create_info->resources;
        sdk->enabled_features |= AF_HOST_SDK_FEATURE_CUSTOM_RESOURCES;
    }
    if (has_vulkan) {
        sdk->vulkan = *create_info->vulkan;
        sdk->record_commands = create_info->record_commands;
        sdk->record_user = create_info->record_user;
        sdk->enabled_features |=
            AF_HOST_SDK_FEATURE_EXISTING_VULKAN_DEVICE | AF_HOST_SDK_FEATURE_HOST_COMMAND_BUFFER;
    }
    sdk->maximum_transfer_size = create_info->maximum_transfer_size;
    if (sdk->maximum_transfer_size == 0U) {
        sdk->maximum_transfer_size = AF_HOST_SDK_MAX_TRANSFER_SIZE;
    }
    *out_sdk = sdk;
    return AF_RESULT_SUCCESS;
}

void af_host_sdk_destroy(AfHostSdk *sdk) {
    AfHostSdkAllocatorCallbacks allocator = {0};
    if (sdk == NULL) {
        return;
    }
    allocator = sdk->allocator;
    memset(sdk, 0, sizeof(*sdk));
    allocator.deallocate(allocator.user, sdk);
}

AfResult af_host_sdk_query_version(AfHostSdkVersion *out_version) {
    uint32_t previous = 0U;
    if (out_version == NULL ||
        !af_host_sdk_table_valid(out_version->struct_size, (uint32_t)sizeof(*out_version),
                                 out_version->api_version)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (AF_PACKAGE_FORMAT_VERSION > 0U) {
        previous = AF_PACKAGE_FORMAT_VERSION - 1U;
    }
    *out_version = (AfHostSdkVersion){.struct_size = (uint32_t)sizeof(*out_version),
                                      .api_version = AF_HOST_SDK_API_VERSION,
                                      .major = AF_HOST_SDK_VERSION_MAJOR,
                                      .minor = AF_HOST_SDK_VERSION_MINOR,
                                      .patch = AF_HOST_SDK_VERSION_PATCH,
                                      .runtime_version = AF_RUNTIME_VERSION,
                                      .current_package_version = AF_PACKAGE_FORMAT_VERSION,
                                      .previous_package_version = previous};
    return AF_RESULT_SUCCESS;
}

bool af_host_sdk_semantic_version_compatible(uint32_t major, uint32_t minor, uint32_t patch) {
    if (major != AF_HOST_SDK_VERSION_MAJOR || minor != AF_HOST_SDK_VERSION_MINOR) {
        return false;
    }
    return patch <= AF_HOST_SDK_VERSION_PATCH;
}

AfResult af_host_sdk_query_features(const AfHostSdk *sdk, AfHostSdkFeatureQuery *out_features) {
    if (sdk == NULL || out_features == NULL ||
        !af_host_sdk_table_valid(out_features->struct_size, (uint32_t)sizeof(*out_features),
                                 out_features->api_version)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_features = (AfHostSdkFeatureQuery){.struct_size = (uint32_t)sizeof(*out_features),
                                            .api_version = AF_HOST_SDK_API_VERSION,
                                            .supported = AF_HOST_SDK_FEATURE_ALL,
                                            .enabled = sdk->enabled_features};
    return AF_RESULT_SUCCESS;
}

AfResult af_host_sdk_query_contract(const AfHostSdk *sdk, AfHostSdkContract *out_contract) {
    if (sdk == NULL || out_contract == NULL ||
        !af_host_sdk_table_valid(out_contract->struct_size, (uint32_t)sizeof(*out_contract),
                                 out_contract->api_version)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_contract = (AfHostSdkContract){
        .struct_size = (uint32_t)sizeof(*out_contract),
        .api_version = AF_HOST_SDK_API_VERSION,
        .threading =
            AF_HOST_THREAD_CREATE_DESTROY_SAME_THREAD | AF_HOST_THREAD_INSTANCE_CALLS_SERIALIZED |
            AF_HOST_THREAD_DIFFERENT_INSTANCES_PARALLEL | AF_HOST_THREAD_CALLBACKS_ON_CALLER_THREAD,
        .ownership = AF_HOST_OWNS_VULKAN_INSTANCE | AF_HOST_OWNS_VULKAN_DEVICE |
                     AF_HOST_OWNS_VULKAN_QUEUE | AF_HOST_OWNS_COMMAND_BUFFER |
                     AF_HOST_OWNS_RENDER_TARGET | AF_SDK_OWNS_INTERNAL_PACKAGE_COPY,
        .synchronization = AF_HOST_SYNC_BEGINS_COMMAND_BUFFER | AF_HOST_SYNC_ENDS_COMMAND_BUFFER |
                           AF_HOST_SYNC_SUBMITS_COMMAND_BUFFER |
                           AF_HOST_SYNC_WAITS_FOR_RESOURCE_DESTRUCTION |
                           AF_SDK_RECORDS_ONLY_INSIDE_CALLBACK,
        .maximum_frames_in_flight = AF_HOST_SDK_MAX_FRAMES_IN_FLIGHT,
        .creates_vulkan_device = false,
        .submits_command_buffers = false,
        .waits_for_device_idle = false};
    return AF_RESULT_SUCCESS;
}

AfResult af_host_sdk_query_package_version(uint32_t package_version,
                                           uint32_t minimum_runtime_version,
                                           AfHostPackageCompatibility *out_compatibility) {
    AfHostPackageCompatibilityKind kind = AF_HOST_PACKAGE_UNSUPPORTED;
    bool can_load_directly = false;
    bool requires_upgrade = false;
    if (out_compatibility == NULL || !af_host_sdk_table_valid(out_compatibility->struct_size,
                                                              (uint32_t)sizeof(*out_compatibility),
                                                              out_compatibility->api_version)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (minimum_runtime_version > AF_RUNTIME_VERSION) {
        kind = AF_HOST_PACKAGE_REQUIRES_NEWER_RUNTIME;
    } else if (package_version == AF_PACKAGE_FORMAT_VERSION) {
        kind = AF_HOST_PACKAGE_CURRENT;
        can_load_directly = true;
    } else if (AF_PACKAGE_FORMAT_VERSION > 0U &&
               package_version == AF_PACKAGE_FORMAT_VERSION - 1U) {
        kind = AF_HOST_PACKAGE_PREVIOUS_REQUIRES_UPGRADE;
        requires_upgrade = true;
    }
    *out_compatibility =
        (AfHostPackageCompatibility){.struct_size = (uint32_t)sizeof(*out_compatibility),
                                     .api_version = AF_HOST_SDK_API_VERSION,
                                     .package_version = package_version,
                                     .minimum_runtime_version = minimum_runtime_version,
                                     .kind = kind,
                                     .can_load_directly = can_load_directly,
                                     .requires_upgrade = requires_upgrade};
    return AF_RESULT_SUCCESS;
}

AfResult af_host_sdk_query_package(const void *data, size_t size,
                                   AfHostPackageCompatibility *out_compatibility) {
    static const uint8_t package_magic[8] = {'A', 'R', 'C', 'F', 'L', 'O', 'W', '\0'};
    const uint8_t *bytes = data;
    if (data == NULL || size < 16U || memcmp(bytes, package_magic, sizeof(package_magic)) != 0) {
        return AF_RESULT_INVALID_PACKAGE;
    }
    return af_host_sdk_query_package_version(af_host_sdk_read_u32(bytes + 8U),
                                             af_host_sdk_read_u32(bytes + 12U), out_compatibility);
}

AfResult af_host_sdk_record_commands(AfHostSdk *sdk, const AfHostSdkRecordRequest *request) {
    const uint32_t required_flags = AF_HOST_RECORD_COMMAND_BUFFER_IS_RECORDING |
                                    AF_HOST_RECORD_ALLOW_DYNAMIC_RENDERING |
                                    AF_HOST_RECORD_ALLOW_SYNCHRONIZATION_2;
    uint64_t render_right = 0U;
    uint64_t render_bottom = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    if (sdk == NULL || request == NULL || sdk->record_commands == NULL || sdk->recording ||
        !af_host_sdk_table_valid(request->struct_size, (uint32_t)sizeof(*request),
                                 request->api_version) ||
        request->command_buffer == VK_NULL_HANDLE || request->target_image == VK_NULL_HANDLE ||
        request->target_view == VK_NULL_HANDLE || request->target_format == VK_FORMAT_UNDEFINED ||
        request->target_extent.width == 0U || request->target_extent.height == 0U ||
        request->target_extent.width > 16384U || request->target_extent.height > 16384U ||
        request->render_area.offset.x < 0 || request->render_area.offset.y < 0 ||
        request->render_area.extent.width == 0U || request->render_area.extent.height == 0U ||
        request->initial_layout == VK_IMAGE_LAYOUT_UNDEFINED ||
        request->final_layout == VK_IMAGE_LAYOUT_UNDEFINED ||
        !af_host_sdk_sample_count_valid(request->sample_count) ||
        request->frame_index >= sdk->vulkan.frames_in_flight ||
        (request->flags & required_flags) != required_flags ||
        (request->flags & ~required_flags) != 0U) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    render_right = (uint64_t)(uint32_t)request->render_area.offset.x +
                   (uint64_t)request->render_area.extent.width;
    render_bottom = (uint64_t)(uint32_t)request->render_area.offset.y +
                    (uint64_t)request->render_area.extent.height;
    if (render_right > (uint64_t)request->target_extent.width ||
        render_bottom > (uint64_t)request->target_extent.height) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    sdk->recording = true;
    result = sdk->record_commands(sdk->record_user, &sdk->vulkan, request);
    sdk->recording = false;
    return result;
}

AfResult af_host_sdk_log(AfHostSdk *sdk, AfHostSdkLogLevel level, const char *message) {
    if (sdk == NULL || message == NULL || level < AF_HOST_SDK_LOG_DEBUG ||
        level > AF_HOST_SDK_LOG_ERROR || !af_host_sdk_text_bounded(message, 4096U)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (sdk->log.log == NULL) {
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    sdk->log.log(sdk->log.user, level, message);
    return AF_RESULT_SUCCESS;
}

AfResult af_host_sdk_load_file(AfHostSdk *sdk, const char *path, const void **out_data,
                               size_t *out_size) {
    AfResult result = AF_RESULT_SUCCESS;
    if (sdk == NULL || out_data == NULL || out_size == NULL || path == NULL || path[0] == '\0' ||
        !af_host_sdk_text_bounded(path, AF_HOST_SDK_MAX_PATH_SIZE)) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_data = NULL;
    *out_size = 0U;
    if (sdk->files.load == NULL) {
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    result = sdk->files.load(sdk->files.user, path, sdk->maximum_transfer_size, out_data, out_size);
    if (result != AF_RESULT_SUCCESS) {
        if (*out_data != NULL) {
            sdk->files.release(sdk->files.user, *out_data, *out_size);
            *out_data = NULL;
            *out_size = 0U;
        }
        return result;
    }
    if (*out_data == NULL || *out_size == 0U || *out_size > sdk->maximum_transfer_size) {
        if (*out_data != NULL) {
            sdk->files.release(sdk->files.user, *out_data, *out_size);
        }
        *out_data = NULL;
        *out_size = 0U;
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    return AF_RESULT_SUCCESS;
}

void af_host_sdk_release_file(AfHostSdk *sdk, const void *data, size_t size) {
    if (sdk != NULL && sdk->files.release != NULL && data != NULL) {
        sdk->files.release(sdk->files.user, data, size);
    }
}

AfResult af_host_sdk_resolve_resource(AfHostSdk *sdk, uint64_t resource_type, uint64_t resource_id,
                                      const void **out_data, size_t *out_size) {
    AfResult result = AF_RESULT_SUCCESS;
    if (sdk == NULL || resource_type == 0U || resource_id == 0U || out_data == NULL ||
        out_size == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_data = NULL;
    *out_size = 0U;
    if (sdk->resources.resolve == NULL) {
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    result =
        sdk->resources.resolve(sdk->resources.user, resource_type, resource_id, out_data, out_size);
    if (result != AF_RESULT_SUCCESS) {
        if (*out_data != NULL) {
            sdk->resources.release(sdk->resources.user, resource_type, resource_id, *out_data,
                                   *out_size);
            *out_data = NULL;
            *out_size = 0U;
        }
        return result;
    }
    if (*out_data == NULL || *out_size == 0U || *out_size > sdk->maximum_transfer_size) {
        if (*out_data != NULL) {
            sdk->resources.release(sdk->resources.user, resource_type, resource_id, *out_data,
                                   *out_size);
        }
        *out_data = NULL;
        *out_size = 0U;
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    return AF_RESULT_SUCCESS;
}

void af_host_sdk_release_resource(AfHostSdk *sdk, uint64_t resource_type, uint64_t resource_id,
                                  const void *data, size_t size) {
    if (sdk != NULL && sdk->resources.release != NULL && resource_type != 0U && resource_id != 0U &&
        data != NULL) {
        sdk->resources.release(sdk->resources.user, resource_type, resource_id, data, size);
    }
}
