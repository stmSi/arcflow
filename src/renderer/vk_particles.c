#include "renderer/vk_particles.h"

#include "core/log.h"
#include "particle/particle.h"

#include <volk.h>

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define AF_PARTICLE_FRAME_COUNT 2U
#define AF_PARTICLE_BUFFER_COUNT 8U

typedef struct AfVkParticleBuffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    void *mapping;
    VkDeviceSize size;
} AfVkParticleBuffer;

typedef struct AfGpuParticle {
    float position_age[4];
    float velocity_rotation[4];
    float sizes[4];
    float color[4];
    uint32_t meta[4];
    float previous_depth[4];
} AfGpuParticle;

typedef struct AfGpuParticleEmitter {
    uint32_t meta0[4];
    uint32_t meta1[4];
    float transform0[4];
    float transform1[4];
    float position_range[4];
    float velocity_range[4];
    float lifetime_spawn[4];
    float rotation_range[4];
    float size_range[4];
    float color_start[4];
    float color_end[4];
    float forces[4];
    float flipbook[4];
    uint32_t settings[4];
    float advanced0[4];
    float force_center[4];
    float attractor[4];
    float repulsor[4];
    float vector_field[4];
    float collision_plane[4];
    float collision_extra[4];
    float depth_velocity[4];
    float render0[4];
    float render1[4];
    uint32_t sub_settings[4];
    uint32_t event_settings[4];
} AfGpuParticleEmitter;

typedef struct AfGpuParticleEmitterState {
    float values[4];
    uint32_t meta[4];
    uint32_t event_meta[4];
} AfGpuParticleEmitterState;

typedef struct AfGpuParticleStep {
    uint32_t pass_index;
    uint32_t parity;
    uint32_t capacity;
    uint32_t emitter_count;
    float delta_time;
    uint32_t step_serial;
    uint32_t reserved[2];
} AfGpuParticleStep;

typedef struct AfGpuParticleView {
    float viewport_size[2];
    uint32_t parity;
    uint32_t reserved;
} AfGpuParticleView;

struct AfVkParticleSystem {
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkFormat color_format;
    VkSampleCountFlagBits sample_count;
    AfVkParticleBuffer particles;
    AfVkParticleBuffer alive[2];
    AfVkParticleBuffer dead;
    AfVkParticleBuffer counters;
    AfVkParticleBuffer emitters[AF_PARTICLE_FRAME_COUNT];
    AfVkParticleBuffer emitter_states;
    AfVkParticleBuffer path_points[AF_PARTICLE_FRAME_COUNT];
    VkDescriptorSetLayout descriptor_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_sets[AF_PARTICLE_FRAME_COUNT];
    VkPipelineLayout pipeline_layout;
    VkPipeline compute_pipeline;
    VkPipeline graphics_pipeline;
    AfPathTessellationScratch *path_scratch;
    AfVec2 *cached_path_points;
    AfGpuParticleEmitter *cached_emitters;
    uint32_t emitter_count;
    uint32_t path_point_count;
    uint64_t definition_hash;
    float accumulator;
    uint32_t parity;
    uint32_t step_serial;
    bool needs_initialize;
    bool renderer_enabled;
    uint32_t effective_capacity;
    float simulation_step;
    float lod_factor;
    uint32_t simulated_steps;
    VkQueryPool timestamp_queries;
    float timestamp_period;
    uint32_t timestamp_valid_bits;
    bool query_written[AF_PARTICLE_FRAME_COUNT];
    bool query_active[AF_PARTICLE_FRAME_COUNT];
    AfVkVfxMetrics metrics;
};

_Static_assert(sizeof(AfGpuParticle) == 96U, "GPU particle layout mismatch");
_Static_assert(sizeof(AfGpuParticleEmitter) == 416U, "GPU emitter layout mismatch");
_Static_assert(sizeof(AfGpuParticleEmitterState) == 48U, "GPU emitter state layout mismatch");
_Static_assert(sizeof(AfGpuParticleStep) == 32U, "GPU step push constant mismatch");
_Static_assert(sizeof(AfGpuParticleView) == 16U, "GPU view push constant mismatch");

static double af_vk_particle_now_milliseconds(void) {
    struct timespec value;
    if (timespec_get(&value, TIME_UTC) != TIME_UTC) {
        return 0.0;
    }
    return (double)value.tv_sec * 1000.0 + (double)value.tv_nsec * 0.000001;
}

static void af_vk_particles_collect_gpu_metrics(AfVkParticleSystem *system, uint32_t frame_index) {
    uint64_t timestamps[2] = {0U, 0U};
    if (system->timestamp_queries == VK_NULL_HANDLE || !system->query_written[frame_index]) {
        return;
    }
    if (vkGetQueryPoolResults(system->device, system->timestamp_queries, frame_index * 2U, 2U,
                              sizeof(timestamps), timestamps, sizeof(uint64_t),
                              VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
        uint64_t delta = timestamps[1] - timestamps[0];
        if (system->timestamp_valid_bits < 64U) {
            const uint64_t mask = (UINT64_C(1) << system->timestamp_valid_bits) - 1U;
            delta &= mask;
        }
        system->metrics.gpu_milliseconds =
            (float)((double)delta * (double)system->timestamp_period * 0.000001);
    }
    system->query_written[frame_index] = false;
}

static uint32_t af_vk_particle_memory_type(AfVkParticleSystem *system, uint32_t type_bits,
                                           VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(system->physical_device, &memory_properties);
    for (uint32_t index = 0U; index < memory_properties.memoryTypeCount; ++index) {
        if ((type_bits & (1U << index)) != 0U &&
            (memory_properties.memoryTypes[index].propertyFlags & properties) == properties) {
            return index;
        }
    }
    return UINT32_MAX;
}

static AfResult af_vk_particle_buffer_create(AfVkParticleSystem *system, VkDeviceSize size,
                                             VkBufferUsageFlags usage,
                                             VkMemoryPropertyFlags properties,
                                             AfVkParticleBuffer *out_buffer) {
    const VkBufferCreateInfo buffer_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                            .size = size,
                                            .usage = usage,
                                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo allocation = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    uint32_t memory_type = UINT32_MAX;
    if (vkCreateBuffer(system->device, &buffer_info, NULL, &out_buffer->buffer) != VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    vkGetBufferMemoryRequirements(system->device, out_buffer->buffer, &requirements);
    memory_type = af_vk_particle_memory_type(system, requirements.memoryTypeBits, properties);
    if (memory_type == UINT32_MAX) {
        return AF_RESULT_UNSUPPORTED_FEATURE;
    }
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memory_type;
    if (vkAllocateMemory(system->device, &allocation, NULL, &out_buffer->memory) != VK_SUCCESS ||
        vkBindBufferMemory(system->device, out_buffer->buffer, out_buffer->memory, 0U) !=
            VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    if ((properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0U &&
        vkMapMemory(system->device, out_buffer->memory, 0U, size, 0U, &out_buffer->mapping) !=
            VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    out_buffer->size = size;
    return AF_RESULT_SUCCESS;
}

static void af_vk_particle_buffer_destroy(AfVkParticleSystem *system, AfVkParticleBuffer *buffer) {
    if (buffer->mapping != NULL) {
        vkUnmapMemory(system->device, buffer->memory);
    }
    if (buffer->buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(system->device, buffer->buffer, NULL);
    }
    if (buffer->memory != VK_NULL_HANDLE) {
        vkFreeMemory(system->device, buffer->memory, NULL);
    }
    *buffer = (AfVkParticleBuffer){0};
}

static AfResult af_vk_particle_shader(AfVkParticleSystem *system, const char *path,
                                      VkShaderModule *out_shader) {
    uint32_t code[65536];
    FILE *file = fopen(path, "rb");
    long length = 0L;
    VkShaderModuleCreateInfo create_info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    if (file == NULL) {
        return AF_RESULT_IO_ERROR;
    }
    if (fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) <= 0L ||
        length > (long)sizeof(code) || (length & 3L) != 0L || fseek(file, 0L, SEEK_SET) != 0 ||
        fread(code, 1U, (size_t)length, file) != (size_t)length) {
        (void)fclose(file);
        return AF_RESULT_IO_ERROR;
    }
    if (fclose(file) != 0) {
        return AF_RESULT_IO_ERROR;
    }
    create_info.codeSize = (size_t)length;
    create_info.pCode = code;
    return vkCreateShaderModule(system->device, &create_info, NULL, out_shader) == VK_SUCCESS
               ? AF_RESULT_SUCCESS
               : AF_RESULT_VULKAN_ERROR;
}

static void af_vk_particles_destroy_graphics_pipeline(AfVkParticleSystem *system) {
    if (system->graphics_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(system->device, system->graphics_pipeline, NULL);
        system->graphics_pipeline = VK_NULL_HANDLE;
    }
}

static AfResult af_vk_particles_create_graphics_pipeline(AfVkParticleSystem *system) {
    VkShaderModule vertex_shader = VK_NULL_HANDLE;
    VkShaderModule fragment_shader = VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo stages[2];
    const VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
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
        .rasterizationSamples = system->sample_count,
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
    const VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamic = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2U,
        .pDynamicStates = dynamic_states};
    const VkPipelineRenderingCreateInfo rendering = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1U,
        .pColorAttachmentFormats = &system->color_format};
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering,
        .stageCount = 2U,
        .pStages = stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &blending,
        .pDynamicState = &dynamic,
        .layout = system->pipeline_layout};
    AfResult result =
        af_vk_particle_shader(system, ARCFLOW_SHADER_DIR "/particle.vert.spv", &vertex_shader);
    if (result == AF_RESULT_SUCCESS) {
        result = af_vk_particle_shader(system, ARCFLOW_SHADER_DIR "/particle.frag.spv",
                                       &fragment_shader);
    }
    if (result == AF_RESULT_SUCCESS) {
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
        if (vkCreateGraphicsPipelines(system->device, VK_NULL_HANDLE, 1U, &pipeline_info, NULL,
                                      &system->graphics_pipeline) != VK_SUCCESS) {
            result = AF_RESULT_VULKAN_ERROR;
        }
    }
    if (fragment_shader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(system->device, fragment_shader, NULL);
    }
    if (vertex_shader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(system->device, vertex_shader, NULL);
    }
    return result;
}

static AfResult af_vk_particles_create_layout(AfVkParticleSystem *system) {
    VkDescriptorSetLayoutBinding bindings[AF_PARTICLE_BUFFER_COUNT];
    const VkPushConstantRange push_range = {.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT |
                                                          VK_SHADER_STAGE_VERTEX_BIT,
                                            .offset = 0U,
                                            .size = sizeof(AfGpuParticleStep)};
    const VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1U,
        .pSetLayouts = &system->descriptor_layout,
        .pushConstantRangeCount = 1U,
        .pPushConstantRanges = &push_range};
    for (uint32_t index = 0U; index < AF_PARTICLE_BUFFER_COUNT; ++index) {
        bindings[index] = (VkDescriptorSetLayoutBinding){
            .binding = index,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1U,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT |
                          ((index == 0U || index == 1U || index == 2U || index == 5U)
                               ? VK_SHADER_STAGE_VERTEX_BIT
                               : 0U)};
    }
    {
        const VkDescriptorSetLayoutCreateInfo layout_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = AF_PARTICLE_BUFFER_COUNT,
            .pBindings = bindings};
        if (vkCreateDescriptorSetLayout(system->device, &layout_info, NULL,
                                        &system->descriptor_layout) != VK_SUCCESS) {
            return AF_RESULT_VULKAN_ERROR;
        }
    }
    return vkCreatePipelineLayout(system->device, &pipeline_layout_info, NULL,
                                  &system->pipeline_layout) == VK_SUCCESS
               ? AF_RESULT_SUCCESS
               : AF_RESULT_VULKAN_ERROR;
}

static AfResult af_vk_particles_create_compute_pipeline(AfVkParticleSystem *system) {
    VkShaderModule shader = VK_NULL_HANDLE;
    AfResult result =
        af_vk_particle_shader(system, ARCFLOW_SHADER_DIR "/particle.comp.spv", &shader);
    if (result == AF_RESULT_SUCCESS) {
        const VkComputePipelineCreateInfo pipeline_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                      .module = shader,
                      .pName = "main"},
            .layout = system->pipeline_layout};
        if (vkCreateComputePipelines(system->device, VK_NULL_HANDLE, 1U, &pipeline_info, NULL,
                                     &system->compute_pipeline) != VK_SUCCESS) {
            result = AF_RESULT_VULKAN_ERROR;
        }
    }
    if (shader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(system->device, shader, NULL);
    }
    return result;
}

static AfResult af_vk_particles_create_buffers(AfVkParticleSystem *system) {
    const VkMemoryPropertyFlags gpu_memory = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    const VkMemoryPropertyFlags upload_memory =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    const VkBufferUsageFlags storage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    AfResult result =
        af_vk_particle_buffer_create(system, (VkDeviceSize)AF_MAX_PARTICLES * sizeof(AfGpuParticle),
                                     storage, gpu_memory, &system->particles);
    for (uint32_t index = 0U; result == AF_RESULT_SUCCESS && index < 2U; ++index) {
        result =
            af_vk_particle_buffer_create(system, (VkDeviceSize)AF_MAX_PARTICLES * sizeof(uint32_t),
                                         storage, gpu_memory, &system->alive[index]);
    }
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_vk_particle_buffer_create(system, (VkDeviceSize)AF_MAX_PARTICLES * sizeof(uint32_t),
                                         storage, gpu_memory, &system->dead);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_vk_particle_buffer_create(system, 8U * sizeof(uint32_t),
                                              storage | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                              gpu_memory, &system->counters);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_vk_particle_buffer_create(
            system, (VkDeviceSize)AF_MAX_PARTICLE_EMITTERS * sizeof(AfGpuParticleEmitterState),
            storage, gpu_memory, &system->emitter_states);
    }
    for (uint32_t index = 0U; result == AF_RESULT_SUCCESS && index < AF_PARTICLE_FRAME_COUNT;
         ++index) {
        result = af_vk_particle_buffer_create(
            system, (VkDeviceSize)AF_MAX_PARTICLE_EMITTERS * sizeof(AfGpuParticleEmitter), storage,
            upload_memory, &system->emitters[index]);
        if (result == AF_RESULT_SUCCESS) {
            result = af_vk_particle_buffer_create(
                system, (VkDeviceSize)AF_MAX_PARTICLE_PATH_POINTS * sizeof(AfVec2), storage,
                upload_memory, &system->path_points[index]);
        }
    }
    return result;
}

static AfResult af_vk_particles_create_descriptors(AfVkParticleSystem *system) {
    const VkDescriptorPoolSize pool_size = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                            .descriptorCount =
                                                AF_PARTICLE_BUFFER_COUNT * AF_PARTICLE_FRAME_COUNT};
    const VkDescriptorPoolCreateInfo pool_info = {.sType =
                                                      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                  .maxSets = AF_PARTICLE_FRAME_COUNT,
                                                  .poolSizeCount = 1U,
                                                  .pPoolSizes = &pool_size};
    VkDescriptorSetLayout layouts[AF_PARTICLE_FRAME_COUNT] = {system->descriptor_layout,
                                                              system->descriptor_layout};
    VkDescriptorSetAllocateInfo allocation = {.sType =
                                                  VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                              .descriptorPool = VK_NULL_HANDLE,
                                              .descriptorSetCount = AF_PARTICLE_FRAME_COUNT,
                                              .pSetLayouts = layouts};
    if (vkCreateDescriptorPool(system->device, &pool_info, NULL, &system->descriptor_pool) !=
        VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    allocation.descriptorPool = system->descriptor_pool;
    if (vkAllocateDescriptorSets(system->device, &allocation, system->descriptor_sets) !=
        VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    for (uint32_t frame = 0U; frame < AF_PARTICLE_FRAME_COUNT; ++frame) {
        AfVkParticleBuffer *buffers[AF_PARTICLE_BUFFER_COUNT] = {
            &system->particles,      &system->alive[0],          &system->alive[1],
            &system->dead,           &system->counters,          &system->emitters[frame],
            &system->emitter_states, &system->path_points[frame]};
        VkDescriptorBufferInfo buffer_infos[AF_PARTICLE_BUFFER_COUNT];
        VkWriteDescriptorSet writes[AF_PARTICLE_BUFFER_COUNT];
        for (uint32_t index = 0U; index < AF_PARTICLE_BUFFER_COUNT; ++index) {
            buffer_infos[index] = (VkDescriptorBufferInfo){
                .buffer = buffers[index]->buffer, .offset = 0U, .range = buffers[index]->size};
            writes[index] =
                (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                       .dstSet = system->descriptor_sets[frame],
                                       .dstBinding = index,
                                       .descriptorCount = 1U,
                                       .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                       .pBufferInfo = &buffer_infos[index]};
        }
        vkUpdateDescriptorSets(system->device, AF_PARTICLE_BUFFER_COUNT, writes, 0U, NULL);
    }
    return AF_RESULT_SUCCESS;
}

AfResult af_vk_particles_create(VkPhysicalDevice physical_device, VkDevice device,
                                VkFormat color_format, VkSampleCountFlagBits sample_count,
                                uint32_t graphics_queue_family, AfVkParticleSystem **out_system) {
    AfVkParticleSystem *system = NULL;
    AfResult result = AF_RESULT_SUCCESS;
    if (physical_device == VK_NULL_HANDLE || device == VK_NULL_HANDLE || out_system == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    *out_system = NULL;
    system = calloc(1U, sizeof(*system));
    if (system == NULL) {
        return AF_RESULT_OUT_OF_MEMORY;
    }
    system->physical_device = physical_device;
    system->device = device;
    system->color_format = color_format;
    system->sample_count = sample_count;
    {
        VkPhysicalDeviceProperties properties;
        uint32_t family_count = 0U;
        vkGetPhysicalDeviceProperties(physical_device, &properties);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_count, NULL);
        if (graphics_queue_family < family_count) {
            VkQueueFamilyProperties *families = calloc(family_count, sizeof(*families));
            if (families == NULL) {
                free(system);
                return AF_RESULT_OUT_OF_MEMORY;
            }
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_count, families);
            system->timestamp_valid_bits = families[graphics_queue_family].timestampValidBits;
            free(families);
        }
        system->timestamp_period = properties.limits.timestampPeriod;
        if (system->timestamp_valid_bits != 0U) {
            const VkQueryPoolCreateInfo query_info = {.sType =
                                                          VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                                      .queryType = VK_QUERY_TYPE_TIMESTAMP,
                                                      .queryCount = AF_PARTICLE_FRAME_COUNT * 2U};
            if (vkCreateQueryPool(device, &query_info, NULL, &system->timestamp_queries) !=
                VK_SUCCESS) {
                system->timestamp_valid_bits = 0U;
            }
        }
    }
    system->path_scratch = calloc(1U, sizeof(*system->path_scratch));
    system->cached_path_points =
        calloc(AF_MAX_PARTICLE_PATH_POINTS, sizeof(*system->cached_path_points));
    system->cached_emitters = calloc(AF_MAX_PARTICLE_EMITTERS, sizeof(*system->cached_emitters));
    if (system->path_scratch == NULL || system->cached_path_points == NULL ||
        system->cached_emitters == NULL) {
        result = AF_RESULT_OUT_OF_MEMORY;
    } else if ((result = af_vk_particles_create_layout(system)) != AF_RESULT_SUCCESS) {
    } else if ((result = af_vk_particles_create_buffers(system)) != AF_RESULT_SUCCESS) {
    } else if ((result = af_vk_particles_create_descriptors(system)) != AF_RESULT_SUCCESS) {
    } else if ((result = af_vk_particles_create_compute_pipeline(system)) != AF_RESULT_SUCCESS) {
    } else {
        result = af_vk_particles_create_graphics_pipeline(system);
    }
    if (result != AF_RESULT_SUCCESS) {
        af_vk_particles_destroy(system);
        return result;
    }
    system->needs_initialize = true;
    *out_system = system;
    return AF_RESULT_SUCCESS;
}

void af_vk_particles_destroy(AfVkParticleSystem *system) {
    if (system == NULL) {
        return;
    }
    if (system->device != VK_NULL_HANDLE) {
        af_vk_particles_destroy_graphics_pipeline(system);
        if (system->compute_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(system->device, system->compute_pipeline, NULL);
        }
        if (system->pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(system->device, system->pipeline_layout, NULL);
        }
        if (system->descriptor_pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(system->device, system->descriptor_pool, NULL);
        }
        if (system->descriptor_layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(system->device, system->descriptor_layout, NULL);
        }
        if (system->timestamp_queries != VK_NULL_HANDLE) {
            vkDestroyQueryPool(system->device, system->timestamp_queries, NULL);
        }
        af_vk_particle_buffer_destroy(system, &system->particles);
        af_vk_particle_buffer_destroy(system, &system->alive[0]);
        af_vk_particle_buffer_destroy(system, &system->alive[1]);
        af_vk_particle_buffer_destroy(system, &system->dead);
        af_vk_particle_buffer_destroy(system, &system->counters);
        af_vk_particle_buffer_destroy(system, &system->emitter_states);
        for (uint32_t index = 0U; index < AF_PARTICLE_FRAME_COUNT; ++index) {
            af_vk_particle_buffer_destroy(system, &system->emitters[index]);
            af_vk_particle_buffer_destroy(system, &system->path_points[index]);
        }
    }
    free(system->cached_emitters);
    free(system->cached_path_points);
    free(system->path_scratch);
    free(system);
}

AfResult af_vk_particles_set_render_target(AfVkParticleSystem *system, VkFormat color_format,
                                           VkSampleCountFlagBits sample_count) {
    if (system == NULL) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    if (system->color_format == color_format && system->sample_count == sample_count) {
        return AF_RESULT_SUCCESS;
    }
    system->color_format = color_format;
    system->sample_count = sample_count;
    af_vk_particles_destroy_graphics_pipeline(system);
    return af_vk_particles_create_graphics_pipeline(system);
}

static uint64_t af_vk_particle_hash_bytes(uint64_t hash, const void *data, size_t size) {
    const uint8_t *bytes = data;
    for (size_t index = 0U; index < size; ++index) {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t af_vk_particle_definition_hash(const AfScene *scene) {
    uint64_t hash = UINT64_C(1469598103934665603);
    const AfParticleData *data = scene->particles;
    hash = af_vk_particle_hash_bytes(hash, &data->count, sizeof(data->count));
    for (uint32_t index = 0U; index < data->count; ++index) {
        const AfParticleEmitter *emitter = &data->emitters[index];
        AfParticleEmitter stable = *emitter;
        stable.event_burst_count = 0U;
        stable.event_sequence = 0U;
        hash = af_vk_particle_hash_bytes(hash, &stable, sizeof(stable));
        if (emitter->type == AF_PARTICLE_EMITTER_VECTOR_PATH && scene->paths != NULL) {
            const AfPath *path = af_path_find_const(scene->paths, emitter->path_id);
            if (path != NULL) {
                hash = af_vk_particle_hash_bytes(hash, &path->geometry_generation,
                                                 sizeof(path->geometry_generation));
            }
        }
    }
    return hash;
}

static AfResult af_vk_particles_cache_paths(AfVkParticleSystem *system, const AfScene *scene) {
    system->path_point_count = 0U;
    for (uint32_t index = 0U; index < system->emitter_count; ++index) {
        const AfParticleEmitter *emitter = &scene->particles->emitters[index];
        AfGpuParticleEmitter *gpu = &system->cached_emitters[index];
        if (emitter->type == AF_PARTICLE_EMITTER_VECTOR_PATH) {
            const AfPath *path = af_path_find_const(scene->paths, emitter->path_id);
            AfFlattenedPath flattened = {.points = system->path_scratch->points,
                                         .point_capacity = AF_MAX_PATH_FLATTENED_POINTS,
                                         .contours = system->path_scratch->contours,
                                         .contour_capacity = AF_MAX_PATH_CONTOURS};
            AfResult result = path == NULL ? AF_RESULT_INVALID_ARGUMENT
                                           : af_path_flatten(scene->paths, path, 0.5F, &flattened);
            if (result != AF_RESULT_SUCCESS ||
                flattened.point_count > AF_MAX_PARTICLE_PATH_POINTS - system->path_point_count) {
                return result == AF_RESULT_SUCCESS ? AF_RESULT_LIMIT_EXCEEDED : result;
            }
            gpu->meta1[2] = system->path_point_count;
            gpu->meta1[3] = flattened.point_count;
            memcpy(&system->cached_path_points[system->path_point_count], flattened.points,
                   (size_t)flattened.point_count * sizeof(AfVec2));
            system->path_point_count += flattened.point_count;
        }
    }
    return AF_RESULT_SUCCESS;
}

static void af_vk_particles_fill_static_emitter(AfGpuParticleEmitter *gpu,
                                                const AfParticleEmitter *emitter) {
    memset(gpu, 0, sizeof(*gpu));
    gpu->meta0[0] = (uint32_t)emitter->type;
    gpu->meta0[1] = (uint32_t)emitter->output;
    gpu->meta0[2] = emitter->capacity;
    gpu->meta0[3] = emitter->seed;
    gpu->meta1[0] = (uint32_t)emitter->id;
    gpu->meta1[1] = (uint32_t)(emitter->id >> 32U);
    gpu->position_range[0] = emitter->position_min.x;
    gpu->position_range[1] = emitter->position_min.y;
    gpu->position_range[2] = emitter->position_max.x;
    gpu->position_range[3] = emitter->position_max.y;
    gpu->velocity_range[0] = emitter->velocity_min.x;
    gpu->velocity_range[1] = emitter->velocity_min.y;
    gpu->velocity_range[2] = emitter->velocity_max.x;
    gpu->velocity_range[3] = emitter->velocity_max.y;
    gpu->lifetime_spawn[0] = emitter->lifetime_min;
    gpu->lifetime_spawn[1] = emitter->lifetime_max;
    gpu->lifetime_spawn[2] = emitter->spawn_rate;
    gpu->lifetime_spawn[3] = emitter->drag;
    gpu->rotation_range[0] = emitter->rotation_min;
    gpu->rotation_range[1] = emitter->rotation_max;
    gpu->rotation_range[2] = emitter->angular_velocity_min;
    gpu->rotation_range[3] = emitter->angular_velocity_max;
    gpu->size_range[0] = emitter->size_start_min;
    gpu->size_range[1] = emitter->size_start_max;
    gpu->size_range[2] = emitter->size_end_min;
    gpu->size_range[3] = emitter->size_end_max;
    memcpy(gpu->color_start, &emitter->color_start, sizeof(gpu->color_start));
    memcpy(gpu->color_end, &emitter->color_end, sizeof(gpu->color_end));
    gpu->forces[0] = emitter->gravity.x;
    gpu->forces[1] = emitter->gravity.y;
    gpu->forces[2] = emitter->wind.x;
    gpu->forces[3] = emitter->wind.y;
    gpu->flipbook[0] = emitter->flipbook_frames_per_second;
    gpu->settings[0] = emitter->burst_count;
    gpu->settings[1] = emitter->enabled ? 1U : 0U;
    gpu->settings[2] = emitter->flipbook_columns;
    gpu->settings[3] = emitter->flipbook_rows;
    gpu->advanced0[0] = emitter->turbulence_strength;
    gpu->advanced0[1] = emitter->turbulence_frequency;
    gpu->advanced0[2] = emitter->radial_force;
    gpu->advanced0[3] = emitter->vortex_force;
    gpu->force_center[0] = emitter->force_center.x;
    gpu->force_center[1] = emitter->force_center.y;
    gpu->attractor[0] = emitter->attractor_position.x;
    gpu->attractor[1] = emitter->attractor_position.y;
    gpu->attractor[2] = emitter->attractor_strength;
    gpu->attractor[3] = emitter->attractor_radius;
    gpu->repulsor[0] = emitter->repulsor_position.x;
    gpu->repulsor[1] = emitter->repulsor_position.y;
    gpu->repulsor[2] = emitter->repulsor_strength;
    gpu->repulsor[3] = emitter->repulsor_radius;
    gpu->vector_field[0] = emitter->vector_field_direction.x;
    gpu->vector_field[1] = emitter->vector_field_direction.y;
    gpu->vector_field[2] = emitter->vector_field_strength;
    gpu->collision_plane[0] = emitter->collision_plane_normal.x;
    gpu->collision_plane[1] = emitter->collision_plane_normal.y;
    gpu->collision_plane[2] = emitter->collision_plane_offset;
    gpu->collision_plane[3] = emitter->collision_restitution;
    gpu->collision_extra[0] = emitter->collision_friction;
    gpu->collision_extra[1] = emitter->depth_min;
    gpu->collision_extra[2] = emitter->depth_max;
    gpu->depth_velocity[0] = emitter->depth_velocity_min;
    gpu->depth_velocity[1] = emitter->depth_velocity_max;
    gpu->render0[0] = emitter->trail_length;
    gpu->render0[1] = emitter->trail_width;
    gpu->render0[2] = emitter->mesh_scale;
    gpu->render0[3] = emitter->decal_rotation_offset;
    gpu->render1[0] = emitter->distortion_strength;
    gpu->render1[1] = emitter->soft_fade_distance;
    gpu->render1[2] = emitter->light_radius;
    gpu->sub_settings[0] = UINT32_MAX;
    gpu->sub_settings[1] = emitter->sub_emit_birth_count;
    gpu->sub_settings[2] = emitter->sub_emit_death_count;
    gpu->sub_settings[3] = emitter->max_recursion_depth;
    gpu->event_settings[0] = emitter->event_burst_count;
    gpu->event_settings[1] = emitter->event_sequence;
    gpu->event_settings[2] = emitter->collision_plane_enabled ? 1U : 0U;
    gpu->event_settings[3] = emitter->depth_collision_enabled ? 1U : 0U;
}

AfResult af_vk_particles_update(AfVkParticleSystem *system, const AfScene *scene,
                                uint32_t frame_index) {
    const AfParticleData empty = {0};
    AfScene validation_scene;
    uint64_t hash = 0U;
    uint32_t effective_capacity = 0U;
    float lod_factor = 1.0F;
    const double cpu_start = af_vk_particle_now_milliseconds();
    if (system == NULL || scene == NULL || frame_index >= AF_PARTICLE_FRAME_COUNT) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    af_vk_particles_collect_gpu_metrics(system, frame_index);
    validation_scene = *scene;
    validation_scene.particles = scene->particles == NULL ? &empty : scene->particles;
    if (af_particle_data_validate(validation_scene.particles, &validation_scene) !=
        AF_RESULT_SUCCESS) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    lod_factor = af_vfx_lod_factor(&validation_scene.particles->lod);
    effective_capacity = validation_scene.particles->lod.maximum_particles == 0U
                             ? AF_MAX_PARTICLES
                             : validation_scene.particles->lod.maximum_particles;
    effective_capacity = (uint32_t)floorf((float)effective_capacity * lod_factor);
    if (effective_capacity > AF_MAX_PARTICLES) {
        effective_capacity = AF_MAX_PARTICLES;
    }
    if (effective_capacity != system->effective_capacity) {
        system->effective_capacity = effective_capacity;
        system->needs_initialize = true;
        system->accumulator = 0.0F;
        system->parity = 0U;
    }
    system->lod_factor = lod_factor;
    system->renderer_enabled =
        validation_scene.particles->count != 0U && effective_capacity != 0U && lod_factor > 0.0F;
    {
        const float authored_frequency =
            validation_scene.particles->lod.maximum_particles == 0U
                ? 1.0F
                : validation_scene.particles->lod.simulation_frequency_scale;
        const float effective_frequency = fmaxf(authored_frequency * lod_factor, 0.1F);
        system->simulation_step = AF_PARTICLE_FIXED_STEP / effective_frequency;
    }
    hash = af_vk_particle_definition_hash(&validation_scene);
    if (hash != system->definition_hash) {
        system->definition_hash = hash;
        system->emitter_count = validation_scene.particles->count;
        memset(system->cached_emitters, 0,
               AF_MAX_PARTICLE_EMITTERS * sizeof(*system->cached_emitters));
        for (uint32_t index = 0U; index < system->emitter_count; ++index) {
            af_vk_particles_fill_static_emitter(&system->cached_emitters[index],
                                                &validation_scene.particles->emitters[index]);
        }
        for (uint32_t index = 0U; index < system->emitter_count; ++index) {
            const uint64_t sub_id = validation_scene.particles->emitters[index].sub_emitter_id;
            if (sub_id != 0U) {
                for (uint32_t target = 0U; target < system->emitter_count; ++target) {
                    if (validation_scene.particles->emitters[target].id == sub_id) {
                        system->cached_emitters[index].sub_settings[0] = target;
                        break;
                    }
                }
            }
        }
        {
            const AfResult result = af_vk_particles_cache_paths(system, &validation_scene);
            if (result != AF_RESULT_SUCCESS) {
                return result;
            }
        }
        system->needs_initialize = true;
        system->accumulator = 0.0F;
        system->parity = 0U;
        system->step_serial = 0U;
    }
    for (uint32_t index = 0U; index < system->emitter_count; ++index) {
        const AfParticleEmitter *emitter = &validation_scene.particles->emitters[index];
        const AfSceneNode *node = af_scene_find_const(&validation_scene, emitter->node_id);
        AfGpuParticleEmitter *gpu = &system->cached_emitters[index];
        const float capacity_scale = fmaxf(system->lod_factor, 0.0F);
        const AfVec2 local_normal = emitter->collision_plane_normal;
        const AfVec2 local_tangent = {-local_normal.y, local_normal.x};
        const AfVec2 world_tangent = {node->world_transform.m00 * local_tangent.x +
                                          node->world_transform.m01 * local_tangent.y,
                                      node->world_transform.m10 * local_tangent.x +
                                          node->world_transform.m11 * local_tangent.y};
        const float tangent_length =
            sqrtf(world_tangent.x * world_tangent.x + world_tangent.y * world_tangent.y);
        AfVec2 world_normal =
            tangent_length > 0.000001F
                ? (AfVec2){-world_tangent.y / tangent_length, world_tangent.x / tangent_length}
                : (AfVec2){0.0F, -1.0F};
        const AfVec2 plane_point_local = {local_normal.x * emitter->collision_plane_offset,
                                          local_normal.y * emitter->collision_plane_offset};
        const AfVec2 plane_point_world = {
            node->world_transform.m00 * plane_point_local.x +
                node->world_transform.m01 * plane_point_local.y + node->world_transform.tx,
            node->world_transform.m10 * plane_point_local.x +
                node->world_transform.m11 * plane_point_local.y + node->world_transform.ty};
        gpu->transform0[0] = node->world_transform.m00;
        gpu->transform0[1] = node->world_transform.m01;
        gpu->transform0[2] = node->world_transform.m10;
        gpu->transform0[3] = node->world_transform.m11;
        gpu->transform1[0] = node->world_transform.tx;
        gpu->transform1[1] = node->world_transform.ty;
        gpu->transform1[2] = node->resolved_bounds.x;
        gpu->transform1[3] = node->resolved_bounds.y;
        gpu->meta0[2] = (uint32_t)floorf((float)emitter->capacity * capacity_scale);
        if (gpu->meta0[2] > system->effective_capacity) {
            gpu->meta0[2] = system->effective_capacity;
        }
        gpu->lifetime_spawn[2] = emitter->spawn_rate *
                                 (validation_scene.particles->lod.maximum_particles == 0U
                                      ? 1.0F
                                      : validation_scene.particles->lod.spawn_rate_scale) *
                                 system->lod_factor;
        gpu->force_center[0] = node->world_transform.m00 * emitter->force_center.x +
                               node->world_transform.m01 * emitter->force_center.y +
                               node->world_transform.tx;
        gpu->force_center[1] = node->world_transform.m10 * emitter->force_center.x +
                               node->world_transform.m11 * emitter->force_center.y +
                               node->world_transform.ty;
        gpu->attractor[0] = node->world_transform.m00 * emitter->attractor_position.x +
                            node->world_transform.m01 * emitter->attractor_position.y +
                            node->world_transform.tx;
        gpu->attractor[1] = node->world_transform.m10 * emitter->attractor_position.x +
                            node->world_transform.m11 * emitter->attractor_position.y +
                            node->world_transform.ty;
        gpu->repulsor[0] = node->world_transform.m00 * emitter->repulsor_position.x +
                           node->world_transform.m01 * emitter->repulsor_position.y +
                           node->world_transform.tx;
        gpu->repulsor[1] = node->world_transform.m10 * emitter->repulsor_position.x +
                           node->world_transform.m11 * emitter->repulsor_position.y +
                           node->world_transform.ty;
        gpu->vector_field[0] = node->world_transform.m00 * emitter->vector_field_direction.x +
                               node->world_transform.m01 * emitter->vector_field_direction.y;
        gpu->vector_field[1] = node->world_transform.m10 * emitter->vector_field_direction.x +
                               node->world_transform.m11 * emitter->vector_field_direction.y;
        gpu->collision_plane[0] = world_normal.x;
        gpu->collision_plane[1] = world_normal.y;
        gpu->collision_plane[2] =
            world_normal.x * plane_point_world.x + world_normal.y * plane_point_world.y;
        gpu->event_settings[0] = emitter->event_burst_count;
        gpu->event_settings[1] = emitter->event_sequence;
    }
    memcpy(system->emitters[frame_index].mapping, system->cached_emitters,
           (size_t)system->emitter_count * sizeof(*system->cached_emitters));
    memcpy(system->path_points[frame_index].mapping, system->cached_path_points,
           (size_t)system->path_point_count * sizeof(*system->cached_path_points));
    system->metrics.cpu_milliseconds = (float)(af_vk_particle_now_milliseconds() - cpu_start);
    system->metrics.lod_factor = system->lod_factor;
    system->metrics.effective_capacity = system->effective_capacity;
    system->metrics.gpu_timing_supported = system->timestamp_queries != VK_NULL_HANDLE;
    return AF_RESULT_SUCCESS;
}

static void af_vk_particles_compute_barrier(VkCommandBuffer command_buffer) {
    const VkMemoryBarrier2 barrier = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                      .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                      .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                      .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                      .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                                                       VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
    const VkDependencyInfo dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                         .memoryBarrierCount = 1U,
                                         .pMemoryBarriers = &barrier};
    vkCmdPipelineBarrier2(command_buffer, &dependency);
}

static void af_vk_particles_dispatch(AfVkParticleSystem *system, VkCommandBuffer command_buffer,
                                     AfGpuParticleStep *step, uint32_t groups) {
    vkCmdPushConstants(command_buffer, system->pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0U, sizeof(*step),
                       step);
    vkCmdDispatch(command_buffer, groups, 1U, 1U);
}

void af_vk_particles_record(AfVkParticleSystem *system, VkCommandBuffer command_buffer,
                            uint32_t frame_index, float delta_time) {
    AfGpuParticleStep step = {0};
    uint32_t step_count = 0U;
    const double cpu_start = af_vk_particle_now_milliseconds();
    if (system == NULL || command_buffer == VK_NULL_HANDLE ||
        frame_index >= AF_PARTICLE_FRAME_COUNT || !isfinite(delta_time) || delta_time < 0.0F) {
        return;
    }
    system->simulated_steps = 0U;
    if (!system->renderer_enabled || system->effective_capacity == 0U) {
        system->metrics.gpu_milliseconds = 0.0F;
        system->metrics.simulated_steps = 0U;
        return;
    }
    if (system->timestamp_queries != VK_NULL_HANDLE) {
        vkCmdResetQueryPool(command_buffer, system->timestamp_queries, frame_index * 2U, 2U);
        vkCmdWriteTimestamp2(command_buffer, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                             system->timestamp_queries, frame_index * 2U);
        system->query_active[frame_index] = true;
    }
    step.capacity = system->effective_capacity;
    step.emitter_count = system->emitter_count;
    step.delta_time = system->simulation_step;
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, system->compute_pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, system->pipeline_layout,
                            0U, 1U, &system->descriptor_sets[frame_index], 0U, NULL);
    if (system->needs_initialize) {
        step.pass_index = 0U;
        {
            const uint32_t initialize_count =
                step.capacity > step.emitter_count ? step.capacity : step.emitter_count;
            af_vk_particles_dispatch(system, command_buffer, &step, (initialize_count + 63U) / 64U);
        }
        af_vk_particles_compute_barrier(command_buffer);
        system->needs_initialize = false;
    }
    system->accumulator += fminf(delta_time, 0.1F);
    step_count = (uint32_t)(system->accumulator / system->simulation_step);
    if (step_count > AF_PARTICLE_MAX_STEPS_PER_FRAME) {
        step_count = AF_PARTICLE_MAX_STEPS_PER_FRAME;
    }
    system->accumulator -= (float)step_count * system->simulation_step;
    if (system->accumulator >= system->simulation_step) {
        system->accumulator = fmodf(system->accumulator, system->simulation_step);
    }
    for (uint32_t index = 0U; index < step_count; ++index) {
        step.parity = system->parity;
        step.step_serial = system->step_serial++;
        step.pass_index = 1U;
        af_vk_particles_dispatch(system, command_buffer, &step, 1U);
        af_vk_particles_compute_barrier(command_buffer);
        step.pass_index = 2U;
        af_vk_particles_dispatch(system, command_buffer, &step,
                                 (system->effective_capacity + 63U) / 64U);
        af_vk_particles_compute_barrier(command_buffer);
        if (system->emitter_count != 0U) {
            step.pass_index = 3U;
            af_vk_particles_dispatch(system, command_buffer, &step,
                                     (system->emitter_count + 63U) / 64U);
            af_vk_particles_compute_barrier(command_buffer);
        }
        step.pass_index = 4U;
        af_vk_particles_dispatch(system, command_buffer, &step, 1U);
        af_vk_particles_compute_barrier(command_buffer);
        system->parity ^= 1U;
    }
    system->simulated_steps = step_count;
    system->metrics.simulated_steps = step_count;
    system->metrics.cpu_milliseconds += (float)(af_vk_particle_now_milliseconds() - cpu_start);
    {
        const VkMemoryBarrier2 barrier = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                                          .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                          .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                                          .dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT |
                                                          VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                                          .dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT |
                                                           VK_ACCESS_2_SHADER_STORAGE_READ_BIT};
        const VkDependencyInfo dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                             .memoryBarrierCount = 1U,
                                             .pMemoryBarriers = &barrier};
        vkCmdPipelineBarrier2(command_buffer, &dependency);
    }
    if (system->query_active[frame_index]) {
        vkCmdWriteTimestamp2(command_buffer, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                             system->timestamp_queries, frame_index * 2U + 1U);
        system->query_active[frame_index] = false;
        system->query_written[frame_index] = true;
    }
}

void af_vk_particles_draw(AfVkParticleSystem *system, VkCommandBuffer command_buffer,
                          uint32_t frame_index, VkExtent2D extent) {
    AfGpuParticleView view = {0};
    if (system == NULL || command_buffer == VK_NULL_HANDLE ||
        frame_index >= AF_PARTICLE_FRAME_COUNT || !system->renderer_enabled) {
        return;
    }
    view.viewport_size[0] = (float)extent.width;
    view.viewport_size[1] = (float)extent.height;
    view.parity = system->parity;
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, system->graphics_pipeline);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            system->pipeline_layout, 0U, 1U, &system->descriptor_sets[frame_index],
                            0U, NULL);
    vkCmdPushConstants(command_buffer, system->pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT, 0U, sizeof(view),
                       &view);
    vkCmdDrawIndirect(command_buffer, system->counters.buffer, 4U * sizeof(uint32_t), 1U,
                      sizeof(uint32_t) * 4U);
}

uint64_t af_vk_particles_memory_size(void) {
    return (uint64_t)AF_MAX_PARTICLES * (sizeof(AfGpuParticle) + 3U * sizeof(uint32_t)) +
           8U * sizeof(uint32_t) +
           (uint64_t)AF_MAX_PARTICLE_EMITTERS * sizeof(AfGpuParticleEmitterState) +
           AF_PARTICLE_FRAME_COUNT *
               ((uint64_t)AF_MAX_PARTICLE_EMITTERS * sizeof(AfGpuParticleEmitter) +
                (uint64_t)AF_MAX_PARTICLE_PATH_POINTS * sizeof(AfVec2));
}

AfVkVfxMetrics af_vk_particles_metrics(const AfVkParticleSystem *system) {
    return system == NULL ? (AfVkVfxMetrics){0} : system->metrics;
}
