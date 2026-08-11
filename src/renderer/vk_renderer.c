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

// clang-format off
#include "renderer/vk_renderer_device.inc"
#include "renderer/vk_renderer_swapchain.inc"
#include "renderer/vk_renderer_lifecycle.inc"
#include "renderer/vk_renderer_upload.inc"
#include "renderer/vk_renderer_draw.inc"
// clang-format on
