#ifndef AF_VK_PARTICLES_H
#define AF_VK_PARTICLES_H

#include "arcflow/arcflow_result.h"
#include "scene/scene.h"

#include <vulkan/vulkan.h>

#include <stdint.h>

typedef struct AfVkParticleSystem AfVkParticleSystem;

typedef struct AfVkVfxMetrics {
    float cpu_milliseconds;
    float gpu_milliseconds;
    float lod_factor;
    uint32_t simulated_steps;
    uint32_t effective_capacity;
    bool gpu_timing_supported;
} AfVkVfxMetrics;

AfResult af_vk_particles_create(VkPhysicalDevice physical_device, VkDevice device,
                                VkFormat color_format, VkSampleCountFlagBits sample_count,
                                uint32_t graphics_queue_family, AfVkParticleSystem **out_system);
void af_vk_particles_destroy(AfVkParticleSystem *system);
AfResult af_vk_particles_set_render_target(AfVkParticleSystem *system, VkFormat color_format,
                                           VkSampleCountFlagBits sample_count);
AfResult af_vk_particles_update(AfVkParticleSystem *system, const AfScene *scene,
                                uint32_t frame_index);
void af_vk_particles_record(AfVkParticleSystem *system, VkCommandBuffer command_buffer,
                            uint32_t frame_index, float delta_time);
void af_vk_particles_draw(AfVkParticleSystem *system, VkCommandBuffer command_buffer,
                          uint32_t frame_index, VkExtent2D extent);
uint64_t af_vk_particles_memory_size(void);
AfVkVfxMetrics af_vk_particles_metrics(const AfVkParticleSystem *system);

#endif
