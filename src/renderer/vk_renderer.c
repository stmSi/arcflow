#include "renderer/vk_renderer.h"

#include "core/log.h"
#include "core/png.h"
#include "renderer/vk_particles.h"

#include <SDL3/SDL_vulkan.h>
#include <volk.h>

#include <math.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AF_FRAME_COUNT 2U
#define AF_MAX_SWAPCHAIN_IMAGES 8U
#define AF_MAX_SURFACE_FORMATS 64U
#define AF_MAX_PRESENT_MODES 32U
#define AF_MAX_QUEUE_FAMILIES 64U
#define AF_MAX_GPU_PATH_VERTICES (AF_MAX_PATH_TESSELLATED_VERTICES * 2U)
#define AF_MAX_RENDER_ITEMS (AF_MAX_NODES * 2U)
#define AF_CAPTURE_PATH_CAPACITY 1024U

typedef struct AfVkFrame {
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkSemaphore image_available;
    VkFence fence;
    VkImage color_image;
    VkDeviceMemory color_memory;
    VkImageView color_view;
    VkBuffer capture_buffer;
    VkDeviceMemory capture_memory;
    uint8_t *capture_mapping;
    VkDeviceSize capture_size;
    bool color_initialized;
} AfVkFrame;

typedef struct AfGpuShape {
    float matrix[4];
    float translation_bounds[4];
    float color[4];
    float parameters[4];
    int32_t layer_order;
} AfGpuShape;

typedef struct AfGpuPathVertex {
    float position[2];
    float color[4];
} AfGpuPathVertex;

typedef struct AfVkPathCache {
    const AfPathData *source;
    uint64_t path_id;
    uint64_t geometry_generation;
    AfVec2 *vertices;
    uint32_t vertex_count;
    uint64_t rebuild_count;
    uint64_t seen_serial;
} AfVkPathCache;

typedef struct AfVkStrokeCache {
    const AfPathData *path_source;
    const AfStyleData *style_source;
    uint64_t path_id;
    uint64_t style_id;
    uint64_t path_geometry_generation;
    uint64_t stroke_geometry_generation;
    AfVec2 *vertices;
    uint32_t vertex_count;
    uint64_t rebuild_count;
    uint64_t seen_serial;
} AfVkStrokeCache;

typedef enum AfVkRenderItemType { AF_VK_RENDER_ANALYTIC = 0, AF_VK_RENDER_PATH } AfVkRenderItemType;

typedef struct AfVkRenderItem {
    AfVkRenderItemType type;
    int32_t layer_order;
    uint32_t first;
    uint32_t count;
} AfVkRenderItem;

struct AfVkRenderer {
    AfPlatformWindow *platform;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;
    uint32_t graphics_family;
    uint32_t present_family;
    VkSurfaceCapabilitiesKHR surface_capabilities;
    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    VkExtent2D extent;
    VkSampleCountFlagBits sample_count;
    VkImage swapchain_images[AF_MAX_SWAPCHAIN_IMAGES];
    VkImageView swapchain_views[AF_MAX_SWAPCHAIN_IMAGES];
    VkSemaphore present_semaphores[AF_MAX_SWAPCHAIN_IMAGES];
    uint32_t swapchain_image_count;
    AfVkFrame frames[AF_FRAME_COUNT];
    uint32_t frame_index;
    VkBuffer upload_buffer;
    VkDeviceMemory upload_memory;
    uint8_t *upload_mapping;
    VkDeviceSize upload_slice_size;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkPipelineLayout path_pipeline_layout;
    VkPipeline path_pipeline;
    AfVkParticleSystem *particles;
    AfVkPathCache path_caches[AF_MAX_PATHS];
    AfVkStrokeCache stroke_caches[AF_MAX_PATHS];
    AfPathTessellationScratch *path_scratch;
    AfVec2 *path_staging_vertices;
    uint32_t cached_path_vertex_count;
    uint32_t cached_stroke_vertex_count;
    uint64_t path_cache_serial;
    AfVkRenderItem render_items[AF_MAX_RENDER_ITEMS];
    bool resize_requested;
    bool validation_enabled;
    bool validation_error;
    bool capture_supported;
    bool capture_requested;
    char capture_path[AF_CAPTURE_PATH_CAPACITY];
};

static VKAPI_ATTR VkBool32 VKAPI_CALL af_vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data) {
    AfVkRenderer *renderer = user_data;
    (void)types;
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0U && renderer != NULL) {
        renderer->validation_error = true;
    }
    af_log((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0U
               ? AF_LOG_ERROR
               : ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0U
                      ? AF_LOG_WARNING
                      : AF_LOG_DEBUG),
           "Vulkan validation: %s", callback_data != NULL ? callback_data->pMessage : "unknown");
    return VK_FALSE;
}

#if defined(AF_ENABLE_VALIDATION)
static bool af_vk_layer_available(const char *name) {
    VkLayerProperties layers[64];
    uint32_t count = 64U;
    uint32_t index = 0U;
    if (vkEnumerateInstanceLayerProperties(&count, layers) != VK_SUCCESS) {
        return false;
    }
    for (index = 0U; index < count; ++index) {
        if (strcmp(layers[index].layerName, name) == 0) {
            return true;
        }
    }
    return false;
}
#endif

static void af_vk_name(AfVkRenderer *renderer, VkObjectType type, uint64_t handle,
                       const char *name) {
    VkDebugUtilsObjectNameInfoEXT info = {.sType =
                                              VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                                          .objectType = type,
                                          .objectHandle = handle,
                                          .pObjectName = name};
    if (renderer->validation_enabled && vkSetDebugUtilsObjectNameEXT != NULL && handle != 0U) {
        (void)vkSetDebugUtilsObjectNameEXT(renderer->device, &info);
    }
}

static AfResult af_vk_create_instance(AfVkRenderer *renderer) {
    const char *extensions[32];
    const char *layers[1] = {"VK_LAYER_KHRONOS_validation"};
    const char *const *sdl_extensions = NULL;
    Uint32 sdl_extension_count = 0U;
    uint32_t extension_count = 0U;
    uint32_t index = 0U;
    VkApplicationInfo application = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                     .pApplicationName = "Arcflow Player",
                                     .applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
                                     .pEngineName = "Arcflow Runtime",
                                     .engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0),
                                     .apiVersion = VK_API_VERSION_1_3};
    VkDebugUtilsMessengerCreateInfoEXT debug_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = af_vk_debug_callback,
        .pUserData = renderer};
    VkInstanceCreateInfo create_info = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                        .pApplicationInfo = &application};
    VkResult vk_result = VK_SUCCESS;
    sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count);
    if (sdl_extensions == NULL || sdl_extension_count > 31U) {
        af_log(AF_LOG_ERROR, "SDL Vulkan extensions unavailable: %s", SDL_GetError());
        return AF_RESULT_VULKAN_ERROR;
    }
    for (index = 0U; index < sdl_extension_count; ++index) {
        extensions[extension_count++] = sdl_extensions[index];
    }
#if defined(AF_ENABLE_VALIDATION)
    renderer->validation_enabled = af_vk_layer_available(layers[0]);
#endif
    if (renderer->validation_enabled) {
        extensions[extension_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        create_info.enabledLayerCount = 1U;
        create_info.ppEnabledLayerNames = layers;
        create_info.pNext = &debug_info;
    }
    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = extensions;
    vk_result = vkCreateInstance(&create_info, NULL, &renderer->instance);
    if (vk_result != VK_SUCCESS) {
        af_log(AF_LOG_ERROR, "vkCreateInstance failed: %d", (int)vk_result);
        return AF_RESULT_VULKAN_ERROR;
    }
    volkLoadInstance(renderer->instance);
    if (renderer->validation_enabled &&
        vkCreateDebugUtilsMessengerEXT(renderer->instance, &debug_info, NULL,
                                       &renderer->debug_messenger) != VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    return AF_RESULT_SUCCESS;
}

static bool af_vk_device_extension_available(VkPhysicalDevice device, const char *name) {
    VkExtensionProperties extensions[512];
    uint32_t count = 512U;
    uint32_t index = 0U;
    if (vkEnumerateDeviceExtensionProperties(device, NULL, &count, extensions) != VK_SUCCESS) {
        return false;
    }
    for (index = 0U; index < count; ++index) {
        if (strcmp(extensions[index].extensionName, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool af_vk_find_queues(AfVkRenderer *renderer, VkPhysicalDevice device,
                              uint32_t *out_graphics, uint32_t *out_present) {
    VkQueueFamilyProperties queues[AF_MAX_QUEUE_FAMILIES];
    uint32_t count = AF_MAX_QUEUE_FAMILIES;
    uint32_t index = 0U;
    uint32_t graphics = UINT32_MAX;
    uint32_t present = UINT32_MAX;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, queues);
    for (index = 0U; index < count; ++index) {
        VkBool32 present_support = VK_FALSE;
        const bool supports_present =
            vkGetPhysicalDeviceSurfaceSupportKHR(device, index, renderer->surface,
                                                 &present_support) == VK_SUCCESS &&
            present_support == VK_TRUE;
        if ((queues[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            if (graphics == UINT32_MAX) {
                graphics = index;
            }
            if (supports_present) {
                graphics = index;
                present = index;
                break;
            }
        }
        if (supports_present && present == UINT32_MAX) {
            present = index;
        }
    }
    if (graphics == UINT32_MAX || present == UINT32_MAX) {
        return false;
    }
    *out_graphics = graphics;
    *out_present = present;
    return true;
}

static AfResult af_vk_create_device(AfVkRenderer *renderer) {
    VkPhysicalDevice devices[32];
    uint32_t device_count = 32U;
    uint32_t device_index = 0U;
    float priority = 1.0F;
    VkResult vk_result = vkEnumeratePhysicalDevices(renderer->instance, &device_count, devices);
    if (vk_result != VK_SUCCESS || device_count == 0U) {
        return AF_RESULT_VULKAN_ERROR;
    }
    for (device_index = 0U; device_index < device_count; ++device_index) {
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceVulkan13Features features13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceVulkan12Features features12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = &features13};
        VkPhysicalDeviceFeatures2 features = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                              .pNext = &features12};
        uint32_t graphics_family = UINT32_MAX;
        uint32_t present_family = UINT32_MAX;
        bool has_swapchain = false;
        bool has_queues = false;
        vkGetPhysicalDeviceProperties(devices[device_index], &properties);
        vkGetPhysicalDeviceFeatures2(devices[device_index], &features);
        has_swapchain = af_vk_device_extension_available(devices[device_index],
                                                         VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        has_queues =
            af_vk_find_queues(renderer, devices[device_index], &graphics_family, &present_family);
        if (properties.apiVersion < VK_API_VERSION_1_3 || !features13.dynamicRendering ||
            !features13.synchronization2 || !features12.timelineSemaphore || !has_swapchain ||
            !has_queues) {
            af_log(AF_LOG_DEBUG,
                   "Rejected device %s (api=%u dynamic=%u sync2=%u timeline=%u swapchain=%u "
                   "queues=%u)",
                   properties.deviceName, properties.apiVersion, features13.dynamicRendering,
                   features13.synchronization2, features12.timelineSemaphore,
                   has_swapchain ? 1U : 0U, has_queues ? 1U : 0U);
            continue;
        }
        renderer->physical_device = devices[device_index];
        renderer->graphics_family = graphics_family;
        renderer->present_family = present_family;
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            break;
        }
    }
    if (renderer->physical_device == VK_NULL_HANDLE) {
        af_log(AF_LOG_ERROR, "No Vulkan 1.3 device supports dynamic rendering, synchronization2, "
                             "timeline semaphores, and presentation");
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    {
        VkPhysicalDeviceProperties properties;
        VkSampleCountFlags counts = 0U;
        vkGetPhysicalDeviceProperties(renderer->physical_device, &properties);
        counts = properties.limits.framebufferColorSampleCounts;
        renderer->sample_count =
            (counts & VK_SAMPLE_COUNT_4_BIT) != 0U
                ? VK_SAMPLE_COUNT_4_BIT
                : ((counts & VK_SAMPLE_COUNT_2_BIT) != 0U ? VK_SAMPLE_COUNT_2_BIT
                                                          : VK_SAMPLE_COUNT_1_BIT);
    }
    {
        const char *extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDeviceQueueCreateInfo queue_infos[2] = {
            {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
             .queueFamilyIndex = renderer->graphics_family,
             .queueCount = 1U,
             .pQueuePriorities = &priority}};
        uint32_t queue_count = 1U;
        VkPhysicalDeviceVulkan13Features features13 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .synchronization2 = VK_TRUE,
            .dynamicRendering = VK_TRUE};
        VkPhysicalDeviceVulkan12Features features12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &features13,
            .timelineSemaphore = VK_TRUE};
        VkDeviceCreateInfo create_info = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                          .pNext = &features12,
                                          .queueCreateInfoCount = queue_count,
                                          .pQueueCreateInfos = queue_infos,
                                          .enabledExtensionCount = 1U,
                                          .ppEnabledExtensionNames = extensions};
        if (renderer->present_family != renderer->graphics_family) {
            queue_infos[1] =
                (VkDeviceQueueCreateInfo){.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                          .queueFamilyIndex = renderer->present_family,
                                          .queueCount = 1U,
                                          .pQueuePriorities = &priority};
            create_info.queueCreateInfoCount = 2U;
        }
        vk_result =
            vkCreateDevice(renderer->physical_device, &create_info, NULL, &renderer->device);
    }
    if (vk_result != VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    volkLoadDevice(renderer->device);
    vkGetDeviceQueue(renderer->device, renderer->graphics_family, 0U, &renderer->graphics_queue);
    vkGetDeviceQueue(renderer->device, renderer->present_family, 0U, &renderer->present_queue);
    af_vk_name(renderer, VK_OBJECT_TYPE_DEVICE, (uint64_t)(uintptr_t)renderer->device,
               "Arcflow device");
    return AF_RESULT_SUCCESS;
}

static VkSurfaceFormatKHR af_vk_choose_surface_format(const VkSurfaceFormatKHR *formats,
                                                      uint32_t count) {
    uint32_t index = 0U;
    for (index = 0U; index < count; ++index) {
        if ((formats[index].format == VK_FORMAT_B8G8R8A8_SRGB ||
             formats[index].format == VK_FORMAT_R8G8B8A8_SRGB) &&
            formats[index].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return formats[index];
        }
    }
    return formats[0];
}

static VkPresentModeKHR af_vk_choose_present_mode(const VkPresentModeKHR *modes, uint32_t count) {
    uint32_t index = 0U;
    for (index = 0U; index < count; ++index) {
        if (modes[index] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return modes[index];
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

static uint32_t af_vk_find_memory_type(AfVkRenderer *renderer, uint32_t type_bits,
                                       VkMemoryPropertyFlags required) {
    VkPhysicalDeviceMemoryProperties properties;
    uint32_t index = 0U;
    vkGetPhysicalDeviceMemoryProperties(renderer->physical_device, &properties);
    for (index = 0U; index < properties.memoryTypeCount; ++index) {
        if ((type_bits & (1U << index)) != 0U &&
            (properties.memoryTypes[index].propertyFlags & required) == required) {
            return index;
        }
    }
    return UINT32_MAX;
}

static void af_vk_destroy_capture_targets(AfVkRenderer *renderer) {
    for (uint32_t index = 0U; index < AF_FRAME_COUNT; ++index) {
        AfVkFrame *frame = &renderer->frames[index];
        if (frame->capture_mapping != NULL) {
            vkUnmapMemory(renderer->device, frame->capture_memory);
            frame->capture_mapping = NULL;
        }
        if (frame->capture_buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(renderer->device, frame->capture_buffer, NULL);
            frame->capture_buffer = VK_NULL_HANDLE;
        }
        if (frame->capture_memory != VK_NULL_HANDLE) {
            vkFreeMemory(renderer->device, frame->capture_memory, NULL);
            frame->capture_memory = VK_NULL_HANDLE;
        }
        frame->capture_size = 0U;
    }
}

static AfResult af_vk_create_capture_targets(AfVkRenderer *renderer) {
    const VkDeviceSize capture_size =
        (VkDeviceSize)renderer->extent.width * renderer->extent.height * 4U;
    if (!renderer->capture_supported) {
        return AF_RESULT_SUCCESS;
    }
    for (uint32_t index = 0U; index < AF_FRAME_COUNT; ++index) {
        AfVkFrame *frame = &renderer->frames[index];
        const VkBufferCreateInfo buffer_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                .size = capture_size,
                                                .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
        VkMemoryRequirements requirements;
        VkMemoryAllocateInfo allocation = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        uint32_t memory_type = UINT32_MAX;
        if (vkCreateBuffer(renderer->device, &buffer_info, NULL, &frame->capture_buffer) !=
            VK_SUCCESS) {
            return AF_RESULT_VULKAN_ERROR;
        }
        vkGetBufferMemoryRequirements(renderer->device, frame->capture_buffer, &requirements);
        memory_type = af_vk_find_memory_type(renderer, requirements.memoryTypeBits,
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (memory_type == UINT32_MAX) {
            af_vk_destroy_capture_targets(renderer);
            renderer->capture_supported = false;
            return AF_RESULT_SUCCESS;
        }
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = memory_type;
        if (vkAllocateMemory(renderer->device, &allocation, NULL, &frame->capture_memory) !=
                VK_SUCCESS ||
            vkBindBufferMemory(renderer->device, frame->capture_buffer, frame->capture_memory,
                               0U) != VK_SUCCESS ||
            vkMapMemory(renderer->device, frame->capture_memory, 0U, capture_size, 0U,
                        (void **)&frame->capture_mapping) != VK_SUCCESS) {
            return AF_RESULT_VULKAN_ERROR;
        }
        frame->capture_size = capture_size;
        af_vk_name(renderer, VK_OBJECT_TYPE_BUFFER, (uint64_t)(uintptr_t)frame->capture_buffer,
                   "Arcflow frame capture readback");
    }
    return AF_RESULT_SUCCESS;
}

static void af_vk_destroy_multisample_targets(AfVkRenderer *renderer) {
    for (uint32_t index = 0U; index < AF_FRAME_COUNT; ++index) {
        AfVkFrame *frame = &renderer->frames[index];
        if (frame->color_view != VK_NULL_HANDLE) {
            vkDestroyImageView(renderer->device, frame->color_view, NULL);
            frame->color_view = VK_NULL_HANDLE;
        }
        if (frame->color_image != VK_NULL_HANDLE) {
            vkDestroyImage(renderer->device, frame->color_image, NULL);
            frame->color_image = VK_NULL_HANDLE;
        }
        if (frame->color_memory != VK_NULL_HANDLE) {
            vkFreeMemory(renderer->device, frame->color_memory, NULL);
            frame->color_memory = VK_NULL_HANDLE;
        }
        frame->color_initialized = false;
    }
}

static AfResult af_vk_create_multisample_targets(AfVkRenderer *renderer) {
    if (renderer->sample_count == VK_SAMPLE_COUNT_1_BIT) {
        return AF_RESULT_SUCCESS;
    }
    for (uint32_t index = 0U; index < AF_FRAME_COUNT; ++index) {
        AfVkFrame *frame = &renderer->frames[index];
        const VkImageCreateInfo image_info = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                              .imageType = VK_IMAGE_TYPE_2D,
                                              .format = renderer->swapchain_format,
                                              .extent = {.width = renderer->extent.width,
                                                         .height = renderer->extent.height,
                                                         .depth = 1U},
                                              .mipLevels = 1U,
                                              .arrayLayers = 1U,
                                              .samples = renderer->sample_count,
                                              .tiling = VK_IMAGE_TILING_OPTIMAL,
                                              .usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                                                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                              .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
        VkMemoryRequirements requirements;
        VkMemoryAllocateInfo allocation = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = renderer->swapchain_format,
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel = 0U,
                                 .levelCount = 1U,
                                 .baseArrayLayer = 0U,
                                 .layerCount = 1U}};
        uint32_t memory_type = UINT32_MAX;
        if (vkCreateImage(renderer->device, &image_info, NULL, &frame->color_image) != VK_SUCCESS) {
            return AF_RESULT_VULKAN_ERROR;
        }
        vkGetImageMemoryRequirements(renderer->device, frame->color_image, &requirements);
        memory_type = af_vk_find_memory_type(renderer, requirements.memoryTypeBits,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memory_type == UINT32_MAX) {
            return AF_RESULT_UNSUPPORTED_FEATURE;
        }
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = memory_type;
        if (vkAllocateMemory(renderer->device, &allocation, NULL, &frame->color_memory) !=
                VK_SUCCESS ||
            vkBindImageMemory(renderer->device, frame->color_image, frame->color_memory, 0U) !=
                VK_SUCCESS) {
            return AF_RESULT_VULKAN_ERROR;
        }
        view_info.image = frame->color_image;
        if (vkCreateImageView(renderer->device, &view_info, NULL, &frame->color_view) !=
            VK_SUCCESS) {
            return AF_RESULT_VULKAN_ERROR;
        }
        af_vk_name(renderer, VK_OBJECT_TYPE_IMAGE, (uint64_t)(uintptr_t)frame->color_image,
                   "Arcflow multisample color target");
        af_vk_name(renderer, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)(uintptr_t)frame->color_view,
                   "Arcflow multisample color view");
    }
    return AF_RESULT_SUCCESS;
}

static void af_vk_destroy_pipeline(AfVkRenderer *renderer) {
    if (renderer->path_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(renderer->device, renderer->path_pipeline, NULL);
        renderer->path_pipeline = VK_NULL_HANDLE;
    }
    if (renderer->path_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(renderer->device, renderer->path_pipeline_layout, NULL);
        renderer->path_pipeline_layout = VK_NULL_HANDLE;
    }
    if (renderer->pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(renderer->device, renderer->pipeline, NULL);
        renderer->pipeline = VK_NULL_HANDLE;
    }
    if (renderer->pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(renderer->device, renderer->pipeline_layout, NULL);
        renderer->pipeline_layout = VK_NULL_HANDLE;
    }
}

static void af_vk_destroy_swapchain(AfVkRenderer *renderer) {
    uint32_t index = 0U;
    af_vk_destroy_capture_targets(renderer);
    af_vk_destroy_multisample_targets(renderer);
    for (index = 0U; index < renderer->swapchain_image_count; ++index) {
        if (renderer->present_semaphores[index] != VK_NULL_HANDLE) {
            vkDestroySemaphore(renderer->device, renderer->present_semaphores[index], NULL);
            renderer->present_semaphores[index] = VK_NULL_HANDLE;
        }
        if (renderer->swapchain_views[index] != VK_NULL_HANDLE) {
            vkDestroyImageView(renderer->device, renderer->swapchain_views[index], NULL);
            renderer->swapchain_views[index] = VK_NULL_HANDLE;
        }
    }
    renderer->swapchain_image_count = 0U;
    if (renderer->swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(renderer->device, renderer->swapchain, NULL);
        renderer->swapchain = VK_NULL_HANDLE;
    }
}

static AfResult af_vk_create_swapchain(AfVkRenderer *renderer) {
    VkSurfaceFormatKHR formats[AF_MAX_SURFACE_FORMATS];
    VkPresentModeKHR modes[AF_MAX_PRESENT_MODES];
    uint32_t format_count = AF_MAX_SURFACE_FORMATS;
    uint32_t mode_count = AF_MAX_PRESENT_MODES;
    VkSurfaceFormatKHR chosen_format;
    VkPresentModeKHR chosen_mode;
    VkExtent2D extent;
    uint32_t image_count = 0U;
    uint32_t queue_families[2] = {renderer->graphics_family, renderer->present_family};
    VkSwapchainCreateInfoKHR create_info = {.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    VkResult result = VK_SUCCESS;
    int pixel_width = 0;
    int pixel_height = 0;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(renderer->physical_device, renderer->surface,
                                                  &renderer->surface_capabilities) != VK_SUCCESS ||
        vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->physical_device, renderer->surface,
                                             &format_count, formats) != VK_SUCCESS ||
        format_count == 0U || format_count > AF_MAX_SURFACE_FORMATS ||
        vkGetPhysicalDeviceSurfacePresentModesKHR(renderer->physical_device, renderer->surface,
                                                  &mode_count, modes) != VK_SUCCESS ||
        mode_count == 0U || mode_count > AF_MAX_PRESENT_MODES) {
        return AF_RESULT_VULKAN_ERROR;
    }
    chosen_format = af_vk_choose_surface_format(formats, format_count);
    chosen_mode = af_vk_choose_present_mode(modes, mode_count);
    extent = renderer->surface_capabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        if (!SDL_GetWindowSizeInPixels(renderer->platform->window, &pixel_width, &pixel_height) ||
            pixel_width <= 0 || pixel_height <= 0) {
            return AF_RESULT_NOT_FOUND;
        }
        extent.width = (uint32_t)pixel_width;
        extent.height = (uint32_t)pixel_height;
        if (extent.width < renderer->surface_capabilities.minImageExtent.width) {
            extent.width = renderer->surface_capabilities.minImageExtent.width;
        }
        if (extent.width > renderer->surface_capabilities.maxImageExtent.width) {
            extent.width = renderer->surface_capabilities.maxImageExtent.width;
        }
        if (extent.height < renderer->surface_capabilities.minImageExtent.height) {
            extent.height = renderer->surface_capabilities.minImageExtent.height;
        }
        if (extent.height > renderer->surface_capabilities.maxImageExtent.height) {
            extent.height = renderer->surface_capabilities.maxImageExtent.height;
        }
    }
    if (extent.width == 0U || extent.height == 0U) {
        return AF_RESULT_NOT_FOUND;
    }
    image_count = renderer->surface_capabilities.minImageCount + 1U;
    if (renderer->surface_capabilities.maxImageCount != 0U &&
        image_count > renderer->surface_capabilities.maxImageCount) {
        image_count = renderer->surface_capabilities.maxImageCount;
    }
    create_info.surface = renderer->surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = chosen_format.format;
    create_info.imageColorSpace = chosen_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1U;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    renderer->capture_supported = (renderer->surface_capabilities.supportedUsageFlags &
                                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0U &&
                                  (chosen_format.format == VK_FORMAT_B8G8R8A8_SRGB ||
                                   chosen_format.format == VK_FORMAT_B8G8R8A8_UNORM ||
                                   chosen_format.format == VK_FORMAT_R8G8B8A8_SRGB ||
                                   chosen_format.format == VK_FORMAT_R8G8B8A8_UNORM);
    if (renderer->capture_supported) {
        create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    create_info.imageSharingMode = renderer->graphics_family == renderer->present_family
                                       ? VK_SHARING_MODE_EXCLUSIVE
                                       : VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount =
        renderer->graphics_family == renderer->present_family ? 0U : 2U;
    create_info.pQueueFamilyIndices =
        renderer->graphics_family == renderer->present_family ? NULL : queue_families;
    create_info.preTransform = renderer->surface_capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = chosen_mode;
    create_info.clipped = VK_TRUE;
    result = vkCreateSwapchainKHR(renderer->device, &create_info, NULL, &renderer->swapchain);
    if (result != VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    renderer->swapchain_image_count = AF_MAX_SWAPCHAIN_IMAGES;
    result = vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain,
                                     &renderer->swapchain_image_count, renderer->swapchain_images);
    if (result != VK_SUCCESS || renderer->swapchain_image_count > AF_MAX_SWAPCHAIN_IMAGES) {
        return AF_RESULT_VULKAN_ERROR;
    }
    renderer->swapchain_format = chosen_format.format;
    renderer->extent = extent;
    for (image_count = 0U; image_count < renderer->swapchain_image_count; ++image_count) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = renderer->swapchain_images[image_count],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = renderer->swapchain_format,
            .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .a = VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .baseMipLevel = 0U,
                                 .levelCount = 1U,
                                 .baseArrayLayer = 0U,
                                 .layerCount = 1U}};
        VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        if (vkCreateImageView(renderer->device, &view_info, NULL,
                              &renderer->swapchain_views[image_count]) != VK_SUCCESS ||
            vkCreateSemaphore(renderer->device, &semaphore_info, NULL,
                              &renderer->present_semaphores[image_count]) != VK_SUCCESS) {
            return AF_RESULT_VULKAN_ERROR;
        }
        af_vk_name(renderer, VK_OBJECT_TYPE_SEMAPHORE,
                   (uint64_t)(uintptr_t)renderer->present_semaphores[image_count],
                   "Arcflow image presentation semaphore");
    }
    af_vk_name(renderer, VK_OBJECT_TYPE_SWAPCHAIN_KHR, (uint64_t)(uintptr_t)renderer->swapchain,
               "Arcflow swapchain");
    {
        AfResult target_result = af_vk_create_multisample_targets(renderer);
        if (target_result == AF_RESULT_SUCCESS) {
            target_result = af_vk_create_capture_targets(renderer);
        }
        return target_result;
    }
}

static AfResult af_vk_load_shader(AfVkRenderer *renderer, const char *path,
                                  VkShaderModule *out_module) {
    uint32_t code[65536];
    FILE *file = NULL;
    long length = 0L;
    VkShaderModuleCreateInfo create_info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    if (path == NULL || out_module == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        af_log(AF_LOG_ERROR, "Unable to load SPIR-V shader: %s", path);
        return AF_RESULT_IO_ERROR;
    }
    if (fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) <= 0L ||
        length > (long)sizeof(code) || (length & 3L) != 0L || fseek(file, 0L, SEEK_SET) != 0 ||
        fread(code, 1U, (size_t)length, file) != (size_t)length) {
        (void)fclose(file);
        af_log(AF_LOG_ERROR, "Unable to load SPIR-V shader: %s", path);
        return AF_RESULT_IO_ERROR;
    }
    if (fclose(file) != 0) {
        return AF_RESULT_IO_ERROR;
    }
    create_info.codeSize = (size_t)length;
    create_info.pCode = code;
    return vkCreateShaderModule(renderer->device, &create_info, NULL, out_module) == VK_SUCCESS
               ? AF_RESULT_SUCCESS
               : AF_RESULT_VULKAN_ERROR;
}

static AfResult af_vk_create_pipeline(AfVkRenderer *renderer) {
    VkShaderModule vertex_shader = VK_NULL_HANDLE;
    VkShaderModule fragment_shader = VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo stages[2];
    VkVertexInputBindingDescription binding = {
        .binding = 0U, .stride = sizeof(AfGpuShape), .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE};
    VkVertexInputAttributeDescription attributes[4] = {
        {.location = 0U,
         .binding = 0U,
         .format = VK_FORMAT_R32G32B32A32_SFLOAT,
         .offset = offsetof(AfGpuShape, matrix)},
        {.location = 1U,
         .binding = 0U,
         .format = VK_FORMAT_R32G32B32A32_SFLOAT,
         .offset = offsetof(AfGpuShape, translation_bounds)},
        {.location = 2U,
         .binding = 0U,
         .format = VK_FORMAT_R32G32B32A32_SFLOAT,
         .offset = offsetof(AfGpuShape, color)},
        {.location = 3U,
         .binding = 0U,
         .format = VK_FORMAT_R32G32B32A32_SFLOAT,
         .offset = offsetof(AfGpuShape, parameters)}};
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1U,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = 4U,
        .pVertexAttributeDescriptions = attributes};
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkPipelineViewportStateCreateInfo viewport = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1U,
        .scissorCount = 1U};
    VkPipelineRasterizationStateCreateInfo rasterization = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0F};
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = renderer->sample_count,
        .minSampleShading = 1.0F};
    VkPipelineColorBlendAttachmentState blend_attachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    VkPipelineColorBlendStateCreateInfo blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1U,
        .pAttachments = &blend_attachment};
    VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2U,
        .pDynamicStates = dynamic_states};
    VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0U, .size = 2U * sizeof(float)};
    VkPipelineLayoutCreateInfo layout_info = {.sType =
                                                  VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                              .pushConstantRangeCount = 1U,
                                              .pPushConstantRanges = &push_range};
    VkPipelineRenderingCreateInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1U,
        .pColorAttachmentFormats = &renderer->swapchain_format};
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering_info,
        .stageCount = 2U,
        .pStages = stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &blending,
        .pDynamicState = &dynamic};
    AfResult result =
        af_vk_load_shader(renderer, ARCFLOW_SHADER_DIR "/analytic.vert.spv", &vertex_shader);
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_vk_load_shader(renderer, ARCFLOW_SHADER_DIR "/analytic.frag.spv", &fragment_shader);
    }
    if (result != AF_RESULT_SUCCESS) {
        if (vertex_shader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(renderer->device, vertex_shader, NULL);
        }
        return result;
    }
    stages[0] = (VkPipelineShaderStageCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertex_shader,
        .pName = "main"};
    stages[1] = (VkPipelineShaderStageCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragment_shader,
        .pName = "main"};
    if (vkCreatePipelineLayout(renderer->device, &layout_info, NULL, &renderer->pipeline_layout) !=
        VK_SUCCESS) {
        result = AF_RESULT_VULKAN_ERROR;
    } else {
        pipeline_info.layout = renderer->pipeline_layout;
        if (vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1U, &pipeline_info, NULL,
                                      &renderer->pipeline) != VK_SUCCESS) {
            result = AF_RESULT_VULKAN_ERROR;
        }
    }
    vkDestroyShaderModule(renderer->device, fragment_shader, NULL);
    vkDestroyShaderModule(renderer->device, vertex_shader, NULL);
    if (result == AF_RESULT_SUCCESS) {
        af_vk_name(renderer, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                   (uint64_t)(uintptr_t)renderer->pipeline_layout, "Arcflow analytic layout");
        af_vk_name(renderer, VK_OBJECT_TYPE_PIPELINE, (uint64_t)(uintptr_t)renderer->pipeline,
                   "Arcflow analytic pipeline");
    }
    return result;
}

static AfResult af_vk_create_path_pipeline(AfVkRenderer *renderer) {
    VkShaderModule vertex_shader = VK_NULL_HANDLE;
    VkShaderModule fragment_shader = VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo stages[2];
    const VkVertexInputBindingDescription binding = {
        .binding = 0U, .stride = sizeof(AfGpuPathVertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    const VkVertexInputAttributeDescription attributes[2] = {
        {.location = 0U,
         .binding = 0U,
         .format = VK_FORMAT_R32G32_SFLOAT,
         .offset = offsetof(AfGpuPathVertex, position)},
        {.location = 1U,
         .binding = 0U,
         .format = VK_FORMAT_R32G32B32A32_SFLOAT,
         .offset = offsetof(AfGpuPathVertex, color)}};
    const VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1U,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = 2U,
        .pVertexAttributeDescriptions = attributes};
    const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    const VkPipelineViewportStateCreateInfo viewport = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1U,
        .scissorCount = 1U};
    const VkPipelineRasterizationStateCreateInfo rasterization = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0F};
    const VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = renderer->sample_count,
        .minSampleShading = 1.0F};
    const VkPipelineColorBlendAttachmentState blend_attachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
    const VkPipelineColorBlendStateCreateInfo blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1U,
        .pAttachments = &blend_attachment};
    const VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamic = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2U,
        .pDynamicStates = dynamic_states};
    const VkPushConstantRange push_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0U, .size = 2U * sizeof(float)};
    const VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1U,
        .pPushConstantRanges = &push_range};
    const VkPipelineRenderingCreateInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1U,
        .pColorAttachmentFormats = &renderer->swapchain_format};
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering_info,
        .stageCount = 2U,
        .pStages = stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &blending,
        .pDynamicState = &dynamic};
    AfResult result =
        af_vk_load_shader(renderer, ARCFLOW_SHADER_DIR "/path.vert.spv", &vertex_shader);
    if (result == AF_RESULT_SUCCESS) {
        result = af_vk_load_shader(renderer, ARCFLOW_SHADER_DIR "/path.frag.spv", &fragment_shader);
    }
    if (result != AF_RESULT_SUCCESS) {
        if (vertex_shader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(renderer->device, vertex_shader, NULL);
        }
        return result;
    }
    stages[0] = (VkPipelineShaderStageCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertex_shader,
        .pName = "main"};
    stages[1] = (VkPipelineShaderStageCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragment_shader,
        .pName = "main"};
    if (vkCreatePipelineLayout(renderer->device, &layout_info, NULL,
                               &renderer->path_pipeline_layout) != VK_SUCCESS) {
        result = AF_RESULT_VULKAN_ERROR;
    } else {
        pipeline_info.layout = renderer->path_pipeline_layout;
        if (vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1U, &pipeline_info, NULL,
                                      &renderer->path_pipeline) != VK_SUCCESS) {
            result = AF_RESULT_VULKAN_ERROR;
        }
    }
    vkDestroyShaderModule(renderer->device, fragment_shader, NULL);
    vkDestroyShaderModule(renderer->device, vertex_shader, NULL);
    if (result == AF_RESULT_SUCCESS) {
        af_vk_name(renderer, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                   (uint64_t)(uintptr_t)renderer->path_pipeline_layout, "Arcflow path layout");
        af_vk_name(renderer, VK_OBJECT_TYPE_PIPELINE, (uint64_t)(uintptr_t)renderer->path_pipeline,
                   "Arcflow path pipeline");
    }
    return result;
}

static AfResult af_vk_create_upload_ring(AfVkRenderer *renderer) {
    const VkDeviceSize slice_size =
        AF_MAX_NODES * sizeof(AfGpuShape) +
        (VkDeviceSize)AF_MAX_GPU_PATH_VERTICES * sizeof(AfGpuPathVertex);
    VkBufferCreateInfo buffer_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                      .size = AF_FRAME_COUNT * slice_size,
                                      .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                      .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo allocation = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    uint32_t memory_type = UINT32_MAX;
    if (vkCreateBuffer(renderer->device, &buffer_info, NULL, &renderer->upload_buffer) !=
        VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    vkGetBufferMemoryRequirements(renderer->device, renderer->upload_buffer, &requirements);
    memory_type = af_vk_find_memory_type(renderer, requirements.memoryTypeBits,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memory_type == UINT32_MAX) {
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memory_type;
    if (vkAllocateMemory(renderer->device, &allocation, NULL, &renderer->upload_memory) !=
            VK_SUCCESS ||
        vkBindBufferMemory(renderer->device, renderer->upload_buffer, renderer->upload_memory,
                           0U) != VK_SUCCESS ||
        vkMapMemory(renderer->device, renderer->upload_memory, 0U, buffer_info.size, 0U,
                    (void **)&renderer->upload_mapping) != VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    renderer->upload_slice_size = slice_size;
    af_vk_name(renderer, VK_OBJECT_TYPE_BUFFER, (uint64_t)(uintptr_t)renderer->upload_buffer,
               "Arcflow persistent upload ring");
    return AF_RESULT_SUCCESS;
}

static AfResult af_vk_create_frames(AfVkRenderer *renderer) {
    uint32_t index = 0U;
    for (index = 0U; index < AF_FRAME_COUNT; ++index) {
        AfVkFrame *frame = &renderer->frames[index];
        VkCommandPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                             .flags =
                                                 VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
                                                 VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                                             .queueFamilyIndex = renderer->graphics_family};
        VkCommandBufferAllocateInfo command_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = VK_NULL_HANDLE,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1U};
        VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fence_info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                        .flags = VK_FENCE_CREATE_SIGNALED_BIT};
        if (vkCreateCommandPool(renderer->device, &pool_info, NULL, &frame->command_pool) !=
            VK_SUCCESS) {
            return AF_RESULT_VULKAN_ERROR;
        }
        command_info.commandPool = frame->command_pool;
        if (vkAllocateCommandBuffers(renderer->device, &command_info, &frame->command_buffer) !=
                VK_SUCCESS ||
            vkCreateSemaphore(renderer->device, &semaphore_info, NULL, &frame->image_available) !=
                VK_SUCCESS ||
            vkCreateFence(renderer->device, &fence_info, NULL, &frame->fence) != VK_SUCCESS) {
            return AF_RESULT_VULKAN_ERROR;
        }
        af_vk_name(renderer, VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)(uintptr_t)frame->command_pool,
                   "Arcflow frame command pool");
        af_vk_name(renderer, VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)(uintptr_t)frame->image_available,
                   "Arcflow image available semaphore");
        af_vk_name(renderer, VK_OBJECT_TYPE_FENCE, (uint64_t)(uintptr_t)frame->fence,
                   "Arcflow frame fence");
    }
    return AF_RESULT_SUCCESS;
}

static AfResult af_vk_recreate_swapchain(AfVkRenderer *renderer) {
    AfResult result = AF_RESULT_SUCCESS;
    if (vkDeviceWaitIdle(renderer->device) != VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    af_vk_destroy_pipeline(renderer);
    af_vk_destroy_swapchain(renderer);
    result = af_vk_create_swapchain(renderer);
    if (result == AF_RESULT_NOT_FOUND) {
        renderer->resize_requested = true;
        return AF_RESULT_SUCCESS;
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_vk_create_pipeline(renderer);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_vk_create_path_pipeline(renderer);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_vk_particles_set_render_target(renderer->particles, renderer->swapchain_format,
                                                   renderer->sample_count);
    }
    renderer->resize_requested = result != AF_RESULT_SUCCESS;
    return result;
}

AfResult af_vk_renderer_create(AfPlatformWindow *window, AfVkRenderer **out_renderer) {
    AfVkRenderer *renderer = NULL;
    AfResult result = AF_RESULT_SUCCESS;
    if (window == NULL || window->window == NULL || out_renderer == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_renderer = NULL;
    renderer = calloc(1U, sizeof(*renderer));
    if (renderer == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    renderer->path_scratch = calloc(1U, sizeof(*renderer->path_scratch));
    renderer->path_staging_vertices =
        calloc(AF_MAX_GPU_PATH_VERTICES, sizeof(*renderer->path_staging_vertices));
    if (renderer->path_scratch == NULL || renderer->path_staging_vertices == NULL) {
        af_vk_renderer_destroy(renderer);
        return AF_RESULT_OUT_OF_MEMORY;
    }
    renderer->platform = window;
    if (volkInitialize() != VK_SUCCESS) {
        result = AF_RESULT_VULKAN_ERROR;
    } else if ((result = af_vk_create_instance(renderer)) != AF_RESULT_SUCCESS) {
    } else if (!SDL_Vulkan_CreateSurface(window->window, renderer->instance, NULL,
                                         &renderer->surface)) {
        af_log(AF_LOG_ERROR, "Vulkan surface creation failed: %s", SDL_GetError());
        result = AF_RESULT_VULKAN_ERROR;
    } else if ((result = af_vk_create_device(renderer)) != AF_RESULT_SUCCESS) {
    } else if ((result = af_vk_create_swapchain(renderer)) != AF_RESULT_SUCCESS) {
    } else if ((result = af_vk_create_pipeline(renderer)) != AF_RESULT_SUCCESS) {
    } else if ((result = af_vk_create_path_pipeline(renderer)) != AF_RESULT_SUCCESS) {
    } else if ((result = af_vk_particles_create(renderer->physical_device, renderer->device,
                                                renderer->swapchain_format, renderer->sample_count,
                                                renderer->graphics_family, &renderer->particles)) !=
               AF_RESULT_SUCCESS) {
    } else if ((result = af_vk_create_upload_ring(renderer)) != AF_RESULT_SUCCESS) {
    } else {
        result = af_vk_create_frames(renderer);
    }
    if (result != AF_RESULT_SUCCESS) {
        af_vk_renderer_destroy(renderer);
        return result;
    }
    *out_renderer = renderer;
    return AF_RESULT_SUCCESS;
}

void af_vk_renderer_destroy(AfVkRenderer *renderer) {
    uint32_t index = 0U;
    if (renderer == NULL) {
        return;
    }
    if (renderer->device != VK_NULL_HANDLE) {
        (void)vkDeviceWaitIdle(renderer->device);
        for (index = 0U; index < AF_FRAME_COUNT; ++index) {
            AfVkFrame *frame = &renderer->frames[index];
            if (frame->fence != VK_NULL_HANDLE) {
                vkDestroyFence(renderer->device, frame->fence, NULL);
            }
            if (frame->image_available != VK_NULL_HANDLE) {
                vkDestroySemaphore(renderer->device, frame->image_available, NULL);
            }
            if (frame->command_pool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(renderer->device, frame->command_pool, NULL);
            }
        }
        if (renderer->upload_mapping != NULL) {
            vkUnmapMemory(renderer->device, renderer->upload_memory);
        }
        if (renderer->upload_buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(renderer->device, renderer->upload_buffer, NULL);
        }
        if (renderer->upload_memory != VK_NULL_HANDLE) {
            vkFreeMemory(renderer->device, renderer->upload_memory, NULL);
        }
        af_vk_particles_destroy(renderer->particles);
        renderer->particles = NULL;
        af_vk_destroy_pipeline(renderer);
        af_vk_destroy_swapchain(renderer);
        vkDestroyDevice(renderer->device, NULL);
    }
    if (renderer->surface != VK_NULL_HANDLE && renderer->instance != VK_NULL_HANDLE) {
        SDL_Vulkan_DestroySurface(renderer->instance, renderer->surface, NULL);
    }
    if (renderer->debug_messenger != VK_NULL_HANDLE && renderer->instance != VK_NULL_HANDLE) {
        vkDestroyDebugUtilsMessengerEXT(renderer->instance, renderer->debug_messenger, NULL);
    }
    if (renderer->instance != VK_NULL_HANDLE) {
        vkDestroyInstance(renderer->instance, NULL);
    }
    for (index = 0U; index < AF_MAX_PATHS; ++index) {
        free(renderer->path_caches[index].vertices);
        free(renderer->stroke_caches[index].vertices);
    }
    free(renderer->path_staging_vertices);
    free(renderer->path_scratch);
    free(renderer);
}

AfRuntimeCreateInfo af_vk_renderer_runtime_create_info(const AfVkRenderer *renderer) {
    if (renderer == NULL) {
        return (AfRuntimeCreateInfo){0};
    }
    return (AfRuntimeCreateInfo){.instance = renderer->instance,
                                 .physical_device = renderer->physical_device,
                                 .device = renderer->device,
                                 .graphics_queue = renderer->graphics_queue,
                                 .graphics_queue_family = renderer->graphics_family,
                                 .frames_in_flight = AF_FRAME_COUNT};
}

static AfResult af_vk_update_path_cache(AfVkRenderer *renderer, const AfPathData *data,
                                        const AfPath *path, AfVkPathCache **out_cache) {
    AfVkPathCache *cache = NULL;
    AfVkPathCache *empty = NULL;
    uint32_t vertex_count = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    for (uint32_t index = 0U; index < AF_MAX_PATHS; ++index) {
        if (renderer->path_caches[index].path_id == path->id) {
            cache = &renderer->path_caches[index];
            break;
        }
        if (empty == NULL && renderer->path_caches[index].path_id == 0U) {
            empty = &renderer->path_caches[index];
        }
    }
    if (cache == NULL) {
        cache = empty;
    }
    if (cache == NULL) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    cache->seen_serial = renderer->path_cache_serial;
    if (cache->source == data && cache->path_id == path->id &&
        cache->geometry_generation == path->geometry_generation) {
        *out_cache = cache;
        return AF_RESULT_SUCCESS;
    }
    result = af_path_tessellate(data, path, 0.25F, renderer->path_scratch,
                                renderer->path_staging_vertices, AF_MAX_PATH_TESSELLATED_VERTICES,
                                &vertex_count);
    if (result != AF_RESULT_SUCCESS ||
        vertex_count >
            AF_MAX_GPU_PATH_VERTICES - (renderer->cached_path_vertex_count - cache->vertex_count)) {
        return result == AF_RESULT_SUCCESS ? AF_RESULT_LIMIT_EXCEEDED : result;
    }
    if (vertex_count != cache->vertex_count) {
        AfVec2 *vertices = NULL;
        if (vertex_count != 0U) {
            vertices = realloc(cache->vertices, (size_t)vertex_count * sizeof(*vertices));
            if (vertices == NULL) {
                return AF_RESULT_OUT_OF_MEMORY;
            }
        } else {
            free(cache->vertices);
        }
        cache->vertices = vertices;
    }
    if (vertex_count != 0U) {
        memcpy(cache->vertices, renderer->path_staging_vertices,
               (size_t)vertex_count * sizeof(*cache->vertices));
    }
    renderer->cached_path_vertex_count =
        renderer->cached_path_vertex_count - cache->vertex_count + vertex_count;
    cache->source = data;
    cache->path_id = path->id;
    cache->geometry_generation = path->geometry_generation;
    cache->vertex_count = vertex_count;
    ++cache->rebuild_count;
    *out_cache = cache;
    return AF_RESULT_SUCCESS;
}

static AfResult af_vk_update_stroke_cache(AfVkRenderer *renderer, const AfPathData *paths,
                                          const AfPath *path, const AfStyleData *styles,
                                          const AfStyle *style, AfVkStrokeCache **out_cache) {
    AfVkStrokeCache *cache = NULL;
    AfVkStrokeCache *empty = NULL;
    AfFlattenedPath flattened = {.points = renderer->path_scratch->points,
                                 .point_capacity = AF_MAX_PATH_FLATTENED_POINTS,
                                 .contours = renderer->path_scratch->contours,
                                 .contour_capacity = AF_MAX_PATH_CONTOURS};
    uint32_t vertex_count = 0U;
    AfResult result = AF_RESULT_SUCCESS;
    for (uint32_t index = 0U; index < AF_MAX_PATHS; ++index) {
        if (renderer->stroke_caches[index].path_id == path->id &&
            renderer->stroke_caches[index].style_id == style->id) {
            cache = &renderer->stroke_caches[index];
            break;
        }
        if (empty == NULL && renderer->stroke_caches[index].path_id == 0U) {
            empty = &renderer->stroke_caches[index];
        }
    }
    if (cache == NULL) {
        cache = empty;
    }
    if (cache == NULL) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    cache->seen_serial = renderer->path_cache_serial;
    if (cache->path_source == paths && cache->style_source == styles &&
        cache->path_id == path->id && cache->style_id == style->id &&
        cache->path_geometry_generation == path->geometry_generation &&
        cache->stroke_geometry_generation == style->stroke_geometry_generation) {
        *out_cache = cache;
        return AF_RESULT_SUCCESS;
    }
    result = af_path_flatten(paths, path, 0.25F, &flattened);
    if (result == AF_RESULT_SUCCESS) {
        result = af_stroke_tessellate(&flattened, &style->stroke, renderer->path_staging_vertices,
                                      AF_MAX_PATH_TESSELLATED_VERTICES, &vertex_count);
    }
    if (result != AF_RESULT_SUCCESS ||
        vertex_count > AF_MAX_PATH_TESSELLATED_VERTICES -
                           (renderer->cached_stroke_vertex_count - cache->vertex_count)) {
        return result == AF_RESULT_SUCCESS ? AF_RESULT_LIMIT_EXCEEDED : result;
    }
    if (vertex_count != cache->vertex_count) {
        AfVec2 *vertices = NULL;
        if (vertex_count != 0U) {
            vertices = realloc(cache->vertices, (size_t)vertex_count * sizeof(*vertices));
            if (vertices == NULL) {
                return AF_RESULT_OUT_OF_MEMORY;
            }
        } else {
            free(cache->vertices);
        }
        cache->vertices = vertices;
    }
    if (vertex_count != 0U) {
        memcpy(cache->vertices, renderer->path_staging_vertices,
               (size_t)vertex_count * sizeof(*cache->vertices));
    }
    renderer->cached_stroke_vertex_count =
        renderer->cached_stroke_vertex_count - cache->vertex_count + vertex_count;
    cache->path_source = paths;
    cache->style_source = styles;
    cache->path_id = path->id;
    cache->style_id = style->id;
    cache->path_geometry_generation = path->geometry_generation;
    cache->stroke_geometry_generation = style->stroke_geometry_generation;
    cache->vertex_count = vertex_count;
    ++cache->rebuild_count;
    *out_cache = cache;
    return AF_RESULT_SUCCESS;
}

static void af_vk_insert_render_item(AfVkRenderer *renderer, uint32_t *count, AfVkRenderItem item) {
    uint32_t insert = *count;
    while (insert > 0U && renderer->render_items[insert - 1U].layer_order > item.layer_order) {
        renderer->render_items[insert] = renderer->render_items[insert - 1U];
        --insert;
    }
    renderer->render_items[insert] = item;
    ++(*count);
}

static AfResult af_vk_upload_scene(AfVkRenderer *renderer, const AfScene *scene,
                                   uint32_t *out_shape_count, uint32_t *out_path_vertex_count,
                                   uint32_t *out_item_count) {
    uint8_t *slice = renderer->upload_mapping + renderer->frame_index * renderer->upload_slice_size;
    AfGpuShape *shapes = (AfGpuShape *)(void *)slice;
    AfGpuPathVertex *path_vertices =
        (AfGpuPathVertex *)(void *)(slice + AF_MAX_NODES * sizeof(AfGpuShape));
    uint32_t shape_count = 0U;
    uint32_t path_vertex_count = 0U;
    uint32_t item_count = 0U;
    uint32_t index = 0U;
    if (++renderer->path_cache_serial == 0U) {
        renderer->path_cache_serial = 1U;
    }
    for (index = 0U; index < scene->count && item_count < AF_MAX_RENDER_ITEMS; ++index) {
        const AfSceneNode *node = &scene->nodes[index];
        const AfStyle *source_style =
            scene->styles == NULL ? NULL : af_style_find_node_const(scene->styles, node->id);
        AfStyle resolved_style;
        const AfStyle *style = NULL;
        if (node->type == AF_NODE_GROUP || !node->world_visible || node->world_opacity <= 0.0F) {
            continue;
        }
        if (source_style != NULL) {
            if (af_style_resolve(scene->styles, source_style, &resolved_style) !=
                AF_RESULT_SUCCESS) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            style = &resolved_style;
        }
        if (node->type == AF_NODE_PATH) {
            const AfPath *path =
                scene->paths == NULL ? NULL : af_path_find_node_const(scene->paths, node->id);
            AfVkPathCache *cache = NULL;
            AfResult result = AF_RESULT_SUCCESS;
            const uint32_t first = path_vertex_count;
            if (path == NULL) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            result = af_vk_update_path_cache(renderer, scene->paths, path, &cache);
            if (result != AF_RESULT_SUCCESS ||
                cache->vertex_count > AF_MAX_GPU_PATH_VERTICES - path_vertex_count) {
                return result == AF_RESULT_SUCCESS ? AF_RESULT_LIMIT_EXCEEDED : result;
            }
            for (uint32_t vertex_index = 0U; vertex_index < cache->vertex_count; ++vertex_index) {
                AfColor color = node->fill;
                const AfVec2 world =
                    af_affine_transform_point(node->world_transform, cache->vertices[vertex_index]);
                if (style != NULL && af_paint_sample(&style->fill, cache->vertices[vertex_index],
                                                     style->fill_opacity * node->world_opacity,
                                                     &color) != AF_RESULT_SUCCESS) {
                    return AF_RESULT_INVALID_ARGUMENT;
                }
                path_vertices[path_vertex_count++] = (AfGpuPathVertex){
                    .position = {world.x, world.y},
                    .color = {color.r, color.g, color.b,
                              style == NULL ? color.a * node->world_opacity : color.a}};
            }
            if (cache->vertex_count != 0U) {
                af_vk_insert_render_item(renderer, &item_count,
                                         (AfVkRenderItem){.type = AF_VK_RENDER_PATH,
                                                          .layer_order = node->layer_order,
                                                          .first = first,
                                                          .count = cache->vertex_count});
            }
            if (style != NULL && style->stroke.enabled) {
                AfVkStrokeCache *stroke_cache = NULL;
                const uint32_t stroke_first = path_vertex_count;
                result = af_vk_update_stroke_cache(renderer, scene->paths, path, scene->styles,
                                                   style, &stroke_cache);
                if (result != AF_RESULT_SUCCESS ||
                    stroke_cache->vertex_count > AF_MAX_GPU_PATH_VERTICES - path_vertex_count) {
                    return result == AF_RESULT_SUCCESS ? AF_RESULT_LIMIT_EXCEEDED : result;
                }
                for (uint32_t vertex_index = 0U; vertex_index < stroke_cache->vertex_count;
                     ++vertex_index) {
                    AfColor color;
                    const AfVec2 local = stroke_cache->vertices[vertex_index];
                    const AfVec2 world = af_affine_transform_point(node->world_transform, local);
                    if (af_paint_sample(&style->stroke_paint, local,
                                        style->stroke_opacity * node->world_opacity,
                                        &color) != AF_RESULT_SUCCESS) {
                        return AF_RESULT_INVALID_ARGUMENT;
                    }
                    path_vertices[path_vertex_count++] =
                        (AfGpuPathVertex){.position = {world.x, world.y},
                                          .color = {color.r, color.g, color.b, color.a}};
                }
                if (stroke_cache->vertex_count != 0U) {
                    af_vk_insert_render_item(renderer, &item_count,
                                             (AfVkRenderItem){.type = AF_VK_RENDER_PATH,
                                                              .layer_order = node->layer_order,
                                                              .first = stroke_first,
                                                              .count = stroke_cache->vertex_count});
                }
            }
        } else {
            if (node->type > AF_NODE_ELLIPSE || shape_count >= AF_MAX_NODES) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            AfColor color = node->fill;
            if (style != NULL &&
                af_paint_sample(
                    &style->fill,
                    (AfVec2){node->resolved_bounds.x * 0.5F, node->resolved_bounds.y * 0.5F},
                    style->fill_opacity, &color) != AF_RESULT_SUCCESS) {
                return AF_RESULT_INVALID_ARGUMENT;
            }
            shapes[shape_count] = (AfGpuShape){
                .matrix = {node->world_transform.m00, node->world_transform.m01,
                           node->world_transform.m10, node->world_transform.m11},
                .translation_bounds = {node->world_transform.tx, node->world_transform.ty,
                                       node->resolved_bounds.x, node->resolved_bounds.y},
                .color = {color.r, color.g, color.b, color.a},
                .parameters = {(float)node->type, node->corner_radius, node->world_opacity, 0.0F},
                .layer_order = node->layer_order};
            af_vk_insert_render_item(renderer, &item_count,
                                     (AfVkRenderItem){.type = AF_VK_RENDER_ANALYTIC,
                                                      .layer_order = node->layer_order,
                                                      .first = shape_count,
                                                      .count = 1U});
            ++shape_count;
        }
    }
    for (index = 0U; index < AF_MAX_PATHS; ++index) {
        AfVkPathCache *cache = &renderer->path_caches[index];
        if (cache->path_id != 0U && cache->seen_serial != renderer->path_cache_serial) {
            renderer->cached_path_vertex_count -= cache->vertex_count;
            free(cache->vertices);
            *cache = (AfVkPathCache){0};
        }
        {
            AfVkStrokeCache *stroke_cache = &renderer->stroke_caches[index];
            if (stroke_cache->path_id != 0U &&
                stroke_cache->seen_serial != renderer->path_cache_serial) {
                renderer->cached_stroke_vertex_count -= stroke_cache->vertex_count;
                free(stroke_cache->vertices);
                *stroke_cache = (AfVkStrokeCache){0};
            }
        }
    }
    *out_shape_count = shape_count;
    *out_path_vertex_count = path_vertex_count;
    *out_item_count = item_count;
    return AF_RESULT_SUCCESS;
}

AfResult af_vk_renderer_draw_timed(AfVkRenderer *renderer, const AfScene *scene, float delta_time) {
    AfVkFrame *frame = NULL;
    uint32_t image_index = 0U;
    uint32_t shape_count = 0U;
    uint32_t path_vertex_count = 0U;
    uint32_t item_count = 0U;
    VkResult result = VK_SUCCESS;
    AfResult capture_result = AF_RESULT_SUCCESS;
    bool capture_this_frame = false;
    bool present_failed = false;
    VkCommandBufferBeginInfo begin_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                           .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VkImageMemoryBarrier2 to_render = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                       .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                                       .srcAccessMask = VK_ACCESS_2_NONE,
                                       .dstStageMask =
                                           VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                       .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                       .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                       .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                       .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                            .baseMipLevel = 0U,
                                                            .levelCount = 1U,
                                                            .baseArrayLayer = 0U,
                                                            .layerCount = 1U}};
    VkDependencyInfo dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                   .imageMemoryBarrierCount = 1U,
                                   .pImageMemoryBarriers = &to_render};
    VkImageMemoryBarrier2 to_multisample = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0U,
                             .levelCount = 1U,
                             .baseArrayLayer = 0U,
                             .layerCount = 1U}};
    VkDependencyInfo multisample_dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                               .imageMemoryBarrierCount = 1U,
                                               .pImageMemoryBarriers = &to_multisample};
    VkRenderingAttachmentInfo color_attachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.color = {{0.018F, 0.024F, 0.04F, 1.0F}}}};
    VkRenderingInfo rendering = {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                 .layerCount = 1U,
                                 .colorAttachmentCount = 1U,
                                 .pColorAttachments = &color_attachment};
    VkViewport viewport;
    VkRect2D scissor;
    float canvas[2];
    VkDeviceSize upload_offset = 0U;
    VkDeviceSize path_upload_offset = 0U;
    VkSemaphoreSubmitInfo wait_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                       .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       .deviceIndex = 0U,
                                       .value = 0U};
    VkSemaphoreSubmitInfo signal_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                         .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                         .deviceIndex = 0U,
                                         .value = 0U};
    VkCommandBufferSubmitInfo command_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                                              .deviceMask = 0U};
    VkSubmitInfo2 submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                            .waitSemaphoreInfoCount = 1U,
                            .pWaitSemaphoreInfos = &wait_info,
                            .commandBufferInfoCount = 1U,
                            .pCommandBufferInfos = &command_info,
                            .signalSemaphoreInfoCount = 1U,
                            .pSignalSemaphoreInfos = &signal_info};
    VkPresentInfoKHR present = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                .waitSemaphoreCount = 1U,
                                .pSwapchains = &renderer->swapchain,
                                .swapchainCount = 1U,
                                .pImageIndices = &image_index};
    if (renderer == NULL || scene == NULL || !isfinite(delta_time) || delta_time < 0.0F) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (renderer->resize_requested) {
        AfResult recreate_result = af_vk_recreate_swapchain(renderer);
        if (recreate_result != AF_RESULT_SUCCESS || renderer->resize_requested) {
            return recreate_result;
        }
    }
    if (renderer->capture_requested && !renderer->capture_supported) {
        renderer->capture_requested = false;
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    capture_this_frame = renderer->capture_requested;
    frame = &renderer->frames[renderer->frame_index];
    if (vkWaitForFences(renderer->device, 1U, &frame->fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    {
        const AfResult upload_result =
            af_vk_upload_scene(renderer, scene, &shape_count, &path_vertex_count, &item_count);
        if (upload_result != AF_RESULT_SUCCESS) {
            return upload_result;
        }
    }
    {
        const AfResult particle_result =
            af_vk_particles_update(renderer->particles, scene, renderer->frame_index);
        if (particle_result != AF_RESULT_SUCCESS) {
            return particle_result;
        }
    }
    result = vkAcquireNextImageKHR(renderer->device, renderer->swapchain, UINT64_MAX,
                                   frame->image_available, VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        renderer->resize_requested = true;
        return AF_RESULT_SUCCESS;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return AF_RESULT_VULKAN_ERROR;
    }
    if (result == VK_SUBOPTIMAL_KHR) {
        renderer->resize_requested = true;
    }
    if (vkResetFences(renderer->device, 1U, &frame->fence) != VK_SUCCESS ||
        vkResetCommandPool(renderer->device, frame->command_pool, 0U) != VK_SUCCESS ||
        vkBeginCommandBuffer(frame->command_buffer, &begin_info) != VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    to_render.image = renderer->swapchain_images[image_index];
    if (renderer->sample_count == VK_SAMPLE_COUNT_1_BIT) {
        color_attachment.imageView = renderer->swapchain_views[image_index];
        color_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
        color_attachment.resolveImageView = VK_NULL_HANDLE;
        color_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    } else {
        color_attachment.imageView = frame->color_view;
        color_attachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        color_attachment.resolveImageView = renderer->swapchain_views[image_index];
        color_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        to_multisample.image = frame->color_image;
    }
    rendering.renderArea.extent = renderer->extent;
    viewport = (VkViewport){.x = 0.0F,
                            .y = 0.0F,
                            .width = (float)renderer->extent.width,
                            .height = (float)renderer->extent.height,
                            .minDepth = 0.0F,
                            .maxDepth = 1.0F};
    scissor = (VkRect2D){.extent = renderer->extent};
    canvas[0] = (float)renderer->extent.width;
    canvas[1] = (float)renderer->extent.height;
    upload_offset = renderer->frame_index * renderer->upload_slice_size;
    path_upload_offset = upload_offset + AF_MAX_NODES * sizeof(AfGpuShape);
    af_vk_particles_record(renderer->particles, frame->command_buffer, renderer->frame_index,
                           delta_time);
    vkCmdPipelineBarrier2(frame->command_buffer, &dependency);
    if (renderer->sample_count != VK_SAMPLE_COUNT_1_BIT && !frame->color_initialized) {
        vkCmdPipelineBarrier2(frame->command_buffer, &multisample_dependency);
    }
    vkCmdBeginRendering(frame->command_buffer, &rendering);
    vkCmdSetViewport(frame->command_buffer, 0U, 1U, &viewport);
    vkCmdSetScissor(frame->command_buffer, 0U, 1U, &scissor);
    {
        AfVkRenderItemType bound_type = (AfVkRenderItemType)UINT32_MAX;
        for (uint32_t item_index = 0U; item_index < item_count; ++item_index) {
            const AfVkRenderItem *item = &renderer->render_items[item_index];
            if (item->type != bound_type) {
                const VkDeviceSize *offset =
                    item->type == AF_VK_RENDER_PATH ? &path_upload_offset : &upload_offset;
                const VkPipeline pipeline =
                    item->type == AF_VK_RENDER_PATH ? renderer->path_pipeline : renderer->pipeline;
                const VkPipelineLayout layout = item->type == AF_VK_RENDER_PATH
                                                    ? renderer->path_pipeline_layout
                                                    : renderer->pipeline_layout;
                vkCmdBindPipeline(frame->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
                vkCmdBindVertexBuffers(frame->command_buffer, 0U, 1U, &renderer->upload_buffer,
                                       offset);
                vkCmdPushConstants(frame->command_buffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0U,
                                   sizeof(canvas), canvas);
                bound_type = item->type;
            }
            if (item->type == AF_VK_RENDER_PATH) {
                vkCmdDraw(frame->command_buffer, item->count, 1U, item->first, 0U);
            } else {
                vkCmdDraw(frame->command_buffer, 6U, 1U, 0U, item->first);
            }
        }
    }
    af_vk_particles_draw(renderer->particles, frame->command_buffer, renderer->frame_index,
                         renderer->extent);
    (void)shape_count;
    (void)path_vertex_count;
    vkCmdEndRendering(frame->command_buffer);
    to_render.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    to_render.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    to_render.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (capture_this_frame) {
        const VkBufferImageCopy copy = {
            .bufferOffset = 0U,
            .bufferRowLength = 0U,
            .bufferImageHeight = 0U,
            .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                 .mipLevel = 0U,
                                 .baseArrayLayer = 0U,
                                 .layerCount = 1U},
            .imageOffset = {0, 0, 0},
            .imageExtent = {
                .width = renderer->extent.width, .height = renderer->extent.height, .depth = 1U}};
        const VkBufferMemoryBarrier2 to_host = {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                                                .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                                .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                                                .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT,
                                                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                .buffer = frame->capture_buffer,
                                                .offset = 0U,
                                                .size = frame->capture_size};
        const VkDependencyInfo host_dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                                  .bufferMemoryBarrierCount = 1U,
                                                  .pBufferMemoryBarriers = &to_host};
        to_render.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        to_render.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        to_render.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        vkCmdPipelineBarrier2(frame->command_buffer, &dependency);
        vkCmdCopyImageToBuffer(frame->command_buffer, renderer->swapchain_images[image_index],
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, frame->capture_buffer, 1U,
                               &copy);
        vkCmdPipelineBarrier2(frame->command_buffer, &host_dependency);
        to_render.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        to_render.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        to_render.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
        to_render.dstAccessMask = VK_ACCESS_2_NONE;
        to_render.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        to_render.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vkCmdPipelineBarrier2(frame->command_buffer, &dependency);
    } else {
        to_render.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
        to_render.dstAccessMask = VK_ACCESS_2_NONE;
        to_render.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vkCmdPipelineBarrier2(frame->command_buffer, &dependency);
    }
    if (vkEndCommandBuffer(frame->command_buffer) != VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    wait_info.semaphore = frame->image_available;
    signal_info.semaphore = renderer->present_semaphores[image_index];
    command_info.commandBuffer = frame->command_buffer;
    present.pWaitSemaphores = &renderer->present_semaphores[image_index];
    if (vkQueueSubmit2(renderer->graphics_queue, 1U, &submit, frame->fence) != VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    frame->color_initialized = true;
    result = vkQueuePresentKHR(renderer->present_queue, &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        renderer->resize_requested = true;
    } else if (result != VK_SUCCESS) {
        present_failed = true;
    }
    if (capture_this_frame) {
        const AfPixelOrder order = renderer->swapchain_format == VK_FORMAT_B8G8R8A8_SRGB ||
                                           renderer->swapchain_format == VK_FORMAT_B8G8R8A8_UNORM
                                       ? AF_PIXEL_ORDER_BGRA
                                       : AF_PIXEL_ORDER_RGBA;
        if (vkWaitForFences(renderer->device, 1U, &frame->fence, VK_TRUE, UINT64_MAX) !=
            VK_SUCCESS) {
            capture_result = AF_RESULT_VULKAN_ERROR;
        } else {
            capture_result = af_png_write_rgba8(renderer->capture_path, frame->capture_mapping,
                                                renderer->extent.width, renderer->extent.height,
                                                renderer->extent.width * 4U, order);
        }
        renderer->capture_requested = false;
    }
    renderer->frame_index = (renderer->frame_index + 1U) % AF_FRAME_COUNT;
    if (present_failed) {
        return AF_RESULT_VULKAN_ERROR;
    }
    return capture_result;
}

AfResult af_vk_renderer_draw(AfVkRenderer *renderer, const AfScene *scene) {
    return af_vk_renderer_draw_timed(renderer, scene, AF_PARTICLE_FIXED_STEP);
}

AfResult af_vk_renderer_capture_next(AfVkRenderer *renderer, const char *path) {
    size_t path_length = 0U;
    if (renderer == NULL || path == NULL || path[0] == '\0') {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (!renderer->capture_supported) {
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    path_length = strlen(path);
    if (path_length >= sizeof(renderer->capture_path)) {
        return AF_RESULT_LIMIT_EXCEEDED;
    }
    memcpy(renderer->capture_path, path, path_length + 1U);
    renderer->capture_requested = true;
    return AF_RESULT_SUCCESS;
}

void af_vk_renderer_request_resize(AfVkRenderer *renderer) {
    if (renderer != NULL) {
        renderer->resize_requested = true;
    }
}

bool af_vk_renderer_has_validation_errors(const AfVkRenderer *renderer) {
    return renderer != NULL && renderer->validation_error;
}

AfVkVfxMetrics af_vk_renderer_vfx_metrics(const AfVkRenderer *renderer) {
    return renderer == NULL ? (AfVkVfxMetrics){0} : af_vk_particles_metrics(renderer->particles);
}
