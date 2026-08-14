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
        char resource_path[1024];
        const char *name = strrchr(path, '/');
        char relative_path[256];
        name = name == NULL ? path : name + 1;
        if (snprintf(relative_path, sizeof(relative_path), "shaders/%s", name) > 0 &&
            af_platform_resource_path(relative_path, resource_path, sizeof(resource_path)) ==
                AF_RESULT_SUCCESS) {
            file = fopen(resource_path, "rb");
        }
    }
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
        (VkDeviceSize)AF_MAX_GPU_PATH_VERTICES * sizeof(AfGpuPathVertex) +
        (VkDeviceSize)AF_MAX_GPU_IMAGE_VERTICES * sizeof(AfGpuImageVertex);
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
        result = af_vk_create_image_pipeline(renderer);
    }
    if (result == AF_RESULT_SUCCESS) {
        result = af_vk_particles_set_render_target(renderer->particles, renderer->swapchain_format,
                                                   renderer->sample_count);
    }
    renderer->resize_requested = result != AF_RESULT_SUCCESS;
    return result;
}
