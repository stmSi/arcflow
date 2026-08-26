static void af_vk_destroy_image_atlases(AfVkRenderer *renderer) {
    for (uint32_t index = 0U; index < AF_MAX_IMAGE_ATLASES; ++index) {
        AfVkImageAtlasResource *atlas = &renderer->image_atlases[index];
        if (atlas->view != VK_NULL_HANDLE) {
            vkDestroyImageView(renderer->device, atlas->view, NULL);
        }
        if (atlas->image != VK_NULL_HANDLE) {
            vkDestroyImage(renderer->device, atlas->image, NULL);
        }
        if (atlas->memory != VK_NULL_HANDLE) {
            vkFreeMemory(renderer->device, atlas->memory, NULL);
        }
        *atlas = (AfVkImageAtlasResource){0};
    }
    renderer->image_source = NULL;
    renderer->image_generation = 0U;
}

static void af_vk_destroy_image_pipeline(AfVkRenderer *renderer) {
    if (renderer->image_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(renderer->device, renderer->image_pipeline, NULL);
        renderer->image_pipeline = VK_NULL_HANDLE;
    }
    if (renderer->image_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(renderer->device, renderer->image_pipeline_layout, NULL);
        renderer->image_pipeline_layout = VK_NULL_HANDLE;
    }
}

static void af_vk_destroy_images(AfVkRenderer *renderer) {
    af_vk_destroy_image_atlases(renderer);
    if (renderer->image_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(renderer->device, renderer->image_descriptor_pool, NULL);
        renderer->image_descriptor_pool = VK_NULL_HANDLE;
    }
    if (renderer->image_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(renderer->device, renderer->image_sampler, NULL);
        renderer->image_sampler = VK_NULL_HANDLE;
    }
    if (renderer->image_descriptor_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(renderer->device, renderer->image_descriptor_layout, NULL);
        renderer->image_descriptor_layout = VK_NULL_HANDLE;
    }
    memset(renderer->image_descriptor_sets, 0, sizeof(renderer->image_descriptor_sets));
}

static AfResult af_vk_create_image_descriptors(AfVkRenderer *renderer) {
    const VkDescriptorSetLayoutBinding binding = {.binding = 0U,
                                                  .descriptorType =
                                                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                  .descriptorCount = 1U,
                                                  .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};
    const VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1U,
        .pBindings = &binding};
    const VkDescriptorPoolSize pool_size = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                            .descriptorCount = AF_MAX_IMAGE_ATLASES};
    const VkDescriptorPoolCreateInfo pool_info = {.sType =
                                                      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                  .maxSets = AF_MAX_IMAGE_ATLASES,
                                                  .poolSizeCount = 1U,
                                                  .pPoolSizes = &pool_size};
    const VkSamplerCreateInfo sampler_info = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                              .magFilter = VK_FILTER_LINEAR,
                                              .minFilter = VK_FILTER_LINEAR,
                                              .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                              .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                              .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                              .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                              .maxLod = (float)AF_IMAGE_MAX_MIPS};
    VkDescriptorSetLayout layouts[AF_MAX_IMAGE_ATLASES];
    VkDescriptorSetAllocateInfo allocate_info = {.sType =
                                                     VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                 .descriptorSetCount = AF_MAX_IMAGE_ATLASES};
    if (renderer->image_descriptor_layout != VK_NULL_HANDLE) {
        return AF_RESULT_SUCCESS;
    }
    if (vkCreateDescriptorSetLayout(renderer->device, &layout_info, NULL,
                                    &renderer->image_descriptor_layout) != VK_SUCCESS ||
        vkCreateDescriptorPool(renderer->device, &pool_info, NULL,
                               &renderer->image_descriptor_pool) != VK_SUCCESS ||
        vkCreateSampler(renderer->device, &sampler_info, NULL, &renderer->image_sampler) !=
            VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    for (uint32_t index = 0U; index < AF_MAX_IMAGE_ATLASES; ++index) {
        layouts[index] = renderer->image_descriptor_layout;
    }
    allocate_info.descriptorPool = renderer->image_descriptor_pool;
    allocate_info.pSetLayouts = layouts;
    if (vkAllocateDescriptorSets(renderer->device, &allocate_info,
                                 renderer->image_descriptor_sets) != VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    return AF_RESULT_SUCCESS;
}

static AfResult af_vk_create_image_pipeline(AfVkRenderer *renderer) {
    VkShaderModule vertex_shader = VK_NULL_HANDLE;
    VkShaderModule fragment_shader = VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo stages[2];
    const uint32_t output_is_srgb = af_vk_format_is_srgb(renderer->swapchain_format) ? 1U : 0U;
    const VkSpecializationMapEntry fragment_specialization_entry = {
        .constantID = 0U, .offset = 0U, .size = sizeof(output_is_srgb)};
    const VkSpecializationInfo fragment_specialization = {
        .mapEntryCount = 1U,
        .pMapEntries = &fragment_specialization_entry,
        .dataSize = sizeof(output_is_srgb),
        .pData = &output_is_srgb};
    const VkVertexInputBindingDescription binding = {.binding = 0U,
                                                     .stride = sizeof(AfGpuImageVertex),
                                                     .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    const VkVertexInputAttributeDescription attributes[3] = {
        {.location = 0U,
         .binding = 0U,
         .format = VK_FORMAT_R32G32_SFLOAT,
         .offset = offsetof(AfGpuImageVertex, position)},
        {.location = 1U,
         .binding = 0U,
         .format = VK_FORMAT_R32G32_SFLOAT,
         .offset = offsetof(AfGpuImageVertex, uv)},
        {.location = 2U,
         .binding = 0U,
         .format = VK_FORMAT_R32G32B32A32_SFLOAT,
         .offset = offsetof(AfGpuImageVertex, color)}};
    const VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1U,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = 3U,
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
    VkPipelineLayoutCreateInfo layout_info = {.sType =
                                                  VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                              .setLayoutCount = 1U,
                                              .pSetLayouts = &renderer->image_descriptor_layout,
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
    AfResult result = af_vk_create_image_descriptors(renderer);
    if (result == AF_RESULT_SUCCESS) {
        result = af_vk_load_shader(renderer, ARCFLOW_SHADER_DIR "/image.vert.spv", &vertex_shader);
    }
    if (result == AF_RESULT_SUCCESS) {
        result =
            af_vk_load_shader(renderer, ARCFLOW_SHADER_DIR "/image.frag.spv", &fragment_shader);
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
        .pName = "main",
        .pSpecializationInfo = &fragment_specialization};
    if (vkCreatePipelineLayout(renderer->device, &layout_info, NULL,
                               &renderer->image_pipeline_layout) != VK_SUCCESS) {
        result = AF_RESULT_VULKAN_ERROR;
    } else {
        pipeline_info.layout = renderer->image_pipeline_layout;
        if (vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1U, &pipeline_info, NULL,
                                      &renderer->image_pipeline) != VK_SUCCESS) {
            result = AF_RESULT_VULKAN_ERROR;
        }
    }
    vkDestroyShaderModule(renderer->device, fragment_shader, NULL);
    vkDestroyShaderModule(renderer->device, vertex_shader, NULL);
    return result;
}

static AfResult af_vk_upload_image_atlas(AfVkRenderer *renderer, const AfImageAtlas *source,
                                         uint32_t atlas_index) {
    AfVkImageAtlasResource *target = &renderer->image_atlases[atlas_index];
    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_memory = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    void *mapping = NULL;
    VkMemoryRequirements requirements;
    uint32_t memory_type = UINT32_MAX;
    VkBufferCreateInfo buffer_info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                      .size = source->pixel_size,
                                      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                      .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = source->color_space == AF_IMAGE_COLOR_SRGB ? VK_FORMAT_R8G8B8A8_SRGB
                                                             : VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {.width = source->width, .height = source->height, .depth = 1U},
        .mipLevels = source->mip_count,
        .arrayLayers = 1U,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
    VkMemoryAllocateInfo allocation = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    VkImageViewCreateInfo view_info = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                       .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                       .format = image_info.format,
                                       .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                            .baseMipLevel = 0U,
                                                            .levelCount = source->mip_count,
                                                            .baseArrayLayer = 0U,
                                                            .layerCount = 1U}};
    VkCommandPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                         .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                                         .queueFamilyIndex = renderer->graphics_family};
    VkCommandBufferAllocateInfo command_info = {.sType =
                                                    VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                .commandBufferCount = 1U};
    VkCommandBufferBeginInfo begin_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                           .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    VkBufferImageCopy copies[AF_IMAGE_MAX_MIPS];
    VkImageMemoryBarrier2 barrier = {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                     .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                                     .srcAccessMask = VK_ACCESS_2_NONE,
                                     .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                                     .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                     .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                     .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                     .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                     .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                     .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                          .baseMipLevel = 0U,
                                                          .levelCount = source->mip_count,
                                                          .baseArrayLayer = 0U,
                                                          .layerCount = 1U}};
    VkDependencyInfo dependency = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                   .imageMemoryBarrierCount = 1U,
                                   .pImageMemoryBarriers = &barrier};
    VkCommandBufferSubmitInfo command_submit = {.sType =
                                                    VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    VkSubmitInfo2 submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                            .commandBufferInfoCount = 1U,
                            .pCommandBufferInfos = &command_submit};
    AfResult result = AF_RESULT_VULKAN_ERROR;
    if (vkCreateBuffer(renderer->device, &buffer_info, NULL, &staging_buffer) != VK_SUCCESS) {
        goto cleanup;
    }
    vkGetBufferMemoryRequirements(renderer->device, staging_buffer, &requirements);
    memory_type = af_vk_find_memory_type(renderer, requirements.memoryTypeBits,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memory_type == UINT32_MAX) {
        result = AF_RESULT_UNSUPPORTED_FEATURE;
        goto cleanup;
    }
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memory_type;
    if (vkAllocateMemory(renderer->device, &allocation, NULL, &staging_memory) != VK_SUCCESS ||
        vkBindBufferMemory(renderer->device, staging_buffer, staging_memory, 0U) != VK_SUCCESS ||
        vkMapMemory(renderer->device, staging_memory, 0U, source->pixel_size, 0U, &mapping) !=
            VK_SUCCESS) {
        goto cleanup;
    }
    memcpy(mapping, source->pixels, source->pixel_size);
    vkUnmapMemory(renderer->device, staging_memory);
    mapping = NULL;
    if (vkCreateImage(renderer->device, &image_info, NULL, &target->image) != VK_SUCCESS) {
        goto cleanup;
    }
    vkGetImageMemoryRequirements(renderer->device, target->image, &requirements);
    memory_type = af_vk_find_memory_type(renderer, requirements.memoryTypeBits,
                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memory_type == UINT32_MAX) {
        result = AF_RESULT_UNSUPPORTED_FEATURE;
        goto cleanup;
    }
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memory_type;
    if (vkAllocateMemory(renderer->device, &allocation, NULL, &target->memory) != VK_SUCCESS ||
        vkBindImageMemory(renderer->device, target->image, target->memory, 0U) != VK_SUCCESS) {
        goto cleanup;
    }
    view_info.image = target->image;
    if (vkCreateImageView(renderer->device, &view_info, NULL, &target->view) != VK_SUCCESS ||
        vkCreateCommandPool(renderer->device, &pool_info, NULL, &command_pool) != VK_SUCCESS) {
        goto cleanup;
    }
    command_info.commandPool = command_pool;
    if (vkAllocateCommandBuffers(renderer->device, &command_info, &command_buffer) != VK_SUCCESS ||
        vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
        goto cleanup;
    }
    {
        uint32_t width = source->width;
        uint32_t height = source->height;
        for (uint32_t mip = 0U; mip < source->mip_count; ++mip) {
            copies[mip] =
                (VkBufferImageCopy){.bufferOffset = source->mip_offsets[mip],
                                    .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                         .mipLevel = mip,
                                                         .baseArrayLayer = 0U,
                                                         .layerCount = 1U},
                                    .imageExtent = {.width = width, .height = height, .depth = 1U}};
            width = width > 1U ? width / 2U : 1U;
            height = height > 1U ? height / 2U : 1U;
        }
    }
    barrier.image = target->image;
    vkCmdPipelineBarrier2(command_buffer, &dependency);
    vkCmdCopyBufferToImage(command_buffer, staging_buffer, target->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, source->mip_count, copies);
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier2(command_buffer, &dependency);
    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        goto cleanup;
    }
    command_submit.commandBuffer = command_buffer;
    if (vkQueueSubmit2(renderer->graphics_queue, 1U, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
        vkQueueWaitIdle(renderer->graphics_queue) != VK_SUCCESS) {
        goto cleanup;
    }
    {
        const VkDescriptorImageInfo descriptor = {.sampler = renderer->image_sampler,
                                                  .imageView = target->view,
                                                  .imageLayout =
                                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        const VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                            .dstSet = renderer->image_descriptor_sets[atlas_index],
                                            .dstBinding = 0U,
                                            .descriptorCount = 1U,
                                            .descriptorType =
                                                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                            .pImageInfo = &descriptor};
        vkUpdateDescriptorSets(renderer->device, 1U, &write, 0U, NULL);
    }
    result = AF_RESULT_SUCCESS;

cleanup:
    if (mapping != NULL) {
        vkUnmapMemory(renderer->device, staging_memory);
    }
    if (command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(renderer->device, command_pool, NULL);
    }
    if (staging_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(renderer->device, staging_buffer, NULL);
    }
    if (staging_memory != VK_NULL_HANDLE) {
        vkFreeMemory(renderer->device, staging_memory, NULL);
    }
    return result;
}

static AfResult af_vk_update_image_atlases(AfVkRenderer *renderer, const AfImageData *images) {
    AfResult result = AF_RESULT_SUCCESS;
    if (renderer->image_source == images &&
        (images == NULL || renderer->image_generation == images->generation)) {
        return AF_RESULT_SUCCESS;
    }
    if (vkDeviceWaitIdle(renderer->device) != VK_SUCCESS) {
        return AF_RESULT_VULKAN_ERROR;
    }
    af_vk_destroy_image_atlases(renderer);
    if (images == NULL || images->asset_count == 0U) {
        renderer->image_source = images;
        renderer->image_generation = images == NULL ? 0U : images->generation;
        return AF_RESULT_SUCCESS;
    }
    if (images->atlas_count == 0U || images->atlas_count > AF_MAX_IMAGE_ATLASES) {
        return AF_RESULT_INVALID_ARGUMENT;
    }
    for (uint32_t index = 0U; index < images->atlas_count; ++index) {
        if (images->atlases[index].pixel_size == 0U) {
            continue;
        }
        result = af_vk_upload_image_atlas(renderer, &images->atlases[index], index);
        if (result != AF_RESULT_SUCCESS) {
            af_vk_destroy_image_atlases(renderer);
            return result;
        }
    }
    renderer->image_source = images;
    renderer->image_generation = images->generation;
    return AF_RESULT_SUCCESS;
}
