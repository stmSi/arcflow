#include "renderer/vk_renderer.h"

#include "arcflow/arcflow_build_info.h"
#include "core/hash.h"
#include "core/log.h"
#include "core/png.h"
#include "renderer/render_items.h"
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
#define AF_MAX_GPU_PAINTS (AF_MAX_NODES * 2U + 1U)
#define AF_MAX_GPU_SHAPES (AF_MAX_NODES * 2U)
#define AF_MAX_GPU_PATH_VERTICES (AF_MAX_PATH_TESSELLATED_VERTICES * 4U)
#define AF_MAX_RENDER_ITEMS (AF_MAX_NODES * 2U)
#define AF_MAX_GPU_IMAGE_VERTICES (AF_MAX_NODES * 54U)
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
    bool timestamps_pending;
} AfVkFrame;

typedef struct AfGpuShape {
    float matrix[4];
    float translation_bounds[4];
    float color[4];
    float parameters[4];
    int32_t layer_order;
    uint32_t paint_index;
} AfGpuShape;

typedef struct AfGpuPathVertex {
    float position[2];
    float color[4];
    uint32_t paint_index;
} AfGpuPathVertex;

typedef struct AfGpuImageVertex {
    float position[2];
    float uv[2];
    float color[4];
} AfGpuImageVertex;

/* Matches shaders/paint.glsl's std430 vec4 layout. */
typedef struct AfGpuPaint {
    float inverse_x[4], inverse_y[4], endpoints[4], parameters[4];
    float colors[AF_MAX_GRADIENT_STOPS][4];
    float offsets[AF_MAX_GRADIENT_STOPS];
} AfGpuPaint;
_Static_assert(sizeof(AfGpuPaint) == 384, "GPU paint layout");

typedef struct AfVkPathCache {
    const AfPathData *source;
    uint64_t path_id;
    uint64_t geometry_generation;
    uint64_t content_hash;
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
    uint64_t path_content_hash;
    uint64_t stroke_geometry_generation;
    AfVec2 *vertices;
    uint32_t vertex_count;
    uint64_t rebuild_count;
    uint64_t seen_serial;
} AfVkStrokeCache;

typedef struct AfVkImageAtlasResource {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
} AfVkImageAtlasResource;

struct AfVkRenderer {
    AfVkFrameMetrics frame_metrics;
    VkQueryPool timestamp_pool;
    float timestamp_period;
    uint32_t timestamp_valid_bits;
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
    VkDeviceSize paint_upload_offset;
    VkDescriptorSetLayout paint_descriptor_layout;
    VkDescriptorPool paint_descriptor_pool;
    VkDescriptorSet paint_descriptor_sets[AF_FRAME_COUNT];
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkPipelineLayout path_pipeline_layout;
    VkPipeline path_pipeline;
    VkDescriptorSetLayout image_descriptor_layout;
    VkDescriptorPool image_descriptor_pool;
    VkDescriptorSet image_descriptor_sets[AF_MAX_IMAGE_ATLASES];
    VkSampler image_sampler;
    VkPipelineLayout image_pipeline_layout;
    VkPipeline image_pipeline;
    AfVkImageAtlasResource image_atlases[AF_MAX_IMAGE_ATLASES];
    const AfImageData *image_source;
    uint64_t image_generation;
    float image_time;
    AfVkParticleSystem *particles;
    AfVkPathCache path_caches[AF_MAX_PATHS];
    AfVkStrokeCache stroke_caches[AF_MAX_PATHS];
    AfPathTessellationScratch *path_scratch;
    AfVec2 *path_staging_vertices;
    uint32_t cached_path_vertex_count;
    uint32_t cached_stroke_vertex_count;
    uint64_t path_cache_serial;
    AfVkRenderItem render_items[AF_MAX_RENDER_ITEMS];
    AfVkRenderItem render_item_scratch[AF_MAX_RENDER_ITEMS];
    bool resize_requested;
    bool validation_enabled;
    bool validation_error;
    bool capture_supported;
    bool capture_requested;
    char capture_path[AF_CAPTURE_PATH_CAPACITY];
};

static void af_vk_destroy_image_pipeline(AfVkRenderer *renderer);
static AfResult af_vk_create_image_pipeline(AfVkRenderer *renderer);

static bool af_vk_format_is_srgb(VkFormat format) {
    return format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB;
}

static float af_vk_srgb_to_linear_component(float component) {
    if (component <= 0.04045F) {
        return component / 12.92F;
    }
    return powf((component + 0.055F) / 1.055F, 2.4F);
}

// clang-format off
#include "renderer/vk_renderer_paints.inc"
#include "renderer/vk_renderer_device.inc"
#include "renderer/vk_renderer_swapchain.inc"
#include "renderer/vk_renderer_images.inc"
#include "renderer/vk_renderer_lifecycle.inc"
#include "renderer/vk_renderer_upload.inc"
#include "renderer/vk_renderer_draw.inc"
// clang-format on

AfVkFrameMetrics af_vk_renderer_frame_metrics(const AfVkRenderer *renderer) {
    return renderer == NULL ? (AfVkFrameMetrics){0} : renderer->frame_metrics;
}
