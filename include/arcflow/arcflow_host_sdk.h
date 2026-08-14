#ifndef ARCFLOW_HOST_SDK_H
#define ARCFLOW_HOST_SDK_H

#include "arcflow/arcflow_result.h"

#include <vulkan/vulkan.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AF_HOST_SDK_API_VERSION 1U
#define AF_HOST_SDK_VERSION_MAJOR 0U
#define AF_HOST_SDK_VERSION_MINOR 21U
#define AF_HOST_SDK_VERSION_PATCH 0U
#define AF_HOST_SDK_MAX_FRAMES_IN_FLIGHT 2U
#define AF_HOST_SDK_MAX_TRANSFER_SIZE (64U * 1024U * 1024U)
#define AF_HOST_SDK_MAX_PATH_SIZE 1024U

typedef uint64_t AfHostSdkFeatures;

#define AF_HOST_SDK_FEATURE_EXISTING_VULKAN_DEVICE (UINT64_C(1) << 0U)
#define AF_HOST_SDK_FEATURE_HOST_COMMAND_BUFFER (UINT64_C(1) << 1U)
#define AF_HOST_SDK_FEATURE_ALLOCATOR_CALLBACKS (UINT64_C(1) << 2U)
#define AF_HOST_SDK_FEATURE_LOG_CALLBACK (UINT64_C(1) << 3U)
#define AF_HOST_SDK_FEATURE_FILE_CALLBACKS (UINT64_C(1) << 4U)
#define AF_HOST_SDK_FEATURE_CUSTOM_RESOURCES (UINT64_C(1) << 5U)
#define AF_HOST_SDK_FEATURE_PACKAGE_COMPATIBILITY_QUERY (UINT64_C(1) << 6U)
#define AF_HOST_SDK_FEATURE_ALL                                                                    \
    (AF_HOST_SDK_FEATURE_EXISTING_VULKAN_DEVICE | AF_HOST_SDK_FEATURE_HOST_COMMAND_BUFFER |        \
     AF_HOST_SDK_FEATURE_ALLOCATOR_CALLBACKS | AF_HOST_SDK_FEATURE_LOG_CALLBACK |                  \
     AF_HOST_SDK_FEATURE_FILE_CALLBACKS | AF_HOST_SDK_FEATURE_CUSTOM_RESOURCES |                   \
     AF_HOST_SDK_FEATURE_PACKAGE_COMPATIBILITY_QUERY)

typedef struct AfHostSdk AfHostSdk;

typedef enum AfHostSdkLogLevel {
    AF_HOST_SDK_LOG_DEBUG = 0,
    AF_HOST_SDK_LOG_INFO,
    AF_HOST_SDK_LOG_WARNING,
    AF_HOST_SDK_LOG_ERROR
} AfHostSdkLogLevel;

typedef void *(*AfHostSdkAllocateFn)(void *user, size_t size, size_t alignment);
typedef void (*AfHostSdkDeallocateFn)(void *user, void *memory);
typedef void (*AfHostSdkLogFn)(void *user, AfHostSdkLogLevel level, const char *message);
typedef AfResult (*AfHostSdkLoadFileFn)(void *user, const char *path, size_t maximum_size,
                                        const void **out_data, size_t *out_size);
typedef void (*AfHostSdkReleaseFileFn)(void *user, const void *data, size_t size);
typedef AfResult (*AfHostSdkResolveResourceFn)(void *user, uint64_t resource_type,
                                               uint64_t resource_id, const void **out_data,
                                               size_t *out_size);
typedef void (*AfHostSdkReleaseResourceFn)(void *user, uint64_t resource_type, uint64_t resource_id,
                                           const void *data, size_t size);

typedef struct AfHostSdkAllocatorCallbacks {
    uint32_t struct_size;
    uint32_t api_version;
    void *user;
    AfHostSdkAllocateFn allocate;
    AfHostSdkDeallocateFn deallocate;
} AfHostSdkAllocatorCallbacks;

typedef struct AfHostSdkLogCallbacks {
    uint32_t struct_size;
    uint32_t api_version;
    void *user;
    AfHostSdkLogFn log;
} AfHostSdkLogCallbacks;

typedef struct AfHostSdkFileCallbacks {
    uint32_t struct_size;
    uint32_t api_version;
    void *user;
    AfHostSdkLoadFileFn load;
    AfHostSdkReleaseFileFn release;
} AfHostSdkFileCallbacks;

typedef struct AfHostSdkResourceCallbacks {
    uint32_t struct_size;
    uint32_t api_version;
    void *user;
    AfHostSdkResolveResourceFn resolve;
    AfHostSdkReleaseResourceFn release;
} AfHostSdkResourceCallbacks;

typedef struct AfHostSdkVulkanDevice {
    uint32_t struct_size;
    uint32_t api_version;
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    uint32_t graphics_queue_family;
    uint32_t frames_in_flight;
} AfHostSdkVulkanDevice;

enum {
    AF_HOST_RECORD_COMMAND_BUFFER_IS_RECORDING = 1U << 0U,
    AF_HOST_RECORD_ALLOW_DYNAMIC_RENDERING = 1U << 1U,
    AF_HOST_RECORD_ALLOW_SYNCHRONIZATION_2 = 1U << 2U
};

typedef struct AfHostSdkRecordRequest {
    uint32_t struct_size;
    uint32_t api_version;
    VkCommandBuffer command_buffer;
    VkImage target_image;
    VkImageView target_view;
    VkFormat target_format;
    VkExtent2D target_extent;
    VkRect2D render_area;
    VkImageLayout initial_layout;
    VkImageLayout final_layout;
    VkSampleCountFlagBits sample_count;
    uint32_t frame_index;
    uint32_t flags;
} AfHostSdkRecordRequest;

typedef AfResult (*AfHostSdkRecordCommandsFn)(void *user, const AfHostSdkVulkanDevice *vulkan,
                                              const AfHostSdkRecordRequest *request);

typedef struct AfHostSdkCreateInfo {
    uint32_t struct_size;
    uint32_t api_version;
    const AfHostSdkAllocatorCallbacks *allocator;
    const AfHostSdkLogCallbacks *log;
    const AfHostSdkFileCallbacks *files;
    const AfHostSdkResourceCallbacks *resources;
    const AfHostSdkVulkanDevice *vulkan;
    AfHostSdkRecordCommandsFn record_commands;
    void *record_user;
    size_t maximum_transfer_size;
} AfHostSdkCreateInfo;

typedef struct AfHostSdkVersion {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    uint32_t runtime_version;
    uint32_t current_package_version;
    uint32_t previous_package_version;
} AfHostSdkVersion;

typedef struct AfHostSdkFeatureQuery {
    uint32_t struct_size;
    uint32_t api_version;
    AfHostSdkFeatures supported;
    AfHostSdkFeatures enabled;
} AfHostSdkFeatureQuery;

typedef uint64_t AfHostSdkThreadingFlags;
typedef uint64_t AfHostSdkOwnershipFlags;
typedef uint64_t AfHostSdkSynchronizationFlags;

#define AF_HOST_THREAD_CREATE_DESTROY_SAME_THREAD (UINT64_C(1) << 0U)
#define AF_HOST_THREAD_INSTANCE_CALLS_SERIALIZED (UINT64_C(1) << 1U)
#define AF_HOST_THREAD_DIFFERENT_INSTANCES_PARALLEL (UINT64_C(1) << 2U)
#define AF_HOST_THREAD_CALLBACKS_ON_CALLER_THREAD (UINT64_C(1) << 3U)

#define AF_HOST_OWNS_VULKAN_INSTANCE (UINT64_C(1) << 0U)
#define AF_HOST_OWNS_VULKAN_DEVICE (UINT64_C(1) << 1U)
#define AF_HOST_OWNS_VULKAN_QUEUE (UINT64_C(1) << 2U)
#define AF_HOST_OWNS_COMMAND_BUFFER (UINT64_C(1) << 3U)
#define AF_HOST_OWNS_RENDER_TARGET (UINT64_C(1) << 4U)
#define AF_SDK_OWNS_INTERNAL_PACKAGE_COPY (UINT64_C(1) << 5U)

#define AF_HOST_SYNC_BEGINS_COMMAND_BUFFER (UINT64_C(1) << 0U)
#define AF_HOST_SYNC_ENDS_COMMAND_BUFFER (UINT64_C(1) << 1U)
#define AF_HOST_SYNC_SUBMITS_COMMAND_BUFFER (UINT64_C(1) << 2U)
#define AF_HOST_SYNC_WAITS_FOR_RESOURCE_DESTRUCTION (UINT64_C(1) << 3U)
#define AF_SDK_RECORDS_ONLY_INSIDE_CALLBACK (UINT64_C(1) << 4U)

typedef struct AfHostSdkContract {
    uint32_t struct_size;
    uint32_t api_version;
    AfHostSdkThreadingFlags threading;
    AfHostSdkOwnershipFlags ownership;
    AfHostSdkSynchronizationFlags synchronization;
    uint32_t maximum_frames_in_flight;
    bool creates_vulkan_device;
    bool submits_command_buffers;
    bool waits_for_device_idle;
} AfHostSdkContract;

typedef enum AfHostPackageCompatibilityKind {
    AF_HOST_PACKAGE_CURRENT = 0,
    AF_HOST_PACKAGE_PREVIOUS_REQUIRES_UPGRADE,
    AF_HOST_PACKAGE_REQUIRES_NEWER_RUNTIME,
    AF_HOST_PACKAGE_UNSUPPORTED
} AfHostPackageCompatibilityKind;

typedef struct AfHostPackageCompatibility {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t package_version;
    uint32_t minimum_runtime_version;
    AfHostPackageCompatibilityKind kind;
    bool can_load_directly;
    bool requires_upgrade;
} AfHostPackageCompatibility;

AfResult af_host_sdk_create(const AfHostSdkCreateInfo *create_info, AfHostSdk **out_sdk);
void af_host_sdk_destroy(AfHostSdk *sdk);
AfResult af_host_sdk_query_version(AfHostSdkVersion *out_version);
bool af_host_sdk_semantic_version_compatible(uint32_t major, uint32_t minor, uint32_t patch);
AfResult af_host_sdk_query_features(const AfHostSdk *sdk, AfHostSdkFeatureQuery *out_features);
AfResult af_host_sdk_query_contract(const AfHostSdk *sdk, AfHostSdkContract *out_contract);
AfResult af_host_sdk_query_package_version(uint32_t package_version,
                                           uint32_t minimum_runtime_version,
                                           AfHostPackageCompatibility *out_compatibility);
AfResult af_host_sdk_query_package(const void *data, size_t size,
                                   AfHostPackageCompatibility *out_compatibility);
AfResult af_host_sdk_record_commands(AfHostSdk *sdk, const AfHostSdkRecordRequest *request);
AfResult af_host_sdk_log(AfHostSdk *sdk, AfHostSdkLogLevel level, const char *message);
AfResult af_host_sdk_load_file(AfHostSdk *sdk, const char *path, const void **out_data,
                               size_t *out_size);
void af_host_sdk_release_file(AfHostSdk *sdk, const void *data, size_t size);
AfResult af_host_sdk_resolve_resource(AfHostSdk *sdk, uint64_t resource_type, uint64_t resource_id,
                                      const void **out_data, size_t *out_size);
void af_host_sdk_release_resource(AfHostSdk *sdk, uint64_t resource_type, uint64_t resource_id,
                                  const void *data, size_t size);

#endif
