#ifndef AF_VK_PARTICLES_H
#define AF_VK_PARTICLES_H

#include "arcflow/arcflow_result.h"
#include "scene/scene.h"

#include <vulkan/vulkan.h>

#include <stdint.h>

typedef struct AfVkParticleSystem AfVkParticleSystem;

AfResult af_vk_particles_create(VkPhysicalDevice physical_device, VkDevice device,
                                VkFormat color_format, VkSampleCountFlagBits sample_count,
                                AfVkParticleSystem **out_system);
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

#endif
